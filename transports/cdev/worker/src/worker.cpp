#include "metaflux/transport/cdev_worker.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace metaflux::transport::cdev {
namespace {

constexpr std::uint32_t kCompletionOpcode = MF_RING_OPCODE_COMPLETION;
constexpr std::uint32_t kBackendCopyRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, copy) + sizeof(((mf_backend_api_v1*)nullptr)->copy));
constexpr std::uint32_t kBackendLaunchRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, submit) + sizeof(((mf_backend_api_v1*)nullptr)->submit));
constexpr std::uint32_t kBackendCancellationRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, cancel_queue) + sizeof(((mf_backend_api_v1*)nullptr)->cancel_queue));

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

bool CdevWorker::valid_copy_backend(const CdevBackendBinding& backend) noexcept {
  return backend.api != nullptr && backend.instance != 0U && backend.queue != 0U &&
         backend.memory != 0U &&
         valid_backend_event(backend) &&
         mf_backend_api_validate_v1(backend.api, kBackendCopyRequiredSize, MF_BACKEND_CAP_COPY) ==
             MF_BACKEND_SUCCESS &&
         backend.api->copy != nullptr;
}

bool CdevWorker::valid_region_copy_backend(const CdevBackendBinding& backend) noexcept {
  return backend.api != nullptr && backend.instance != 0U && backend.queue != 0U &&
         backend.copy_resolver != nullptr &&
         valid_backend_event(backend) &&
         mf_backend_api_validate_v1(backend.api, kBackendCopyRequiredSize, MF_BACKEND_CAP_COPY) ==
             MF_BACKEND_SUCCESS &&
         backend.api->copy != nullptr;
}

bool CdevWorker::valid_memory_reference(
    mf_backend_memory_v1 memory, const CdevBackendMemoryReference& reference) noexcept {
  return memory != 0U && reference.handle == memory && reference.retain != nullptr &&
         reference.release != nullptr;
}

bool CdevWorker::valid_copy_resolution(const CdevCopyResolution& resolution) noexcept {
  return valid_memory_reference(resolution.destination, resolution.destination_reference) &&
         valid_memory_reference(resolution.source, resolution.source_reference) &&
         resolution.byte_count != 0U &&
         resolution.destination_offset <= UINT64_MAX - resolution.byte_count &&
         resolution.source_offset <= UINT64_MAX - resolution.byte_count &&
         resolution.reserved_word == 0U;
}

bool CdevWorker::valid_backend_cancellation(const CdevBackendBinding& backend) noexcept {
  return backend.api != nullptr &&
         mf_backend_api_validate_v1(backend.api, kBackendCancellationRequiredSize,
                                    MF_BACKEND_CAP_CANCELLATION) == MF_BACKEND_SUCCESS &&
         backend.api->cancel_queue != nullptr;
}

bool CdevWorker::valid_launch_backend(const CdevBackendBinding& backend) noexcept {
  return backend.api != nullptr && backend.instance != 0U && backend.queue != 0U &&
         backend.memory != 0U && backend.launch_resolver != nullptr &&
         valid_backend_event(backend) &&
         mf_backend_api_validate_v1(backend.api, kBackendLaunchRequiredSize,
                                    MF_BACKEND_CAP_LAUNCH) == MF_BACKEND_SUCCESS &&
         backend.api->submit != nullptr;
}

bool CdevWorker::valid_backend_lease(const CdevBackendBinding& backend) noexcept {
  return backend.lease_acquire != nullptr && backend.lease_release != nullptr;
}

mf_shared_status_v1
CdevWorker::retain_copy_references(CdevCopyResolution& resolution) noexcept {
  const CdevBackendMemoryReference* references[] = {
      &resolution.destination_reference,
      &resolution.source_reference,
  };
  std::size_t retained = 0U;
  for (const auto* reference : references) {
    const mf_shared_status_v1 status = reference->retain(reference->context, reference->handle);
    if (status != MF_SHARED_SUCCESS) {
      while (retained > 0U) {
        --retained;
        const auto* retained_reference = references[retained];
        retained_reference->release(retained_reference->context, retained_reference->handle);
      }
      return status;
    }
    ++retained;
  }
  return MF_SHARED_SUCCESS;
}

