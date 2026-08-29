#define _GNU_SOURCE

#include "metaflux/client/fastpath.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/memfd.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_MEMORY_BYTES (UINT64_C(4) * UINT64_C(1024) * UINT64_C(1024))
#define DEAD_PROCESS_MEMORY_BYTES (UINT64_C(2) * UINT64_C(1024) * UINT64_C(1024))
#define TEST_MAPPING_BYTES (UINT64_C(64) * UINT64_C(1024) * UINT64_C(1024))
#define LEGACY_TEST_MEMORY_BYTES UINT64_C(4096)
#define TEST_QUOTA_REPLACEMENT_SESSION_COUNT UINT32_C(14)
#define TEST_OBJECT_REUSE_COUNT UINT32_C(4100)
#define TEST_PRUNE_BACKLOG_REQUEST_BASE UINT64_C(0x6600)
#define TEST_BOUNDED_CLOSE_ATTEMPTS UINT32_C(100)

typedef struct legacy_session {
  mf_registry_view_id_v1 view_id;
  uint64_t next_request_id;
  int socket_fd;
} legacy_session;

static void short_pause(void) {
  const struct timespec duration = {.tv_sec = 0, .tv_nsec = 10000000};
  (void)nanosleep(&duration, (struct timespec*)0);
}

static void close_fds(int* fds, uint32_t count) {
  uint32_t index = 0;
  for (index = 0; index < count; ++index) {
    if (fds[index] >= 0) {
      (void)close(fds[index]);
      fds[index] = -1;
    }
  }
}

static int receive_negotiation(int socket_fd, mf_client_negotiation_response_v1* response) {
  union {
    struct cmsghdr alignment;
    uint8_t bytes[CMSG_SPACE(sizeof(int) * 3U)];
  } ancillary;
  struct iovec vector = {.iov_base = response->bytes, .iov_len = sizeof(response->bytes)};
  struct msghdr message;
  struct cmsghdr* control = (struct cmsghdr*)0;
  int received_fds[3] = {-1, -1, -1};
  uint32_t received_count = 0;
  ssize_t received = 0;
  (void)memset(&message, 0, sizeof(message));
  (void)memset(&ancillary, 0, sizeof(ancillary));
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  message.msg_control = ancillary.bytes;
  message.msg_controllen = sizeof(ancillary.bytes);
  do {
    received = recvmsg(socket_fd, &message, MSG_CMSG_CLOEXEC | MSG_TRUNC);
  } while (received < 0 && errno == EINTR);
  if (received != (ssize_t)sizeof(response->bytes) ||
      (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0) {
    return 0;
  }
  for (control = CMSG_FIRSTHDR(&message); control != (struct cmsghdr*)0;
       control = CMSG_NXTHDR(&message, control)) {
    size_t payload_size = 0;
    uint32_t descriptor_count = 0;
    if (control->cmsg_level != SOL_SOCKET || control->cmsg_type != SCM_RIGHTS ||
        control->cmsg_len < CMSG_LEN(0)) {
      close_fds(received_fds, received_count);
      return 0;
    }
    payload_size = control->cmsg_len - CMSG_LEN(0);
    if (payload_size % sizeof(int) != 0U || payload_size / sizeof(int) > 3U - received_count) {
      close_fds(received_fds, received_count);
      return 0;
    }
    descriptor_count = (uint32_t)(payload_size / sizeof(int));
    (void)memcpy(received_fds + received_count, CMSG_DATA(control), payload_size);
    received_count += descriptor_count;
  }
  close_fds(received_fds, received_count);
  return received_count == UINT32_C(3);
}

static void legacy_session_close(legacy_session* session) {
  if (session->socket_fd >= 0) {
    (void)close(session->socket_fd);
    session->socket_fd = -1;
  }
}

static int connect_legacy(const char* path, legacy_session* session) {
  const uint64_t required = MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
                            MF_CLIENT_CAP_FUTEX_DOORBELL_V1;
  const uint64_t optional = MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1;
  mf_client_negotiation_request_v1 request;
  mf_client_negotiation_response_v1 response;
  struct sockaddr_un address;
  size_t path_length = strlen(path);
  socklen_t address_length = 0;
  uint32_t attempt = 0;
  (void)memset(session, 0, sizeof(*session));
  session->socket_fd = -1;
  if (path_length == 0U || path_length >= sizeof(address.sun_path)) {
    return 0;
  }
  (void)memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  (void)memcpy(address.sun_path, path, path_length + 1U);
  address_length = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_length + 1U);
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    session->socket_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (session->socket_fd >= 0 &&
        connect(session->socket_fd, (const struct sockaddr*)&address, address_length) == 0) {
      break;
    }
    legacy_session_close(session);
    short_pause();
  }
  if (session->socket_fd < 0) {
    return 0;
  }
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), required, optional,
                                        MF_CLIENT_FLAG_JOIN_EXISTING_VIEW_V1);
  if (send(session->socket_fd, request.bytes, sizeof(request.bytes), MSG_NOSIGNAL) !=
          (ssize_t)sizeof(request.bytes) ||
      !receive_negotiation(session->socket_fd, &response) ||
      mf_client_negotiation_response_validate_v1(&response) != MF_CLIENT_NEGOTIATION_OK ||
      mf_client_load_le32_v1(response.bytes + 12) != MF_CLIENT_NEGOTIATION_OK ||
      (mf_client_load_le64_v1(response.bytes + 24) & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) !=
          UINT64_C(0)) {
    legacy_session_close(session);
    return 0;
  }
  session->view_id.daemon_incarnation = mf_client_load_le64_v1(response.bytes + 40);
  session->view_id.view_serial = mf_client_load_le64_v1(response.bytes + 48);
  session->next_request_id = UINT64_C(1);
  return 1;
}

