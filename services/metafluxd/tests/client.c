#define _GNU_SOURCE

#include "metaflux/client/fastpath.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct object_ref {
  uint64_t id;
  uint64_t generation;
} object_ref;

typedef struct add_argument_block {
  mf_argument_block_header_v1 header;
  mf_argument_entry_v1 entries[4];
} add_argument_block;

typedef struct copy_argument_block {
  mf_argument_block_header_v1 header;
  mf_argument_entry_v1 entries[4];
} copy_argument_block;

#define MF_TEST_COPY_ARGUMENT_SIZE                                                                 \
  (sizeof(mf_argument_block_header_v1) +                                                           \
   ((size_t)MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 * sizeof(mf_argument_entry_v1)))

static void initialize_copy_argument_block(copy_argument_block* arguments, object_ref destination,
                                           uint64_t destination_offset, object_ref source,
                                           uint64_t source_offset, uint64_t byte_count) {
  (void)memset(arguments, 0, sizeof(*arguments));
  arguments->header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
  arguments->header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  arguments->header.header_size = (uint32_t)sizeof(arguments->header);
  arguments->header.entry_size = (uint32_t)sizeof(arguments->entries[0]);
  arguments->header.entry_count = MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1;
  arguments->header.flags = MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1;
  arguments->header.total_size = MF_TEST_COPY_ARGUMENT_SIZE;
  arguments->entries[MF_COPY_REGION_DESTINATION_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments->entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags = MF_ARGUMENT_BUFFER_WRITE;
  arguments->entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_id = destination.id;
  arguments->entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_generation =
      destination.generation;
  arguments->entries[MF_COPY_REGION_DESTINATION_INDEX_V1].value = destination_offset;
  arguments->entries[MF_COPY_REGION_SOURCE_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments->entries[MF_COPY_REGION_SOURCE_INDEX_V1].flags = MF_ARGUMENT_BUFFER_READ;
  arguments->entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_id = source.id;
  arguments->entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_generation = source.generation;
  arguments->entries[MF_COPY_REGION_SOURCE_INDEX_V1].value = source_offset;
  arguments->entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].kind = MF_ARGUMENT_KIND_U64;
  arguments->entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = byte_count;
}

static const char add_ptx[] = ".version 9.0\n"
                              ".target sm_70\n"
                              ".address_size 64\n"
                              ".visible .entry add_u32(\n"
                              "  .param .u64 destination,\n"
                              "  .param .u64 left,\n"
                              "  .param .u64 right,\n"
                              "  .param .u32 count\n"
                              ")\n"
                              "{\n"
                              "  .reg .pred %p;\n"
                              "  .reg .b32 %r<10>;\n"
                              "  .reg .b64 %rd<10>;\n"
                              "  ld.param.u64 %rd0, [destination];\n"
                              "  ld.param.u64 %rd1, [left];\n"
                              "  ld.param.u64 %rd2, [right];\n"
                              "  ld.param.u32 %r0, [count];\n"
                              "  mov.u32 %r1, %tid.x;\n"
                              "  mov.u32 %r2, %ctaid.x;\n"
                              "  mov.u32 %r3, %ntid.x;\n"
                              "  mad.lo.u32 %r4, %r2, %r3, %r1;\n"
                              "  setp.ge.u32 %p, %r4, %r0;\n"
                              "  @%p bra done;\n"
                              "  mul.wide.u32 %rd3, %r4, 4;\n"
                              "  add.u64 %rd4, %rd0, %rd3;\n"
                              "  add.u64 %rd5, %rd1, %rd3;\n"
                              "  add.u64 %rd6, %rd2, %rd3;\n"
                              "  ld.global.u32 %r5, [%rd5];\n"
                              "  ld.global.u32 %r6, [%rd6];\n"
                              "  add.u32 %r7, %r5, %r6;\n"
                              "  st.global.u32 [%rd4], %r7;\n"
                              "done:\n"
                              "  ret;\n"
                              "}\n";

static void short_pause(void) {
  const struct timespec duration = {.tv_sec = 0, .tv_nsec = 10000000};
  (void)nanosleep(&duration, (struct timespec*)0);
}

static int wait_for_child(pid_t child) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    int status = 0;
    const pid_t result = waitpid(child, &status, WNOHANG);
    if (result == child) {
      return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    if (result < 0) {
      return 0;
    }
    short_pause();
  }
  (void)kill(child, SIGKILL);
  (void)waitpid(child, (int*)0, 0);
  return 0;
}

static int connect_with_retry(const char* path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_session_connect_v1(path, session) == MF_SHARED_SUCCESS) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int connect_default_with_retry(const char* path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  int connected = 0;
  if (setenv("METAFLUX_SOCKET", path, 1) != 0) {
    return 0;
  }
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_session_connect_default_v1(session) == MF_SHARED_SUCCESS) {
      connected = 1;
      break;
    }
    short_pause();
  }
  (void)unsetenv("METAFLUX_SOCKET");
  return connected;
}

static void close_descriptors(int32_t* descriptors, uint32_t count) {
  uint32_t index = 0;
  for (index = 0; index < count; ++index) {
    if (descriptors[index] >= 0) {
      (void)close(descriptors[index]);
      descriptors[index] = -1;
    }
  }
}

