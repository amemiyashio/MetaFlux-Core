#include "metaflux/transport/cdev_worker.hpp"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "metaflux/uapi/transport.h"

namespace metaflux::transport::cdev {
namespace {

constexpr std::uint32_t kCompletionOpcode = MF_RING_OPCODE_COMPLETION;
constexpr std::uint32_t kBackendCopyRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, copy) + sizeof(((mf_backend_api_v1*)nullptr)->copy));
constexpr std::uint32_t kBackendLaunchRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, submit) + sizeof(((mf_backend_api_v1*)nullptr)->submit));
constexpr std::uint32_t kBackendCancellationRequiredSize = static_cast<std::uint32_t>(
    offsetof(mf_backend_api_v1, cancel_queue) + sizeof(((mf_backend_api_v1*)nullptr)->cancel_queue));
constexpr char kDefaultControlPath[] = "/dev/metafluxctl";
constexpr char kDefaultDataPath[] = "/dev/metaflux0";
constexpr std::uint32_t kCdevRingCapacity = 256U;
constexpr std::uint64_t kCdevSubmissionQueueId = 1U;
constexpr std::uint64_t kCdevCompletionQueueId = 2U;
constexpr std::uint64_t kPageSize = 4096U;
constexpr std::uint64_t kPayloadMmapOffset = 2U * kPageSize;
constexpr std::uint64_t kPayloadMaximumSize = 64U * 1024U * 1024U;

bool complete_memory_reference(const CdevBackendMemoryReference& reference) noexcept {
  return reference.handle != 0U && reference.retain != nullptr && reference.release != nullptr;
}

bool empty_memory_reference(const CdevBackendMemoryReference& reference) noexcept {
  return reference.handle == 0U && reference.retain == nullptr && reference.release == nullptr &&
         reference.context == nullptr;
}

void release_memory_reference_now(const CdevBackendMemoryReference& reference) noexcept {
  if (complete_memory_reference(reference)) {
    reference.release(reference.context, reference.handle);
  }
}

bool valid_copy_region_argument_block(const std::uint8_t* bytes, std::uint64_t byte_count,
                                      const mf_argument_entry_v1** out_entries) noexcept {
  if (out_entries == nullptr || bytes == nullptr || byte_count > SIZE_MAX ||
      reinterpret_cast<std::uintptr_t>(bytes) % alignof(mf_argument_block_header_v1) != 0U ||
      byte_count != sizeof(mf_argument_block_header_v1) +
                        3U * sizeof(mf_argument_entry_v1)) {
    return false;
  }
  const auto* header = reinterpret_cast<const mf_argument_block_header_v1*>(bytes);
  if (header->magic != MF_SHARED_ARGUMENT_BLOCK_MAGIC ||
      header->abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      header->header_size != sizeof(mf_argument_block_header_v1) ||
      header->entry_size != sizeof(mf_argument_entry_v1) ||
      header->entry_count != MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 ||
      header->flags != MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1 || header->total_size != byte_count) {
    return false;
  }
  for (const auto reserved : header->reserved) {
    if (reserved != 0U) {
      return false;
    }
  }
  const auto* entries = reinterpret_cast<const mf_argument_entry_v1*>(
      bytes + sizeof(mf_argument_block_header_v1));
  const auto& destination = entries[MF_COPY_REGION_DESTINATION_INDEX_V1];
  const auto& source = entries[MF_COPY_REGION_SOURCE_INDEX_V1];
  const auto& count = entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1];
  if (destination.kind != MF_ARGUMENT_KIND_BUFFER || destination.flags != MF_ARGUMENT_BUFFER_WRITE ||
      destination.object_id == 0U || destination.object_generation == 0U ||
      source.kind != MF_ARGUMENT_KIND_BUFFER || source.flags != MF_ARGUMENT_BUFFER_READ ||
      source.object_id == 0U || source.object_generation == 0U || count.kind != MF_ARGUMENT_KIND_U64 ||
      count.flags != 0U || count.object_id != 0U || count.object_generation != 0U ||
      count.value == 0U) {
    return false;
  }
  *out_entries = entries;
  return true;
}

bool bytes_zero(const std::uint8_t* bytes, std::size_t size) noexcept {
  if (bytes == nullptr) {
    return false;
  }
  for (std::size_t index = 0U; index < size; ++index) {
    if (bytes[index] != 0U) {
      return false;
    }
  }
  return true;
}

bool ring_mapping_size(std::uint32_t capacity, std::uint64_t& out) noexcept {
  if (capacity < 2U || (capacity & (capacity - 1U)) != 0U) {
    return false;
  }
  const std::uint64_t descriptor_bytes =
      static_cast<std::uint64_t>(capacity) * sizeof(mf_ring_descriptor_v1);
  if (descriptor_bytes > UINT64_MAX - sizeof(mf_ring_header_v1)) {
    return false;
  }
  out = sizeof(mf_ring_header_v1) + descriptor_bytes;
  if (out > UINT64_MAX - (kPageSize - 1U)) {
    return false;
  }
  out = (out + (kPageSize - 1U)) & ~(kPageSize - 1U);
  return true;
}

mf_shared_status_v1 map_open_error(int error) noexcept {
  switch (error) {
  case ENOENT:
  case ENODEV:
  case ENOTTY:
  case EOPNOTSUPP:
    return MF_SHARED_NOT_SUPPORTED;
  case EBUSY:
    return MF_SHARED_WOULD_BLOCK;
  case ESTALE:
    return MF_SHARED_STALE_HANDLE;
  case EACCES:
  case EPERM:
    return MF_SHARED_PERMISSION_DENIED;
  case ENOMEM:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  case EINVAL:
    return MF_SHARED_INVALID_ARGUMENT;
  case EAGAIN:
    return MF_SHARED_RETRY;
  default:
    return MF_SHARED_SYSTEM_ERROR;
  }
}

