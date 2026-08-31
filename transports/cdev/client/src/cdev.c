#define _GNU_SOURCE

#include "metaflux/transport/cdev.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int mf_cdev_reserved_zero(const uint8_t* bytes, size_t count) {
  size_t index = 0;
  for (index = 0; index < count; ++index) {
    if (bytes[index] != 0U) {
      return 0;
    }
  }
  return 1;
}

static mf_shared_status_v1 mf_cdev_validate_ring(void* mapping, uint64_t mapping_size,
                                                  const mf_uapi_queue_v0* queue,
                                                  mf_cdev_session_v0* session) {
  mf_ring_header_v1* header = (mf_ring_header_v1*)mapping;
  mf_ring_header_v1* completion = NULL;
  uint32_t capacity = 0;
  uint64_t single_size = 0;
  uint64_t expected_size = 0;
  if (mapping == NULL || mapping_size < sizeof(mf_ring_header_v1) ||
      mapping_size > (uint64_t)SIZE_MAX || queue == NULL || session == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  capacity = header->metadata.capacity;
  if (header->metadata.magic != MF_SHARED_RING_MAGIC ||
      header->metadata.abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      header->metadata.header_size != sizeof(mf_ring_header_v1) ||
      header->metadata.descriptor_size != sizeof(mf_ring_descriptor_v1) ||
      header->metadata.flags != 0U || capacity < 2U ||
      (capacity & (capacity - 1U)) != 0U ||
      mf_client_ring_mapping_size_v1(capacity, &single_size) != MF_SHARED_SUCCESS ||
      single_size > UINT64_MAX / 2U || (expected_size = single_size * 2U) != mapping_size ||
      header->metadata.mapping_size != single_size ||
      header->metadata.queue_id != queue->queue_id ||
      header->metadata.queue_generation != queue->queue_generation ||
      header->metadata.registry_view_id.daemon_incarnation == 0U ||
      header->metadata.registry_view_id.view_serial == 0U) {
    return MF_SHARED_MALFORMED;
  }
  completion = (mf_ring_header_v1*)((uint8_t*)mapping + single_size);
  if (completion->metadata.magic != MF_SHARED_RING_MAGIC ||
      completion->metadata.abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      completion->metadata.header_size != sizeof(mf_ring_header_v1) ||
      completion->metadata.descriptor_size != sizeof(mf_ring_descriptor_v1) ||
      completion->metadata.flags != 0U || completion->metadata.capacity != capacity ||
      completion->metadata.mapping_size != single_size ||
      !mf_registry_view_id_equal_v1(completion->metadata.registry_view_id,
                                    header->metadata.registry_view_id) ||
      completion->metadata.queue_id != queue->queue_id + 1U ||
      completion->metadata.queue_generation != queue->queue_generation) {
    return MF_SHARED_MALFORMED;
  }
  session->mapping = mapping;
  session->mapping_size = mapping_size;
  session->submission.mapping = mapping;
  session->submission.mapping_size = single_size;
  session->submission.header = header;
  session->submission.descriptors =
      (mf_ring_descriptor_v1*)((uint8_t*)mapping + sizeof(*header));
  session->submission.registry_view_id = header->metadata.registry_view_id;
  session->submission.queue_id = queue->queue_id;
  session->submission.queue_generation = queue->queue_generation;
  session->submission.owned_fd = -1;
  session->submission.capacity = capacity;
  session->completion.mapping = completion;
  session->completion.mapping_size = single_size;
  session->completion.header = completion;
  session->completion.descriptors =
      (mf_ring_descriptor_v1*)((uint8_t*)completion + sizeof(*completion));
  session->completion.registry_view_id = completion->metadata.registry_view_id;
  session->completion.queue_id = completion->metadata.queue_id;
  session->completion.queue_generation = completion->metadata.queue_generation;
  session->completion.owned_fd = -1;
  session->completion.capacity = capacity;
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_cdev_session_open_internal_v0(const char* device_path,
                                                             int32_t submission_eventfd,
                                                             int32_t completion_eventfd,
                                                             mf_cdev_session_v0* out_session) {
  const char* path = device_path == NULL ? MF_CDEV_DEFAULT_PATH_V0 : device_path;
  mf_uapi_negotiate_v0 negotiate;
  mf_uapi_queue_v0 queue;
  void* mapping = MAP_FAILED;
  size_t path_length = 0;
  int32_t fd = -1;
  int result = -1;

  if (out_session == NULL || path == NULL || submission_eventfd < -1 || completion_eventfd < -1) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  path_length = strlen(path);
  if (path_length == 0U || path_length >= 256U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_session, 0, sizeof(*out_session));
  out_session->device_fd = -1;
  out_session->submission.owned_fd = -1;
  out_session->completion.owned_fd = -1;
  out_session->submission_eventfd = -1;
  out_session->completion_eventfd = -1;
  fd = open(path, O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    return (errno == ENOENT || errno == ENODEV) ? MF_SHARED_NOT_SUPPORTED : MF_SHARED_SYSTEM_ERROR;
  }
  (void)memset(&negotiate, 0, sizeof(negotiate));
  negotiate.struct_size = sizeof(negotiate);
  negotiate.version = MF_UAPI_VERSION_V0;
  negotiate.required_features = MF_UAPI_FEATURE_QUEUE_MMAP_V0;
  negotiate.optional_features = MF_UAPI_FEATURE_EVENTFD_V0 | MF_UAPI_FEATURE_REGISTERED_MEMORY_V0;
  result = ioctl(fd, MF_UAPI_IOCTL_NEGOTIATE, &negotiate);
  if (result < 0) {
    const int error = errno;
    (void)close(fd);
    return error == ENOTTY || error == ENODEV ? MF_SHARED_NOT_SUPPORTED : MF_SHARED_SYSTEM_ERROR;
  }
  if (negotiate.struct_size != sizeof(negotiate) || negotiate.version != MF_UAPI_VERSION_V0 ||
      (negotiate.required_features & MF_UAPI_FEATURE_QUEUE_MMAP_V0) !=
          MF_UAPI_FEATURE_QUEUE_MMAP_V0 ||
      !mf_cdev_reserved_zero(negotiate.reserved, sizeof(negotiate.reserved)) ||
      negotiate.device_generation == 0U || negotiate.registry_view_daemon == 0U ||
      negotiate.registry_view_serial == 0U) {
    (void)close(fd);
    return MF_SHARED_MALFORMED;
  }
  (void)memset(&queue, 0, sizeof(queue));
  queue.struct_size = sizeof(queue);
  queue.submission_eventfd = submission_eventfd;
  queue.completion_eventfd = completion_eventfd;
  result = ioctl(fd, MF_UAPI_IOCTL_QUEUE_CREATE, &queue);
  if (result < 0) {
    const int error = errno;
    (void)close(fd);
    return error == ENOTTY || error == ENODEV ? MF_SHARED_NOT_SUPPORTED : MF_SHARED_SYSTEM_ERROR;
  }
  if (queue.struct_size != sizeof(queue) || queue.queue_id == 0U ||
      queue.queue_generation != negotiate.device_generation || queue.mapping_size == 0U ||
      queue.mapping_size > (uint64_t)SIZE_MAX || (queue.mmap_offset % 4096U) != 0U ||
      (queue.submission_eventfd < -1 || queue.completion_eventfd < -1) ||
      !mf_cdev_reserved_zero(queue.reserved, sizeof(queue.reserved))) {
    (void)close(fd);
    return MF_SHARED_MALFORMED;
  }
  mapping = mmap(NULL, (size_t)queue.mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                 (off_t)queue.mmap_offset);
  if (mapping == MAP_FAILED) {
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  out_session->device_fd = fd;
  out_session->registry_view_id.daemon_incarnation = negotiate.registry_view_daemon;
  out_session->registry_view_id.view_serial = negotiate.registry_view_serial;
  out_session->device_generation = negotiate.device_generation;
  out_session->queue_id = queue.queue_id;
  out_session->negotiated_features =
      (uint32_t)(negotiate.required_features | negotiate.optional_features);
  out_session->submission_eventfd = queue.submission_eventfd;
  out_session->completion_eventfd = queue.completion_eventfd;
  if (mf_cdev_validate_ring(mapping, queue.mapping_size, &queue, out_session) !=
      MF_SHARED_SUCCESS) {
    (void)munmap(mapping, (size_t)queue.mapping_size);
    (void)close(fd);
    (void)memset(out_session, 0, sizeof(*out_session));
    out_session->device_fd = -1;
    out_session->submission.owned_fd = -1;
    out_session->completion.owned_fd = -1;
    out_session->submission_eventfd = -1;
    out_session->completion_eventfd = -1;
    return MF_SHARED_MALFORMED;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_cdev_session_open_v0(const char* device_path,
                                             mf_cdev_session_v0* out_session) {
  return mf_cdev_session_open_internal_v0(device_path, -1, -1, out_session);
}

mf_shared_status_v1 mf_cdev_session_open_with_eventfds_v0(const char* device_path,
                                                          int32_t submission_eventfd,
                                                          int32_t completion_eventfd,
                                                          mf_cdev_session_v0* out_session) {
  return mf_cdev_session_open_internal_v0(device_path, submission_eventfd, completion_eventfd,
                                          out_session);
}

mf_shared_status_v1 mf_cdev_session_open_default_v0(mf_cdev_session_v0* out_session) {
  const char* configured = getenv("METAFLUX_CDEV_PATH");
  return mf_cdev_session_open_v0(configured == NULL ? MF_CDEV_DEFAULT_PATH_V0 : configured,
                                 out_session);
}

void mf_cdev_session_close_v0(mf_cdev_session_v0* session) {
  if (session == NULL) {
    return;
  }
  if (session->mapping != NULL && session->mapping_size <= (uint64_t)SIZE_MAX) {
    (void)munmap(session->mapping, (size_t)session->mapping_size);
  }
  if (session->device_fd >= 0) {
    (void)close(session->device_fd);
  }
  (void)memset(session, 0, sizeof(*session));
  session->device_fd = -1;
  session->submission.owned_fd = -1;
  session->completion.owned_fd = -1;
  session->submission_eventfd = -1;
  session->completion_eventfd = -1;
}

mf_shared_status_v1 mf_cdev_memory_alloc_v0(mf_cdev_session_v0* session, uint64_t byte_count,
                                            uint64_t alignment,
                                            mf_cdev_memory_v0* out_memory) {
  mf_uapi_memory_v0 request;
  void* mapping = MAP_FAILED;
  int result = -1;

  if (session == NULL || session->device_fd < 0 || out_memory == NULL || byte_count == 0U ||
      byte_count > MF_CDEV_PAYLOAD_MAX_SIZE_V0 || alignment == 0U ||
      (alignment & (alignment - 1U)) != 0U || alignment < 4096U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_memory, 0, sizeof(*out_memory));
  out_memory->device_fd = -1;
  (void)memset(&request, 0, sizeof(request));
  request.struct_size = sizeof(request);
  request.generation = session->device_generation;
  request.byte_count = byte_count;
  request.alignment = alignment;
  request.fd = -1;
  result = ioctl(session->device_fd, MF_UAPI_IOCTL_MEMORY_ALLOC, &request);
  if (result < 0) {
    if (errno == ENOTTY || errno == EOPNOTSUPP || errno == ENODEV) {
      return MF_SHARED_NOT_SUPPORTED;
    }
    if (errno == EBUSY || errno == ENOMEM) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    if (errno == ESTALE) {
      return MF_SHARED_STALE_HANDLE;
    }
    return errno == EINVAL ? MF_SHARED_INVALID_ARGUMENT : MF_SHARED_SYSTEM_ERROR;
  }
  if (request.struct_size != sizeof(request) || request.handle == 0U ||
      request.generation != session->device_generation || request.byte_count < byte_count ||
      request.byte_count > MF_CDEV_PAYLOAD_MAX_SIZE_V0 || request.alignment < 4096U ||
      (request.alignment & (request.alignment - 1U)) != 0U ||
      request.offset == 0U || (request.offset % 4096U) != 0U || request.fd != -1 ||
      !mf_cdev_reserved_zero(request.reserved, sizeof(request.reserved))) {
    return MF_SHARED_MALFORMED;
  }
  mapping = mmap(NULL, (size_t)request.byte_count, PROT_READ | PROT_WRITE, MAP_SHARED,
                 session->device_fd, (off_t)request.offset);
  if (mapping == MAP_FAILED) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  out_memory->mapping = mapping;
  out_memory->mapping_size = request.byte_count;
  out_memory->byte_count = request.byte_count;
  out_memory->handle = request.handle;
  out_memory->generation = request.generation;
  out_memory->device_fd = session->device_fd;
  out_memory->kind = MF_CDEV_MEMORY_KIND_ALLOC_V0;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_cdev_memory_register_v0(mf_cdev_session_v0* session, void* address,
                                               uint64_t byte_count, uint32_t flags,
                                               mf_cdev_memory_v0* out_memory) {
  mf_uapi_memory_v0 request;
  int result = -1;

  if (session == NULL || session->device_fd < 0 || address == NULL || byte_count == 0U ||
      byte_count > MF_CDEV_PAYLOAD_MAX_SIZE_V0 ||
      (flags & ~(uint32_t)MF_CDEV_MEMORY_REGISTER_KNOWN_FLAGS_V0) != 0U ||
      (flags & (MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0 |
                MF_CDEV_MEMORY_REGISTER_FLAG_WRITE_V0)) == 0U || out_memory == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if ((session->negotiated_features & (uint32_t)MF_UAPI_FEATURE_REGISTERED_MEMORY_V0) == 0U) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  (void)memset(out_memory, 0, sizeof(*out_memory));
  out_memory->device_fd = -1;
  (void)memset(&request, 0, sizeof(request));
  request.struct_size = sizeof(request);
  request.flags = flags;
  request.generation = session->device_generation;
  request.byte_count = byte_count;
  request.alignment = 4096U;
  request.offset = (uint64_t)(uintptr_t)address;
  request.fd = -1;
  result = ioctl(session->device_fd, MF_UAPI_IOCTL_MEMORY_REGISTER, &request);
  if (result < 0) {
    if (errno == ENOTTY || errno == EOPNOTSUPP || errno == ENODEV) {
      return MF_SHARED_NOT_SUPPORTED;
    }
    if (errno == EBUSY || errno == ENOMEM) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    if (errno == EPERM) {
      return MF_SHARED_PERMISSION_DENIED;
    }
    if (errno == ESTALE) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (errno == EOVERFLOW) {
      return MF_SHARED_OVERFLOW;
    }
    return errno == EINVAL || errno == EFAULT ? MF_SHARED_INVALID_ARGUMENT
                                               : MF_SHARED_SYSTEM_ERROR;
  }
  if (request.struct_size != sizeof(request) || request.flags != flags || request.handle == 0U ||
      request.generation != session->device_generation || request.byte_count != byte_count ||
      request.alignment < 4096U || (request.alignment & (request.alignment - 1U)) != 0U ||
      request.offset != (uint64_t)(uintptr_t)address || request.fd != -1 ||
      !mf_cdev_reserved_zero(request.reserved, sizeof(request.reserved))) {
    return MF_SHARED_MALFORMED;
  }
  out_memory->byte_count = request.byte_count;
  out_memory->handle = request.handle;
  out_memory->generation = request.generation;
  out_memory->device_fd = session->device_fd;
  out_memory->kind = MF_CDEV_MEMORY_KIND_REGISTERED_V0;
  return MF_SHARED_SUCCESS;
}

void mf_cdev_memory_close_v0(mf_cdev_memory_v0* memory) {
  if (memory == NULL) {
    return;
  }
  if (memory->mapping != NULL && memory->mapping_size <= (uint64_t)SIZE_MAX) {
    (void)munmap(memory->mapping, (size_t)memory->mapping_size);
  }
  if (memory->kind == MF_CDEV_MEMORY_KIND_REGISTERED_V0 && memory->device_fd >= 0 &&
      memory->handle != 0U && memory->generation != 0U) {
    mf_uapi_memory_v0 request;
    (void)memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.handle = memory->handle;
    request.generation = memory->generation;
    request.fd = -1;
    (void)ioctl(memory->device_fd, MF_UAPI_IOCTL_MEMORY_REGISTER, &request);
  }
  (void)memset(memory, 0, sizeof(*memory));
  memory->device_fd = -1;
}

mf_shared_status_v1 mf_cdev_copy_descriptor_v0(uint64_t request_id, uint64_t generation,
                                               const mf_cdev_copy_v0* copy,
                                               mf_ring_descriptor_v1* out_descriptor) {
  if (copy == NULL || out_descriptor == NULL || request_id == 0U || generation == 0U ||
      copy->byte_count == 0U || copy->destination_offset > UINT64_MAX - copy->byte_count ||
      copy->source_offset > UINT64_MAX - copy->byte_count) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_descriptor, 0, sizeof(*out_descriptor));
  out_descriptor->opcode = MF_RING_OPCODE_COPY;
  out_descriptor->request_id = request_id;
  out_descriptor->target_id = generation;
  out_descriptor->arguments[0] = copy->destination_offset;
  out_descriptor->arguments[1] = copy->source_offset;
  out_descriptor->arguments[2] = copy->byte_count;
  out_descriptor->arguments[3] = MF_CDEV_PAYLOAD_OFFSET_V0;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_cdev_submit_copy_v0(mf_cdev_session_v0* session, uint64_t request_id,
                                           const mf_cdev_copy_v0* copy) {
  mf_ring_descriptor_v1 descriptor;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (session == NULL || session->device_fd < 0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_cdev_copy_descriptor_v0(request_id, session->device_generation, copy, &descriptor);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  return mf_client_ring_try_submit_v1(&session->submission, &descriptor);
}

mf_shared_status_v1 mf_cdev_launch_descriptor_v0(uint64_t request_id, uint64_t generation,
                                                 const mf_cdev_launch_v0* launch,
                                                 mf_ring_descriptor_v1* out_descriptor) {
  if (launch == NULL || out_descriptor == NULL || request_id == 0U || generation == 0U ||
      launch->module_id == 0U || launch->module_generation == 0U ||
      launch->argument_block_id == 0U || launch->argument_block_generation == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_descriptor, 0, sizeof(*out_descriptor));
  out_descriptor->opcode = MF_RING_OPCODE_LAUNCH;
  out_descriptor->request_id = request_id;
  out_descriptor->target_id = generation;
  out_descriptor->arguments[0] = launch->module_id;
  out_descriptor->arguments[1] = launch->module_generation;
  out_descriptor->arguments[2] = launch->argument_block_id;
  out_descriptor->arguments[3] = launch->argument_block_generation;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_cdev_submit_launch_v0(mf_cdev_session_v0* session, uint64_t request_id,
                                             const mf_cdev_launch_v0* launch) {
  mf_ring_descriptor_v1 descriptor;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (session == NULL || session->device_fd < 0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status =
      mf_cdev_launch_descriptor_v0(request_id, session->device_generation, launch, &descriptor);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  return mf_client_ring_try_submit_v1(&session->submission, &descriptor);
}

mf_shared_status_v1 mf_cdev_try_consume_completion_v0(mf_cdev_session_v0* session,
                                                      mf_ring_descriptor_v1* out_descriptor) {
  if (session == NULL || session->device_fd < 0 || out_descriptor == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_ring_try_consume_v1(&session->completion, out_descriptor);
}

mf_shared_status_v1 mf_cdev_wait_v0(mf_cdev_session_v0* session, uint64_t timeline,
                                    uint64_t timeout_ns) {
  mf_uapi_wait_v0 wait_request;
  int result = -1;
  if (session == NULL || session->device_fd < 0 || timeline == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(&wait_request, 0, sizeof(wait_request));
  wait_request.struct_size = sizeof(wait_request);
  wait_request.queue_id = session->queue_id;
  wait_request.timeline = timeline;
  wait_request.timeout_ns = timeout_ns;
  result = ioctl(session->device_fd, MF_UAPI_IOCTL_WAIT, &wait_request);
  if (result == 0) {
    return MF_SHARED_SUCCESS;
  }
  if (errno == ETIMEDOUT) {
    return MF_SHARED_TIMEOUT;
  }
  if (errno == EINTR) {
    return MF_SHARED_INTERRUPTED;
  }
  if (errno == ENODEV) {
    return MF_SHARED_DEVICE_LOST;
  }
  return errno == EINVAL ? MF_SHARED_INVALID_ARGUMENT : MF_SHARED_SYSTEM_ERROR;
}

int32_t mf_cdev_borrow_fd_v0(const mf_cdev_session_v0* session) {
  return session == NULL ? -1 : session->device_fd;
}