static int receive_negotiation(int socket_fd, mf_client_negotiation_response_v1* response) {
  union {
    struct cmsghdr alignment;
    uint8_t bytes[CMSG_SPACE(sizeof(int32_t) * 3U)];
  } ancillary;
  struct iovec vector = {.iov_base = response->bytes, .iov_len = sizeof(response->bytes)};
  struct msghdr message;
  struct cmsghdr* control = (struct cmsghdr*)0;
  int32_t descriptors[3] = {-1, -1, -1};
  uint32_t descriptor_count = 0;
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
    uint32_t current_count = 0;
    if (control->cmsg_level != SOL_SOCKET || control->cmsg_type != SCM_RIGHTS ||
        control->cmsg_len < CMSG_LEN(0)) {
      close_descriptors(descriptors, descriptor_count);
      return 0;
    }
    payload_size = control->cmsg_len - CMSG_LEN(0);
    if (payload_size % sizeof(int32_t) != 0U ||
        payload_size / sizeof(int32_t) > 3U - descriptor_count) {
      close_descriptors(descriptors, descriptor_count);
      return 0;
    }
    current_count = (uint32_t)(payload_size / sizeof(int32_t));
    (void)memcpy(descriptors + descriptor_count, CMSG_DATA(control), payload_size);
    descriptor_count += current_count;
  }
  close_descriptors(descriptors, descriptor_count);
  return descriptor_count == UINT32_C(3);
}

static int connect_without_copy_region(const char* path, mf_client_session_v1* session) {
  const uint64_t required = MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
                            MF_CLIENT_CAP_FUTEX_DOORBELL_V1 |
                            MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1;
  const uint64_t optional = MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1;
  mf_client_negotiation_request_v1 request;
  mf_client_negotiation_response_v1 response;
  struct sockaddr_un address;
  const size_t path_length = strlen(path);
  const socklen_t address_length =
      (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_length + (size_t)1);
  uint32_t attempt = 0;
  (void)memset(session, 0, sizeof(*session));
  session->socket_fd = -1;
  session->registry.owned_fd = -1;
  session->submission.owned_fd = -1;
  session->completion.owned_fd = -1;
  if (path_length == (size_t)0 || path_length >= sizeof(address.sun_path)) {
    return 0;
  }
  (void)memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  (void)memcpy(address.sun_path, path, path_length + (size_t)1);
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    session->socket_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (session->socket_fd >= 0 &&
        connect(session->socket_fd, (const struct sockaddr*)&address, address_length) == 0) {
      break;
    }
    mf_client_session_close_v1(session);
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
      (mf_client_load_le64_v1(response.bytes + 24) & required) != required ||
      (mf_client_load_le64_v1(response.bytes + 24) & MF_CLIENT_CAP_COPY_REGION_V1) != UINT64_C(0)) {
    mf_client_session_close_v1(session);
    return 0;
  }
  session->negotiated_version = mf_client_load_le32_v1(response.bytes + 16);
  session->negotiated_capabilities = mf_client_load_le64_v1(response.bytes + 24);
  session->registry_view_id.daemon_incarnation = mf_client_load_le64_v1(response.bytes + 40);
  session->registry_view_id.view_serial = mf_client_load_le64_v1(response.bytes + 48);
  session->next_control_request_id = UINT64_C(1);
  return 1;
}

static int control_object(mf_client_session_v1* session, uint16_t opcode, uint16_t flags,
                          uint64_t object_id, uint64_t argument, int32_t payload_fd,
                          uint32_t expected_status, object_ref* out_object,
                          int32_t* out_received_fd) {
  mf_client_control_response_v1 response;
  int32_t received_fd = -1;
  if (mf_client_session_control_v1(session, opcode, flags, object_id, argument, payload_fd,
                                   &response, &received_fd) != MF_SHARED_SUCCESS ||
      mf_client_load_le32_v1(response.bytes + 12) != expected_status) {
    if (received_fd >= 0) {
      (void)close(received_fd);
    }
    return 0;
  }
  if (out_object != (object_ref*)0) {
    out_object->id = mf_client_load_le64_v1(response.bytes + 48);
    out_object->generation = mf_client_load_le64_v1(response.bytes + 56);
  }
  if (out_received_fd != (int32_t*)0) {
    *out_received_fd = received_fd;
  } else if (received_fd >= 0) {
    (void)close(received_fd);
  }
  return expected_status != MF_CLIENT_CONTROL_OK ||
         (mf_client_load_le64_v1(response.bytes + 48) != UINT64_C(0) &&
          mf_client_load_le64_v1(response.bytes + 56) != UINT64_C(0));
}

