#include "metaflux/transport/cdev_worker.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace metaflux::transport::cdev {
namespace {

constexpr std::uint32_t kCompletionOpcode = MF_RING_OPCODE_COMPLETION;
constexpr std::uint32_t kBackendCopyRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, copy) + sizeof(((mf_backend_api_v1*)nullptr)->copy));

bool valid_queue(const mf_ring_header_v1* header) noexcept {
  if (header == nullptr) {
    return false;
  }
  const auto capacity = header->metadata.capacity;
  return header->metadata.magic == MF_SHARED_RING_MAGIC &&
         header->metadata.abi_version == MF_SHARED_DEVICE_ABI_VERSION_1 &&
         header->metadata.header_size == sizeof(mf_ring_header_v1) &&
         header->metadata.descriptor_size == sizeof(mf_ring_descriptor_v1) && capacity >= 2U &&
         (capacity & (capacity - 1U)) == 0U;
}

mf_ring_descriptor_v1* descriptor_at(mf_ring_header_v1* header, std::uint64_t position) noexcept {
  auto* bytes = reinterpret_cast<std::uint8_t*>(header);
  return reinterpret_cast<mf_ring_descriptor_v1*>(bytes + sizeof(mf_ring_header_v1)) +
         (position & static_cast<std::uint64_t>(header->metadata.capacity - 1U));
}

const mf_ring_descriptor_v1* descriptor_at(const mf_ring_header_v1* header,
                                           std::uint64_t position) noexcept {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(header);
  return reinterpret_cast<const mf_ring_descriptor_v1*>(bytes + sizeof(mf_ring_header_v1)) +
         (position & static_cast<std::uint64_t>(header->metadata.capacity - 1U));
}

} // namespace

bool CdevWorker::valid_backend(const CdevBackendBinding& backend) noexcept {
  return backend.api != nullptr && backend.instance != 0U && backend.queue != 0U &&
         backend.memory != 0U &&
         mf_backend_api_validate_v1(backend.api, kBackendCopyRequiredSize, MF_BACKEND_CAP_COPY) ==
             MF_BACKEND_SUCCESS &&
         backend.api->copy != nullptr;
}

bool CdevWorker::backend_bound() const noexcept { return valid_backend(backend_); }

std::int32_t CdevWorker::map_backend_status(mf_backend_status_v1 status) noexcept {
  switch (status) {
  case MF_BACKEND_SUCCESS:
    return MF_SHARED_SUCCESS;
  case MF_BACKEND_INVALID_ARGUMENT:
    return MF_SHARED_INVALID_ARGUMENT;
  case MF_BACKEND_UNSUPPORTED:
    return MF_SHARED_NOT_SUPPORTED;
  case MF_BACKEND_OUT_OF_MEMORY:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  case MF_BACKEND_DEVICE_LOST:
    return MF_SHARED_DEVICE_LOST;
  case MF_BACKEND_TIMEOUT:
    return MF_SHARED_TIMEOUT;
  case MF_BACKEND_BUSY:
    return MF_SHARED_WOULD_BLOCK;
  default:
    return MF_SHARED_SYSTEM_ERROR;
  }
}

mf_backend_status_v1 CdevWorker::dispatch_copy(std::uint64_t base, std::uint64_t destination,
                                               std::uint64_t source,
                                               std::uint64_t byte_count) const noexcept {
  if (!valid_backend(backend_) || base > UINT64_MAX - destination || base > UINT64_MAX - source) {
    return MF_BACKEND_UNSUPPORTED;
  }
  mf_backend_copy_v1 copy{};
  copy.struct_size = sizeof(copy);
  copy.destination = backend_.memory;
  copy.destination_offset = base + destination;
  copy.source = backend_.memory;
  copy.source_offset = base + source;
  copy.byte_count = byte_count;
  return backend_.api->copy(backend_.instance, backend_.queue, &copy, backend_.completion_event);
}

bool CdevWorker::queue_readable(const mf_ring_header_v1* header) noexcept {
  if (!valid_queue(header)) {
    return false;
  }
  const auto position = mf_atomic_load_u64_relaxed(&header->consumer.position);
  return mf_atomic_load_u64_acquire(&descriptor_at(header, position)->sequence) == position + 1U;
}