mf_shared_status_v1 map_memory_error(int error) noexcept {
  switch (error) {
  case ENOENT:
  case ENODEV:
  case ENOTTY:
  case EOPNOTSUPP:
    return MF_SHARED_NOT_SUPPORTED;
  case EBUSY:
  case ENOMEM:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  case EACCES:
  case EPERM:
    return MF_SHARED_PERMISSION_DENIED;
  case EOVERFLOW:
    return MF_SHARED_OVERFLOW;
  case EFAULT:
  case EINVAL:
    return MF_SHARED_INVALID_ARGUMENT;
  case ESTALE:
    return MF_SHARED_STALE_HANDLE;
  case EAGAIN:
    return MF_SHARED_RETRY;
  default:
    return MF_SHARED_SYSTEM_ERROR;
  }
}

bool valid_worker_lease_response(const mf_uapi_worker_lease_v0& response,
                                 mf_registry_view_id_v1 expected_view_id,
                                 std::uint64_t expected_generation,
                                 std::uint64_t& single_mapping_size) noexcept {
  if (response.struct_size != sizeof(response) || response.flags != 0U ||
      response.daemon_incarnation != expected_view_id.daemon_incarnation ||
      response.registry_view_serial != expected_view_id.view_serial ||
      response.identity_record_id != MF_KERNEL_PRIMARY_ENTRY_ID ||
      response.device_generation != expected_generation || response.lease_id == 0U ||
      response.queue_mmap_offset % kPageSize != 0U || response.queue_mapping_size == 0U ||
      response.queue_mapping_size > SIZE_MAX || response.kick_eventfd != -1 ||
      response.completion_eventfd != -1 || !bytes_zero(response.reserved, sizeof(response.reserved)) ||
      !ring_mapping_size(kCdevRingCapacity, single_mapping_size) ||
      single_mapping_size > UINT64_MAX / 2U ||
      response.queue_mapping_size != single_mapping_size * 2U ||
      response.queue_mmap_offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
    return false;
  }
  return true;
}

bool valid_negotiate_response(const mf_uapi_negotiate_v0& response,
                              mf_registry_view_id_v1& out_view_id,
                              std::uint64_t& out_generation) noexcept {
  if (response.struct_size != sizeof(response) || response.version != MF_UAPI_VERSION_V0 ||
      response.flags != 0U ||
      (response.required_features & MF_UAPI_FEATURE_QUEUE_MMAP_V0) == 0U ||
      response.registry_view_daemon == 0U || response.registry_view_serial == 0U ||
      response.device_generation == 0U ||
      response.descriptor_size != sizeof(mf_ring_descriptor_v1) || response.ring_order != 8U ||
      response.max_queues == 0U || response.max_inflight < kCdevRingCapacity ||
      response.dma_width == 0U || response.dma_alignment == 0U ||
      !bytes_zero(response.reserved, sizeof(response.reserved))) {
    return false;
  }
  out_view_id.daemon_incarnation = response.registry_view_daemon;
  out_view_id.view_serial = response.registry_view_serial;
  out_generation = response.device_generation;
  return true;
}

bool valid_payload_query_response(const mf_uapi_memory_v0& response,
                                  std::uint64_t expected_generation,
                                  std::uint64_t& out_size) noexcept {
  if (response.struct_size != sizeof(response) || response.flags != 0U ||
      response.handle == 0U || response.generation != expected_generation ||
      response.byte_count == 0U || response.byte_count > kPayloadMaximumSize ||
      response.byte_count % kPageSize != 0U || response.alignment != kPageSize ||
      response.offset != kPayloadMmapOffset || response.fd != -1 ||
      !bytes_zero(response.reserved, sizeof(response.reserved))) {
    return false;
  }
  out_size = response.byte_count;
  return true;
}

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

CdevWorkerSession::~CdevWorkerSession() { close(); }

CdevWorkerSession::CdevWorkerSession(CdevWorkerSession&& other) noexcept
    : control_fd_(other.control_fd_),
      data_fd_(other.data_fd_),
      mapping_(other.mapping_),
      mapping_size_(other.mapping_size_),
      payload_mapping_(other.payload_mapping_),
      payload_mapping_size_(other.payload_mapping_size_),
      lease_(other.lease_) {
  other.control_fd_ = -1;
  other.data_fd_ = -1;
  other.mapping_ = nullptr;
  other.mapping_size_ = 0U;
  other.payload_mapping_ = nullptr;
  other.payload_mapping_size_ = 0U;
  other.lease_ = {};
}

CdevWorkerSession& CdevWorkerSession::operator=(CdevWorkerSession&& other) noexcept {
  if (this != &other) {
    close();
    control_fd_ = other.control_fd_;
    data_fd_ = other.data_fd_;
    mapping_ = other.mapping_;
    mapping_size_ = other.mapping_size_;
    payload_mapping_ = other.payload_mapping_;
    payload_mapping_size_ = other.payload_mapping_size_;
    lease_ = other.lease_;
    other.control_fd_ = -1;
    other.data_fd_ = -1;
    other.mapping_ = nullptr;
    other.mapping_size_ = 0U;
    other.payload_mapping_ = nullptr;
    other.payload_mapping_size_ = 0U;
    other.lease_ = {};
  }
  return *this;
}

mf_shared_status_v1 CdevWorkerSession::open(const char* control_path,
                                             mf_registry_view_id_v1 expected_view_id,
                                             std::uint64_t expected_generation,
                                             CdevWorkerSession& out) noexcept {
  return open_internal(control_path, expected_view_id, expected_generation, false, out);
}