static int copy_region_register_requires_capability(const char* socket_path) {
  mf_client_session_v1 session;
  mf_client_payload_v1 payload = {.owned_fd = -1};
  object_ref returned = {UINT64_MAX, UINT64_MAX};
  const object_ref destination = {UINT64_C(1), UINT64_C(1)};
  const object_ref source = {UINT64_C(2), UINT64_C(1)};
  _Alignas(64) copy_argument_block arguments;
  int connected = 0;
  int result = 0;
  initialize_copy_argument_block(&arguments, destination, UINT64_C(1), source, UINT64_C(0),
                                 UINT64_C(1));
  if (!connect_without_copy_region(socket_path, &session)) {
    result = 1;
    goto cleanup;
  }
  connected = 1;
  if (mf_client_copy_region_argument_block_validate_v1(
          (const uint8_t*)&arguments, MF_TEST_COPY_ARGUMENT_SIZE) != MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1((const uint8_t*)&arguments, MF_TEST_COPY_ARGUMENT_SIZE, 0U,
                                  &payload) != MF_SHARED_SUCCESS ||
      !control_object(&session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                      MF_TEST_COPY_ARGUMENT_SIZE, payload.owned_fd, MF_CLIENT_CONTROL_UNSUPPORTED,
                      &returned, (int32_t*)0) ||
      returned.id != UINT64_C(0) || returned.generation != UINT64_C(0)) {
    result = 2;
  }

cleanup:
  mf_client_payload_close_v1(&payload);
  if (connected) {
    mf_client_session_close_v1(&session);
  }
  return result == 0;
}

static int wait_completion(mf_client_session_v1* session, uint64_t request_id,
                           mf_shared_status_v1 expected_status,
                           mf_client_completion_v1* out_completion) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    const mf_shared_status_v1 status =
        mf_client_try_consume_completion_v1(&session->completion, out_completion);
    if (status == MF_SHARED_SUCCESS) {
      return out_completion->request_id == request_id && out_completion->status == expected_status;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      return 0;
    }
    (void)mf_client_ring_wait_readable_v1(&session->completion, UINT64_C(10000000));
  }
  return 0;
}

static int submit_direct_copy(mf_client_session_v1* session, uint64_t request_id, uint32_t flags,
                              object_ref device_memory, uint64_t host_address,
                              uint64_t device_offset, uint64_t byte_count,
                              mf_shared_status_v1 expected_status) {
  mf_ring_descriptor_v1 command;
  mf_client_completion_v1 completion;
  (void)memset(&command, 0, sizeof(command));
  command.opcode = MF_RING_OPCODE_COPY;
  command.flags = flags;
  command.request_id = request_id;
  command.target_id = device_memory.id;
  command.arguments[0] = device_memory.generation;
  command.arguments[1] = host_address;
  command.arguments[2] = device_offset;
  command.arguments[3] = byte_count;
  return mf_client_ring_try_submit_v1(&session->submission, &command) == MF_SHARED_SUCCESS &&
         wait_completion(session, request_id, expected_status, &completion);
}