static int legacy_control(legacy_session* session, uint16_t opcode, uint64_t object_id,
                          uint64_t argument, uint32_t expected_status, uint64_t* out_object_id,
                          uint64_t* out_generation) {
  mf_client_control_request_v1 request;
  mf_client_control_response_v1 response;
  ssize_t received = 0;
  const uint64_t request_id = session->next_request_id++;
  mf_client_control_request_init_v1(&request, opcode, UINT16_C(0), request_id,
                                    session->view_id.daemon_incarnation,
                                    session->view_id.view_serial, object_id, argument);
  if (send(session->socket_fd, request.bytes, sizeof(request.bytes), MSG_NOSIGNAL) !=
      (ssize_t)sizeof(request.bytes)) {
    return 0;
  }
  do {
    received = recv(session->socket_fd, response.bytes, sizeof(response.bytes), MSG_TRUNC);
  } while (received < 0 && errno == EINTR);
  if (received != (ssize_t)sizeof(response.bytes) ||
      mf_client_control_response_validate_v1(&response) != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le32_v1(response.bytes + 12) != expected_status ||
      mf_client_load_le64_v1(response.bytes + 24) != request_id ||
      mf_client_load_le64_v1(response.bytes + 32) != session->view_id.daemon_incarnation ||
      mf_client_load_le64_v1(response.bytes + 40) != session->view_id.view_serial) {
    return 0;
  }
  if (out_object_id != (uint64_t*)0) {
    *out_object_id = mf_client_load_le64_v1(response.bytes + 48);
  }
  if (out_generation != (uint64_t*)0) {
    *out_generation = mf_client_load_le64_v1(response.bytes + 56);
  }
  return 1;
}

static int transfer_exact(int fd, void* bytes, size_t size, int writing) {
  size_t offset = 0;
  while (offset < size) {
    ssize_t transferred = writing ? write(fd, (const uint8_t*)bytes + offset, size - offset)
                                  : read(fd, (uint8_t*)bytes + offset, size - offset);
    if (transferred < 0 && errno == EINTR) {
      continue;
    }
    if (transferred <= 0) {
      return 0;
    }
    offset += (size_t)transferred;
  }
  return 1;
}

static int connect_compute(const char* path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_session_connect_v1(path, session) == MF_SHARED_SUCCESS) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int connect_observer(const char* path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_observer_connect_v1(path, session) == MF_SHARED_SUCCESS) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int allocate_memory(mf_client_session_v1* session, uint64_t bytes, uint64_t* out_id,
                           uint64_t* out_generation) {
  mf_client_control_response_v1 response;
  if (mf_client_session_control_v1(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1, UINT16_C(0),
                                   MF_CLIENT_RUNTIME_CONTEXT_ID_V1, bytes, -1, &response,
                                   (int32_t*)0) != MF_SHARED_SUCCESS ||
      mf_client_load_le32_v1(response.bytes + 12) != MF_CLIENT_CONTROL_OK) {
    return 0;
  }
  *out_id = mf_client_load_le64_v1(response.bytes + 48);
  *out_generation = mf_client_load_le64_v1(response.bytes + 56);
  return *out_id != UINT64_C(0) && *out_generation != UINT64_C(0);
}

static int free_memory(mf_client_session_v1* session, uint64_t id, uint64_t generation) {
  mf_client_control_response_v1 response;
  return mf_client_session_control_v1(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, UINT16_C(0),
                                      id, generation, -1, &response,
                                      (int32_t*)0) == MF_SHARED_SUCCESS &&
         mf_client_load_le32_v1(response.bytes + 12) == MF_CLIENT_CONTROL_OK;
}

static int context_control(mf_client_session_v1* session, uint16_t opcode,
                           uint32_t expected_status) {
  mf_client_control_response_v1 response;
  return mf_client_session_control_v1(session, opcode, UINT16_C(0), MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                                      UINT64_C(1), -1, &response,
                                      (int32_t*)0) == MF_SHARED_SUCCESS &&
         mf_client_load_le32_v1(response.bytes + 12) == expected_status;
}

