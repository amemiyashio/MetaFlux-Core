#include "metaflux/transport/cdev_worker.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace metaflux::transport::cdev {
namespace {

constexpr std::uint32_t kCompletionOpcode = MF_RING_OPCODE_COMPLETION;

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

}  // namespace

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
  return produce(view_.completion, completion) ? WorkerResult::Completed : WorkerResult::Backpressure;
}

WorkerResult CdevWorker::consume_once() noexcept {
  mf_ring_descriptor_v1 request{};
  if (!valid_queue(view_.submission) || !valid_queue(view_.completion) || view_.payload == nullptr ||
      view_.payload_size == 0U || view_.generation == 0U) {
    return WorkerResult::Malformed;
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

}  // namespace metaflux::transport::cdev