static int run_direct_host_copy(mf_client_session_v1* session, uint64_t* next_request_id) {
  uint8_t source[64];
  uint8_t destination[64];
  object_ref device_memory = {0, 0};
  object_ref ignored = {0, 0};
  char foreign_memory_path[64];
  int child_release_pipe[2] = {-1, -1};
  int null_fd = -1;
  int readonly_memory_fd = -1;
  int foreign_memory_fd = -1;
  int memory_fd = -1;
  int duplicate_memory_fd = -1;
  pid_t foreign_memory_child = -1;
  uint64_t request_id = *next_request_id;
  uint32_t index = 0;
  int result = 0;

  for (index = 0; index < (uint32_t)sizeof(source); ++index) {
    source[index] = (uint8_t)(index * UINT32_C(3) + UINT32_C(1));
  }
  (void)memset(destination, 0, sizeof(destination));
  if ((session->negotiated_capabilities & MF_CLIENT_CAP_DIRECT_HOST_COPY_V1) == UINT64_C(0) ||
      !control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1, 0U,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(source), -1, MF_CLIENT_CONTROL_OK,
                      &device_memory, (int32_t*)0)) {
    result = 1;
    goto cleanup;
  }
  if (!submit_direct_copy(session, request_id++, MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1,
                          device_memory, (uint64_t)(uintptr_t)source, UINT64_C(0), UINT64_C(8),
                          MF_SHARED_INVALID_ARGUMENT)) {
    result = 2;
    goto cleanup;
  }

  if (pipe2(child_release_pipe, O_CLOEXEC) != 0) {
    result = 3;
    goto cleanup;
  }
  foreign_memory_child = fork();
  if (foreign_memory_child == 0) {
    char release = '\0';
    ssize_t received = -1;
    (void)close(child_release_pipe[1]);
    do {
      received = read(child_release_pipe[0], &release, sizeof(release));
    } while (received < 0 && errno == EINTR);
    (void)close(child_release_pipe[0]);
    _exit(received == (ssize_t)sizeof(release) ? 0 : 1);
  }
  (void)close(child_release_pipe[0]);
  child_release_pipe[0] = -1;
  if (foreign_memory_child < 0 || snprintf(foreign_memory_path, sizeof(foreign_memory_path),
                                           "/proc/%ld/mem", (long)foreign_memory_child) <= 0) {
    result = 3;
    goto cleanup;
  }
  foreign_memory_fd = open(foreign_memory_path, O_RDWR | O_CLOEXEC);
  null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);
  readonly_memory_fd = open("/proc/self/mem", O_RDONLY | O_CLOEXEC);
  if (foreign_memory_fd < 0 || null_fd < 0 || readonly_memory_fd < 0 ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
                          MF_CLIENT_CONTROL_FLAG_WRITE,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(0), foreign_memory_fd,
                      MF_CLIENT_CONTROL_INVALID_ARGUMENT, &ignored, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
                          MF_CLIENT_CONTROL_FLAG_WRITE,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(0), null_fd,
                      MF_CLIENT_CONTROL_INVALID_ARGUMENT, &ignored, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
                          MF_CLIENT_CONTROL_FLAG_WRITE,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(0), readonly_memory_fd,
                      MF_CLIENT_CONTROL_INVALID_ARGUMENT, &ignored, (int32_t*)0)) {
    result = 3;
    goto cleanup;
  }
  (void)close(foreign_memory_fd);
  foreign_memory_fd = -1;
  (void)close(null_fd);
  null_fd = -1;
  (void)close(readonly_memory_fd);
  readonly_memory_fd = -1;
  {
    const char release = 'x';
    if (write(child_release_pipe[1], &release, sizeof(release)) != (ssize_t)sizeof(release)) {
      result = 3;
      goto cleanup;
    }
  }
  (void)close(child_release_pipe[1]);
  child_release_pipe[1] = -1;
  if (!wait_for_child(foreign_memory_child)) {
    result = 3;
    goto cleanup;
  }
  foreign_memory_child = -1;

  memory_fd = open("/proc/self/mem", O_RDWR | O_CLOEXEC);
  if (memory_fd < 0 ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
                          MF_CLIENT_CONTROL_FLAG_WRITE,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(0), memory_fd, MF_CLIENT_CONTROL_OK,
                      &ignored, (int32_t*)0)) {
    result = 4;
    goto cleanup;
  }
  (void)close(memory_fd);
  memory_fd = -1;

  duplicate_memory_fd = open("/proc/self/mem", O_RDWR | O_CLOEXEC);
  if (duplicate_memory_fd < 0 ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
                          MF_CLIENT_CONTROL_FLAG_WRITE,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(0), duplicate_memory_fd,
                      MF_CLIENT_CONTROL_INVALID_ARGUMENT, &ignored, (int32_t*)0)) {
    result = 5;
    goto cleanup;
  }
  (void)close(duplicate_memory_fd);
  duplicate_memory_fd = -1;

  if (!submit_direct_copy(session, request_id++, MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1,
                          device_memory, (uint64_t)(uintptr_t)(source + 5), UINT64_C(7),
                          UINT64_C(16), MF_SHARED_SUCCESS) ||
      !submit_direct_copy(session, request_id++, MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1,
                          device_memory, (uint64_t)(uintptr_t)(destination + 9), UINT64_C(7),
                          UINT64_C(16), MF_SHARED_SUCCESS) ||
      memcmp(destination + 9, source + 5, (size_t)16) != 0) {
    result = 6;
    goto cleanup;
  }
  for (index = 0; index < (uint32_t)sizeof(destination); ++index) {
    if ((index < UINT32_C(9) || index >= UINT32_C(25)) && destination[index] != UINT8_C(0)) {
      result = 7;
      goto cleanup;
    }
  }
  if (!submit_direct_copy(session, request_id++, MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1,
                          device_memory, (uint64_t)(uintptr_t)source, UINT64_C(60), UINT64_C(8),
                          MF_SHARED_INVALID_ARGUMENT) ||
      !submit_direct_copy(session, request_id++, MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1,
                          device_memory, UINT64_MAX - UINT64_C(3), UINT64_C(0), UINT64_C(8),
                          MF_SHARED_INVALID_ARGUMENT) ||
      !submit_direct_copy(session, request_id++,
                          MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 |
                              MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1,
                          device_memory, (uint64_t)(uintptr_t)source, UINT64_C(0), UINT64_C(8),
                          MF_SHARED_MALFORMED)) {
    result = 8;
    goto cleanup;
  }
  if (!control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, 0U, device_memory.id,
                      device_memory.generation, -1, MF_CLIENT_CONTROL_OK, &ignored, (int32_t*)0)) {
    result = 9;
    goto cleanup;
  }
  device_memory.id = 0U;
  device_memory.generation = 0U;

cleanup:
  if (foreign_memory_fd >= 0) {
    (void)close(foreign_memory_fd);
  }
  if (child_release_pipe[0] >= 0) {
    (void)close(child_release_pipe[0]);
  }
  if (child_release_pipe[1] >= 0) {
    const char release = 'x';
    if (foreign_memory_child > 0) {
      (void)write(child_release_pipe[1], &release, sizeof(release));
    }
    (void)close(child_release_pipe[1]);
  }
  if (foreign_memory_child > 0) {
    (void)wait_for_child(foreign_memory_child);
  }
  if (duplicate_memory_fd >= 0) {
    (void)close(duplicate_memory_fd);
  }
  if (memory_fd >= 0) {
    (void)close(memory_fd);
  }
  if (readonly_memory_fd >= 0) {
    (void)close(readonly_memory_fd);
  }
  if (null_fd >= 0) {
    (void)close(null_fd);
  }
  if (device_memory.id != 0U) {
    (void)control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, 0U, device_memory.id,
                         device_memory.generation, -1, MF_CLIENT_CONTROL_OK, &ignored, (int32_t*)0);
  }
  *next_request_id = request_id;
  return result;
}