bool CdevWorker::queue_writable(const mf_ring_header_v1* header) noexcept {
  if (!valid_queue(header)) {
    return false;
  }
  const auto position = mf_atomic_load_u64_relaxed(&header->producer.position);
  return mf_atomic_load_u64_acquire(&descriptor_at(header, position)->sequence) == position;
}

bool CdevWorker::consume(mf_ring_header_v1* header, mf_ring_descriptor_v1* out) noexcept {
  if (!queue_readable(header) || out == nullptr) {
    return false;
  }
  const auto position = mf_atomic_load_u64_relaxed(&header->consumer.position);
  auto* slot = descriptor_at(header, position);
  std::memcpy(out, slot, sizeof(*out));
  mf_atomic_store_u64_release(&header->consumer.position, position + 1U);
  mf_atomic_store_u64_release(&slot->sequence,
                              position + static_cast<std::uint64_t>(header->metadata.capacity));
  return true;
}

bool CdevWorker::produce(mf_ring_header_v1* header,
                         const mf_ring_descriptor_v1& descriptor) noexcept {
  if (!queue_writable(header)) {
    return false;
  }
  const auto position = mf_atomic_load_u64_relaxed(&header->producer.position);
  auto* slot = descriptor_at(header, position);
  mf_ring_descriptor_v1 copy = descriptor;
  copy.sequence = position + 1U;
  std::memcpy(slot, &copy, sizeof(copy));
  mf_atomic_store_u64_release(&slot->sequence, position + 1U);
  mf_atomic_store_u64_release(&header->producer.position, position + 1U);
  return true;
}

WorkerResult CdevWorker::complete(const mf_ring_descriptor_v1& request,
                                  std::int32_t status) noexcept {
  mf_ring_descriptor_v1 completion{};
  completion.opcode = kCompletionOpcode;
  completion.request_id = request.request_id;
  completion.target_id = request.target_id;
  completion.arguments[0] = static_cast<std::uint64_t>(static_cast<std::uint32_t>(status));
  completion.arguments[1] = ++timeline_;
  return produce(view_.completion, completion) ? WorkerResult::Completed
                                               : WorkerResult::Backpressure;
}

WorkerResult CdevWorker::consume_once() noexcept {
  mf_ring_descriptor_v1 request{};
  if (!valid_queue(view_.submission) || !valid_queue(view_.completion) ||
      view_.payload == nullptr || view_.payload_size == 0U) {
    return WorkerResult::Malformed;
  }
  if (!lifecycle_online_) {
    if (!queue_readable(view_.submission)) {
      return WorkerResult::Idle;
    }
    if (!queue_writable(view_.completion) || !consume(view_.submission, &request)) {
      return WorkerResult::Backpressure;
    }
    return complete(request, MF_SHARED_DEVICE_LOST);
  }
  if (view_.generation == 0U) {
    return WorkerResult::Malformed;
  }
  if (!lifecycle_accepting_) {
    return WorkerResult::Idle;
  }
  if (!queue_readable(view_.submission)) {
    return WorkerResult::Idle;
  }
  if (!queue_writable(view_.completion)) {
    return WorkerResult::Backpressure;
  }
  if (!consume(view_.submission, &request)) {
    return WorkerResult::Idle;
  }
  if (request.target_id != view_.generation) {
    return complete(request, MF_SHARED_STALE_HANDLE);
  }
  if (request.opcode == MF_RING_OPCODE_NOOP) {
    return complete(request, MF_SHARED_SUCCESS);
  }
  if (request.opcode != MF_RING_OPCODE_COPY || request.arguments[3] > view_.payload_size) {
    return complete(request, MF_SHARED_NOT_SUPPORTED);
  }
  const auto base = request.arguments[3];
  const auto destination = request.arguments[0];
  const auto source = request.arguments[1];
  const auto byte_count = request.arguments[2];
  if (destination > view_.payload_size - base || source > view_.payload_size - base ||
      byte_count > view_.payload_size - base - destination ||
      byte_count > view_.payload_size - base - source) {
    return complete(request, MF_SHARED_INVALID_ARGUMENT);
  }
  if (backend_.api != nullptr) {
    if (request.flags != 0U || !valid_backend(backend_)) {
      return complete(request, MF_SHARED_NOT_SUPPORTED);
    }
    return complete(request,
                    map_backend_status(dispatch_copy(base, destination, source, byte_count)));
  }
  std::memmove(view_.payload + base + destination, view_.payload + base + source,
               static_cast<std::size_t>(byte_count));
  return complete(request, MF_SHARED_SUCCESS);
}

