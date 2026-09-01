#define _GNU_SOURCE

#include "metaflux/client/fastpath.h"

#include "metaflux/client/protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/futex.h>
#include <linux/memfd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MF_CLIENT_MAX_TRANSFERRED_FDS UINT32_C(3)

static int mf_client_ring_capacity_valid(uint32_t capacity) {
  return capacity >= UINT32_C(2) && capacity <= (UINT32_C(1) << 30U) &&
         (capacity & (capacity - UINT32_C(1))) == UINT32_C(0);
}

static void mf_client_close_transferred_fds(int32_t* descriptors, uint32_t count) {
  uint32_t index = 0;
  for (index = 0; index < count; ++index) {
    if (descriptors[index] >= 0) {
      (void)close(descriptors[index]);
      descriptors[index] = -1;
    }
  }
}

static mf_shared_status_v1 mf_client_send_packet(int32_t socket_fd, const uint8_t* bytes,
                                                 uint64_t byte_count, const int32_t* descriptors,
                                                 uint32_t descriptor_count) {
  union {
    struct cmsghdr alignment;
    uint8_t bytes[CMSG_SPACE(sizeof(int32_t) * MF_CLIENT_MAX_TRANSFERRED_FDS)];
  } control;
  struct iovec vector;
  struct msghdr message;
  ssize_t sent = -1;

  if (socket_fd < 0 || bytes == (const uint8_t*)0 || byte_count == UINT64_C(0) ||
      byte_count > (uint64_t)SIZE_MAX || descriptor_count > MF_CLIENT_MAX_TRANSFERRED_FDS ||
      (descriptor_count != UINT32_C(0) && descriptors == (const int32_t*)0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(&control, 0, sizeof(control));
  (void)memset(&message, 0, sizeof(message));
  vector.iov_base = (void*)bytes;
  vector.iov_len = (size_t)byte_count;
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  if (descriptor_count != UINT32_C(0)) {
    struct cmsghdr* header = (struct cmsghdr*)control.bytes;
    message.msg_control = control.bytes;
    message.msg_controllen = CMSG_SPACE(sizeof(int32_t) * descriptor_count);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int32_t) * descriptor_count);
    (void)memcpy(CMSG_DATA(header), descriptors, sizeof(int32_t) * descriptor_count);
  }
  do {
    sent = sendmsg(socket_fd, &message, MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  if (sent < 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  return (uint64_t)sent == byte_count ? MF_SHARED_SUCCESS : MF_SHARED_MALFORMED;
}

static mf_shared_status_v1 mf_client_receive_packet(int32_t socket_fd, uint8_t* bytes,
                                                    uint64_t byte_count, int32_t* out_descriptors,
                                                    uint32_t descriptor_capacity,
                                                    uint32_t* out_descriptor_count) {
  union {
    struct cmsghdr alignment;
    uint8_t bytes[CMSG_SPACE(sizeof(int32_t) * MF_CLIENT_MAX_TRANSFERRED_FDS)];
  } control;
  struct iovec vector;
  struct msghdr message;
  ssize_t received = -1;
  uint32_t descriptor_count = 0;
  uint32_t index = 0;

  if (socket_fd < 0 || bytes == (uint8_t*)0 || byte_count == UINT64_C(0) ||
      byte_count > (uint64_t)SIZE_MAX || out_descriptor_count == (uint32_t*)0 ||
      descriptor_capacity > MF_CLIENT_MAX_TRANSFERRED_FDS ||
      (descriptor_capacity != UINT32_C(0) && out_descriptors == (int32_t*)0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_descriptor_count = UINT32_C(0);
  for (index = 0; index < descriptor_capacity; ++index) {
    out_descriptors[index] = -1;
  }

  for (;;) {
    (void)memset(&control, 0, sizeof(control));
    (void)memset(&message, 0, sizeof(message));
    vector.iov_base = bytes;
    vector.iov_len = (size_t)byte_count;
    message.msg_iov = &vector;
    message.msg_iovlen = 1;
    message.msg_control = control.bytes;
    message.msg_controllen = sizeof(control.bytes);
    received = recvmsg(socket_fd, &message, MSG_CMSG_CLOEXEC);
    if (received < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
  if (received == 0) {
    return MF_SHARED_TERMINAL_VIEW;
  }
  if (received < 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }

  for (struct cmsghdr* header = CMSG_FIRSTHDR(&message); header != (struct cmsghdr*)0;
       header = CMSG_NXTHDR(&message, header)) {
    size_t payload_size = 0;
    uint32_t incoming_count = 0;
    if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS ||
        header->cmsg_len < CMSG_LEN(0)) {
      mf_client_close_transferred_fds(out_descriptors, descriptor_count);
      return MF_SHARED_MALFORMED;
    }
    payload_size = header->cmsg_len - CMSG_LEN(0);
    if ((payload_size % sizeof(int32_t)) != (size_t)0 ||
        payload_size / sizeof(int32_t) > UINT32_MAX) {
      mf_client_close_transferred_fds(out_descriptors, descriptor_count);
      return MF_SHARED_MALFORMED;
    }
    incoming_count = (uint32_t)(payload_size / sizeof(int32_t));
    if (incoming_count > descriptor_capacity - descriptor_count) {
      int32_t extras[MF_CLIENT_MAX_TRANSFERRED_FDS] = {-1, -1, -1};
      const uint32_t copy_count = incoming_count > MF_CLIENT_MAX_TRANSFERRED_FDS
                                      ? MF_CLIENT_MAX_TRANSFERRED_FDS
                                      : incoming_count;
      (void)memcpy(extras, CMSG_DATA(header), sizeof(int32_t) * copy_count);
      mf_client_close_transferred_fds(extras, copy_count);
      mf_client_close_transferred_fds(out_descriptors, descriptor_count);
      return MF_SHARED_MALFORMED;
    }
    (void)memcpy(out_descriptors + descriptor_count, CMSG_DATA(header), payload_size);
    descriptor_count += incoming_count;
  }
  if ((message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0 || (uint64_t)received != byte_count) {
    mf_client_close_transferred_fds(out_descriptors, descriptor_count);
    return MF_SHARED_MALFORMED;
  }
  *out_descriptor_count = descriptor_count;
  return MF_SHARED_SUCCESS;
}

static int mf_client_registry_advance(uint64_t* offset, uint64_t count, uint64_t stride) {
  if (offset == (uint64_t*)0 || count > (UINT64_MAX - *offset) / stride) {
    return 0;
  }
  *offset += count * stride;
  return 1;
}

static int mf_client_registry_offsets_valid(const mf_shared_registry_header_v1* header,
                                            const uint8_t* bytes, uint64_t mapping_size) {
  const uint64_t count = header->device_count;
  uint64_t offset = sizeof(mf_shared_registry_header_v1);
  if (header->view_admission_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, UINT64_C(1), sizeof(mf_view_admission_control_v1))) {
    return 0;
  }
  if (header->view_control_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, UINT64_C(1), sizeof(mf_registry_view_control_v1))) {
    return 0;
  }
  if (header->identities_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, count, sizeof(mf_virtual_device_identity_v1))) {
    return 0;
  }
  if (header->device_admission_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, count, sizeof(mf_device_admission_control_v1))) {
    return 0;
  }
  if (header->lifecycle_fences_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, count, sizeof(mf_virtual_device_lifecycle_fence_v1))) {
    return 0;
  }
  if (header->telemetry_control_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, UINT64_C(1), sizeof(mf_telemetry_control_v1))) {
    return 0;
  }
  if (header->telemetry_bank0_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, count, sizeof(mf_virtual_device_telemetry_v1))) {
    return 0;
  }
  if (header->telemetry_bank1_offset != offset) {
    return 0;
  }
  if (!mf_client_registry_advance(&offset, count, sizeof(mf_virtual_device_telemetry_v1))) {
    return 0;
  }
  if (header->flags == UINT32_C(0)) {
    return offset == header->total_size && offset == mapping_size;
  }
  if (header->flags != MF_SHARED_REGISTRY_FLAG_RECOVERY_TABLES_V1 || offset > mapping_size ||
      sizeof(mf_shared_registry_extension_header_v1) > mapping_size - offset) {
    return 0;
  }

  const mf_shared_registry_extension_header_v1* extension =
      (const mf_shared_registry_extension_header_v1*)(bytes + offset);
  if (extension->magic != MF_SHARED_REGISTRY_EXTENSION_MAGIC ||
      extension->abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      extension->header_size != sizeof(*extension) || extension->flags != UINT32_C(0) ||
      extension->total_size != mapping_size ||
      !mf_registry_view_id_equal_v1(extension->registry_view_id, header->registry_view_id) ||
      extension->admission_attempt_capacity != MF_SHARED_M0100_ADMISSION_ATTEMPT_CAPACITY ||
      extension->admission_lease_capacity != MF_SHARED_M0100_ADMISSION_LEASE_CAPACITY ||
      extension->device_update_capacity != MF_SHARED_M0100_DEVICE_UPDATE_CAPACITY ||
      extension->lifecycle_range_capacity != MF_SHARED_M0100_LIFECYCLE_RANGE_CAPACITY ||
      extension->view_publish_capacity != MF_SHARED_M0100_VIEW_PUBLISH_CAPACITY ||
      extension->telemetry_publish_capacity != MF_SHARED_M0100_TELEMETRY_PUBLISH_CAPACITY ||
      extension->ordinary_view_publish_capacity != MF_SHARED_M0100_VIEW_PUBLISH_ORDINARY_CAPACITY ||
      extension->ordinary_telemetry_publish_capacity !=
          MF_SHARED_M0100_TELEMETRY_PUBLISH_ORDINARY_CAPACITY ||
      extension->close_closing_slot != MF_SHARED_M0100_VIEW_PUBLISH_ORDINARY_CAPACITY ||
      extension->close_terminal_slot !=
          MF_SHARED_M0100_VIEW_PUBLISH_ORDINARY_CAPACITY + UINT32_C(1) ||
      extension->telemetry_terminal_slot != MF_SHARED_M0100_TELEMETRY_PUBLISH_ORDINARY_CAPACITY) {
    return 0;
  }

  if (!mf_client_registry_advance(&offset, UINT64_C(1),
                                  sizeof(mf_shared_registry_extension_header_v1)) ||
      extension->view_publisher_control_offset != offset ||
      !mf_client_registry_advance(&offset, UINT64_C(1), sizeof(mf_view_publisher_control_v1)) ||
      extension->telemetry_publisher_control_offset != offset ||
      !mf_client_registry_advance(&offset, UINT64_C(1),
                                  sizeof(mf_telemetry_publisher_control_v1)) ||
      extension->admission_attempts_offset != offset ||
      !mf_client_registry_advance(&offset, extension->admission_attempt_capacity,
                                  sizeof(mf_admission_attempt_record_v1)) ||
      extension->admission_leases_offset != offset ||
      !mf_client_registry_advance(&offset, extension->admission_lease_capacity,
                                  sizeof(mf_admission_lease_record_v1)) ||
      extension->device_updates_offset != offset ||
      !mf_client_registry_advance(&offset, extension->device_update_capacity,
                                  sizeof(mf_device_validation_update_record_v1)) ||
      extension->lifecycle_ranges_offset != offset ||
      !mf_client_registry_advance(&offset, extension->lifecycle_range_capacity,
                                  sizeof(mf_lifecycle_range_record_v1)) ||
      extension->view_publish_records_offset != offset ||
      !mf_client_registry_advance(&offset, extension->view_publish_capacity,
                                  sizeof(mf_view_publish_record_v1)) ||
      extension->telemetry_publish_records_offset != offset ||
      !mf_client_registry_advance(&offset, extension->telemetry_publish_capacity,
                                  sizeof(mf_telemetry_publish_record_v1))) {
    return 0;
  }
  return offset == mapping_size;
}