static pid_t spawn_standalone(const char* daemon_path, const char* socket_path) {
  const pid_t child = fork();
  if (child == 0) {
    execl(daemon_path, daemon_path, "--socket", socket_path, (char*)0);
    _exit(127);
  }
  return child;
}

static int create_listener(const char* socket_path) {
  const size_t path_length = strlen(socket_path);
  struct sockaddr_un address;
  int listener = -1;
  socklen_t address_size = 0;
  if (path_length == (size_t)0 || path_length >= sizeof(address.sun_path)) {
    return -1;
  }
  listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (listener < 0) {
    return -1;
  }
  (void)memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  (void)memcpy(address.sun_path, socket_path, path_length + (size_t)1);
  address_size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_length + (size_t)1);
  if (bind(listener, (const struct sockaddr*)&address, address_size) != 0 ||
      listen(listener, 16) != 0) {
    (void)close(listener);
    return -1;
  }
  return listener;
}

static pid_t spawn_activated(const char* daemon_path, int listener) {
  const pid_t child = fork();
  if (child == 0) {
    char pid_text[32];
    int descriptor_flags = 0;
    if (listener != 3 && dup2(listener, 3) != 3) {
      _exit(126);
    }
    if (listener != 3) {
      (void)close(listener);
    }
    descriptor_flags = fcntl(3, F_GETFD);
    if (descriptor_flags < 0 || fcntl(3, F_SETFD, descriptor_flags & ~FD_CLOEXEC) != 0 ||
        snprintf(pid_text, sizeof(pid_text), "%ld", (long)getpid()) <= 0 ||
        setenv("LISTEN_PID", pid_text, 1) != 0 || setenv("LISTEN_FDS", "1", 1) != 0) {
      _exit(126);
    }
    execl(daemon_path, daemon_path, (char*)0);
    _exit(127);
  }
  return child;
}

static int wait_for_terminal_registry(mf_client_registry_v1* registry,
                                      const mf_generation_handle_v1* handle) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    mf_client_fence_snapshot_v1 fence;
    const mf_shared_status_v1 status =
        mf_client_registry_validate_device_v1(registry, handle, &fence);
    if (status == MF_SHARED_TERMINAL_VIEW) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int registry_remains_live(mf_client_registry_v1* registry,
                                 const mf_generation_handle_v1* handle) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    mf_client_fence_snapshot_v1 fence;
    const mf_shared_status_v1 status =
        mf_client_registry_validate_device_v1(registry, handle, &fence);
    if (status == MF_SHARED_SUCCESS) {
      return 1;
    }
    if (status != MF_SHARED_RETRY) {
      return 0;
    }
    short_pause();
  }
  return 0;
}