mf_shared_status_v1 CdevWorkerSession::open_current(const char* control_path,
                                                     CdevWorkerSession& out) noexcept {
  return open_internal(control_path, {}, 0U, true, out);
}

mf_shared_status_v1 CdevWorkerSession::open_internal(const char* control_path,
                                                     mf_registry_view_id_v1 expected_view_id,
                                                     std::uint64_t expected_generation,
                                                     bool discover_current,
                                                     CdevWorkerSession& out) noexcept {
  const char* path = control_path == nullptr ? kDefaultControlPath : control_path;
  const char* configured_data_path = std::getenv("METAFLUX_CDEV_PATH");
  const char* data_path = configured_data_path == nullptr ? kDefaultDataPath : configured_data_path;
  if (path == nullptr || path[0] == '\0' ||
      (!discover_current && (expected_view_id.daemon_incarnation == 0U ||
                             expected_view_id.view_serial == 0U || expected_generation == 0U))) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  out.close();
  const int fd = ::open(path, O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    return map_open_error(errno);
  }

  mf_uapi_negotiate_v0 negotiate{};
  negotiate.struct_size = sizeof(negotiate);
  negotiate.version = MF_UAPI_VERSION_V0;
  negotiate.required_features = MF_UAPI_FEATURE_QUEUE_MMAP_V0;
  if (::ioctl(fd, MF_UAPI_IOCTL_NEGOTIATE, &negotiate) < 0) {
    const int error = errno;
    (void)::close(fd);
    return map_open_error(error);
  }
  mf_registry_view_id_v1 negotiated_view_id{};
  std::uint64_t negotiated_generation = 0U;
  if (!valid_negotiate_response(negotiate, negotiated_view_id, negotiated_generation)) {
    (void)::close(fd);
    return MF_SHARED_MALFORMED;
  }
  if (discover_current) {
    expected_view_id = negotiated_view_id;
    expected_generation = negotiated_generation;
  } else if (!mf_registry_view_id_equal_v1(negotiated_view_id, expected_view_id) ||
             negotiated_generation != expected_generation) {
    (void)::close(fd);
    return MF_SHARED_STALE_HANDLE;
  }

  const int data_fd = ::open(data_path, O_RDWR | O_CLOEXEC);
  if (data_fd < 0) {
    const int error = errno;
    (void)::close(fd);
    return map_open_error(error);
  }
  mf_uapi_negotiate_v0 data_negotiate{};
  data_negotiate.struct_size = sizeof(data_negotiate);
  data_negotiate.version = MF_UAPI_VERSION_V0;
  data_negotiate.required_features =
      MF_UAPI_FEATURE_QUEUE_MMAP_V0 | MF_UAPI_FEATURE_REGISTERED_MEMORY_V0;
  if (::ioctl(data_fd, MF_UAPI_IOCTL_NEGOTIATE, &data_negotiate) < 0) {
    const int error = errno;
    (void)::close(data_fd);
    (void)::close(fd);
    return map_open_error(error);
  }
  mf_registry_view_id_v1 data_view_id{};
  std::uint64_t data_generation = 0U;
  if (!valid_negotiate_response(data_negotiate, data_view_id, data_generation)) {
    (void)::close(data_fd);
    (void)::close(fd);
    return MF_SHARED_MALFORMED;
  }
  if ((data_negotiate.required_features & MF_UAPI_FEATURE_REGISTERED_MEMORY_V0) == 0U) {
    (void)::close(data_fd);
    (void)::close(fd);
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (!mf_registry_view_id_equal_v1(data_view_id, expected_view_id) ||
      data_generation != expected_generation) {
    (void)::close(data_fd);
    (void)::close(fd);
    return MF_SHARED_STALE_HANDLE;
  }

  mf_uapi_worker_lease_v0 request{};
  request.struct_size = sizeof(request);
  request.daemon_incarnation = expected_view_id.daemon_incarnation;
  request.registry_view_serial = expected_view_id.view_serial;
  request.device_generation = expected_generation;
  request.kick_eventfd = -1;
  request.completion_eventfd = -1;
  if (::ioctl(fd, MF_UAPI_IOCTL_WORKER_LEASE, &request) < 0) {
    const int error = errno;
    (void)::close(data_fd);
    (void)::close(fd);
    return map_open_error(error);
  }

  std::uint64_t single_mapping_size = 0U;
  if (!valid_worker_lease_response(request, expected_view_id, expected_generation,
                                   single_mapping_size)) {
    (void)::close(data_fd);
    (void)::close(fd);
    return MF_SHARED_MALFORMED;
  }
  void* mapping = ::mmap(nullptr, static_cast<std::size_t>(request.queue_mapping_size),
                         PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                         static_cast<off_t>(request.queue_mmap_offset));
  if (mapping == MAP_FAILED) {
    const int error = errno;
    (void)::close(data_fd);
    (void)::close(fd);
    return map_open_error(error);
  }

  auto* submission = static_cast<mf_ring_header_v1*>(mapping);
  auto* completion = reinterpret_cast<mf_ring_header_v1*>(
      static_cast<std::uint8_t*>(mapping) + single_mapping_size);
  if (!valid_queue(submission) || !valid_queue(completion) ||
      submission->metadata.flags != 0U || completion->metadata.flags != 0U ||
      submission->metadata.mapping_size != single_mapping_size ||
      completion->metadata.mapping_size != single_mapping_size ||
      submission->metadata.queue_id != kCdevSubmissionQueueId ||
      completion->metadata.queue_id != kCdevCompletionQueueId ||
      submission->metadata.queue_generation != expected_generation ||
      completion->metadata.queue_generation != expected_generation ||
      !mf_registry_view_id_equal_v1(submission->metadata.registry_view_id, expected_view_id) ||
      !mf_registry_view_id_equal_v1(completion->metadata.registry_view_id, expected_view_id) ||
      submission->metadata.capacity != completion->metadata.capacity ||
      !mf_registry_view_id_equal_v1(submission->metadata.registry_view_id,
                                    completion->metadata.registry_view_id)) {
    (void)::munmap(mapping, static_cast<std::size_t>(request.queue_mapping_size));
    (void)::close(data_fd);
    (void)::close(fd);
    return MF_SHARED_MALFORMED;
  }

  out.control_fd_ = fd;
  out.data_fd_ = data_fd;
  out.mapping_ = mapping;
  out.mapping_size_ = request.queue_mapping_size;
  out.payload_mapping_ = nullptr;
  out.payload_mapping_size_ = 0U;
  out.lease_.registry_view_id = expected_view_id;
  out.lease_.identity_record_id = request.identity_record_id;
  out.lease_.generation = request.device_generation;
  out.lease_.lease_id = request.lease_id;
  out.lease_.mapping_size = request.queue_mapping_size;
  return MF_SHARED_SUCCESS;
}

void CdevWorkerSession::close() noexcept {
  if (payload_mapping_ != nullptr && payload_mapping_size_ <= SIZE_MAX) {
    (void)::munmap(payload_mapping_, static_cast<std::size_t>(payload_mapping_size_));
  }
  if (mapping_ != nullptr && mapping_size_ <= SIZE_MAX) {
    (void)::munmap(mapping_, static_cast<std::size_t>(mapping_size_));
  }
  if (control_fd_ >= 0) {
    (void)::close(control_fd_);
  }
  if (data_fd_ >= 0) {
    (void)::close(data_fd_);
  }
  control_fd_ = -1;
  data_fd_ = -1;
  mapping_ = nullptr;
  mapping_size_ = 0U;
  payload_mapping_ = nullptr;
  payload_mapping_size_ = 0U;
  lease_ = {};
}

mf_shared_status_v1 CdevWorkerSession::map_payload(std::uint64_t mapping_size) noexcept {
  if (!is_open() || payload_mapping_ != nullptr || mapping_size == 0U ||
      mapping_size > kPayloadMaximumSize || mapping_size > SIZE_MAX ||
      mapping_size % kPageSize != 0U || lease_.generation == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  void* mapping = ::mmap(nullptr, static_cast<std::size_t>(mapping_size), PROT_READ | PROT_WRITE,
                         MAP_SHARED, control_fd_, static_cast<off_t>(kPayloadMmapOffset));
  if (mapping == MAP_FAILED) {
    return map_open_error(errno);
  }
  payload_mapping_ = mapping;
  payload_mapping_size_ = mapping_size;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 CdevWorkerSession::query_payload_size(std::uint64_t& out_size) const noexcept {
  if (!is_open() || lease_.generation == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_uapi_memory_v0 request{};
  request.struct_size = sizeof(request);
  request.fd = -1;
  if (::ioctl(control_fd_, MF_UAPI_IOCTL_MEMORY_QUERY, &request) < 0) {
    return map_open_error(errno);
  }
  if (!valid_payload_query_response(request, lease_.generation, out_size)) {
    return MF_SHARED_MALFORMED;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 CdevWorkerSession::map_current_payload() noexcept {
  if (!is_open() || payload_mapping_ != nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  std::uint64_t mapping_size = 0U;
  const mf_shared_status_v1 query_status = query_payload_size(mapping_size);
  return query_status == MF_SHARED_SUCCESS ? map_payload(mapping_size) : query_status;
}

mf_shared_status_v1 CdevWorkerSession::register_memory(void* address, std::uint64_t byte_count,
                                                       std::uint32_t flags,
                                                       CdevRegisteredMemory& out) noexcept {
  out = {};
  if (!is_open() || data_fd_ < 0 || address == nullptr || byte_count == 0U ||
      byte_count > kPayloadMaximumSize ||
      (flags & ~static_cast<std::uint32_t>(MF_UAPI_MEMORY_REGISTER_KNOWN_FLAGS_V0)) != 0U ||
      (flags & (MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0 | MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0)) ==
          0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_uapi_memory_v0 request{};
  request.struct_size = sizeof(request);
  request.flags = flags;
  request.generation = lease_.generation;
  request.byte_count = byte_count;
  request.alignment = kPageSize;
  request.offset = reinterpret_cast<std::uintptr_t>(address);
  request.fd = -1;
  if (::ioctl(data_fd_, MF_UAPI_IOCTL_MEMORY_REGISTER, &request) < 0) {
    return map_memory_error(errno);
  }
  if (request.struct_size != sizeof(request) || request.flags != flags || request.handle == 0U ||
      request.generation != lease_.generation || request.byte_count != byte_count ||
      request.alignment != kPageSize || request.offset != reinterpret_cast<std::uintptr_t>(address) ||
      request.fd != -1 || !bytes_zero(request.reserved, sizeof(request.reserved))) {
    if (request.handle != 0U && request.generation != 0U) {
      mf_uapi_memory_v0 unregister_request{};
      unregister_request.struct_size = sizeof(unregister_request);
      unregister_request.handle = request.handle;
      unregister_request.generation = request.generation;
      unregister_request.fd = -1;
      (void)::ioctl(data_fd_, MF_UAPI_IOCTL_MEMORY_REGISTER, &unregister_request);
    }
    return MF_SHARED_MALFORMED;
  }
  out.address = address;
  out.byte_count = byte_count;
  out.handle = request.handle;
  out.generation = request.generation;
  return MF_SHARED_SUCCESS;
}

void CdevWorkerSession::close_registered_memory(CdevRegisteredMemory& memory) noexcept {
  if (data_fd_ >= 0 && memory.handle != 0U && memory.generation != 0U) {
    mf_uapi_memory_v0 request{};
    request.struct_size = sizeof(request);
    request.handle = memory.handle;
    request.generation = memory.generation;
    request.fd = -1;
    (void)::ioctl(data_fd_, MF_UAPI_IOCTL_MEMORY_REGISTER, &request);
  }
  memory = {};
}

WorkerQueueView CdevWorkerSession::queue_view(std::uint8_t* payload,
                                              std::uint64_t payload_size) const noexcept {
  WorkerQueueView view{};
  if (!is_open() || mapping_ == nullptr || mapping_size_ == 0U ||
      (payload_size != 0U && payload == nullptr) || mapping_size_ % 2U != 0U) {
    return view;
  }
  view.submission = static_cast<mf_ring_header_v1*>(mapping_);
  view.completion = reinterpret_cast<mf_ring_header_v1*>(
      static_cast<std::uint8_t*>(mapping_) + mapping_size_ / 2U);
  view.payload = payload == nullptr && payload_size == 0U
                     ? payload_mapping()
                     : payload;
  view.payload_size = payload == nullptr && payload_size == 0U ? payload_mapping_size_ : payload_size;
  view.generation = lease_.generation;
  return view;
}

CdevObjectTableResolver::CdevObjectTableResolver(
    void* object_context, CdevObjectTableLookup lookup, void* importer_context,
    CdevBackendMemoryImporter importer, mf_backend_instance_v1 instance,
    mf_backend_context_v1 backend_context) noexcept {
  configure(object_context, lookup, importer_context, importer, instance, backend_context);
}

void CdevObjectTableResolver::configure(void* object_context, CdevObjectTableLookup lookup,
                                        void* importer_context,
                                        CdevBackendMemoryImporter importer,
                                        mf_backend_instance_v1 instance,
                                        mf_backend_context_v1 backend_context) noexcept {
  object_context_ = object_context;
  lookup_ = lookup;
  importer_context_ = importer_context;
  importer_ = importer;
  instance_ = instance;
  backend_context_ = backend_context;
}

mf_shared_status_v1 CdevObjectTableResolver::resolve_memory(
    const mf_argument_entry_v1& entry, bool for_write, std::uint64_t byte_count,
    CdevBackendMemoryReference* out_reference, std::uint64_t* out_offset) const noexcept {
  if (out_reference == nullptr || out_offset == nullptr || lookup_ == nullptr ||
      entry.kind != MF_ARGUMENT_KIND_BUFFER || entry.object_id == 0U ||
      entry.object_generation == 0U || byte_count == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_reference = {};
  *out_offset = entry.value;
  CdevObjectTableView object{};
  const mf_shared_status_v1 lookup_status = lookup_(
      object_context_, entry.object_id, entry.object_generation, 0U, for_write, &object);
  if (lookup_status != MF_SHARED_SUCCESS) {
    return lookup_status;
  }
  if (object.object_id != entry.object_id || object.object_generation != entry.object_generation ||
      (object.object_kind != MF_OBJECT_TYPE_DEVICE_MEMORY &&
       object.object_kind != MF_OBJECT_TYPE_HOST_MEMORY)) {
    return MF_SHARED_MALFORMED;
  }
  if (object.byte_size == 0U || entry.value > object.byte_size ||
      byte_count > object.byte_size - entry.value) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (object.object_kind == MF_OBJECT_TYPE_HOST_MEMORY) {
    const std::uint32_t required = for_write ? MF_ARGUMENT_BUFFER_WRITE : MF_ARGUMENT_BUFFER_READ;
    if ((object.access_flags & required) == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
  }
  if (!empty_memory_reference(object.backend_reference)) {
    if (!complete_memory_reference(object.backend_reference)) {
      return MF_SHARED_MALFORMED;
    }
    *out_reference = object.backend_reference;
    return MF_SHARED_SUCCESS;
  }
  if (importer_ == nullptr || object.address == nullptr || instance_ == 0U ||
      backend_context_ == 0U) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  auto* address = static_cast<std::uint8_t*>(object.address) + entry.value;
  const mf_shared_status_v1 import_status = importer_(
      importer_context_, instance_, backend_context_, address, byte_count, out_reference);
  if (import_status != MF_SHARED_SUCCESS) {
    return import_status;
  }
  if (!complete_memory_reference(*out_reference)) {
    *out_reference = {};
    return MF_SHARED_MALFORMED;
  }
  *out_offset = 0U;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 CdevObjectTableResolver::resolve_copy(
    const mf_ring_descriptor_v1* request, CdevCopyResolution* out) const noexcept {
  if (request == nullptr || out == nullptr || request->opcode != MF_RING_OPCODE_COPY ||
      request->flags != MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1 || request->target_id == 0U ||
      request->arguments[0] == 0U || request->arguments[1] != 0U || request->arguments[2] != 0U ||
      request->arguments[3] != 0U || lookup_ == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out = {};
  CdevObjectTableView argument_block{};
  const mf_shared_status_v1 lookup_status = lookup_(
      object_context_, request->target_id, request->arguments[0], MF_OBJECT_TYPE_ARGUMENT_BLOCK,
      false, &argument_block);
  if (lookup_status != MF_SHARED_SUCCESS) {
    return lookup_status;
  }
  if (argument_block.object_id != request->target_id ||
      argument_block.object_generation != request->arguments[0] ||
      argument_block.object_kind != MF_OBJECT_TYPE_ARGUMENT_BLOCK || argument_block.data == nullptr ||
      argument_block.byte_size == 0U) {
    return MF_SHARED_MALFORMED;
  }
  const mf_argument_entry_v1* entries = nullptr;
  if (!valid_copy_region_argument_block(argument_block.data, argument_block.byte_size, &entries)) {
    return MF_SHARED_MALFORMED;
  }
  const std::uint64_t byte_count = entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value;
  CdevBackendMemoryReference destination_reference{};
  CdevBackendMemoryReference source_reference{};
  std::uint64_t destination_offset = 0U;
  std::uint64_t source_offset = 0U;
  mf_shared_status_v1 status = resolve_memory(
      entries[MF_COPY_REGION_DESTINATION_INDEX_V1], true, byte_count, &destination_reference,
      &destination_offset);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status = resolve_memory(entries[MF_COPY_REGION_SOURCE_INDEX_V1], false, byte_count,
                          &source_reference, &source_offset);
  if (status != MF_SHARED_SUCCESS) {
    release_memory_reference_now(destination_reference);
    return status;
  }
  out->destination = destination_reference.handle;
  out->destination_offset = destination_offset;
  out->destination_reference = destination_reference;
  out->source = source_reference.handle;
  out->source_offset = source_offset;
  out->source_reference = source_reference;
  out->byte_count = byte_count;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 CdevObjectTableResolver::callback(
    void* context, const mf_ring_descriptor_v1* request, CdevCopyResolution* out) noexcept {
  auto* resolver = static_cast<CdevObjectTableResolver*>(context);
  return resolver == nullptr ? MF_SHARED_INVALID_ARGUMENT : resolver->resolve_copy(request, out);
}

bool CdevWorker::valid_copy_backend(const CdevBackendBinding& backend) noexcept {
  return backend.api != nullptr && backend.instance != 0U && backend.queue != 0U &&
         backend.memory != 0U &&
         valid_optional_memory_reference(backend.memory, backend.memory_reference) &&
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

bool CdevWorker::valid_optional_memory_reference(
    mf_backend_memory_v1 memory, const CdevBackendMemoryReference& reference) noexcept {
  const bool empty = reference.handle == 0U && reference.retain == nullptr &&
                     reference.release == nullptr && reference.context == nullptr;
  return empty || valid_memory_reference(memory, reference);
}

bool CdevWorker::valid_copy_resolution(const CdevCopyResolution& resolution) noexcept {
  return valid_memory_reference(resolution.destination, resolution.destination_reference) &&
         valid_memory_reference(resolution.source, resolution.source_reference) &&
         resolution.byte_count != 0U &&
         resolution.destination_offset <= UINT64_MAX - resolution.byte_count &&
         resolution.source_offset <= UINT64_MAX - resolution.byte_count &&
         resolution.reserved_word == 0U;
}

bool CdevWorker::valid_launch_resolution(const CdevLaunchResolution& resolution) noexcept {
  if (resolution.memory_reference_count > kCdevLaunchMemoryReferenceCapacity) {
    return false;
  }
  for (std::size_t index = 0U; index < resolution.memory_reference_count; ++index) {
    const auto& reference = resolution.memory_references[index];
    if (!valid_memory_reference(reference.handle, reference)) {
      return false;
    }
  }
  return true;
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

mf_shared_status_v1
CdevWorker::retain_launch_references(CdevLaunchResolution& resolution) noexcept {
  std::size_t retained = 0U;
  while (retained < resolution.memory_reference_count) {
    const auto& reference = resolution.memory_references[retained];
    const mf_shared_status_v1 status = reference.retain(reference.context, reference.handle);
    if (status != MF_SHARED_SUCCESS) {
      while (retained > 0U) {
        --retained;
        const auto& retained_reference = resolution.memory_references[retained];
        retained_reference.release(retained_reference.context, retained_reference.handle);
      }
      return status;
    }
    ++retained;
  }
  return MF_SHARED_SUCCESS;
}

void CdevWorker::release_launch_references(const CdevLaunchResolution& resolution) noexcept {
  for (std::size_t index = 0U; index < resolution.memory_reference_count; ++index) {
    const auto& reference = resolution.memory_references[index];
    reference.release(reference.context, reference.handle);
  }
}

mf_shared_status_v1 CdevWorker::retain_memory_reference(
    const CdevBackendMemoryReference& reference) noexcept {
  if (reference.handle == 0U) {
    return MF_SHARED_SUCCESS;
  }
  return reference.retain(reference.context, reference.handle);
}

void CdevWorker::release_memory_reference(
    const CdevBackendMemoryReference& reference) noexcept {
  if (reference.handle != 0U && reference.release != nullptr) {
    reference.release(reference.context, reference.handle);
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
  return backend.generation != 0U && valid_backend_lease(backend) &&
         (valid_copy_backend(backend) || valid_region_copy_backend(backend) ||
          valid_launch_backend(backend));
}

bool CdevWorker::same_backend_binding(const CdevBackendBinding& left,
                                      const CdevBackendBinding& right) noexcept {
  return left.api == right.api && left.instance == right.instance && left.queue == right.queue &&
         left.memory == right.memory && left.memory_reference.handle == right.memory_reference.handle &&
         left.memory_reference.retain == right.memory_reference.retain &&
         left.memory_reference.release == right.memory_reference.release &&
         left.memory_reference.context == right.memory_reference.context &&
         left.completion_event == right.completion_event && left.copy_resolver == right.copy_resolver &&
         left.copy_context == right.copy_context && left.launch_resolver == right.launch_resolver &&
         left.launch_context == right.launch_context && left.lease_acquire == right.lease_acquire &&
         left.lease_release == right.lease_release && left.lease_context == right.lease_context &&
         left.retire == right.retire && left.retire_context == right.retire_context &&
         left.generation == right.generation && left.rebind == right.rebind &&
         left.rebind_context == right.rebind_context;
}

void CdevWorker::retire_backend_binding(const CdevBackendBinding& backend) noexcept {
  if (backend.retire != nullptr) {
    backend.retire(backend.retire_context);
  }
}

bool CdevWorker::bind_backend(CdevBackendBinding backend) noexcept {
  if (staged_rebind_ || !lifecycle_online_ || view_.generation == 0U || !valid_backend(backend) ||
      backend.generation != view_.generation) {
    return false;
  }
  if (same_backend_binding(backend_, backend)) {
    return true;
  }
  const CdevBackendBinding previous = backend_;
  if (pending_.active) {
    if (!same_backend_binding(previous, pending_.backend)) {
      retire_backend_binding(previous);
    }
    backend_ = backend;
    pending_.retire_backend = !same_backend_binding(backend_, pending_.backend);
  } else {
    backend_ = backend;
    retire_backend_binding(previous);
  }
  return true;
}

bool CdevWorker::stage_rebind(std::uint64_t generation) noexcept {
  if (staged_rebind_ || generation == 0U || backend_.rebind == nullptr) {
    return false;
  }
  WorkerQueueView view{};
  CdevBackendBinding backend{};
  if (!backend_.rebind(backend_.rebind_context, generation, &view, &backend) ||
      view.generation != generation || !valid_queue(view.submission) ||
      !valid_queue(view.completion) ||
      (view.payload == nullptr && view.payload_size != 0U) ||
      (view.payload != nullptr && view.payload_size == 0U) || !valid_backend(backend) ||
      backend.generation != generation) {
    retire_backend_binding(backend);
    return false;
  }
  staged_view_ = view;
  staged_backend_ = backend;
  staged_rebind_ = true;
  return true;
}

void CdevWorker::discard_staged_rebind() noexcept {
  if (!staged_rebind_) {
    return;
  }
  const CdevBackendBinding staged_backend = staged_backend_;
  staged_view_ = {};
  staged_backend_ = {};
  staged_rebind_ = false;
  retire_backend_binding(staged_backend);
}

bool CdevWorker::backend_matches_generation() const noexcept {
  return backend_.api == nullptr ||
         (backend_.generation != 0U && backend_.generation == view_.generation);
}

bool CdevWorker::backend_bound() const noexcept {
  return valid_backend(backend_) && backend_matches_generation();
}

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
  if (!backend_bound() || base > UINT64_MAX - destination || base > UINT64_MAX - source) {
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
  if (!backend_bound() || !valid_region_copy_backend(backend_) ||
      !valid_copy_resolution(resolution)) {
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
CdevWorker::dispatch_launch(const mf_ring_descriptor_v1& request,
                            CdevLaunchResolution* out) const noexcept {
  if (!backend_bound() || !valid_launch_backend(backend_)) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (out == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  CdevLaunchResolution& resolution = *out;
  resolution = {};
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
      resolution.reserved_word != 0U || !valid_launch_resolution(resolution)) {
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
  if (!backend_bound()) {
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
                                                const CdevCopyResolution* resolution,
                                                const CdevBackendMemoryReference* memory_reference,
                                                const CdevLaunchResolution* launch_resolution) noexcept {
  if (status != MF_SHARED_SUCCESS || backend_.completion_event == 0U) {
    if (resolution != nullptr) {
      release_copy_references(*resolution);
    }
    if (memory_reference != nullptr) {
      release_memory_reference(*memory_reference);
    }
    if (launch_resolution != nullptr) {
      release_launch_references(*launch_resolution);
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
  if (memory_reference != nullptr) {
    pending_.memory_reference = *memory_reference;
    pending_.has_backend_memory_reference = true;
  }
  if (launch_resolution != nullptr) {
    pending_.launch_resolution = *launch_resolution;
    pending_.has_launch_memory_references = launch_resolution->memory_reference_count != 0U;
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
      const bool has_backend_memory_reference = pending_.has_backend_memory_reference;
      const CdevBackendMemoryReference pending_memory_reference = pending_.memory_reference;
      const bool has_launch_memory_references = pending_.has_launch_memory_references;
      const CdevLaunchResolution pending_launch_resolution = pending_.launch_resolution;
      const bool retire_backend = pending_.retire_backend;
      pending_ = {};
      if (has_memory_references) {
        release_copy_references(pending_resolution);
      }
      if (has_backend_memory_reference) {
        release_memory_reference(pending_memory_reference);
      }
      if (has_launch_memory_references) {
        release_launch_references(pending_launch_resolution);
      }
      release_backend_lease(pending_backend);
      if (retire_backend) {
        retire_backend_binding(pending_backend);
      }
    }
    return result;
  }
  if (!valid_backend(pending_backend) || pending_backend.completion_event != pending_.event ||
      pending_backend.api->query_event == nullptr) {
    const mf_ring_descriptor_v1 request = pending_.request;
    const bool has_memory_references = pending_.has_memory_references;
    const CdevCopyResolution pending_resolution = pending_.resolution;
    const bool has_backend_memory_reference = pending_.has_backend_memory_reference;
    const CdevBackendMemoryReference pending_memory_reference = pending_.memory_reference;
    const bool has_launch_memory_references = pending_.has_launch_memory_references;
    const CdevLaunchResolution pending_launch_resolution = pending_.launch_resolution;
    const bool retire_backend = pending_.retire_backend;
    pending_ = {};
    if (has_memory_references) {
      release_copy_references(pending_resolution);
    }
    if (has_backend_memory_reference) {
      release_memory_reference(pending_memory_reference);
    }
    if (has_launch_memory_references) {
      release_launch_references(pending_launch_resolution);
    }
    release_backend_lease(pending_backend);
    if (retire_backend) {
      retire_backend_binding(pending_backend);
    }
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
    const bool has_backend_memory_reference = pending_.has_backend_memory_reference;
    const CdevBackendMemoryReference pending_memory_reference = pending_.memory_reference;
    const bool has_launch_memory_references = pending_.has_launch_memory_references;
    const CdevLaunchResolution pending_launch_resolution = pending_.launch_resolution;
    const bool retire_backend = pending_.retire_backend;
    pending_ = {};
    if (has_memory_references) {
      release_copy_references(pending_resolution);
    }
    if (has_backend_memory_reference) {
      release_memory_reference(pending_memory_reference);
    }
    if (has_launch_memory_references) {
      release_launch_references(pending_launch_resolution);
    }
    release_backend_lease(pending_backend);
    if (retire_backend) {
      retire_backend_binding(pending_backend);
    }
  }
  return result;
}

WorkerResult CdevWorker::consume_once() noexcept {
  mf_ring_descriptor_v1 request{};
  if (!valid_queue(view_.submission) || !valid_queue(view_.completion)) {
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
  const bool payload_available = view_.payload != nullptr && view_.payload_size != 0U;
  if (!payload_available && request.opcode != MF_RING_OPCODE_NOOP && !region_copy) {
    return complete(request, MF_SHARED_NOT_SUPPORTED);
  }
  if (!region_copy && request.target_id != view_.generation) {
    return complete(request, MF_SHARED_STALE_HANDLE);
  }
  if (request.opcode == MF_RING_OPCODE_NOOP) {
    return complete(request, MF_SHARED_SUCCESS);
  }
  if (backend_.api != nullptr && valid_backend(backend_) && !backend_matches_generation()) {
    return complete(request, MF_SHARED_STALE_HANDLE);
  }
  if (request.opcode == MF_RING_OPCODE_LAUNCH) {
    if (request.flags != 0U) {
      return complete(request, MF_SHARED_MALFORMED);
    }
    const mf_shared_status_v1 lease_status = acquire_backend_lease();
    if (lease_status != MF_SHARED_SUCCESS) {
      return complete(request, lease_status);
    }
    CdevLaunchResolution resolution{};
    const mf_shared_status_v1 status = dispatch_launch(request, &resolution);
    if (status != MF_SHARED_SUCCESS) {
      return finish_backend_request(request, status);
    }
    const mf_shared_status_v1 retain_status = retain_launch_references(resolution);
    if (retain_status != MF_SHARED_SUCCESS) {
      return finish_backend_request(request, retain_status);
    }
    return finish_backend_request(request, status, nullptr, nullptr, &resolution);
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
    const CdevBackendMemoryReference* memory_reference =
        backend_.memory_reference.handle == 0U ? nullptr : &backend_.memory_reference;
    if (memory_reference != nullptr) {
      const mf_shared_status_v1 retain_status = retain_memory_reference(*memory_reference);
      if (retain_status != MF_SHARED_SUCCESS) {
        release_backend_lease();
        return complete(request, retain_status);
      }
    }
    const mf_shared_status_v1 status =
        map_backend_status(dispatch_copy(base, destination, source, byte_count));
    return finish_backend_request(request, status, nullptr, memory_reference);
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
  const bool payload_available = worker != nullptr && worker->view_.payload != nullptr &&
                                 worker->view_.payload_size != 0U;
  const bool region_backend_available = worker != nullptr &&
                                        valid_region_copy_backend(worker->backend_);
  if (worker == nullptr || !valid_queue(worker->view_.submission) ||
      !valid_queue(worker->view_.completion) || (!payload_available && !region_backend_available)) {
    return false;
  }
  if (event.candidate.generation == 0U) {
    return event.request.operation == metaflux::runtime::lifecycle::Operation::Remove;
  }
  return worker->backend_.api == nullptr || worker->stage_rebind(event.candidate.generation);
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
  if (worker == nullptr || worker->pending_.active) {
    return false;
  }
  const CdevBackendBinding previous = worker->backend_;
  if (event.candidate.generation != 0U) {
    if (previous.api == nullptr) {
      worker->view_.generation = event.candidate.generation;
    } else if (!worker->staged_rebind_ ||
               worker->staged_view_.generation != event.candidate.generation ||
        worker->staged_backend_.generation != event.candidate.generation) {
      return false;
    } else {
      worker->view_ = worker->staged_view_;
      worker->backend_ = worker->staged_backend_;
      worker->staged_view_ = {};
      worker->staged_backend_ = {};
      worker->staged_rebind_ = false;
      retire_backend_binding(previous);
    }
  } else if (event.state_after == metaflux::runtime::lifecycle::State::Absent) {
    worker->backend_ = {};
    worker->view_.generation = 0U;
    retire_backend_binding(previous);
  } else {
    return false;
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
  worker->discard_staged_rebind();
  worker->lifecycle_online_ = event.state_before == metaflux::runtime::lifecycle::State::Online;
  worker->lifecycle_accepting_ = worker->lifecycle_online_;
  return true;
}

void CdevWorker::lifecycle_lost(void* context,
                                const metaflux::runtime::lifecycle::MirrorEvent&) noexcept {
  auto* worker = static_cast<CdevWorker*>(context);
  if (worker != nullptr) {
    worker->discard_staged_rebind();
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