static mf_shared_status_v1 mf_client_registry_stable_control(const mf_client_registry_v1* registry,
                                                             uint64_t* out_latch) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(4); ++attempt) {
    const uint64_t first =
        mf_atomic_load_u64_acquire(&registry->view_control->control_latch_sequence);
    uint64_t daemon_incarnation = 0;
    uint64_t view_serial = 0;
    uint32_t gate_state = 0;
    uint32_t mapping_terminal = 0;
    uint64_t second = 0;
    if ((first & UINT64_C(1)) != UINT64_C(0)) {
      continue;
    }
    daemon_incarnation =
        mf_atomic_load_u64_relaxed(&registry->view_control->registry_view_id.daemon_incarnation);
    view_serial = mf_atomic_load_u64_relaxed(&registry->view_control->registry_view_id.view_serial);
    gate_state = mf_atomic_load_u32_relaxed(&registry->view_control->gate_state);
    mapping_terminal = mf_atomic_load_u32_relaxed(&registry->view_control->mapping_terminal);
    mf_atomic_signal_fence_seq_cst();
    second = mf_atomic_load_u64_acquire(&registry->view_control->control_latch_sequence);
    if (first == second) {
      if (daemon_incarnation != registry->registry_view_id.daemon_incarnation ||
          view_serial != registry->registry_view_id.view_serial ||
          gate_state != MF_VIEW_GATE_OPEN || mapping_terminal != UINT32_C(0)) {
        return MF_SHARED_TERMINAL_VIEW;
      }
      *out_latch = first;
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

static mf_shared_status_v1 mf_client_registry_stable_fence(const mf_client_registry_v1* registry,
                                                           uint32_t device_index,
                                                           mf_client_fence_snapshot_v1* out_fence,
                                                           uint64_t* out_latch) {
  const mf_virtual_device_lifecycle_fence_v1* fence = &registry->lifecycle_fences[device_index];
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(4); ++attempt) {
    const uint64_t first = mf_atomic_load_u64_acquire(&fence->fence_latch_sequence);
    uint64_t second = 0;
    if ((first & UINT64_C(1)) != UINT64_C(0)) {
      continue;
    }
    out_fence->identity_record_id = mf_atomic_load_u64_relaxed(&fence->identity_record_id);
    out_fence->lifecycle_sequence = mf_atomic_load_u64_relaxed(&fence->lifecycle_sequence);
    out_fence->epoch = mf_atomic_load_u64_relaxed(&fence->epoch);
    out_fence->effective_quota_bytes = mf_atomic_load_u64_relaxed(&fence->effective_quota_bytes);
    out_fence->policy_bits = mf_atomic_load_u64_relaxed(&fence->policy_bits);
    out_fence->device_state = mf_atomic_load_u32_relaxed(&fence->device_state);
    out_fence->reserved = UINT32_C(0);
    mf_atomic_signal_fence_seq_cst();
    second = mf_atomic_load_u64_acquire(&fence->fence_latch_sequence);
    if (first == second) {
      *out_latch = first;
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

static uint32_t mf_client_registry_find_identity(const mf_client_registry_v1* registry,
                                                 uint64_t identity_record_id) {
  uint32_t index = 0;
  for (index = 0; index < registry->device_count; ++index) {
    if (registry->identities[index].identity_record_id == identity_record_id) {
      return index;
    }
  }
  return registry->device_count;
}

static mf_shared_status_v1 mf_client_submit_command(mf_client_ring_v1* ring, uint32_t opcode,
                                                    uint32_t flags, uint64_t request_id,
                                                    uint64_t target_id, uint64_t argument0,
                                                    uint64_t argument1, uint64_t argument2,
                                                    uint64_t argument3) {
  mf_ring_descriptor_v1 descriptor;
  if (request_id == UINT64_C(0) || target_id == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(&descriptor, 0, sizeof(descriptor));
  descriptor.opcode = opcode;
  descriptor.flags = flags;
  descriptor.request_id = request_id;
  descriptor.target_id = target_id;
  descriptor.arguments[0] = argument0;
  descriptor.arguments[1] = argument1;
  descriptor.arguments[2] = argument2;
  descriptor.arguments[3] = argument3;
  return mf_client_ring_try_submit_v1(ring, &descriptor);
}

static mf_shared_status_v1
mf_client_ring_validate_mapping(void* mapping, uint64_t mapping_size,
                                mf_registry_view_id_v1 expected_view_id, uint64_t expected_queue_id,
                                uint64_t expected_queue_generation, mf_ring_header_v1** out_header,
                                mf_ring_descriptor_v1** out_descriptors, uint32_t* out_capacity) {
  mf_ring_header_v1* header = (mf_ring_header_v1*)mapping;
  uint32_t capacity = 0;
  uint64_t expected_size = 0;

  if (mapping == (void*)0 || ((uintptr_t)mapping % MF_SHARED_CACHE_LINE_SIZE) != (uintptr_t)0 ||
      mapping_size < sizeof(mf_ring_header_v1)) {
    return MF_SHARED_MALFORMED;
  }
  capacity = header->metadata.capacity;
  if (header->metadata.magic != MF_SHARED_RING_MAGIC ||
      header->metadata.abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      header->metadata.header_size != sizeof(mf_ring_header_v1) ||
      header->metadata.descriptor_size != sizeof(mf_ring_descriptor_v1) ||
      header->metadata.flags != UINT32_C(0) || header->metadata.mapping_size != mapping_size ||
      !mf_client_ring_capacity_valid(capacity) ||
      !mf_registry_view_id_equal_v1(header->metadata.registry_view_id, expected_view_id) ||
      header->metadata.queue_id != expected_queue_id ||
      header->metadata.queue_generation != expected_queue_generation) {
    return MF_SHARED_MALFORMED;
  }
  if (mf_client_ring_mapping_size_v1(capacity, &expected_size) != MF_SHARED_SUCCESS ||
      expected_size != mapping_size) {
    return MF_SHARED_MALFORMED;
  }

  *out_header = header;
  *out_descriptors = (mf_ring_descriptor_v1*)((uint8_t*)mapping + sizeof(*header));
  *out_capacity = capacity;
  return MF_SHARED_SUCCESS;
}

static int mf_client_ring_handle_valid(const mf_client_ring_v1* ring) {
  return ring != (const mf_client_ring_v1*)0 && ring->header != (mf_ring_header_v1*)0 &&
         ring->descriptors != (mf_ring_descriptor_v1*)0 &&
         mf_client_ring_capacity_valid(ring->capacity);
}

static int mf_client_ring_is_readable(const mf_client_ring_v1* ring) {
  const uint64_t position = mf_atomic_load_u64_relaxed(&ring->header->consumer.position);
  const uint32_t mask = ring->capacity - UINT32_C(1);
  const mf_ring_descriptor_v1* slot = &ring->descriptors[position & (uint64_t)mask];
  return mf_atomic_load_u64_acquire(&slot->sequence) == position + UINT64_C(1);
}

static int mf_client_ring_is_writable(const mf_client_ring_v1* ring) {
  const uint64_t position = mf_atomic_load_u64_relaxed(&ring->header->producer.position);
  const uint32_t mask = ring->capacity - UINT32_C(1);
  const mf_ring_descriptor_v1* slot = &ring->descriptors[position & (uint64_t)mask];
  return mf_atomic_load_u64_acquire(&slot->sequence) == position;
}

static void mf_client_ring_wake(uint32_t* waiter_count, uint32_t* wake_sequence,
                                uint64_t* doorbell_count) {
  if (mf_atomic_load_u32_acquire(waiter_count) != UINT32_C(0)) {
    (void)mf_atomic_fetch_add_u32_acq_rel(wake_sequence, UINT32_C(1));
    (void)mf_atomic_fetch_add_u64_acq_rel(doorbell_count, UINT64_C(1));
    (void)syscall(SYS_futex, wake_sequence, FUTEX_WAKE, 1, (void*)0, (void*)0, 0);
  }
}

static mf_shared_status_v1 mf_client_ring_register_waiter(uint32_t* waiter_count) {
  uint32_t count = mf_atomic_load_u32_acquire(waiter_count);
  for (;;) {
    uint32_t expected = count;
    if (count == UINT32_MAX) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    if (mf_atomic_compare_exchange_u32_seq_cst(waiter_count, &expected, count + UINT32_C(1))) {
      return MF_SHARED_SUCCESS;
    }
    count = expected;
  }
}

static mf_shared_status_v1 mf_client_ring_wait(mf_client_ring_v1* ring, uint32_t* waiter_count,
                                               uint32_t* wake_sequence, uint64_t timeout_ns,
                                               int (*ready)(const mf_client_ring_v1*)) {
  struct timespec timeout = {0, 0};
  const struct timespec* timeout_pointer = &timeout;
  uint32_t observed_sequence = 0;
  long result = 0;

  if (!mf_client_ring_handle_valid(ring)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (ready(ring) != 0) {
    return MF_SHARED_SUCCESS;
  }
  if (timeout_ns == UINT64_C(0)) {
    return MF_SHARED_WOULD_BLOCK;
  }

  observed_sequence = mf_atomic_load_u32_acquire(wake_sequence);
  {
    const mf_shared_status_v1 register_status = mf_client_ring_register_waiter(waiter_count);
    if (register_status != MF_SHARED_SUCCESS) {
      return register_status;
    }
  }
  if (ready(ring) != 0) {
    (void)mf_atomic_fetch_sub_u32_acq_rel(waiter_count, UINT32_C(1));
    return MF_SHARED_SUCCESS;
  }

  if (timeout_ns == MF_CLIENT_RING_INFINITE_TIMEOUT_NS) {
    timeout_pointer = (const struct timespec*)0;
  } else {
    timeout.tv_sec = (time_t)(timeout_ns / UINT64_C(1000000000));
    timeout.tv_nsec = (long)(timeout_ns % UINT64_C(1000000000));
  }

  result = syscall(SYS_futex, wake_sequence, FUTEX_WAIT, observed_sequence, timeout_pointer,
                   (void*)0, 0);
  (void)mf_atomic_fetch_sub_u32_acq_rel(waiter_count, UINT32_C(1));
  if (ready(ring) != 0) {
    return MF_SHARED_SUCCESS;
  }
  if (result == 0 || errno == EAGAIN) {
    return MF_SHARED_RETRY;
  }
  if (errno == ETIMEDOUT) {
    return MF_SHARED_TIMEOUT;
  }
  if (errno == EINTR) {
    return MF_SHARED_INTERRUPTED;
  }
  return MF_SHARED_SYSTEM_ERROR;
}

uint32_t mf_client_fastpath_bootstrap_abi_version(void) { return MF_CLIENT_PROTOCOL_ABI_VERSION_1; }

mf_shared_status_v1 mf_client_argument_block_size_v1(uint32_t entry_count,
                                                     uint64_t* out_byte_count) {
  if (entry_count == UINT32_C(0) || out_byte_count == (uint64_t*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_byte_count =
      sizeof(mf_argument_block_header_v1) + ((uint64_t)entry_count * sizeof(mf_argument_entry_v1));
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_argument_block_validate_v1(const uint8_t* bytes,
                                                         uint64_t byte_count) {
  const mf_argument_block_header_v1* header = (const mf_argument_block_header_v1*)bytes;
  const mf_argument_entry_v1* entries = (const mf_argument_entry_v1*)0;
  uint64_t expected_size = 0;
  uint32_t index = 0;
  uint32_t reserved_index = 0;
  if (bytes == (const uint8_t*)0 ||
      ((uintptr_t)bytes % _Alignof(mf_argument_block_header_v1)) != (uintptr_t)0 ||
      byte_count < sizeof(*header) || header->magic != MF_SHARED_ARGUMENT_BLOCK_MAGIC ||
      header->abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      header->header_size != sizeof(*header) ||
      header->entry_size != sizeof(mf_argument_entry_v1) ||
      (header->flags & ~MF_ARGUMENT_BLOCK_KNOWN_FLAGS_V1) != UINT32_C(0) ||
      (header->flags != UINT32_C(0) &&
       header->flags != MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1 &&
       header->flags != MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1) ||
      header->total_size != byte_count ||
      mf_client_argument_block_size_v1(header->entry_count, &expected_size) != MF_SHARED_SUCCESS ||
      expected_size != byte_count) {
    return MF_SHARED_MALFORMED;
  }
  for (reserved_index = 0; reserved_index < UINT32_C(4); ++reserved_index) {
    if (((header->flags == UINT32_C(0) || header->flags == MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1) &&
         header->reserved[reserved_index] != UINT64_C(0)) ||
        (header->flags == MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1 &&
         (header->reserved[reserved_index] == UINT64_C(0) ||
          header->reserved[reserved_index] > UINT32_MAX))) {
      return MF_SHARED_MALFORMED;
    }
  }
  entries = (const mf_argument_entry_v1*)(bytes + sizeof(*header));
  for (index = 0; index < header->entry_count; ++index) {
    const mf_argument_entry_v1* entry = &entries[index];
    if (entry->kind == MF_ARGUMENT_KIND_U32) {
      if (entry->flags != UINT32_C(0) || entry->object_id != UINT64_C(0) ||
          entry->object_generation != UINT64_C(0) || entry->value > UINT32_MAX) {
        return MF_SHARED_MALFORMED;
      }
    } else if (entry->kind == MF_ARGUMENT_KIND_U64) {
      if (entry->flags != UINT32_C(0) || entry->object_id != UINT64_C(0) ||
          entry->object_generation != UINT64_C(0)) {
        return MF_SHARED_MALFORMED;
      }
    } else if (entry->kind == MF_ARGUMENT_KIND_BUFFER) {
      if (entry->flags == UINT32_C(0) ||
          (entry->flags & ~MF_ARGUMENT_BUFFER_KNOWN_FLAGS) != UINT32_C(0) ||
          entry->object_id == UINT64_C(0) || entry->object_generation == UINT64_C(0) ||
          (header->flags != MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1 &&
           (entry->value % sizeof(uint32_t)) != UINT64_C(0))) {
        return MF_SHARED_MALFORMED;
      }
    } else {
      return MF_SHARED_MALFORMED;
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_copy_region_argument_block_validate_v1(const uint8_t* bytes,
                                                                     uint64_t byte_count) {
  const mf_argument_block_header_v1* header = (const mf_argument_block_header_v1*)bytes;
  const mf_argument_entry_v1* entries = (const mf_argument_entry_v1*)0;
  uint64_t expected_size = UINT64_C(0);
  mf_shared_status_v1 status = mf_client_argument_block_validate_v1(bytes, byte_count);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (mf_client_argument_block_size_v1(MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1, &expected_size) !=
          MF_SHARED_SUCCESS ||
      byte_count != expected_size || header->flags != MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1 ||
      header->entry_count != MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1) {
    return MF_SHARED_MALFORMED;
  }
  entries = (const mf_argument_entry_v1*)(bytes + sizeof(*header));
  if (entries[MF_COPY_REGION_DESTINATION_INDEX_V1].kind != MF_ARGUMENT_KIND_BUFFER ||
      entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags != MF_ARGUMENT_BUFFER_WRITE ||
      entries[MF_COPY_REGION_SOURCE_INDEX_V1].kind != MF_ARGUMENT_KIND_BUFFER ||
      entries[MF_COPY_REGION_SOURCE_INDEX_V1].flags != MF_ARGUMENT_BUFFER_READ ||
      entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].kind != MF_ARGUMENT_KIND_U64 ||
      entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value == UINT64_C(0)) {
    return MF_SHARED_MALFORMED;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_payload_create_v1(const uint8_t* initial_bytes, uint64_t byte_count,
                                                uint32_t flags, mf_client_payload_v1* out_payload) {
  long created_fd = -1;
  int32_t fd = -1;
  void* mapping = MAP_FAILED;
  int seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;

  if (out_payload == (mf_client_payload_v1*)0 || byte_count == UINT64_C(0) ||
      byte_count > (uint64_t)SIZE_MAX || byte_count > (uint64_t)INT64_MAX ||
      (flags & ~MF_CLIENT_PAYLOAD_WRITABLE_V1) != UINT32_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_payload, 0, sizeof(*out_payload));
  out_payload->owned_fd = -1;
  created_fd =
      syscall(SYS_memfd_create, "metaflux-control-payload-v1", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (created_fd < 0 || created_fd > INT_MAX) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  fd = (int32_t)created_fd;
  if (ftruncate(fd, (off_t)byte_count) != 0) {
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping = mmap((void*)0, (size_t)byte_count, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  if (initial_bytes == (const uint8_t*)0) {
    (void)memset(mapping, 0, (size_t)byte_count);
  } else {
    (void)memcpy(mapping, initial_bytes, (size_t)byte_count);
  }

  if ((flags & MF_CLIENT_PAYLOAD_WRITABLE_V1) == UINT32_C(0)) {
    if (munmap(mapping, (size_t)byte_count) != 0) {
      (void)close(fd);
      return MF_SHARED_SYSTEM_ERROR;
    }
    mapping = MAP_FAILED;
    seals |= F_SEAL_WRITE;
  }
  if (fcntl(fd, F_ADD_SEALS, seals) != 0) {
    if (mapping != MAP_FAILED) {
      (void)munmap(mapping, (size_t)byte_count);
    }
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  if (mapping == MAP_FAILED) {
    mapping = mmap((void*)0, (size_t)byte_count, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
      (void)close(fd);
      return MF_SHARED_SYSTEM_ERROR;
    }
  }
  out_payload->mapping = mapping;
  out_payload->mapping_size = byte_count;
  out_payload->owned_fd = fd;
  out_payload->flags = flags;
  return MF_SHARED_SUCCESS;
}

void mf_client_payload_close_v1(mf_client_payload_v1* payload) {
  if (payload == (mf_client_payload_v1*)0) {
    return;
  }
  if (payload->mapping != (void*)0 && payload->mapping_size <= (uint64_t)SIZE_MAX) {
    (void)munmap(payload->mapping, (size_t)payload->mapping_size);
  }
  if (payload->owned_fd >= 0) {
    (void)close(payload->owned_fd);
  }
  (void)memset(payload, 0, sizeof(*payload));
  payload->owned_fd = -1;
}

int32_t mf_client_payload_borrow_fd_v1(const mf_client_payload_v1* payload) {
  return payload == (const mf_client_payload_v1*)0 ? -1 : payload->owned_fd;
}

static void mf_client_session_initialize_empty(mf_client_session_v1* session) {
  (void)memset(session, 0, sizeof(*session));
  session->socket_fd = -1;
  session->registry.owned_fd = -1;
  session->submission.owned_fd = -1;
  session->completion.owned_fd = -1;
}

void mf_client_session_close_v1(mf_client_session_v1* session) {
  if (session == (mf_client_session_v1*)0) {
    return;
  }
  if (session->socket_fd >= 0) {
    (void)close(session->socket_fd);
  }
  mf_client_ring_close_v1(&session->completion);
  mf_client_ring_close_v1(&session->submission);
  mf_client_registry_close_v1(&session->registry);
  mf_client_session_initialize_empty(session);
}

mf_shared_status_v1
mf_client_session_connect_capabilities_v1(const char* socket_path, uint64_t required_capabilities,
                                          uint64_t optional_capabilities,
                                          mf_client_session_v1* out_session) {
  struct sockaddr_un address;
  mf_client_negotiation_request_v1 request;
  mf_client_negotiation_response_v1 response;
  int32_t transferred_fds[MF_CLIENT_MAX_TRANSFERRED_FDS] = {-1, -1, -1};
  uint32_t transferred_count = 0;
  size_t path_length = 0;
  socklen_t address_length = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (socket_path == (const char*)0 || out_session == (mf_client_session_v1*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_client_session_initialize_empty(out_session);
  path_length = strlen(socket_path);
  if (path_length == (size_t)0 || path_length >= sizeof(address.sun_path)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  out_session->socket_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (out_session->socket_fd < 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  (void)memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  (void)memcpy(address.sun_path, socket_path, path_length + (size_t)1);
  address_length = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_length + (size_t)1);
  if (connect(out_session->socket_fd, (const struct sockaddr*)&address, address_length) != 0) {
    mf_client_session_close_v1(out_session);
    return MF_SHARED_SYSTEM_ERROR;
  }

  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), required_capabilities,
                                        optional_capabilities,
                                        MF_CLIENT_FLAG_JOIN_EXISTING_VIEW_V1);
  status = mf_client_send_packet(out_session->socket_fd, request.bytes,
                                 MF_CLIENT_PROTOCOL_WIRE_SIZE_V1, (const int32_t*)0, UINT32_C(0));
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_receive_packet(out_session->socket_fd, response.bytes,
                                      MF_CLIENT_PROTOCOL_WIRE_SIZE_V1, transferred_fds,
                                      MF_CLIENT_MAX_TRANSFERRED_FDS, &transferred_count);
  }
  if (status != MF_SHARED_SUCCESS ||
      mf_client_negotiation_response_validate_v1(&response) != MF_CLIENT_NEGOTIATION_OK) {
    mf_client_close_transferred_fds(transferred_fds, transferred_count);
    mf_client_session_close_v1(out_session);
    return status == MF_SHARED_SUCCESS ? MF_SHARED_MALFORMED : status;
  }
  if (mf_client_load_le32_v1(response.bytes + 12) != MF_CLIENT_NEGOTIATION_OK) {
    const uint32_t negotiation_status = mf_client_load_le32_v1(response.bytes + 12);
    mf_client_close_transferred_fds(transferred_fds, transferred_count);
    mf_client_session_close_v1(out_session);
    return negotiation_status == MF_CLIENT_NEGOTIATION_INCOMPATIBLE_VIEW ? MF_SHARED_STALE_HANDLE
           : negotiation_status == MF_CLIENT_NEGOTIATION_MALFORMED       ? MF_SHARED_MALFORMED
                                                                         : MF_SHARED_NOT_SUPPORTED;
  }
  if (transferred_count != MF_CLIENT_MAX_TRANSFERRED_FDS ||
      (mf_client_load_le64_v1(response.bytes + 24) & required_capabilities) !=
          required_capabilities) {
    mf_client_close_transferred_fds(transferred_fds, transferred_count);
    mf_client_session_close_v1(out_session);
    return MF_SHARED_MALFORMED;
  }

  out_session->negotiated_version = mf_client_load_le32_v1(response.bytes + 16);
  out_session->negotiated_capabilities = mf_client_load_le64_v1(response.bytes + 24);
  out_session->registry_view_id.daemon_incarnation = mf_client_load_le64_v1(response.bytes + 40);
  out_session->registry_view_id.view_serial = mf_client_load_le64_v1(response.bytes + 48);
  out_session->next_control_request_id = UINT64_C(1);
  status = mf_client_registry_attach_v1(transferred_fds[0], out_session->registry_view_id,
                                        &out_session->registry);
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_ring_attach_v1(transferred_fds[1], out_session->registry_view_id,
                                      MF_CLIENT_SUBMISSION_QUEUE_ID_V1,
                                      MF_CLIENT_QUEUE_GENERATION_V1, &out_session->submission);
  }
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_ring_attach_v1(transferred_fds[2], out_session->registry_view_id,
                                      MF_CLIENT_COMPLETION_QUEUE_ID_V1,
                                      MF_CLIENT_QUEUE_GENERATION_V1, &out_session->completion);
  }
  mf_client_close_transferred_fds(transferred_fds, transferred_count);
  if (status != MF_SHARED_SUCCESS) {
    mf_client_session_close_v1(out_session);
  }
  return status;
}

mf_shared_status_v1 mf_client_session_connect_v1(const char* socket_path,
                                                 mf_client_session_v1* out_session) {
  const uint64_t required_capabilities =
      MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
      MF_CLIENT_CAP_FUTEX_DOORBELL_V1 | MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1;
  const uint64_t optional_capabilities = MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1 |
                                         MF_CLIENT_CAP_COPY_REGION_V1 |
                                         MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
  return mf_client_session_connect_capabilities_v1(socket_path, required_capabilities,
                                                   optional_capabilities, out_session);
}

mf_shared_status_v1 mf_client_observer_connect_v1(const char* socket_path,
                                                  mf_client_session_v1* out_session) {
  const uint64_t required_capabilities =
      MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
      MF_CLIENT_CAP_FUTEX_DOORBELL_V1 | MF_CLIENT_CAP_PROCESS_SNAPSHOT_V1;
  const uint64_t optional_capabilities =
      MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1 | MF_CLIENT_CAP_POLICY_SETTERS_V1;
  return mf_client_session_connect_capabilities_v1(socket_path, required_capabilities,
                                                   optional_capabilities, out_session);
}

mf_shared_status_v1 mf_client_session_connect_default_v1(mf_client_session_v1* out_session) {
  const uint64_t required_capabilities =
      MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
      MF_CLIENT_CAP_FUTEX_DOORBELL_V1 | MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1;
  const uint64_t optional_capabilities = MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1 |
                                         MF_CLIENT_CAP_COPY_REGION_V1 |
                                         MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
  return mf_client_session_connect_default_capabilities_v1(required_capabilities,
                                                           optional_capabilities, out_session);
}

mf_shared_status_v1 mf_client_session_connect_default_capabilities_v1(
    uint64_t required_capabilities, uint64_t optional_capabilities,
    mf_client_session_v1* out_session) {
  const char* configured = getenv("METAFLUX_SOCKET");
  const char* runtime_directory = getenv("XDG_RUNTIME_DIR");
  char socket_path[sizeof(((struct sockaddr_un*)0)->sun_path)];
  int written = 0;
  if (configured != (const char*)0 && configured[0] != '\0') {
    return mf_client_session_connect_capabilities_v1(configured, required_capabilities,
                                                      optional_capabilities, out_session);
  }
  if (runtime_directory != (const char*)0 && runtime_directory[0] != '\0') {
    written = snprintf(socket_path, sizeof(socket_path), "%s/metafluxd.sock", runtime_directory);
  } else {
    written = snprintf(socket_path, sizeof(socket_path), "/run/user/%lu/metafluxd.sock",
                       (unsigned long)geteuid());
  }
  if (written <= 0 || (size_t)written >= sizeof(socket_path)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_session_connect_capabilities_v1(socket_path, required_capabilities,
                                                   optional_capabilities, out_session);
}

mf_shared_status_v1 mf_client_observer_connect_default_v1(mf_client_session_v1* out_session) {
  const char* configured = getenv("METAFLUX_SOCKET");
  const char* runtime_directory = getenv("XDG_RUNTIME_DIR");
  char socket_path[sizeof(((struct sockaddr_un*)0)->sun_path)];
  int written = 0;
  if (configured != (const char*)0 && configured[0] != '\0') {
    return mf_client_observer_connect_v1(configured, out_session);
  }
  if (runtime_directory != (const char*)0 && runtime_directory[0] != '\0') {
    written = snprintf(socket_path, sizeof(socket_path), "%s/metafluxd.sock", runtime_directory);
  } else {
    written = snprintf(socket_path, sizeof(socket_path), "/run/user/%lu/metafluxd.sock",
                       (unsigned long)geteuid());
  }
  if (written <= 0 || (size_t)written >= sizeof(socket_path)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_observer_connect_v1(socket_path, out_session);
}

mf_shared_status_v1 mf_client_session_control_v1(mf_client_session_v1* session, uint16_t opcode,
                                                 uint16_t flags, uint64_t object_id,
                                                 uint64_t argument, int32_t payload_fd,
                                                 mf_client_control_response_v1* out_response,
                                                 int32_t* out_received_payload_fd) {
  mf_client_control_request_v1 request;
  int32_t received_fd = -1;
  uint32_t received_count = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  uint32_t response_flags = 0;
  const int has_payload = (flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) != UINT16_C(0);

  if (session == (mf_client_session_v1*)0 || session->socket_fd < 0 ||
      out_response == (mf_client_control_response_v1*)0 ||
      session->next_control_request_id == UINT64_MAX || (has_payload != 0 && payload_fd < 0) ||
      (has_payload == 0 && payload_fd >= 0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (out_received_payload_fd != (int32_t*)0) {
    *out_received_payload_fd = -1;
  }
  mf_client_control_request_init_v1(&request, opcode, flags, session->next_control_request_id,
                                    session->registry_view_id.daemon_incarnation,
                                    session->registry_view_id.view_serial, object_id, argument);
  if (mf_client_control_request_validate_v1(&request) != MF_CLIENT_CONTROL_OK) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_client_send_packet(session->socket_fd, request.bytes, MF_CLIENT_PROTOCOL_WIRE_SIZE_V1,
                                 has_payload != 0 ? &payload_fd : (const int32_t*)0,
                                 has_payload != 0 ? UINT32_C(1) : UINT32_C(0));
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status = mf_client_receive_packet(session->socket_fd, out_response->bytes,
                                    MF_CLIENT_PROTOCOL_WIRE_SIZE_V1, &received_fd, UINT32_C(1),
                                    &received_count);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  response_flags = mf_client_load_le32_v1(out_response->bytes + 16);
  if (mf_client_control_response_validate_v1(out_response) != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le64_v1(out_response->bytes + 24) != session->next_control_request_id ||
      mf_client_load_le64_v1(out_response->bytes + 32) !=
          session->registry_view_id.daemon_incarnation ||
      mf_client_load_le64_v1(out_response->bytes + 40) != session->registry_view_id.view_serial ||
      (response_flags & ~(uint32_t)MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) != UINT32_C(0) ||
      (((response_flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) != UINT32_C(0)) !=
       (received_count == UINT32_C(1)))) {
    mf_client_close_transferred_fds(&received_fd, received_count);
    return MF_SHARED_MALFORMED;
  }
  ++session->next_control_request_id;
  if (received_count == UINT32_C(1)) {
    if (out_received_payload_fd == (int32_t*)0) {
      (void)close(received_fd);
    } else {
      *out_received_payload_fd = received_fd;
    }
  }
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_client_control_status_to_shared(uint32_t status) {
  switch (status) {
  case MF_CLIENT_CONTROL_OK:
    return MF_SHARED_SUCCESS;
  case MF_CLIENT_CONTROL_MALFORMED:
    return MF_SHARED_MALFORMED;
  case MF_CLIENT_CONTROL_STALE_GENERATION:
    return MF_SHARED_STALE_HANDLE;
  case MF_CLIENT_CONTROL_NOT_FOUND:
    return MF_SHARED_STALE_HANDLE;
  case MF_CLIENT_CONTROL_INVALID_ARGUMENT:
    return MF_SHARED_INVALID_ARGUMENT;
  case MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  case MF_CLIENT_CONTROL_UNSUPPORTED:
    return MF_SHARED_NOT_SUPPORTED;
  case MF_CLIENT_CONTROL_NO_PERMISSION:
    return MF_SHARED_PERMISSION_DENIED;
  case MF_CLIENT_CONTROL_INTERNAL_ERROR:
  default:
    return MF_SHARED_SYSTEM_ERROR;
  }
}

static void mf_client_process_snapshot_initialize_empty(mf_client_process_snapshot_v1* snapshot) {
  (void)memset(snapshot, 0, sizeof(*snapshot));
  snapshot->owned_fd = -1;
}

void mf_client_process_snapshot_close_v1(mf_client_process_snapshot_v1* snapshot) {
  if (snapshot == (mf_client_process_snapshot_v1*)0) {
    return;
  }
  if (snapshot->mapping != (void*)0 && snapshot->mapping_size <= (uint64_t)SIZE_MAX) {
    (void)munmap(snapshot->mapping, (size_t)snapshot->mapping_size);
  }
  if (snapshot->owned_fd >= 0) {
    (void)close(snapshot->owned_fd);
  }
  mf_client_process_snapshot_initialize_empty(snapshot);
}

mf_shared_status_v1
mf_client_process_snapshot_fetch_v1(mf_client_session_v1* observer,
                                    mf_client_process_snapshot_v1* out_snapshot) {
  mf_client_control_response_v1 response;
  struct stat attributes;
  int32_t payload_fd = -1;
  int seals = 0;
  uint64_t maximum_size = 0;
  uint64_t response_revision = 0;
  uint64_t response_size = 0;
  uint32_t response_status = MF_CLIENT_CONTROL_INTERNAL_ERROR;
  void* mapping = MAP_FAILED;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;

  if (observer == (mf_client_session_v1*)0 || out_snapshot == (mf_client_process_snapshot_v1*)0 ||
      observer->socket_fd < 0 ||
      (observer->negotiated_capabilities & MF_CLIENT_CAP_PROCESS_SNAPSHOT_V1) == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_client_process_snapshot_initialize_empty(out_snapshot);
  status = mf_client_session_control_v1(observer, MF_CLIENT_CONTROL_PROCESS_SNAPSHOT_V1,
                                        UINT16_C(0), MF_CLIENT_PROCESS_SNAPSHOT_VERSION_V1,
                                        MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1, -1, &response,
                                        &payload_fd);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  response_status = mf_client_load_le32_v1(response.bytes + 12);
  status = mf_client_control_status_to_shared(response_status);
  if (status != MF_SHARED_SUCCESS) {
    if (payload_fd >= 0) {
      (void)close(payload_fd);
    }
    return status;
  }
  response_revision = mf_client_load_le64_v1(response.bytes + 48);
  response_size = mf_client_load_le64_v1(response.bytes + 56);
  if (payload_fd < 0 || response_revision == UINT64_C(0) ||
      mf_client_process_snapshot_size_v1(MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1, &maximum_size) !=
          MF_CLIENT_CONTROL_OK ||
      response_size < MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 || response_size > maximum_size ||
      response_size > (uint64_t)SIZE_MAX || response_size > (uint64_t)INT64_MAX ||
      fstat(payload_fd, &attributes) != 0 || !S_ISREG(attributes.st_mode) ||
      attributes.st_size < 0 || (uint64_t)attributes.st_size != response_size) {
    if (payload_fd >= 0) {
      (void)close(payload_fd);
    }
    return MF_SHARED_MALFORMED;
  }
  seals = fcntl(payload_fd, F_GET_SEALS);
  if (seals < 0 || (seals & (F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_WRITE | F_SEAL_SEAL)) !=
                       (F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_WRITE | F_SEAL_SEAL)) {
    (void)close(payload_fd);
    return MF_SHARED_MALFORMED;
  }
  mapping = mmap((void*)0, (size_t)response_size, PROT_READ, MAP_SHARED, payload_fd, 0);
  if (mapping == MAP_FAILED) {
    (void)close(payload_fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  if (mf_client_process_snapshot_validate_v1((const uint8_t*)mapping, response_size) !=
          MF_CLIENT_CONTROL_OK ||
      mf_client_process_snapshot_revision_v1((const uint8_t*)mapping) != response_revision) {
    (void)munmap(mapping, (size_t)response_size);
    (void)close(payload_fd);
    return MF_SHARED_MALFORMED;
  }
  out_snapshot->mapping = mapping;
  out_snapshot->mapping_size = response_size;
  out_snapshot->revision = response_revision;
  out_snapshot->owned_fd = payload_fd;
  out_snapshot->row_count =
      mf_client_process_snapshot_count_v1((const uint8_t*)out_snapshot->mapping);
  return MF_SHARED_SUCCESS;
}

uint64_t
mf_client_process_snapshot_revision_value_v1(const mf_client_process_snapshot_v1* snapshot) {
  return snapshot == (const mf_client_process_snapshot_v1*)0 ? UINT64_C(0) : snapshot->revision;
}

uint32_t mf_client_process_snapshot_row_count_v1(const mf_client_process_snapshot_v1* snapshot) {
  return snapshot == (const mf_client_process_snapshot_v1*)0 ? UINT32_C(0) : snapshot->row_count;
}

mf_shared_status_v1
mf_client_process_snapshot_fill_v1(const mf_client_process_snapshot_v1* snapshot, uint32_t* count,
                                   mf_client_process_snapshot_row_v1* rows) {
  uint32_t index = 0;
  if (snapshot == (const mf_client_process_snapshot_v1*)0 || count == (uint32_t*)0 ||
      snapshot->mapping == (void*)0 || snapshot->owned_fd < 0 ||
      snapshot->row_count > MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1 ||
      mf_client_process_snapshot_validate_v1((const uint8_t*)snapshot->mapping,
                                             snapshot->mapping_size) != MF_CLIENT_CONTROL_OK ||
      mf_client_process_snapshot_revision_v1((const uint8_t*)snapshot->mapping) !=
          snapshot->revision ||
      mf_client_process_snapshot_count_v1((const uint8_t*)snapshot->mapping) !=
          snapshot->row_count) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (rows == (mf_client_process_snapshot_row_v1*)0) {
    *count = snapshot->row_count;
    return MF_SHARED_SUCCESS;
  }
  if (*count < snapshot->row_count) {
    *count = snapshot->row_count;
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  for (index = 0; index < snapshot->row_count; ++index) {
    const mf_client_process_snapshot_row_wire_v1* source =
        mf_client_process_snapshot_row_v1_at((const uint8_t*)snapshot->mapping, index);
    rows[index].pid = mf_client_process_snapshot_row_pid_v1(source);
    rows[index].kinds = mf_client_process_snapshot_row_kinds_v1(source);
    rows[index].process_start_time_ticks = mf_client_process_snapshot_row_start_time_v1(source);
    rows[index].identity_record_id = mf_client_process_snapshot_row_identity_v1(source);
    rows[index].device_generation = mf_client_process_snapshot_row_generation_v1(source);
    rows[index].used_memory_bytes = mf_client_process_snapshot_row_used_memory_v1(source);
    (void)memcpy(rows[index].process_name, mf_client_process_snapshot_row_name_v1(source),
                 MF_CLIENT_PROCESS_NAME_SIZE_V1);
  }
  *count = snapshot->row_count;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_registry_attach_v1(int32_t fd,
                                                 mf_registry_view_id_v1 expected_view_id,
                                                 mf_client_registry_v1* out_registry) {
  struct stat attributes;
  int duplicate_fd = -1;
  uint64_t mapping_size = 0;
  void* mapping = MAP_FAILED;
  const uint8_t* bytes = (const uint8_t*)0;
  const mf_shared_registry_header_v1* header = (const mf_shared_registry_header_v1*)0;
  uint32_t index = 0;

  if (fd < 0 || out_registry == (mf_client_registry_v1*)0 ||
      expected_view_id.daemon_incarnation == UINT64_C(0) ||
      expected_view_id.view_serial == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_registry, 0, sizeof(*out_registry));
  out_registry->owned_fd = -1;
  if (fstat(fd, &attributes) != 0 || attributes.st_size <= 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping_size = (uint64_t)attributes.st_size;
  if (mapping_size > (uint64_t)SIZE_MAX || mapping_size < sizeof(*header)) {
    return MF_SHARED_MALFORMED;
  }
  duplicate_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
  if (duplicate_fd < 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping = mmap((void*)0, (size_t)mapping_size, PROT_READ, MAP_SHARED, duplicate_fd, 0);
  if (mapping == MAP_FAILED) {
    (void)close(duplicate_fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  mf_atomic_thread_fence_acquire();
  bytes = (const uint8_t*)mapping;
  header = (const mf_shared_registry_header_v1*)mapping;
  if (((uintptr_t)mapping % MF_SHARED_CACHE_LINE_SIZE) != (uintptr_t)0 ||
      header->magic != MF_SHARED_REGISTRY_MAGIC ||
      header->abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      header->header_size != sizeof(*header) || header->total_size != mapping_size ||
      header->process_view_revision == UINT64_C(0) ||
      (header->flags & ~MF_SHARED_REGISTRY_KNOWN_FLAGS_V1) != UINT32_C(0) ||
      header->telemetry_row_count != header->device_count ||
      !mf_client_registry_offsets_valid(header, bytes, mapping_size)) {
    (void)munmap(mapping, (size_t)mapping_size);
    (void)close(duplicate_fd);
    return MF_SHARED_MALFORMED;
  }
  if (!mf_registry_view_id_equal_v1(header->registry_view_id, expected_view_id)) {
    (void)munmap(mapping, (size_t)mapping_size);
    (void)close(duplicate_fd);
    return MF_SHARED_STALE_HANDLE;
  }

  out_registry->mapping = mapping;
  out_registry->mapping_size = mapping_size;
  out_registry->header = header;
  out_registry->view_admission =
      (const mf_view_admission_control_v1*)(bytes + header->view_admission_offset);
  out_registry->view_control =
      (const mf_registry_view_control_v1*)(bytes + header->view_control_offset);
  out_registry->identities =
      (const mf_virtual_device_identity_v1*)(bytes + header->identities_offset);
  out_registry->device_admission =
      (const mf_device_admission_control_v1*)(bytes + header->device_admission_offset);
  out_registry->lifecycle_fences =
      (const mf_virtual_device_lifecycle_fence_v1*)(bytes + header->lifecycle_fences_offset);
  out_registry->telemetry_control =
      (const mf_telemetry_control_v1*)(bytes + header->telemetry_control_offset);
  out_registry->telemetry_banks[0] =
      (const mf_virtual_device_telemetry_v1*)(bytes + header->telemetry_bank0_offset);
  out_registry->telemetry_banks[1] =
      (const mf_virtual_device_telemetry_v1*)(bytes + header->telemetry_bank1_offset);
  out_registry->registry_view_id = header->registry_view_id;
  out_registry->owned_fd = duplicate_fd;
  out_registry->device_count = header->device_count;
  out_registry->process_view_revision = header->process_view_revision;

  if (!mf_registry_view_id_equal_v1(out_registry->view_admission->registry_view_id,
                                    expected_view_id) ||
      !mf_registry_view_id_equal_v1(out_registry->view_control->registry_view_id,
                                    expected_view_id) ||
      out_registry->view_control->process_view_revision !=
          out_registry->process_view_revision) {
    mf_client_registry_close_v1(out_registry);
    return MF_SHARED_MALFORMED;
  }
  for (index = 0; index < out_registry->device_count; ++index) {
    if (out_registry->identities[index].identity_record_id == UINT64_C(0) ||
        out_registry->identities[index].committed_generation == UINT64_C(0) ||
        out_registry->device_admission[index].identity_record_id !=
            out_registry->identities[index].identity_record_id ||
        out_registry->lifecycle_fences[index].identity_record_id !=
            out_registry->identities[index].identity_record_id) {
      mf_client_registry_close_v1(out_registry);
      return MF_SHARED_MALFORMED;
    }
  }
  return MF_SHARED_SUCCESS;
}

void mf_client_registry_close_v1(mf_client_registry_v1* registry) {
  if (registry == (mf_client_registry_v1*)0) {
    return;
  }
  if (registry->mapping != (void*)0 && registry->mapping_size <= (uint64_t)SIZE_MAX) {
    (void)munmap(registry->mapping, (size_t)registry->mapping_size);
  }
  if (registry->owned_fd >= 0) {
    (void)close(registry->owned_fd);
  }
  (void)memset(registry, 0, sizeof(*registry));
  registry->owned_fd = -1;
}

int32_t mf_client_registry_borrow_fd_v1(const mf_client_registry_v1* registry) {
  return registry == (const mf_client_registry_v1*)0 ? -1 : registry->owned_fd;
}

uint32_t mf_client_registry_device_count_v1(const mf_client_registry_v1* registry) {
  return registry == (const mf_client_registry_v1*)0 ? UINT32_C(0) : registry->device_count;
}

uint64_t mf_client_registry_process_view_revision_v1(const mf_client_registry_v1* registry) {
  return registry == (const mf_client_registry_v1*)0 ? UINT64_C(0)
                                                     : registry->process_view_revision;
}

mf_shared_status_v1 mf_client_registry_identity_v1(const mf_client_registry_v1* registry,
                                                   uint32_t device_index,
                                                   mf_virtual_device_identity_v1* out_identity) {
  uint64_t admission_before = 0;
  uint64_t admission_after = 0;
  uint64_t control_latch = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (registry == (const mf_client_registry_v1*)0 ||
      out_identity == (mf_virtual_device_identity_v1*)0 || device_index >= registry->device_count) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  admission_before = mf_atomic_load_u64_acquire(&registry->view_admission->state_generation);
  if (mf_view_admission_state_v1(admission_before) != MF_VIEW_ADMISSION_OPEN) {
    return MF_SHARED_TERMINAL_VIEW;
  }
  status = mf_client_registry_stable_control(registry, &control_latch);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  *out_identity = registry->identities[device_index];
  mf_atomic_signal_fence_seq_cst();
  admission_after = mf_atomic_load_u64_acquire(&registry->view_admission->state_generation);
  if (admission_before != admission_after ||
      control_latch !=
          mf_atomic_load_u64_acquire(&registry->view_control->control_latch_sequence)) {
    return MF_SHARED_RETRY;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_registry_make_handle_v1(const mf_client_registry_v1* registry,
                                                      uint32_t device_index, uint64_t object_id,
                                                      uint64_t object_generation,
                                                      uint32_t object_type,
                                                      mf_generation_handle_v1* out_handle) {
  mf_virtual_device_identity_v1 identity;
  uint64_t device_control = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (out_handle == (mf_generation_handle_v1*)0 || object_id == UINT64_C(0) ||
      object_generation == UINT64_C(0) || object_type == UINT32_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_client_registry_identity_v1(registry, device_index, &identity);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  device_control =
      mf_atomic_load_u64_acquire(&registry->device_admission[device_index].state_generation_tag);
  if (mf_device_admission_state_v1(device_control) != MF_DEVICE_ADMISSION_OPEN) {
    return mf_device_admission_state_v1(device_control) == MF_DEVICE_ADMISSION_UPDATING
               ? MF_SHARED_RETRY
               : MF_SHARED_DEVICE_LOST;
  }
  (void)memset(out_handle, 0, sizeof(*out_handle));
  out_handle->registry_view_id = registry->registry_view_id;
  out_handle->identity_record_id = identity.identity_record_id;
  out_handle->device_generation = identity.committed_generation;
  out_handle->object_id = object_id;
  out_handle->object_generation = object_generation;
  out_handle->object_type = object_type;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_registry_validate_device_v1(const mf_client_registry_v1* registry,
                                                          const mf_generation_handle_v1* handle,
                                                          mf_client_fence_snapshot_v1* out_fence) {
  uint32_t device_index = 0;
  uint32_t attempt = 0;
  if (registry == (const mf_client_registry_v1*)0 || handle == (const mf_generation_handle_v1*)0 ||
      out_fence == (mf_client_fence_snapshot_v1*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (!mf_registry_view_id_equal_v1(handle->registry_view_id, registry->registry_view_id)) {
    return MF_SHARED_STALE_HANDLE;
  }
  device_index = mf_client_registry_find_identity(registry, handle->identity_record_id);
  if (device_index == registry->device_count ||
      registry->identities[device_index].committed_generation != handle->device_generation) {
    return MF_SHARED_STALE_HANDLE;
  }

  for (attempt = 0; attempt < UINT32_C(4); ++attempt) {
    const uint64_t view_before =
        mf_atomic_load_u64_acquire(&registry->view_admission->state_generation);
    uint64_t control_latch = 0;
    uint64_t device_before = 0;
    uint64_t fence_latch = 0;
    uint64_t view_after = 0;
    uint64_t control_after = 0;
    uint64_t device_after = 0;
    mf_shared_status_v1 status = MF_SHARED_SUCCESS;
    if (mf_view_admission_state_v1(view_before) != MF_VIEW_ADMISSION_OPEN) {
      return MF_SHARED_TERMINAL_VIEW;
    }
    status = mf_client_registry_stable_control(registry, &control_latch);
    if (status != MF_SHARED_SUCCESS) {
      if (status == MF_SHARED_TERMINAL_VIEW) {
        return status;
      }
      continue;
    }
    device_before =
        mf_atomic_load_u64_acquire(&registry->device_admission[device_index].state_generation_tag);
    if (mf_device_admission_state_v1(device_before) != MF_DEVICE_ADMISSION_OPEN) {
      if (mf_device_admission_state_v1(device_before) == MF_DEVICE_ADMISSION_UPDATING) {
        continue;
      }
      return MF_SHARED_DEVICE_LOST;
    }
    if (mf_client_registry_stable_fence(registry, device_index, out_fence, &fence_latch) !=
        MF_SHARED_SUCCESS) {
      continue;
    }
    if (out_fence->identity_record_id != handle->identity_record_id) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (out_fence->device_state != MF_DEVICE_STATE_ONLINE) {
      return MF_SHARED_DEVICE_LOST;
    }
    view_after = mf_atomic_load_u64_acquire(&registry->view_admission->state_generation);
    control_after = mf_atomic_load_u64_acquire(&registry->view_control->control_latch_sequence);
    device_after =
        mf_atomic_load_u64_acquire(&registry->device_admission[device_index].state_generation_tag);
    if (view_before == view_after && control_latch == control_after &&
        device_before == device_after && (fence_latch & UINT64_C(1)) == UINT64_C(0)) {
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1
mf_client_registry_read_telemetry_v1(const mf_client_registry_v1* registry,
                                     const mf_generation_handle_v1* handle,
                                     mf_client_telemetry_snapshot_v1* out_snapshot) {
  mf_client_fence_snapshot_v1 fence_before;
  uint32_t device_index = 0;
  uint32_t attempt = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (registry == (const mf_client_registry_v1*)0 || handle == (const mf_generation_handle_v1*)0 ||
      out_snapshot == (mf_client_telemetry_snapshot_v1*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_client_registry_validate_device_v1(registry, handle, &fence_before);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  device_index = mf_client_registry_find_identity(registry, handle->identity_record_id);
  for (attempt = 0; attempt < UINT32_C(4); ++attempt) {
    const uint64_t first =
        mf_atomic_load_u64_acquire(&registry->telemetry_control->telemetry_latch_sequence);
    uint64_t snapshot_sequence = 0;
    uint64_t bank_state = 0;
    uint32_t row_count = 0;
    uint32_t bank = 0;
    const mf_virtual_device_telemetry_v1* row = (const mf_virtual_device_telemetry_v1*)0;
    uint64_t second = 0;
    mf_client_fence_snapshot_v1 fence_after;
    if ((first & UINT64_C(1)) != UINT64_C(0)) {
      continue;
    }
    snapshot_sequence = mf_atomic_load_u64_relaxed(&registry->telemetry_control->snapshot_sequence);
    bank_state = mf_atomic_load_u64_relaxed(&registry->telemetry_control->active_bank_state);
    row_count = mf_atomic_load_u32_relaxed(&registry->telemetry_control->row_count);
    bank = mf_telemetry_active_bank_v1(bank_state);
    if (mf_telemetry_state_v1(bank_state) != MF_TELEMETRY_STATE_READY || bank > UINT32_C(1) ||
        device_index >= row_count) {
      return MF_SHARED_RETRY;
    }
    row = &registry->telemetry_banks[bank][device_index];
    out_snapshot->identity_record_id = mf_atomic_load_u64_relaxed(&row->identity_record_id);
    out_snapshot->observed_lifecycle_sequence =
        mf_atomic_load_u64_relaxed(&row->observed_lifecycle_sequence);
    out_snapshot->committed_work_items = mf_atomic_load_u64_relaxed(&row->committed_work_items);
    out_snapshot->completed_work_items = mf_atomic_load_u64_relaxed(&row->completed_work_items);
    out_snapshot->active_time_ns = mf_atomic_load_u64_relaxed(&row->active_time_ns);
    out_snapshot->memory_active_time_ns = mf_atomic_load_u64_relaxed(&row->memory_active_time_ns);
    out_snapshot->memory_used_bytes = mf_atomic_load_u64_relaxed(&row->memory_used_bytes);
    out_snapshot->memory_capacity_bytes = mf_atomic_load_u64_relaxed(&row->memory_capacity_bytes);
    out_snapshot->sample_time_ns = mf_atomic_load_u64_relaxed(&row->sample_time_ns);
    out_snapshot->snapshot_sequence = snapshot_sequence;
    mf_atomic_signal_fence_seq_cst();
    second = mf_atomic_load_u64_acquire(&registry->telemetry_control->telemetry_latch_sequence);
    if (first != second) {
      continue;
    }
    if (out_snapshot->identity_record_id != handle->identity_record_id ||
        out_snapshot->observed_lifecycle_sequence < fence_before.lifecycle_sequence) {
      return MF_SHARED_RETRY;
    }
    status = mf_client_registry_validate_device_v1(registry, handle, &fence_after);
    if (status != MF_SHARED_SUCCESS) {
      return status;
    }
    if (fence_after.lifecycle_sequence == fence_before.lifecycle_sequence) {
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1 mf_client_ring_mapping_size_v1(uint32_t capacity, uint64_t* out_mapping_size) {
  if (out_mapping_size == (uint64_t*)0 || !mf_client_ring_capacity_valid(capacity)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_mapping_size =
      sizeof(mf_ring_header_v1) + ((uint64_t)capacity * (uint64_t)sizeof(mf_ring_descriptor_v1));
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_ring_create_v1(uint32_t capacity,
                                             mf_registry_view_id_v1 registry_view_id,
                                             uint64_t queue_id, uint64_t queue_generation,
                                             mf_client_ring_v1* out_ring) {
  uint64_t mapping_size = 0;
  long created_fd = -1;
  int32_t fd = -1;
  void* mapping = MAP_FAILED;
  mf_ring_header_v1* header = (mf_ring_header_v1*)0;
  mf_ring_descriptor_v1* descriptors = (mf_ring_descriptor_v1*)0;
  uint32_t index = 0;

  if (out_ring == (mf_client_ring_v1*)0 || registry_view_id.daemon_incarnation == UINT64_C(0) ||
      registry_view_id.view_serial == UINT64_C(0) || queue_id == UINT64_C(0) ||
      queue_generation == UINT64_C(0) ||
      mf_client_ring_mapping_size_v1(capacity, &mapping_size) != MF_SHARED_SUCCESS ||
      mapping_size > (uint64_t)INT64_MAX) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_ring, 0, sizeof(*out_ring));
  out_ring->owned_fd = -1;

  created_fd = syscall(SYS_memfd_create, "metaflux-ring-v1", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (created_fd < 0 || created_fd > INT_MAX) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  fd = (int32_t)created_fd;
  if (ftruncate(fd, (off_t)mapping_size) != 0) {
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping = mmap((void*)0, (size_t)mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  (void)memset(mapping, 0, (size_t)mapping_size);

  header = (mf_ring_header_v1*)mapping;
  descriptors = (mf_ring_descriptor_v1*)((uint8_t*)mapping + sizeof(*header));
  header->metadata.magic = MF_SHARED_RING_MAGIC;
  header->metadata.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  header->metadata.header_size = (uint32_t)sizeof(*header);
  header->metadata.descriptor_size = (uint32_t)sizeof(*descriptors);
  header->metadata.capacity = capacity;
  header->metadata.mapping_size = mapping_size;
  header->metadata.registry_view_id = registry_view_id;
  header->metadata.queue_id = queue_id;
  header->metadata.queue_generation = queue_generation;
  for (index = 0; index < capacity; ++index) {
    mf_atomic_store_u64_relaxed(&descriptors[index].sequence, (uint64_t)index);
  }
  mf_atomic_thread_fence_release();

  if (fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) != 0) {
    (void)munmap(mapping, (size_t)mapping_size);
    (void)close(fd);
    return MF_SHARED_SYSTEM_ERROR;
  }

  out_ring->mapping = mapping;
  out_ring->mapping_size = mapping_size;
  out_ring->header = header;
  out_ring->descriptors = descriptors;
  out_ring->registry_view_id = registry_view_id;
  out_ring->queue_id = queue_id;
  out_ring->queue_generation = queue_generation;
  out_ring->owned_fd = fd;
  out_ring->capacity = capacity;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_ring_attach_v1(int32_t fd, mf_registry_view_id_v1 expected_view_id,
                                             uint64_t expected_queue_id,
                                             uint64_t expected_queue_generation,
                                             mf_client_ring_v1* out_ring) {
  struct stat attributes;
  int duplicate_fd = -1;
  int seals = 0;
  uint64_t mapping_size = 0;
  void* mapping = MAP_FAILED;
  mf_ring_header_v1* header = (mf_ring_header_v1*)0;
  mf_ring_descriptor_v1* descriptors = (mf_ring_descriptor_v1*)0;
  uint32_t capacity = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;

  if (fd < 0 || out_ring == (mf_client_ring_v1*)0 ||
      expected_view_id.daemon_incarnation == UINT64_C(0) ||
      expected_view_id.view_serial == UINT64_C(0) || expected_queue_id == UINT64_C(0) ||
      expected_queue_generation == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(out_ring, 0, sizeof(*out_ring));
  out_ring->owned_fd = -1;
  if (fstat(fd, &attributes) != 0 || attributes.st_size <= 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping_size = (uint64_t)attributes.st_size;
  if (mapping_size > (uint64_t)SIZE_MAX) {
    return MF_SHARED_OVERFLOW;
  }
  seals = fcntl(fd, F_GET_SEALS);
  if (seals < 0 || (seals & (F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL)) !=
                       (F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL)) {
    return MF_SHARED_MALFORMED;
  }
  duplicate_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
  if (duplicate_fd < 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping =
      mmap((void*)0, (size_t)mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, duplicate_fd, 0);
  if (mapping == MAP_FAILED) {
    (void)close(duplicate_fd);
    return MF_SHARED_SYSTEM_ERROR;
  }
  status =
      mf_client_ring_validate_mapping(mapping, mapping_size, expected_view_id, expected_queue_id,
                                      expected_queue_generation, &header, &descriptors, &capacity);
  if (status != MF_SHARED_SUCCESS) {
    (void)munmap(mapping, (size_t)mapping_size);
    (void)close(duplicate_fd);
    return status;
  }

  out_ring->mapping = mapping;
  out_ring->mapping_size = mapping_size;
  out_ring->header = header;
  out_ring->descriptors = descriptors;
  out_ring->registry_view_id = expected_view_id;
  out_ring->queue_id = expected_queue_id;
  out_ring->queue_generation = expected_queue_generation;
  out_ring->owned_fd = duplicate_fd;
  out_ring->capacity = capacity;
  return MF_SHARED_SUCCESS;
}

void mf_client_ring_close_v1(mf_client_ring_v1* ring) {
  if (ring == (mf_client_ring_v1*)0) {
    return;
  }
  if (ring->mapping != (void*)0 && ring->mapping_size <= (uint64_t)SIZE_MAX) {
    (void)munmap(ring->mapping, (size_t)ring->mapping_size);
  }
  if (ring->owned_fd >= 0) {
    (void)close(ring->owned_fd);
  }
  (void)memset(ring, 0, sizeof(*ring));
  ring->owned_fd = -1;
}

int32_t mf_client_ring_borrow_fd_v1(const mf_client_ring_v1* ring) {
  return ring == (const mf_client_ring_v1*)0 ? -1 : ring->owned_fd;
}

mf_shared_status_v1 mf_client_ring_try_submit_v1(mf_client_ring_v1* ring,
                                                 const mf_ring_descriptor_v1* descriptor) {
  uint64_t position = 0;
  uint64_t sequence = 0;
  uint32_t mask = 0;
  mf_ring_descriptor_v1* slot = (mf_ring_descriptor_v1*)0;
  uint32_t argument = 0;

  if (!mf_client_ring_handle_valid(ring) || descriptor == (const mf_ring_descriptor_v1*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mask = ring->capacity - UINT32_C(1);
  position = mf_atomic_load_u64_relaxed(&ring->header->producer.position);
  for (;;) {
    slot = &ring->descriptors[position & (uint64_t)mask];
    sequence = mf_atomic_load_u64_acquire(&slot->sequence);
    if (sequence == position) {
      uint64_t expected = position;
      if (mf_atomic_compare_exchange_u64_weak_relaxed(&ring->header->producer.position, &expected,
                                                      position + UINT64_C(1))) {
        break;
      }
      position = expected;
      continue;
    }
    if (sequence - position > UINT64_MAX / UINT64_C(2)) {
      return MF_SHARED_WOULD_BLOCK;
    }
    position = mf_atomic_load_u64_relaxed(&ring->header->producer.position);
  }

  slot->opcode = descriptor->opcode;
  slot->flags = descriptor->flags;
  slot->request_id = descriptor->request_id;
  slot->target_id = descriptor->target_id;
  for (argument = 0; argument < UINT32_C(4); ++argument) {
    slot->arguments[argument] = descriptor->arguments[argument];
  }
  mf_atomic_store_u64_release(&slot->sequence, position + UINT64_C(1));
  mf_client_ring_wake(&ring->header->wait.consumer_wait_state,
                      &ring->header->wait.consumer_wake_sequence,
                      &ring->header->wait.consumer_doorbell_count);
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_ring_try_consume_v1(mf_client_ring_v1* ring,
                                                  mf_ring_descriptor_v1* out_descriptor) {
  uint64_t position = 0;
  uint64_t sequence = 0;
  uint64_t published_sequence = 0;
  uint32_t mask = 0;
  mf_ring_descriptor_v1* slot = (mf_ring_descriptor_v1*)0;
  uint32_t argument = 0;

  if (!mf_client_ring_handle_valid(ring) || out_descriptor == (mf_ring_descriptor_v1*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mask = ring->capacity - UINT32_C(1);
  position = mf_atomic_load_u64_relaxed(&ring->header->consumer.position);
  for (;;) {
    slot = &ring->descriptors[position & (uint64_t)mask];
    published_sequence = position + UINT64_C(1);
    sequence = mf_atomic_load_u64_acquire(&slot->sequence);
    if (sequence == published_sequence) {
      uint64_t expected = position;
      if (mf_atomic_compare_exchange_u64_weak_relaxed(&ring->header->consumer.position, &expected,
                                                      position + UINT64_C(1))) {
        break;
      }
      position = expected;
      continue;
    }
    if (sequence - published_sequence > UINT64_MAX / UINT64_C(2)) {
      return MF_SHARED_WOULD_BLOCK;
    }
    position = mf_atomic_load_u64_relaxed(&ring->header->consumer.position);
  }

  out_descriptor->sequence = position;
  out_descriptor->opcode = slot->opcode;
  out_descriptor->flags = slot->flags;
  out_descriptor->request_id = slot->request_id;
  out_descriptor->target_id = slot->target_id;
  for (argument = 0; argument < UINT32_C(4); ++argument) {
    out_descriptor->arguments[argument] = slot->arguments[argument];
  }
  mf_atomic_store_u64_release(&slot->sequence, position + (uint64_t)ring->capacity);
  mf_client_ring_wake(&ring->header->wait.producer_wait_state,
                      &ring->header->wait.producer_wake_sequence,
                      &ring->header->wait.producer_doorbell_count);
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_client_ring_wait_readable_v1(mf_client_ring_v1* ring, uint64_t timeout_ns) {
  if (!mf_client_ring_handle_valid(ring)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_ring_wait(ring, &ring->header->wait.consumer_wait_state,
                             &ring->header->wait.consumer_wake_sequence, timeout_ns,
                             mf_client_ring_is_readable);
}

mf_shared_status_v1 mf_client_ring_wait_writable_v1(mf_client_ring_v1* ring, uint64_t timeout_ns) {
  if (!mf_client_ring_handle_valid(ring)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_ring_wait(ring, &ring->header->wait.producer_wait_state,
                             &ring->header->wait.producer_wake_sequence, timeout_ns,
                             mf_client_ring_is_writable);
}

uint64_t mf_client_ring_consumer_doorbells_v1(const mf_client_ring_v1* ring) {
  return ring == (const mf_client_ring_v1*)0 || ring->header == (mf_ring_header_v1*)0
             ? UINT64_C(0)
             : mf_atomic_load_u64_acquire(&ring->header->wait.consumer_doorbell_count);
}

uint64_t mf_client_ring_producer_doorbells_v1(const mf_client_ring_v1* ring) {
  return ring == (const mf_client_ring_v1*)0 || ring->header == (mf_ring_header_v1*)0
             ? UINT64_C(0)
             : mf_atomic_load_u64_acquire(&ring->header->wait.producer_doorbell_count);
}

mf_shared_status_v1 mf_client_submit_memory_alloc_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                     uint64_t context_id, uint64_t byte_count,
                                                     uint64_t alignment, uint32_t flags) {
  if (byte_count == UINT64_C(0) || alignment == UINT64_C(0) ||
      (alignment & (alignment - UINT64_C(1))) != UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_MEMORY_ALLOC, flags, request_id, context_id,
                                  byte_count, alignment, UINT64_C(0), UINT64_C(0));
}

mf_shared_status_v1 mf_client_submit_memory_free_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                    uint64_t memory_id,
                                                    uint64_t memory_generation) {
  if (memory_generation == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_MEMORY_FREE, UINT32_C(0), request_id,
                                  memory_id, memory_generation, UINT64_C(0), UINT64_C(0),
                                  UINT64_C(0));
}

mf_shared_status_v1 mf_client_submit_module_load_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                    uint64_t artifact_id,
                                                    uint64_t artifact_generation, uint32_t flags) {
  if (artifact_id == UINT64_C(0) || artifact_generation == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_MODULE_LOAD, flags, request_id, artifact_id,
                                  artifact_generation, UINT64_C(0), UINT64_C(0), UINT64_C(0));
}

mf_shared_status_v1 mf_client_submit_module_unload_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                      uint64_t module_id,
                                                      uint64_t module_generation) {
  if (module_generation == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_MODULE_UNLOAD, UINT32_C(0), request_id,
                                  module_id, module_generation, UINT64_C(0), UINT64_C(0),
                                  UINT64_C(0));
}

mf_shared_status_v1 mf_client_submit_copy_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                             uint64_t destination_memory_id,
                                             uint64_t destination_generation,
                                             uint64_t source_memory_id, uint64_t source_generation,
                                             uint64_t byte_count, uint32_t flags) {
  if (destination_memory_id == UINT64_C(0) || source_memory_id == UINT64_C(0) ||
      destination_generation == UINT64_C(0) || source_generation == UINT64_C(0) ||
      byte_count == UINT64_C(0) || flags != UINT32_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_COPY, flags, request_id,
                                  destination_memory_id, destination_generation, source_memory_id,
                                  source_generation, byte_count);
}

mf_shared_status_v1 mf_client_submit_copy_region_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                    uint64_t argument_block_id,
                                                    uint64_t argument_block_generation) {
  if (argument_block_id == UINT64_C(0) || argument_block_generation == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(
      ring, MF_RING_OPCODE_COPY, MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1, request_id,
      argument_block_id, argument_block_generation, UINT64_C(0), UINT64_C(0), UINT64_C(0));
}

mf_shared_status_v1
mf_client_submit_direct_host_copy_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                     uint64_t device_memory_id, uint64_t device_generation,
                                     uint64_t host_address, uint64_t device_offset,
                                     uint64_t byte_count, uint32_t flags) {
  if (device_memory_id == UINT64_C(0) || device_generation == UINT64_C(0) ||
      host_address == UINT64_C(0) || byte_count == UINT64_C(0) ||
      (flags != MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 &&
       flags != MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1) ||
      byte_count > UINT64_MAX - host_address || byte_count > UINT64_MAX - device_offset) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_COPY, flags, request_id, device_memory_id,
                                  device_generation, host_address, device_offset, byte_count);
}

mf_shared_status_v1 mf_client_submit_launch_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                               uint64_t module_id, uint64_t module_generation,
                                               uint64_t kernel_id, uint64_t argument_block_id,
                                               uint64_t argument_block_generation, uint32_t flags) {
  if (module_id == UINT64_C(0) || module_generation == UINT64_C(0) || kernel_id == UINT64_C(0) ||
      argument_block_id == UINT64_C(0) || argument_block_generation == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_LAUNCH, flags, request_id, module_id,
                                  module_generation, kernel_id, argument_block_id,
                                  argument_block_generation);
}

mf_shared_status_v1 mf_client_submit_event_record_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                     uint64_t event_id, uint64_t event_generation,
                                                     uint64_t timeline_value, uint32_t flags) {
  if (event_id == UINT64_C(0) || event_generation == UINT64_C(0) || timeline_value == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_EVENT_RECORD, flags, request_id, event_id,
                                  event_generation, timeline_value, UINT64_C(0), UINT64_C(0));
}

mf_shared_status_v1 mf_client_submit_event_wait_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                   uint64_t event_id, uint64_t event_generation,
                                                   uint64_t timeline_value, uint32_t flags) {
  if (event_id == UINT64_C(0) || event_generation == UINT64_C(0) || timeline_value == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_submit_command(ring, MF_RING_OPCODE_EVENT_WAIT, flags, request_id, event_id,
                                  event_generation, timeline_value, UINT64_C(0), UINT64_C(0));
}

mf_shared_status_v1 mf_client_submit_queue_control_v1(mf_client_ring_v1* ring, uint32_t opcode,
                                                      uint64_t request_id, uint64_t timeout_ns,
                                                      uint32_t flags) {
  uint64_t queue_id = UINT64_C(0);
  if (opcode != MF_RING_OPCODE_QUEUE_SYNCHRONIZE && opcode != MF_RING_OPCODE_QUEUE_CANCEL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (!mf_client_ring_handle_valid(ring) || ring->queue_id == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  queue_id = ring->queue_id;
  return mf_client_submit_command(ring, opcode, flags, request_id, queue_id, timeout_ns,
                                  UINT64_C(0), UINT64_C(0), UINT64_C(0));
}

mf_shared_status_v1 mf_client_try_consume_completion_v1(mf_client_ring_v1* ring,
                                                        mf_client_completion_v1* out_completion) {
  mf_ring_descriptor_v1 descriptor;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (out_completion == (mf_client_completion_v1*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_client_ring_try_consume_v1(ring, &descriptor);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (descriptor.opcode != MF_RING_OPCODE_COMPLETION || descriptor.request_id == UINT64_C(0)) {
    return MF_SHARED_MALFORMED;
  }
  out_completion->request_id = descriptor.request_id;
  out_completion->status = (int32_t)(uint32_t)descriptor.arguments[0];
  out_completion->flags = descriptor.flags;
  out_completion->result_id = descriptor.target_id;
  out_completion->result_generation = descriptor.arguments[1];
  out_completion->timeline_value = descriptor.arguments[2];
  out_completion->detail = descriptor.arguments[3];
  return MF_SHARED_SUCCESS;
}