static int run_execution_session(mf_client_session_v1* session,
                                 mf_client_registry_v1* stale_registry,
                                 mf_generation_handle_v1* stale_handle) {
  static const uint32_t left_values[9] = {0x11111111U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  static const uint32_t right_values[9] = {0x22222222U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
  static const uint32_t zero_values[9] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
  mf_client_payload_v1 host_left = {.owned_fd = -1};
  mf_client_payload_v1 host_right = {.owned_fd = -1};
  mf_client_payload_v1 host_output = {.owned_fd = -1};
  mf_client_payload_v1 artifact_payload = {.owned_fd = -1};
  mf_client_payload_v1 argument_payload = {.owned_fd = -1};
  mf_client_payload_v1 invalid_argument_payload = {.owned_fd = -1};
  mf_client_payload_v1 copy_argument_payload = {.owned_fd = -1};
  mf_client_payload_v1 invalid_copy_argument_payload = {.owned_fd = -1};
  object_ref left_host = {0, 0};
  object_ref right_host = {0, 0};
  object_ref output_host = {0, 0};
  object_ref left_device = {0, 0};
  object_ref right_device = {0, 0};
  object_ref output_device = {0, 0};
  object_ref artifact = {0, 0};
  object_ref argument_block = {0, 0};
  object_ref invalid_argument_block = {0, 0};
  object_ref copy_argument = {0, 0};
  object_ref invalid_copy_argument = {0, 0};
  object_ref ignored = {0, 0};
  object_ref module = {0, 0};
  mf_client_completion_v1 completion;
  mf_client_telemetry_snapshot_v1 telemetry;
  add_argument_block arguments;
  _Alignas(64) copy_argument_block copy_arguments;
  _Alignas(64) copy_argument_block invalid_copy_arguments;
  uint64_t request_id = UINT64_C(100);
  int32_t resolved_artifact = -1;
  int result = 0;

  if (mf_client_registry_make_handle_v1(&session->registry, 0U, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                                        UINT64_C(1), MF_OBJECT_TYPE_CONTEXT,
                                        stale_handle) != MF_SHARED_SUCCESS ||
      mf_client_registry_attach_v1(mf_client_registry_borrow_fd_v1(&session->registry),
                                   session->registry_view_id,
                                   stale_registry) != MF_SHARED_SUCCESS) {
    result = 1;
    goto cleanup;
  }
  result = run_direct_host_copy(session, &request_id);
  if (result != 0) {
    result += 100;
    goto cleanup;
  }
  if (mf_client_payload_create_v1((const uint8_t*)left_values, sizeof(left_values), 0U,
                                  &host_left) != MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1((const uint8_t*)right_values, sizeof(right_values), 0U,
                                  &host_right) != MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1((const uint8_t*)zero_values, sizeof(zero_values),
                                  MF_CLIENT_PAYLOAD_WRITABLE_V1,
                                  &host_output) != MF_SHARED_SUCCESS) {
    result = 2;
    goto cleanup;
  }
  if (!control_object(session, MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(left_values), host_left.owned_fd,
                      MF_CLIENT_CONTROL_OK, &left_host, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(right_values), host_right.owned_fd,
                      MF_CLIENT_CONTROL_OK, &right_host, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_WRITE,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(zero_values), host_output.owned_fd,
                      MF_CLIENT_CONTROL_OK, &output_host, (int32_t*)0)) {
    result = 3;
    goto cleanup;
  }
  if (!control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1, 0U,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(left_values) + (size_t)1, -1,
                      MF_CLIENT_CONTROL_OK, &left_device, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1, 0U,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(right_values) + (size_t)1, -1,
                      MF_CLIENT_CONTROL_OK, &right_device, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1, 0U,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(zero_values) + (size_t)1, -1,
                      MF_CLIENT_CONTROL_OK, &output_device, (int32_t*)0)) {
    result = 4;
    goto cleanup;
  }
  if (mf_client_submit_copy_v1(&session->submission, request_id, left_device.id,
                               left_device.generation, left_host.id, left_host.generation,
                               sizeof(left_values), 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion) ||
      mf_client_submit_copy_v1(&session->submission, request_id, right_device.id,
                               right_device.generation, right_host.id, right_host.generation,
                               sizeof(right_values), 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion)) {
    result = 5;
    goto cleanup;
  }
  if ((session->negotiated_capabilities & MF_CLIENT_CAP_COPY_REGION_V1) == UINT64_C(0)) {
    result = 14;
    goto cleanup;
  }
  initialize_copy_argument_block(&copy_arguments, output_device, UINT64_C(33), left_device,
                                 UINT64_C(1), UINT64_C(3));
  initialize_copy_argument_block(&invalid_copy_arguments, output_device, UINT64_C(36), left_device,
                                 UINT64_C(0), UINT64_C(2));
  if (mf_client_copy_region_argument_block_validate_v1(
          (const uint8_t*)&copy_arguments, MF_TEST_COPY_ARGUMENT_SIZE) != MF_SHARED_SUCCESS ||
      mf_client_copy_region_argument_block_validate_v1((const uint8_t*)&invalid_copy_arguments,
                                                       MF_TEST_COPY_ARGUMENT_SIZE) !=
          MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1((const uint8_t*)&copy_arguments, MF_TEST_COPY_ARGUMENT_SIZE, 0U,
                                  &copy_argument_payload) != MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1((const uint8_t*)&invalid_copy_arguments,
                                  MF_TEST_COPY_ARGUMENT_SIZE, 0U,
                                  &invalid_copy_argument_payload) != MF_SHARED_SUCCESS ||
      !control_object(session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                      MF_TEST_COPY_ARGUMENT_SIZE, copy_argument_payload.owned_fd,
                      MF_CLIENT_CONTROL_OK, &copy_argument, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                      MF_TEST_COPY_ARGUMENT_SIZE, invalid_copy_argument_payload.owned_fd,
                      MF_CLIENT_CONTROL_OK, &invalid_copy_argument, (int32_t*)0)) {
    result = 15;
    goto cleanup;
  }
  if (mf_client_submit_copy_region_v1(&session->submission, request_id, copy_argument.id,
                                      copy_argument.generation) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion)) {
    result = 16;
    goto cleanup;
  }
  if (mf_client_submit_copy_v1(&session->submission, request_id, output_host.id,
                               output_host.generation, output_device.id, output_device.generation,
                               sizeof(zero_values), 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion) ||
      memcmp((const uint8_t*)host_output.mapping + (size_t)33,
             (const uint8_t*)left_values + (size_t)1, (size_t)3) != 0) {
    result = 17;
    goto cleanup;
  }
  if (mf_client_submit_copy_region_v1(&session->submission, request_id, invalid_copy_argument.id,
                                      invalid_copy_argument.generation) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_INVALID_ARGUMENT, &completion)) {
    result = 18;
    goto cleanup;
  }

  if (mf_client_payload_create_v1((const uint8_t*)add_ptx, sizeof(add_ptx) - (size_t)1, 0U,
                                  &artifact_payload) != MF_SHARED_SUCCESS ||
      !control_object(session, MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX,
                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(add_ptx) - (size_t)1,
                      artifact_payload.owned_fd, MF_CLIENT_CONTROL_OK, &artifact, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1, 0U, artifact.id,
                      artifact.generation, -1, MF_CLIENT_CONTROL_OK, &ignored,
                      &resolved_artifact) ||
      resolved_artifact < 0) {
    result = 6;
    goto cleanup;
  }
  (void)close(resolved_artifact);
  resolved_artifact = -1;
  if (mf_client_submit_module_load_v1(&session->submission, request_id, artifact.id,
                                      artifact.generation, 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion)) {
    result = 7;
    goto cleanup;
  }
  module.id = completion.result_id;
  module.generation = completion.result_generation;

  (void)memset(&arguments, 0, sizeof(arguments));
  arguments.header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
  arguments.header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  arguments.header.header_size = (uint32_t)sizeof(arguments.header);
  arguments.header.entry_size = (uint32_t)sizeof(arguments.entries[0]);
  arguments.header.entry_count = UINT32_C(4);
  arguments.header.total_size = sizeof(arguments);
  arguments.entries[0].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments.entries[0].flags = MF_ARGUMENT_BUFFER_WRITE;
  arguments.entries[0].object_id = output_device.id;
  arguments.entries[0].object_generation = output_device.generation;
  arguments.entries[0].value = sizeof(uint32_t);
  arguments.entries[1].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments.entries[1].flags = MF_ARGUMENT_BUFFER_READ;
  arguments.entries[1].object_id = left_device.id;
  arguments.entries[1].object_generation = left_device.generation;
  arguments.entries[1].value = sizeof(uint32_t);
  arguments.entries[2].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments.entries[2].flags = MF_ARGUMENT_BUFFER_READ;
  arguments.entries[2].object_id = right_device.id;
  arguments.entries[2].object_generation = right_device.generation;
  arguments.entries[2].value = sizeof(uint32_t);
  arguments.entries[3].kind = MF_ARGUMENT_KIND_U32;
  arguments.entries[3].value = UINT64_C(8);
  if (mf_client_argument_block_validate_v1((const uint8_t*)&arguments, sizeof(arguments)) !=
          MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1((const uint8_t*)&arguments, sizeof(arguments), 0U,
                                  &argument_payload) != MF_SHARED_SUCCESS ||
      !control_object(session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                      sizeof(arguments), argument_payload.owned_fd, MF_CLIENT_CONTROL_OK,
                      &argument_block, (int32_t*)0) ||
      mf_client_submit_launch_v1(&session->submission, request_id, module.id, module.generation,
                                 MF_KERNEL_PRIMARY_ENTRY_ID, argument_block.id,
                                 argument_block.generation, 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion)) {
    result = 8;
    goto cleanup;
  }
  if (mf_client_submit_copy_v1(&session->submission, request_id, output_host.id,
                               output_host.generation, output_device.id, output_device.generation,
                               sizeof(zero_values), 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_SUCCESS, &completion)) {
    result = 9;
    goto cleanup;
  }
  {
    const uint32_t* output = (const uint32_t*)host_output.mapping;
    uint32_t index = 0;
    if (output[0] != UINT32_C(0)) {
      result = 10;
      goto cleanup;
    }
    for (index = UINT32_C(1); index < UINT32_C(9); ++index) {
      if (output[index] != left_values[index] + right_values[index]) {
        result = 10;
        goto cleanup;
      }
    }
  }
  arguments.entries[0].value = sizeof(zero_values) + sizeof(uint32_t);
  if (mf_client_payload_create_v1((const uint8_t*)&arguments, sizeof(arguments), 0U,
                                  &invalid_argument_payload) != MF_SHARED_SUCCESS ||
      !control_object(session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1,
                      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                      sizeof(arguments), invalid_argument_payload.owned_fd, MF_CLIENT_CONTROL_OK,
                      &invalid_argument_block, (int32_t*)0) ||
      mf_client_submit_launch_v1(&session->submission, request_id, module.id, module.generation,
                                 MF_KERNEL_PRIMARY_ENTRY_ID, invalid_argument_block.id,
                                 invalid_argument_block.generation, 0U) != MF_SHARED_SUCCESS ||
      !wait_completion(session, request_id++, MF_SHARED_INVALID_ARGUMENT, &completion)) {
    result = 11;
    goto cleanup;
  }
  if (mf_client_registry_read_telemetry_v1(&session->registry, stale_handle, &telemetry) !=
          MF_SHARED_SUCCESS ||
      telemetry.completed_work_items < UINT64_C(4) ||
      telemetry.memory_active_time_ns == UINT64_C(0) ||
      telemetry.memory_used_bytes == UINT64_C(0)) {
    result = 12;
    goto cleanup;
  }
  if (!control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, 0U, output_device.id,
                      output_device.generation, -1, MF_CLIENT_CONTROL_OK, &ignored, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, 0U, output_device.id,
                      output_device.generation, -1, MF_CLIENT_CONTROL_STALE_GENERATION, &ignored,
                      (int32_t*)0)) {
    result = 13;
    goto cleanup;
  }
  if (!control_object(session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, 0U, copy_argument.id,
                      copy_argument.generation, -1, MF_CLIENT_CONTROL_OK, &ignored, (int32_t*)0) ||
      !control_object(session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, 0U,
                      invalid_copy_argument.id, invalid_copy_argument.generation, -1,
                      MF_CLIENT_CONTROL_OK, &ignored, (int32_t*)0)) {
    result = 19;
    goto cleanup;
  }