std::uint32_t CdevWorker::drain(std::uint32_t maximum) noexcept {
  std::uint32_t completed = 0U;
  while (completed < maximum && consume_once() == WorkerResult::Completed) {
    ++completed;
  }
  return completed;
}

bool CdevWorker::drain_lifecycle() noexcept {
  if (!valid_queue(view_.submission) || !valid_queue(view_.completion)) {
    return false;
  }
  const bool accepting = lifecycle_accepting_;
  lifecycle_accepting_ = true;
  const std::uint32_t capacity = view_.submission->metadata.capacity;
  for (std::uint32_t count = 0U; count < capacity; ++count) {
    if (!queue_readable(view_.submission)) {
      lifecycle_accepting_ = accepting;
      return true;
    }
    if (consume_once() != WorkerResult::Completed) {
      lifecycle_accepting_ = accepting;
      return false;
    }
  }
  const bool drained = !queue_readable(view_.submission);
  lifecycle_accepting_ = accepting;
  return drained;
}

bool CdevWorker::lifecycle_prepare(
    void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  if (worker == nullptr || !valid_queue(worker->view_.submission) ||
      !valid_queue(worker->view_.completion) || worker->view_.payload == nullptr ||
      worker->view_.payload_size == 0U) {
    return false;
  }
  if (event.request.operation != metaflux::runtime::lifecycle::Operation::Add &&
      event.candidate.generation == 0U) {
    return event.request.operation == metaflux::runtime::lifecycle::Operation::Remove;
  }
  return true;
}

bool CdevWorker::lifecycle_quiesce(
    void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  if (worker == nullptr ||
      (event.request.operation == metaflux::runtime::lifecycle::Operation::Reset &&
       !worker->lifecycle_online_)) {
    return false;
  }
  worker->lifecycle_accepting_ = false;
  return true;
}

bool CdevWorker::lifecycle_drain(void* context,
                                 const metaflux::runtime::lifecycle::MirrorEvent&) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  return worker != nullptr && worker->drain_lifecycle();
}

bool CdevWorker::lifecycle_commit(void* context,
                                  const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  if (worker == nullptr) {
    return false;
  }
  if (event.candidate.generation != 0U) {
    worker->view_.generation = event.candidate.generation;
  } else if (event.state_after == metaflux::runtime::lifecycle::State::Absent) {
    worker->view_.generation = 0U;
  }
  worker->lifecycle_online_ = event.state_after == metaflux::runtime::lifecycle::State::Online;
  worker->lifecycle_accepting_ = worker->lifecycle_online_;
  return true;
}

bool CdevWorker::lifecycle_abort(void* context,
                                 const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  if (worker == nullptr) {
    return false;
  }
  worker->lifecycle_online_ = event.state_before == metaflux::runtime::lifecycle::State::Online;
  worker->lifecycle_accepting_ = worker->lifecycle_online_;
  return true;
}

void CdevWorker::lifecycle_lost(void* context,
                                const metaflux::runtime::lifecycle::MirrorEvent&) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  if (worker != nullptr) {
    worker->lifecycle_online_ = false;
    worker->lifecycle_accepting_ = false;
  }
}

bool CdevWorker::attach_lifecycle(metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept {
  return coordinator.register_mirror(lifecycle_mirror());
}

metaflux::runtime::lifecycle::Mirror CdevWorker::lifecycle_mirror() noexcept {
  return metaflux::runtime::lifecycle::Mirror{
      .kind = metaflux::runtime::lifecycle::MirrorKind::Cdev,
      .name = "cdev",
      .context = this,
      .prepare = lifecycle_prepare,
      .quiesce = lifecycle_quiesce,
      .drain = lifecycle_drain,
      .commit = lifecycle_commit,
      .abort = lifecycle_abort,
      .publish_lost = lifecycle_lost,
  };
}

} // namespace metaflux::transport::cdev