static int send_context_control_no_wait(mf_client_session_v1* session) {
  mf_client_control_request_v1 request;
  ssize_t sent = -1;
  if (session == (mf_client_session_v1*)0 || session->socket_fd < 0 ||
      session->next_control_request_id == UINT64_MAX) {
    return 0;
  }
  mf_client_control_request_init_v1(
      &request, MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1, UINT16_C(0), session->next_control_request_id,
      session->registry_view_id.daemon_incarnation, session->registry_view_id.view_serial,
      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1));
  if (mf_client_control_request_validate_v1(&request) != MF_CLIENT_CONTROL_OK) {
    return 0;
  }
  do {
    sent = send(session->socket_fd, request.bytes, sizeof(request.bytes), MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR);
  if (sent != (ssize_t)sizeof(request.bytes)) {
    return 0;
  }
  ++session->next_control_request_id;
  return 1;
}

static int register_host_memory(mf_client_session_v1* session, const mf_client_payload_v1* payload,
                                uint32_t expected_status, uint64_t* out_id,
                                uint64_t* out_generation) {
  mf_client_control_response_v1 response;
  if (mf_client_session_control_v1(session, MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                                   MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
                                   MF_CLIENT_RUNTIME_CONTEXT_ID_V1, payload->mapping_size,
                                   payload->owned_fd, &response,
                                   (int32_t*)0) != MF_SHARED_SUCCESS ||
      mf_client_load_le32_v1(response.bytes + 12) != expected_status) {
    return 0;
  }
  if (expected_status == MF_CLIENT_CONTROL_OK) {
    *out_id = mf_client_load_le64_v1(response.bytes + 48);
    *out_generation = mf_client_load_le64_v1(response.bytes + 56);
    return *out_id != UINT64_C(0) && *out_generation != UINT64_C(0);
  }
  return 1;
}

static int release_host_memory(mf_client_session_v1* session, uint64_t id, uint64_t generation) {
  mf_client_control_response_v1 response;
  return mf_client_session_control_v1(session, MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1,
                                      UINT16_C(0), id, generation, -1, &response,
                                      (int32_t*)0) == MF_SHARED_SUCCESS &&
         mf_client_load_le32_v1(response.bytes + 12) == MF_CLIENT_CONTROL_OK;
}

static int fetch_rows(mf_client_session_v1* observer, uint64_t* revision, uint32_t* count,
                      mf_client_process_snapshot_row_v1* rows) {
  mf_client_process_snapshot_v1 snapshot = {.owned_fd = -1};
  uint32_t capacity = *count;
  const mf_shared_status_v1 status = mf_client_process_snapshot_fetch_v1(observer, &snapshot);
  if (status != MF_SHARED_SUCCESS ||
      mf_client_process_snapshot_fill_v1(&snapshot, &capacity, rows) != MF_SHARED_SUCCESS) {
    mf_client_process_snapshot_close_v1(&snapshot);
    return 0;
  }
  *revision = mf_client_process_snapshot_revision_value_v1(&snapshot);
  *count = capacity;
  mf_client_process_snapshot_close_v1(&snapshot);
  return 1;
}

static int wait_for_row_count(mf_client_session_v1* observer, uint64_t minimum_revision,
                              uint32_t expected_count, uint64_t* revision,
                              mf_client_process_snapshot_row_v1* rows) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    uint32_t count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
    uint64_t current_revision = 0;
    if (fetch_rows(observer, &current_revision, &count, rows) && count == expected_count &&
        current_revision > minimum_revision) {
      *revision = current_revision;
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int read_memory_used(mf_client_session_v1* observer, uint64_t expected_bytes) {
  mf_generation_handle_v1 handle;
  mf_client_telemetry_snapshot_v1 telemetry;
  uint32_t attempt = 0;
  if (mf_client_registry_make_handle_v1(&observer->registry, UINT32_C(0),
                                        MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1),
                                        MF_OBJECT_TYPE_CONTEXT, &handle) != MF_SHARED_SUCCESS) {
    return 0;
  }
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_registry_read_telemetry_v1(&observer->registry, &handle, &telemetry) ==
            MF_SHARED_SUCCESS &&
        telemetry.memory_used_bytes == expected_bytes &&
        telemetry.memory_capacity_bytes == UINT64_C(256) * UINT64_C(1024) * UINT64_C(1024)) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int read_active_state(mf_client_session_v1* observer, int expect_active,
                             int expect_memory_active) {
  mf_generation_handle_v1 handle;
  mf_client_telemetry_snapshot_v1 telemetry;
  uint32_t attempt = 0;
  if (mf_client_registry_make_handle_v1(&observer->registry, UINT32_C(0),
                                        MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1),
                                        MF_OBJECT_TYPE_CONTEXT, &handle) != MF_SHARED_SUCCESS) {
    return 0;
  }
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_registry_read_telemetry_v1(&observer->registry, &handle, &telemetry) ==
            MF_SHARED_SUCCESS &&
        ((expect_active && telemetry.active_time_ns != UINT64_C(0)) ||
         (!expect_active && telemetry.active_time_ns == UINT64_C(0))) &&
        ((expect_memory_active && telemetry.memory_active_time_ns != UINT64_C(0)) ||
         (!expect_memory_active && telemetry.memory_active_time_ns == UINT64_C(0)))) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int wait_for_completion_status(mf_client_session_v1* compute, uint64_t request_id,
                                      int32_t expected_status) {
  mf_client_completion_v1 completion;
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    const mf_shared_status_v1 status =
        mf_client_try_consume_completion_v1(&compute->completion, &completion);
    if (status == MF_SHARED_SUCCESS) {
      return completion.request_id == request_id && completion.status == expected_status;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      return 0;
    }
    (void)mf_client_ring_wait_readable_v1(&compute->completion, UINT64_C(10000000));
  }
  return 0;
}