cleanup:
  if (resolved_artifact >= 0) {
    (void)close(resolved_artifact);
  }
  mf_client_payload_close_v1(&invalid_argument_payload);
  mf_client_payload_close_v1(&argument_payload);
  mf_client_payload_close_v1(&invalid_copy_argument_payload);
  mf_client_payload_close_v1(&copy_argument_payload);
  mf_client_payload_close_v1(&artifact_payload);
  mf_client_payload_close_v1(&host_output);
  mf_client_payload_close_v1(&host_right);
  mf_client_payload_close_v1(&host_left);
  return result;
}

static int run_standalone(const char* daemon_path, const char* socket_path) {
  mf_client_session_v1 first_session;
  mf_client_session_v1 second_session;
  mf_client_registry_v1 first_stale_registry = {.owned_fd = -1};
  mf_client_registry_v1 second_stale_registry = {.owned_fd = -1};
  mf_generation_handle_v1 first_handle;
  mf_generation_handle_v1 second_handle;
  mf_registry_view_id_v1 first_view;
  pid_t child = spawn_standalone(daemon_path, socket_path);
  int result = 0;
  int first_connected = 0;
  int second_connected = 0;
  if (child <= 0 || !connect_default_with_retry(socket_path, &first_session)) {
    result = 1;
    goto cleanup;
  }
  first_connected = 1;
  first_view = first_session.registry_view_id;
  result = run_execution_session(&first_session, &first_stale_registry, &first_handle);
  if (result != 0) {
    result += 10;
    goto cleanup;
  }
  if (!copy_region_register_requires_capability(socket_path)) {
    result = 29;
    goto cleanup;
  }
  mf_client_session_close_v1(&first_session);
  first_connected = 0;
  if (!registry_remains_live(&first_stale_registry, &first_handle) ||
      !connect_default_with_retry(socket_path, &second_session)) {
    result = 30;
    goto cleanup;
  }
  second_connected = 1;
  if (mf_registry_view_id_equal_v1(first_view, second_session.registry_view_id) == 0 ||
      mf_client_registry_make_handle_v1(
          &second_session.registry, 0U, MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1),
          MF_OBJECT_TYPE_CONTEXT, &second_handle) != MF_SHARED_SUCCESS ||
      mf_client_registry_attach_v1(mf_client_registry_borrow_fd_v1(&second_session.registry),
                                   second_session.registry_view_id,
                                   &second_stale_registry) != MF_SHARED_SUCCESS ||
      kill(child, SIGTERM) != 0 || !wait_for_child(child)) {
    result = 31;
    goto cleanup;
  }
  child = -1;
  if (!wait_for_terminal_registry(&first_stale_registry, &first_handle) ||
      !wait_for_terminal_registry(&second_stale_registry, &second_handle)) {
    result = 32;
    goto cleanup;
  }
  {
    struct stat attributes;
    if (lstat(socket_path, &attributes) == 0 || errno != ENOENT) {
      result = 33;
    }
  }