void CdevWorker::release_copy_references(const CdevCopyResolution& resolution) noexcept {
  const CdevBackendMemoryReference* references[] = {
      &resolution.destination_reference,
      &resolution.source_reference,
  };
  for (const auto* reference : references) {
    if (reference->release != nullptr && reference->handle != 0U) {
      reference->release(reference->context, reference->handle);
    }
  }
}

bool CdevWorker::valid_backend_event(const CdevBackendBinding& backend) noexcept {
  return backend.completion_event == 0U ||
         (backend.api != nullptr &&
          mf_backend_api_validate_v1(backend.api, offsetof(mf_backend_api_v1, query_event) +
                                                   sizeof(backend.api->query_event),
                                     MF_BACKEND_CAP_EVENTS) == MF_BACKEND_SUCCESS &&
          backend.api->query_event != nullptr);
}

bool CdevWorker::valid_backend(const CdevBackendBinding& backend) noexcept {
  return valid_backend_lease(backend) &&
         (valid_copy_backend(backend) || valid_region_copy_backend(backend) ||
          valid_launch_backend(backend));
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

mf_backend_status_v1
CdevWorker::dispatch_region_copy(const CdevCopyResolution& resolution) const noexcept {
  if (!valid_region_copy_backend(backend_) || !valid_copy_resolution(resolution)) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  mf_backend_copy_v1 copy{};
  copy.struct_size = sizeof(copy);
  copy.destination = resolution.destination;
  copy.destination_offset = resolution.destination_offset;
  copy.source = resolution.source;
  copy.source_offset = resolution.source_offset;
  copy.byte_count = resolution.byte_count;
  return backend_.api->copy(backend_.instance, backend_.queue, &copy, backend_.completion_event);
}

mf_shared_status_v1
CdevWorker::dispatch_launch(const mf_ring_descriptor_v1& request) const noexcept {
  if (!valid_launch_backend(backend_)) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  CdevLaunchResolution resolution{};
  const mf_shared_status_v1 resolve_status =
      backend_.launch_resolver(backend_.launch_context, &request, &resolution);
  if (resolve_status != MF_SHARED_SUCCESS) {
    return resolve_status;
  }
  if (resolution.module == 0U || resolution.kernel_id != MF_KERNEL_PRIMARY_ENTRY_ID ||
      resolution.argument_size == 0U || resolution.argument_offset > view_.payload_size ||
      resolution.argument_size > view_.payload_size - resolution.argument_offset ||
      resolution.grid[0] == 0U || resolution.grid[1] == 0U || resolution.grid[2] != 1U ||
      resolution.block[0] == 0U || resolution.block[1] == 0U || resolution.block[2] != 1U ||
      resolution.reserved_word != 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_backend_launch_v1 launch{};
  launch.struct_size = sizeof(launch);
  launch.module = resolution.module;
  launch.kernel_id = resolution.kernel_id;
  launch.argument_bytes = view_.payload + resolution.argument_offset;
  launch.argument_size = resolution.argument_size;
  for (std::size_t index = 0; index < 3U; ++index) {
    launch.grid[index] = resolution.grid[index];
    launch.block[index] = resolution.block[index];
  }
  launch.dynamic_shared_bytes = resolution.dynamic_shared_bytes;
  return map_backend_status(
      backend_.api->submit(backend_.instance, backend_.queue, &launch, backend_.completion_event));
}

mf_shared_status_v1 CdevWorker::acquire_backend_lease() const noexcept {
  if (backend_.api == nullptr) {
    return MF_SHARED_SUCCESS;
  }
  if (!valid_backend(backend_)) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  return backend_.lease_acquire(backend_.lease_context);
}

bool CdevWorker::cancel_pending() noexcept {
  if (!pending_.active || pending_.cancellation_requested) {
    return pending_.cancellation_requested;
  }
  const CdevBackendBinding pending_backend = pending_.backend;
  if (!valid_backend_cancellation(pending_backend)) {
    return false;
  }
  const mf_backend_status_v1 status = pending_backend.api->cancel_queue(
      pending_backend.instance, pending_backend.queue);
  if (status == MF_BACKEND_SUCCESS || status == MF_BACKEND_DEVICE_LOST) {
    pending_.cancellation_requested = true;
    return true;
  }
  return false;
}

void CdevWorker::release_backend_lease() const noexcept {
  release_backend_lease(backend_);
}

void CdevWorker::release_backend_lease(const CdevBackendBinding& backend) const noexcept {
  if (backend.api != nullptr && valid_backend_lease(backend)) {
    backend.lease_release(backend.lease_context);
  }
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
  if (timeline_ == UINT64_MAX) {
    return WorkerResult::Malformed;
  }
  const std::uint64_t next_timeline = timeline_ + 1U;
  completion.opcode = kCompletionOpcode;
  completion.request_id = request.request_id;
  completion.target_id = request.target_id;
  completion.arguments[0] = static_cast<std::uint64_t>(static_cast<std::uint32_t>(status));
  completion.arguments[1] = next_timeline;
  if (!produce(view_.completion, completion)) {
    return WorkerResult::Backpressure;
  }
  timeline_ = next_timeline;
  return WorkerResult::Completed;
}

WorkerResult CdevWorker::finish_backend_request(const mf_ring_descriptor_v1& request,
                                                mf_shared_status_v1 status,
                                                const CdevCopyResolution* resolution) noexcept {
  if (status != MF_SHARED_SUCCESS || backend_.completion_event == 0U) {
    if (resolution != nullptr) {
      release_copy_references(*resolution);
    }
    release_backend_lease();
    return complete(request, status);
  }
  pending_.active = true;
  pending_.request = request;
  pending_.backend = backend_;
  if (resolution != nullptr) {
    pending_.resolution = *resolution;
    pending_.has_memory_references = true;
  }
  pending_.event = backend_.completion_event;
  return WorkerResult::Idle;
}

WorkerResult CdevWorker::progress_pending() noexcept {
  if (!pending_.active) {
    return WorkerResult::Idle;
  }
  const CdevBackendBinding pending_backend = pending_.backend;
  if (pending_.cancellation_requested) {
    const mf_ring_descriptor_v1 request = pending_.request;
    const WorkerResult result = complete(request, MF_SHARED_DEVICE_LOST);
    if (result != WorkerResult::Backpressure) {
      const bool has_memory_references = pending_.has_memory_references;
      const CdevCopyResolution pending_resolution = pending_.resolution;
      pending_ = {};
      if (has_memory_references) {
        release_copy_references(pending_resolution);
      }
      release_backend_lease(pending_backend);
    }
    return result;
  }
  if (!valid_backend(pending_backend) || pending_backend.completion_event != pending_.event ||
      pending_backend.api->query_event == nullptr) {
    const mf_ring_descriptor_v1 request = pending_.request;
    const bool has_memory_references = pending_.has_memory_references;
    const CdevCopyResolution pending_resolution = pending_.resolution;
    pending_ = {};
    if (has_memory_references) {
      release_copy_references(pending_resolution);
    }
    release_backend_lease(pending_backend);
    return complete(request, MF_SHARED_NOT_SUPPORTED);
  }
  std::uint32_t complete_flag = 0U;
  const mf_backend_status_v1 query_status =
      pending_backend.api->query_event(pending_backend.instance, pending_.event, &complete_flag);
  if ((query_status == MF_BACKEND_SUCCESS && complete_flag == 0U) ||
      query_status == MF_BACKEND_BUSY) {
    return WorkerResult::Idle;
  }
  const mf_shared_status_v1 status =
      query_status == MF_BACKEND_SUCCESS ? MF_SHARED_SUCCESS : map_backend_status(query_status);
  const mf_ring_descriptor_v1 request = pending_.request;
  const WorkerResult result = complete(request, status);
  if (result != WorkerResult::Backpressure) {
    const bool has_memory_references = pending_.has_memory_references;
    const CdevCopyResolution pending_resolution = pending_.resolution;
    pending_ = {};
    if (has_memory_references) {
      release_copy_references(pending_resolution);
    }
    release_backend_lease(pending_backend);
  }
  return result;
}

WorkerResult CdevWorker::consume_once() noexcept {
  mf_ring_descriptor_v1 request{};
  if (!valid_queue(view_.submission) || !valid_queue(view_.completion) ||
      view_.payload == nullptr || view_.payload_size == 0U) {
    return WorkerResult::Malformed;
  }
  if (pending_.active) {
    return progress_pending();
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
  const bool region_copy = request.opcode == MF_RING_OPCODE_COPY &&
                           request.flags == MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1;
  if (!region_copy && request.target_id != view_.generation) {
    return complete(request, MF_SHARED_STALE_HANDLE);
  }
  if (request.opcode == MF_RING_OPCODE_NOOP) {
    return complete(request, MF_SHARED_SUCCESS);
  }
  if (request.opcode == MF_RING_OPCODE_LAUNCH) {
    if (request.flags != 0U) {
      return complete(request, MF_SHARED_MALFORMED);
    }
    const mf_shared_status_v1 lease_status = acquire_backend_lease();
    if (lease_status != MF_SHARED_SUCCESS) {
      return complete(request, lease_status);
    }
    const mf_shared_status_v1 status = dispatch_launch(request);
    return finish_backend_request(request, status);
  }
  if (request.opcode != MF_RING_OPCODE_COPY) {
    return complete(request, MF_SHARED_NOT_SUPPORTED);
  }
  if ((request.flags & ~MF_RING_COPY_KNOWN_FLAGS_V1) != 0U) {
    return complete(request, MF_SHARED_MALFORMED);
  }
  if ((request.flags & (MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 |
                        MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1)) != 0U) {
    return complete(request, MF_SHARED_NOT_SUPPORTED);
  }
  if (request.flags == MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1) {
    if (request.target_id == 0U || request.arguments[0] == 0U || request.arguments[2] != 0U ||
        request.arguments[3] != 0U) {
      return complete(request, MF_SHARED_MALFORMED);
    }
    if (!valid_region_copy_backend(backend_)) {
      return complete(request, MF_SHARED_NOT_SUPPORTED);
    }
    const mf_shared_status_v1 lease_status = acquire_backend_lease();
    if (lease_status != MF_SHARED_SUCCESS) {
      return complete(request, lease_status);
    }
    CdevCopyResolution resolution{};
    const mf_shared_status_v1 resolve_status =
        backend_.copy_resolver(backend_.copy_context, &request, &resolution);
    if (resolve_status != MF_SHARED_SUCCESS) {
      return finish_backend_request(request, resolve_status);
    }
    if (!valid_copy_resolution(resolution)) {
      return finish_backend_request(request, MF_SHARED_INVALID_ARGUMENT);
    }
    const mf_shared_status_v1 retain_status = retain_copy_references(resolution);
    if (retain_status != MF_SHARED_SUCCESS) {
      return finish_backend_request(request, retain_status);
    }
    const mf_shared_status_v1 status = map_backend_status(dispatch_region_copy(resolution));
    return finish_backend_request(request, status, &resolution);
  }
  if (request.flags != 0U || request.arguments[3] > view_.payload_size) {
    return complete(request, MF_SHARED_NOT_SUPPORTED);
  }
  const auto base = request.arguments[3];
  const auto destination = request.arguments[0];
  const auto source = request.arguments[1];
  const auto byte_count = request.arguments[2];
  if (byte_count == 0U) {
    return complete(request, MF_SHARED_INVALID_ARGUMENT);
  }
  if (destination > view_.payload_size - base || source > view_.payload_size - base ||
      byte_count > view_.payload_size - base - destination ||
      byte_count > view_.payload_size - base - source) {
    return complete(request, MF_SHARED_INVALID_ARGUMENT);
  }
  if (backend_.api != nullptr) {
    if (request.flags != 0U || !valid_copy_backend(backend_)) {
      return complete(request, MF_SHARED_NOT_SUPPORTED);
    }
    const mf_shared_status_v1 lease_status = acquire_backend_lease();
    if (lease_status != MF_SHARED_SUCCESS) {
      return complete(request, lease_status);
    }
    const mf_shared_status_v1 status =
        map_backend_status(dispatch_copy(base, destination, source, byte_count));
    return finish_backend_request(request, status);
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
  if (pending_.active && consume_once() != WorkerResult::Completed) {
    lifecycle_accepting_ = accepting;
    return false;
  }
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
  if (!worker->pending_.active) {
    return true;
  }
  return worker->cancel_pending();
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
    (void)worker->cancel_pending();
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