static int submit_work(mf_client_session_v1* compute) {
  const uint64_t request_id = UINT64_C(0x5502);
  if (mf_client_submit_queue_control_v1(&compute->submission, MF_RING_OPCODE_QUEUE_SYNCHRONIZE,
                                        request_id, UINT64_C(0),
                                        UINT32_C(0)) != MF_SHARED_SUCCESS) {
    return 0;
  }
  return wait_for_completion_status(compute, request_id, MF_SHARED_SUCCESS);
}

static int wait_for_session_close_attempts(int socket_fd, uint32_t maximum_attempts) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < maximum_attempts; ++attempt) {
    struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN, .revents = 0};
    int poll_result = -1;
    do {
      poll_result = poll(&descriptor, 1U, 10);
    } while (poll_result < 0 && errno == EINTR);
    if (poll_result < 0) {
      return 0;
    }
    if (poll_result > 0 && (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      return 1;
    }
  }
  return 0;
}

static int wait_for_session_close(int socket_fd) {
  return wait_for_session_close_attempts(socket_fd, UINT32_C(500));
}

static int submit_queue_control_retry(mf_client_session_v1* compute, uint64_t request_id) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    const mf_shared_status_v1 status =
        mf_client_submit_queue_control_v1(&compute->submission, MF_RING_OPCODE_QUEUE_SYNCHRONIZE,
                                          request_id, UINT64_C(0), UINT32_C(0));
    if (status == MF_SHARED_SUCCESS) {
      return 1;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      return 0;
    }
    (void)mf_client_ring_wait_writable_v1(&compute->submission, UINT64_C(10000000));
  }
  return 0;
}

static int submit_memory_free_retry(mf_client_session_v1* compute, uint64_t request_id,
                                    uint64_t object_id, uint64_t object_generation) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    const mf_shared_status_v1 status = mf_client_submit_memory_free_v1(
        &compute->submission, request_id, object_id, object_generation);
    if (status == MF_SHARED_SUCCESS) {
      return 1;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      return 0;
    }
    (void)mf_client_ring_wait_writable_v1(&compute->submission, UINT64_C(10000000));
  }
  return 0;
}

static int queue_prune_rejection(mf_client_session_v1* compute, uint64_t object_id,
                                 uint64_t object_generation) {
  const uint32_t capacity = compute->completion.header->metadata.capacity;
  uint32_t index = 0;
  uint32_t attempt = 0;
  if (capacity == 0U || capacity > UINT32_MAX - UINT32_C(2)) {
    return 0;
  }
  for (index = 0; index <= capacity; ++index) {
    if (!submit_queue_control_retry(compute, TEST_PRUNE_BACKLOG_REQUEST_BASE + (uint64_t)index)) {
      return 0;
    }
  }
  if (!submit_memory_free_retry(compute,
                                TEST_PRUNE_BACKLOG_REQUEST_BASE + (uint64_t)capacity + UINT64_C(1),
                                object_id, object_generation)) {
    return 0;
  }
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    const uint64_t completion_producer =
        mf_atomic_load_u64_acquire(&compute->completion.header->producer.position);
    const uint64_t submission_consumer =
        mf_atomic_load_u64_acquire(&compute->submission.header->consumer.position);
    const uint64_t submission_producer =
        mf_atomic_load_u64_acquire(&compute->submission.header->producer.position);
    if (completion_producer == (uint64_t)capacity &&
        submission_consumer == (uint64_t)capacity + UINT64_C(1) &&
        submission_producer == (uint64_t)capacity + UINT64_C(2)) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int drain_prune_rejection(mf_client_session_v1* compute) {
  const uint32_t capacity = compute->completion.header->metadata.capacity;
  uint32_t index = 0;
  if (capacity == 0U || capacity > UINT32_MAX - UINT32_C(2)) {
    return 0;
  }
  for (index = 0; index < capacity + UINT32_C(2); ++index) {
    mf_client_completion_v1 completion;
    uint32_t attempt = 0;
    mf_shared_status_v1 status = MF_SHARED_WOULD_BLOCK;
    for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
      status = mf_client_try_consume_completion_v1(&compute->completion, &completion);
      if (status == MF_SHARED_SUCCESS) {
        break;
      }
      if (status != MF_SHARED_WOULD_BLOCK) {
        return 0;
      }
      (void)mf_client_ring_wait_readable_v1(&compute->completion, UINT64_C(10000000));
    }
    if (status != MF_SHARED_SUCCESS ||
        completion.request_id != TEST_PRUNE_BACKLOG_REQUEST_BASE + (uint64_t)index ||
        completion.status !=
            (index == capacity + UINT32_C(1) ? MF_SHARED_STALE_HANDLE : MF_SHARED_SUCCESS)) {
      return 0;
    }
  }
  return wait_for_session_close(compute->socket_fd);
}