cleanup:
  if (first_connected) {
    mf_client_session_close_v1(&first_session);
  }
  if (second_connected) {
    mf_client_session_close_v1(&second_session);
  }
  mf_client_registry_close_v1(&second_stale_registry);
  mf_client_registry_close_v1(&first_stale_registry);
  if (child > 0) {
    (void)kill(child, SIGKILL);
    (void)waitpid(child, (int*)0, 0);
  }
  return result;
}

static int run_activation(const char* daemon_path, const char* socket_path) {
  mf_client_session_v1 session;
  int listener = create_listener(socket_path);
  pid_t child = -1;
  int connected = 0;
  int result = 0;
  if (listener < 0) {
    return 1;
  }
  child = spawn_activated(daemon_path, listener);
  (void)close(listener);
  if (child <= 0 || !connect_with_retry(socket_path, &session)) {
    result = 2;
    goto cleanup;
  }
  connected = 1;
  if (session.registry_view_id.daemon_incarnation == UINT64_C(0) || kill(child, SIGTERM) != 0 ||
      !wait_for_child(child)) {
    result = 3;
    goto cleanup;
  }
  child = -1;

cleanup:
  if (connected) {
    mf_client_session_close_v1(&session);
  }
  if (child > 0) {
    (void)kill(child, SIGKILL);
    (void)waitpid(child, (int*)0, 0);
  }
  (void)unlink(socket_path);
  return result;
}

int main(int argc, char** argv) {
  char directory_template[] = "/tmp/metafluxd-e2e-XXXXXX";
  char standalone_path[PATH_MAX];
  char activation_path[PATH_MAX];
  char* directory = (char*)0;
  int result = 0;
  if (argc != 2) {
    return 64;
  }
  directory = mkdtemp(directory_template);
  if (directory == (char*)0 ||
      snprintf(standalone_path, sizeof(standalone_path), "%s/standalone.sock", directory) <= 0 ||
      snprintf(activation_path, sizeof(activation_path), "%s/activated.sock", directory) <= 0) {
    return 1;
  }
  result = run_standalone(argv[1], standalone_path);
  if (result == 0) {
    result = run_activation(argv[1], activation_path);
    if (result != 0) {
      result += 100;
    }
  }
  (void)unlink(standalone_path);
  (void)unlink(activation_path);
  (void)rmdir(directory);
  return result;
}