static int registry_rejects_writes(mf_client_session_v1* observer) {
  const int fd = mf_client_registry_borrow_fd_v1(&observer->registry);
  const int seals = fd < 0 ? -1 : fcntl(fd, F_GET_SEALS);
  uint8_t changed_byte = UINT8_C(0);
  void* writable = MAP_FAILED;
  if (seals < 0 || (seals & F_SEAL_FUTURE_WRITE) == 0) {
    return 0;
  }
  writable = mmap((void*)0, (size_t)observer->registry.mapping_size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);
  if (writable != MAP_FAILED) {
    (void)munmap(writable, (size_t)observer->registry.mapping_size);
    return 0;
  }
  if (mprotect(observer->registry.mapping, (size_t)observer->registry.mapping_size,
               PROT_READ | PROT_WRITE) == 0) {
    (void)mprotect(observer->registry.mapping, (size_t)observer->registry.mapping_size, PROT_READ);
    return 0;
  }
  changed_byte = (uint8_t)(*(const uint8_t*)observer->registry.mapping ^ UINT8_C(1));
  if (pwrite(fd, &changed_byte, sizeof(changed_byte), (off_t)0) >= 0 || errno != EPERM) {
    return 0;
  }
  return 1;
}

int main(int argc, char** argv) {
  char directory_template[] = "/tmp/metaflux-process-snapshot-XXXXXX";
  char socket_path[PATH_MAX];
  mf_client_session_v1 compute;
  mf_client_session_v1 second_compute;
  mf_client_session_v1 observer;
  mf_client_session_v1 second_observer;
  mf_client_session_v1 quota_replacements[TEST_QUOTA_REPLACEMENT_SESSION_COUNT];
  legacy_session legacy = {.socket_fd = -1};
  mf_virtual_device_identity_v1 identity_before;
  mf_virtual_device_identity_v1 identity_after;
  mf_client_process_snapshot_row_v1 rows[MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1];
  mf_client_payload_v1 shared_payload = {.owned_fd = -1};
  uint64_t mapped_ids[5] = {0};
  uint64_t mapped_generations[5] = {0};
  uint64_t empty_revision = 0;
  uint64_t connected_revision = 0;
  uint64_t active_revision = 0;
  uint64_t closed_revision = 0;
  uint64_t legacy_active_revision = 0;
  uint64_t legacy_connected_revision = 0;
  uint64_t legacy_closed_revision = 0;
  uint64_t legacy_object_id = 0;
  uint64_t legacy_object_generation = 0;
  uint64_t dead_live_revision = 0;
  uint64_t dead_pruned_revision = 0;
  uint64_t object_id = 0;
  uint64_t object_generation = 0;
  uint64_t second_object_id = 0;
  uint64_t second_object_generation = 0;
  uint32_t count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  uint32_t reuse_index = 0;
  uint32_t quota_replacement_count = 0;
  int compute_connected = 0;
  int second_compute_connected = 0;
  int observer_connected = 0;
  int second_observer_connected = 0;
  int legacy_connected = 0;
  int result = 1;
  pid_t daemon = -1;
  pid_t exiting_compute = -1;
  pid_t holder = -1;
  int ready_pipe[2] = {-1, -1};
  int release_pipe[2] = {-1, -1};
  int queued_pipe[2] = {-1, -1};
  int drain_pipe[2] = {-1, -1};
  int result_pipe[2] = {-1, -1};
  char* directory = (char*)0;
  if (argc != 2) {
    return 64;
  }
  directory = mkdtemp(directory_template);
  if (directory == (char*)0 ||
      snprintf(socket_path, sizeof(socket_path), "%s/metafluxd.sock", directory) <= 0) {
    return 1;
  }
  daemon = fork();
  if (daemon == 0) {
    execl(argv[1], argv[1], "--socket", socket_path, (char*)0);
    _exit(127);
  }
  if (daemon <= 0 || !connect_observer(socket_path, &observer)) {
    goto cleanup;
  }
  observer_connected = 1;
  if (!fetch_rows(&observer, &empty_revision, &count, rows) || count != UINT32_C(0) ||
      (observer.negotiated_capabilities & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) !=
          UINT64_C(0) ||
      mf_client_registry_identity_v1(&observer.registry, UINT32_C(0), &identity_before) !=
          MF_SHARED_SUCCESS ||
      !registry_rejects_writes(&observer) || !connect_observer(socket_path, &second_observer)) {
    goto cleanup;
  }
  second_observer_connected = 1;
  if (mf_client_registry_identity_v1(&second_observer.registry, UINT32_C(0), &identity_after) !=
          MF_SHARED_SUCCESS ||
      (second_observer.negotiated_capabilities & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) !=
          UINT64_C(0) ||
      memcmp(&identity_before, &identity_after, sizeof(identity_before)) != 0) {
    goto cleanup;
  }
  if (!connect_legacy(socket_path, &legacy)) {
    (void)fprintf(stderr, "legacy negotiation failed\n");
    goto cleanup;
  }
  legacy_connected = 1;
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (!fetch_rows(&observer, &legacy_connected_revision, &count, rows) || count != UINT32_C(1) ||
      legacy_connected_revision <= empty_revision || rows[0].pid != (uint32_t)getpid() ||
      rows[0].used_memory_bytes != UINT64_C(0) ||
      !legacy_control(&legacy, MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1), MF_CLIENT_CONTROL_UNSUPPORTED,
                      (uint64_t*)0, (uint64_t*)0) ||
      !legacy_control(&legacy, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, LEGACY_TEST_MEMORY_BYTES,
                      MF_CLIENT_CONTROL_OK, &legacy_object_id, &legacy_object_generation)) {
    (void)fprintf(stderr, "legacy publication/control start failed\n");
    goto cleanup;
  }
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (legacy_object_id == UINT64_C(0) || legacy_object_generation == UINT64_C(0) ||
      !fetch_rows(&observer, &legacy_active_revision, &count, rows) || count != UINT32_C(1) ||
      legacy_active_revision <= legacy_connected_revision ||
      rows[0].used_memory_bytes != LEGACY_TEST_MEMORY_BYTES ||
      !legacy_control(&legacy, MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, legacy_object_id,
                      legacy_object_generation, MF_CLIENT_CONTROL_OK, (uint64_t*)0, (uint64_t*)0)) {
    (void)fprintf(stderr, "legacy post-control publication failed\n");
    goto cleanup;
  }
  legacy_session_close(&legacy);
  legacy_connected = 0;
  if (!wait_for_row_count(&observer, legacy_active_revision, UINT32_C(0), &legacy_closed_revision,
                          rows) ||
      !read_memory_used(&observer, UINT64_C(0))) {
    (void)fprintf(stderr, "legacy disconnect publication failed\n");
    goto cleanup;
  }
  empty_revision = legacy_closed_revision;

  if (!connect_compute(socket_path, &compute) ||
      (compute.negotiated_capabilities & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) == UINT64_C(0)) {
    (void)fprintf(stderr, "modern capability negotiation failed\n");
    goto cleanup;
  }
  compute_connected = 1;
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (!fetch_rows(&observer, &connected_revision, &count, rows) || count != UINT32_C(0) ||
      connected_revision <= empty_revision ||
      !context_control(&compute, MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1, MF_CLIENT_CONTROL_OK) ||
      !submit_work(&compute) || !read_active_state(&observer, 1, 0)) {
    goto cleanup;
  }
  {
    const struct timespec idle_window = {.tv_sec = 0, .tv_nsec = 150000000};
    (void)nanosleep(&idle_window, (struct timespec*)0);
  }
  if (!read_active_state(&observer, 0, 0)) {
    goto cleanup;
  }
  for (reuse_index = 0; reuse_index < TEST_OBJECT_REUSE_COUNT; ++reuse_index) {
    if (!allocate_memory(&compute, UINT64_C(1), &object_id, &object_generation) ||
        !free_memory(&compute, object_id, object_generation)) {
      (void)fprintf(stderr, "released object capacity was not reusable at iteration %u\n",
                    reuse_index);
      goto cleanup;
    }
  }
  object_id = UINT64_C(0);
  object_generation = UINT64_C(0);
  if (!connect_compute(socket_path, &second_compute) ||
      (second_compute.negotiated_capabilities & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) ==
          UINT64_C(0)) {
    goto cleanup;
  }
  second_compute_connected = 1;
  if (mf_client_payload_create_v1((const uint8_t*)0, TEST_MAPPING_BYTES, UINT32_C(0),
                                  &shared_payload) != MF_SHARED_SUCCESS ||
      !register_host_memory(&compute, &shared_payload, MF_CLIENT_CONTROL_OK, &mapped_ids[0],
                            &mapped_generations[0]) ||
      !register_host_memory(&compute, &shared_payload, MF_CLIENT_CONTROL_OK, &mapped_ids[1],
                            &mapped_generations[1]) ||
      !register_host_memory(&second_compute, &shared_payload, MF_CLIENT_CONTROL_OK, &mapped_ids[2],
                            &mapped_generations[2]) ||
      !register_host_memory(&second_compute, &shared_payload, MF_CLIENT_CONTROL_OK, &mapped_ids[3],
                            &mapped_generations[3]) ||
      !register_host_memory(&second_compute, &shared_payload, MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED,
                            &mapped_ids[4], &mapped_generations[4]) ||
      !release_host_memory(&compute, mapped_ids[0], mapped_generations[0]) ||
      !register_host_memory(&second_compute, &shared_payload, MF_CLIENT_CONTROL_OK, &mapped_ids[4],
                            &mapped_generations[4])) {
    goto cleanup;
  }
  if (!allocate_memory(&compute, TEST_MEMORY_BYTES, &object_id, &object_generation) ||
      !allocate_memory(&second_compute, TEST_MEMORY_BYTES, &second_object_id,
                       &second_object_generation)) {
    goto cleanup;
  }
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (!fetch_rows(&observer, &active_revision, &count, rows) || count != UINT32_C(1) ||
      active_revision <= empty_revision || rows[0].pid != (uint32_t)getpid() ||
      rows[0].used_memory_bytes != UINT64_C(2) * TEST_MEMORY_BYTES ||
      rows[0].identity_record_id != UINT64_C(1) || rows[0].device_generation != UINT64_C(1) ||
      (rows[0].kinds & MF_CLIENT_PROCESS_KIND_COMPUTE_V1) == UINT32_C(0) ||
      !read_memory_used(&second_observer, UINT64_C(2) * TEST_MEMORY_BYTES)) {
    goto cleanup;
  }
  if (!free_memory(&compute, object_id, object_generation) ||
      !free_memory(&second_compute, second_object_id, second_object_generation) ||
      !context_control(&compute, MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1, MF_CLIENT_CONTROL_OK) ||
      !context_control(&compute, MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1,
                       MF_CLIENT_CONTROL_INVALID_ARGUMENT)) {
    goto cleanup;
  }
  object_id = UINT64_C(0);
  object_generation = UINT64_C(0);
  second_object_id = UINT64_C(0);
  second_object_generation = UINT64_C(0);
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (!fetch_rows(&second_observer, &closed_revision, &count, rows) || count != UINT32_C(0) ||
      closed_revision <= active_revision || !read_memory_used(&observer, UINT64_C(0))) {
    goto cleanup;
  }
  mf_client_session_close_v1(&compute);
  compute_connected = 0;
  mf_client_session_close_v1(&second_compute);
  second_compute_connected = 0;

  if (prctl(PR_SET_CHILD_SUBREAPER, 1) != 0 || pipe2(ready_pipe, O_CLOEXEC) != 0 ||
      pipe2(release_pipe, O_CLOEXEC) != 0 || pipe2(queued_pipe, O_CLOEXEC) != 0 ||
      pipe2(drain_pipe, O_CLOEXEC) != 0 || pipe2(result_pipe, O_CLOEXEC) != 0) {
    goto cleanup;
  }
  exiting_compute = fork();
  if (exiting_compute == 0) {
    mf_client_session_v1 dead_compute;
    mf_client_session_v1 idle_compute;
    mf_client_session_v1 stalled_compute;
    uint64_t dead_object_id = 0;
    uint64_t dead_object_generation = 0;
    pid_t child_holder = -1;
    char release_token = '\0';
    (void)close(ready_pipe[0]);
    (void)close(release_pipe[1]);
    (void)close(queued_pipe[0]);
    (void)close(drain_pipe[1]);
    (void)close(result_pipe[0]);
    if (!connect_compute(socket_path, &dead_compute) ||
        !context_control(&dead_compute, MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1,
                         MF_CLIENT_CONTROL_OK) ||
        !allocate_memory(&dead_compute, DEAD_PROCESS_MEMORY_BYTES, &dead_object_id,
                         &dead_object_generation) ||
        !connect_compute(socket_path, &idle_compute) ||
        !connect_compute(socket_path, &stalled_compute)) {
      _exit(2);
    }
    child_holder = fork();
    if (child_holder == 0) {
      char drain_token = '\0';
      int queued = 0;
      int retired = 0;
      (void)close(ready_pipe[1]);
      (void)close(release_pipe[0]);
      queued = queue_prune_rejection(&dead_compute, dead_object_id, dead_object_generation) &&
               queue_prune_rejection(&stalled_compute, dead_object_id, dead_object_generation);
      if (!transfer_exact(queued_pipe[1], &queued, sizeof(queued), 1) || queued == 0 ||
          !wait_for_session_close(idle_compute.socket_fd) ||
          !send_context_control_no_wait(&dead_compute) ||
          !transfer_exact(drain_pipe[0], &drain_token, sizeof(drain_token), 0)) {
        (void)transfer_exact(result_pipe[1], &retired, sizeof(retired), 1);
        _exit(4);
      }
      retired =
          drain_prune_rejection(&dead_compute) &&
          wait_for_session_close_attempts(stalled_compute.socket_fd, TEST_BOUNDED_CLOSE_ATTEMPTS);
      (void)transfer_exact(result_pipe[1], &retired, sizeof(retired), 1);
      _exit(retired ? 0 : 5);
    }
    (void)close(queued_pipe[1]);
    (void)close(drain_pipe[0]);
    (void)close(result_pipe[1]);
    if (child_holder < 0 ||
        !transfer_exact(ready_pipe[1], &child_holder, sizeof(child_holder), 1) ||
        !transfer_exact(release_pipe[0], &release_token, sizeof(release_token), 0)) {
      if (child_holder > 0) {
        (void)kill(child_holder, SIGTERM);
      }
      _exit(3);
    }
    _exit(0);
  }
  if (exiting_compute < 0) {
    goto cleanup;
  }
  (void)close(ready_pipe[1]);
  ready_pipe[1] = -1;
  (void)close(release_pipe[0]);
  release_pipe[0] = -1;
  (void)close(queued_pipe[1]);
  queued_pipe[1] = -1;
  (void)close(drain_pipe[0]);
  drain_pipe[0] = -1;
  (void)close(result_pipe[1]);
  result_pipe[1] = -1;
  if (!transfer_exact(ready_pipe[0], &holder, sizeof(holder), 0) || holder <= 0) {
    goto cleanup;
  }
  (void)close(ready_pipe[0]);
  ready_pipe[0] = -1;
  {
    int queued = 0;
    if (!transfer_exact(queued_pipe[0], &queued, sizeof(queued), 0) || queued == 0) {
      goto cleanup;
    }
    (void)close(queued_pipe[0]);
    queued_pipe[0] = -1;
  }
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (!fetch_rows(&observer, &dead_live_revision, &count, rows) || count != UINT32_C(1) ||
      rows[0].pid != (uint32_t)exiting_compute ||
      rows[0].used_memory_bytes != DEAD_PROCESS_MEMORY_BYTES ||
      !read_memory_used(&observer, DEAD_PROCESS_MEMORY_BYTES)) {
    goto cleanup;
  }
  {
    char release_token = 'x';
    int child_status = 0;
    if (!transfer_exact(release_pipe[1], &release_token, sizeof(release_token), 1)) {
      goto cleanup;
    }
    (void)close(release_pipe[1]);
    release_pipe[1] = -1;
    if (waitpid(exiting_compute, &child_status, 0) != exiting_compute || !WIFEXITED(child_status) ||
        WEXITSTATUS(child_status) != 0) {
      goto cleanup;
    }
    exiting_compute = -1;
  }
  count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  if (!fetch_rows(&second_observer, &dead_pruned_revision, &count, rows) || count != UINT32_C(0) ||
      dead_pruned_revision <= dead_live_revision ||
      !read_memory_used(&second_observer, DEAD_PROCESS_MEMORY_BYTES)) {
    goto cleanup;
  }
  {
    char drain_token = 'x';
    int retired = 0;
    int holder_status = 0;
    if (!transfer_exact(drain_pipe[1], &drain_token, sizeof(drain_token), 1) ||
        !transfer_exact(result_pipe[0], &retired, sizeof(retired), 0) || retired == 0 ||
        waitpid(holder, &holder_status, 0) != holder || !WIFEXITED(holder_status) ||
        WEXITSTATUS(holder_status) != 0) {
      goto cleanup;
    }
    (void)close(drain_pipe[1]);
    drain_pipe[1] = -1;
  }
  holder = -1;
  if (!read_memory_used(&second_observer, UINT64_C(0))) {
    goto cleanup;
  }
  for (quota_replacement_count = 0; quota_replacement_count < TEST_QUOTA_REPLACEMENT_SESSION_COUNT;
       ++quota_replacement_count) {
    if (!connect_observer(socket_path, &quota_replacements[quota_replacement_count])) {
      goto cleanup;
    }
  }
  result = 0;

cleanup:
  if (ready_pipe[0] >= 0) {
    (void)close(ready_pipe[0]);
  }
  if (ready_pipe[1] >= 0) {
    (void)close(ready_pipe[1]);
  }
  if (release_pipe[0] >= 0) {
    (void)close(release_pipe[0]);
  }
  if (release_pipe[1] >= 0) {
    (void)close(release_pipe[1]);
  }
  if (queued_pipe[0] >= 0) {
    (void)close(queued_pipe[0]);
  }
  if (queued_pipe[1] >= 0) {
    (void)close(queued_pipe[1]);
  }
  if (drain_pipe[0] >= 0) {
    (void)close(drain_pipe[0]);
  }
  if (drain_pipe[1] >= 0) {
    (void)close(drain_pipe[1]);
  }
  if (result_pipe[0] >= 0) {
    (void)close(result_pipe[0]);
  }
  if (result_pipe[1] >= 0) {
    (void)close(result_pipe[1]);
  }
  if (exiting_compute > 0) {
    (void)kill(exiting_compute, SIGTERM);
    (void)waitpid(exiting_compute, (int*)0, 0);
  }
  if (holder > 0) {
    (void)kill(holder, SIGTERM);
    (void)waitpid(holder, (int*)0, 0);
  }
  if (compute_connected) {
    mf_client_session_close_v1(&compute);
  }
  if (second_compute_connected) {
    mf_client_session_close_v1(&second_compute);
  }
  if (legacy_connected) {
    legacy_session_close(&legacy);
  }
  while (quota_replacement_count != 0U) {
    --quota_replacement_count;
    mf_client_session_close_v1(&quota_replacements[quota_replacement_count]);
  }
  mf_client_payload_close_v1(&shared_payload);
  if (observer_connected) {
    mf_client_session_close_v1(&observer);
  }
  if (second_observer_connected) {
    mf_client_session_close_v1(&second_observer);
  }
  if (daemon > 0) {
    (void)kill(daemon, SIGTERM);
    (void)waitpid(daemon, (int*)0, 0);
  }
  (void)unlink(socket_path);
  (void)rmdir(directory);
  return result;
}
