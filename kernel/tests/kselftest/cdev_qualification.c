#define _GNU_SOURCE

#include "metaflux/client/protocol.h"
#include "metaflux/transport/cdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef METAFLUX_DAEMON_EXECUTABLE
#define METAFLUX_DAEMON_EXECUTABLE ""
#endif
#ifndef METAFLUX_DAEMON_LIBRARY_PATH
#define METAFLUX_DAEMON_LIBRARY_PATH ""
#endif

enum {
  MF_CDEV_TEST_SKIP = 77,
  MF_CDEV_TEST_PAGE_SIZE = 4096,
  MF_CDEV_TEST_REGISTER_BYTES = 1024,
};

static int failf(const char* step, const char* detail) {
  (void)fprintf(stderr, "cdev qualification: FAIL: %s (%s)\n", step, detail);
  return 1;
}

static void short_pause(void) {
  const struct timespec duration = {.tv_sec = 0, .tv_nsec = 10000000};
  (void)nanosleep(&duration, NULL);
}

static int wait_for_child(pid_t child) {
  uint32_t attempt = 0U;
  for (attempt = 0U; attempt < UINT32_C(500); ++attempt) {
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
  (void)waitpid(child, NULL, 0);
  return 0;
}

static int control_ok(mf_client_session_v1* session, uint16_t opcode, uint16_t flags,
                      uint64_t object_id, uint64_t argument, int32_t payload_fd,
                      uint64_t* out_id, uint64_t* out_generation, uint32_t* out_status) {
  mf_client_control_response_v1 response;
  int32_t received_fd = -1;
  const mf_shared_status_v1 transport =
      mf_client_session_control_v1(session, opcode, flags, object_id, argument, payload_fd,
                                   &response, &received_fd);
  if (received_fd >= 0) {
    (void)close(received_fd);
  }
  if (transport != MF_SHARED_SUCCESS) {
    if (out_status != NULL) {
      *out_status = (uint32_t)transport;
    }
    return 0;
  }
  {
    const uint32_t status = mf_client_load_le32_v1(response.bytes + 12);
    if (out_status != NULL) {
      *out_status = status;
    }
    if (status != MF_CLIENT_CONTROL_OK) {
      return 0;
    }
  }
  if (out_id != NULL) {
    *out_id = mf_client_load_le64_v1(response.bytes + 48);
  }
  if (out_generation != NULL) {
    *out_generation = mf_client_load_le64_v1(response.bytes + 56);
  }
  return 1;
}

static int wait_completion(mf_client_session_v1* session, uint64_t request_id,
                           mf_shared_status_v1 expected) {
  mf_client_completion_v1 completion;
  uint32_t attempt = 0U;
  for (attempt = 0U; attempt < UINT32_C(500); ++attempt) {
    const mf_shared_status_v1 status =
        mf_client_try_consume_completion_v1(&session->completion, &completion);
    if (status == MF_SHARED_SUCCESS) {
      return completion.request_id == request_id && completion.status == expected;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      return 0;
    }
    (void)mf_client_ring_wait_readable_v1(&session->completion, UINT64_C(10000000));
  }
  return 0;
}

/*
 * Prove daemon object-table COPY after CDEV_BIND. Requires METAFLUX_DAEMON_EXECUTABLE
 * and root-accessible /dev/metaflux{ctl,0}. Skips when the daemon binary is unset.
 */
static int prove_daemon_cdev_add_copy(void) {
  const char* daemon_path = METAFLUX_DAEMON_EXECUTABLE;
  char directory_template[] = "/tmp/metaflux-cdev-live-XXXXXX";
  char socket_path[256];
  char* directory = NULL;
  pid_t child = -1;
  mf_client_session_v1 session;
  mf_virtual_device_identity_v1 identity;
  mf_client_payload_v1 host_source = {.owned_fd = -1};
  mf_client_payload_v1 host_destination = {.owned_fd = -1};
  mf_client_payload_v1 argument_payload = {.owned_fd = -1};
  uint8_t source_bytes[64];
  uint8_t destination_bytes[64];
  uint64_t source_id = 0U;
  uint64_t source_generation = 0U;
  uint64_t destination_id = 0U;
  uint64_t destination_generation = 0U;
  uint64_t argument_id = 0U;
  uint64_t argument_generation = 0U;
  uint64_t request_id = UINT64_C(900);
  uint32_t index = 0U;
  int connected = 0;
  int result = 1;
  struct {
    mf_argument_block_header_v1 header;
    mf_argument_entry_v1 entries[4];
  } arguments;

  if (daemon_path == NULL || daemon_path[0] == '\0') {
    (void)fprintf(stdout, "cdev qualification: daemon Add/Copy skipped (no daemon binary)\n");
    return 0;
  }

  for (index = 0U; index < (uint32_t)sizeof(source_bytes); ++index) {
    source_bytes[index] = (uint8_t)(index * UINT32_C(5) + UINT32_C(7));
  }
  (void)memset(destination_bytes, 0, sizeof(destination_bytes));

  directory = mkdtemp(directory_template);
  if (directory == NULL ||
      snprintf(socket_path, sizeof(socket_path), "%s/daemon.sock", directory) <= 0) {
    return failf("daemon live setup", "temp directory");
  }

  child = fork();
  if (child == 0) {
    /*
     * driver-live clears the environment. Prefer the compile-time Nix library
     * path so the build-tree daemon resolves zlib/libstdcxx without host libc.
     */
    if (METAFLUX_DAEMON_LIBRARY_PATH[0] != '\0') {
      (void)setenv("LD_LIBRARY_PATH", METAFLUX_DAEMON_LIBRARY_PATH, 1);
    }
    (void)setenv("PATH", "/usr/bin:/bin", 1);
    execl(daemon_path, daemon_path, "--socket", socket_path, (char*)0);
    _exit(127);
  }
  if (child < 0) {
    result = failf("daemon live spawn", strerror(errno));
    goto cleanup;
  }

  {
    uint32_t attempt = 0U;
    const uint64_t required =
        MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
        MF_CLIENT_CAP_FUTEX_DOORBELL_V1 | MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1;
    const uint64_t optional = MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1 |
                              MF_CLIENT_CAP_COPY_REGION_V1 | MF_CLIENT_CAP_CDEV_BINDING_V1 |
                              MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
    for (attempt = 0U; attempt < UINT32_C(500); ++attempt) {
      if (mf_client_session_connect_capabilities_v1(socket_path, required, optional, &session) ==
          MF_SHARED_SUCCESS) {
        connected = 1;
        break;
      }
      short_pause();
    }
  }
  if (!connected) {
    result = failf("daemon live connect", "session connect timed out");
    goto cleanup;
  }
  if ((session.negotiated_capabilities & MF_CLIENT_CAP_CDEV_BINDING_V1) == 0U ||
      (session.negotiated_capabilities & MF_CLIENT_CAP_COPY_REGION_V1) == 0U) {
    result = failf("daemon live negotiate", "cdev binding or copy-region capability missing");
    goto cleanup;
  }
  if (mf_client_registry_identity_v1(&session.registry, 0U, &identity) != MF_SHARED_SUCCESS ||
      identity.committed_generation == 0U) {
    result = failf("daemon live identity", "missing committed generation");
    goto cleanup;
  }
  {
    uint32_t bind_status = UINT32_MAX;
    char detail[96];
    if (!control_ok(&session, MF_CLIENT_CONTROL_CDEV_BIND_V1, UINT16_C(0),
                    MF_CLIENT_RUNTIME_CONTEXT_ID_V1, identity.committed_generation, -1, NULL, NULL,
                    &bind_status)) {
      (void)snprintf(detail, sizeof(detail), "CDEV_BIND control status %u generation %llu",
                     bind_status, (unsigned long long)identity.committed_generation);
      result = failf("daemon live cdev bind", detail);
      goto cleanup;
    }
  }

  if (mf_client_payload_create_v1(source_bytes, sizeof(source_bytes), 0U, &host_source) !=
          MF_SHARED_SUCCESS ||
      mf_client_payload_create_v1(destination_bytes, sizeof(destination_bytes),
                                  MF_CLIENT_PAYLOAD_WRITABLE_V1, &host_destination) !=
          MF_SHARED_SUCCESS) {
    result = failf("daemon live payloads", "payload create failed");
    goto cleanup;
  }
  if (!control_ok(&session, MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                  MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
                  MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(source_bytes), host_source.owned_fd,
                  &source_id, &source_generation, NULL) ||
      !control_ok(&session, MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                  MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_WRITE,
                  MF_CLIENT_RUNTIME_CONTEXT_ID_V1, sizeof(destination_bytes),
                  host_destination.owned_fd, &destination_id, &destination_generation, NULL)) {
    result = failf("daemon live register", "host memory register failed");
    goto cleanup;
  }

  (void)memset(&arguments, 0, sizeof(arguments));
  arguments.header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
  arguments.header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  arguments.header.header_size = (uint32_t)sizeof(arguments.header);
  arguments.header.entry_size = (uint32_t)sizeof(arguments.entries[0]);
  arguments.header.entry_count = MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1;
  arguments.header.flags = MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1;
  arguments.header.total_size =
      sizeof(mf_argument_block_header_v1) +
      (size_t)MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 * sizeof(mf_argument_entry_v1);
  arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags = MF_ARGUMENT_BUFFER_WRITE;
  arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_id = destination_id;
  arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_generation = destination_generation;
  arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].value = UINT64_C(0);
  arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].flags = MF_ARGUMENT_BUFFER_READ;
  arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_id = source_id;
  arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_generation = source_generation;
  arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].value = UINT64_C(0);
  arguments.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].kind = MF_ARGUMENT_KIND_U64;
  arguments.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = sizeof(source_bytes);

  if (mf_client_payload_create_v1((const uint8_t*)&arguments, arguments.header.total_size, 0U,
                                  &argument_payload) != MF_SHARED_SUCCESS ||
      !control_ok(&session, MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1,
                  MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, MF_CLIENT_RUNTIME_CONTEXT_ID_V1,
                  arguments.header.total_size, argument_payload.owned_fd, &argument_id,
                  &argument_generation, NULL)) {
    result = failf("daemon live argument block", "register failed");
    goto cleanup;
  }
  if (mf_client_submit_copy_region_v1(&session.submission, request_id, argument_id,
                                      argument_generation) != MF_SHARED_SUCCESS ||
      !wait_completion(&session, request_id, MF_SHARED_SUCCESS)) {
    result = failf("daemon live region copy", "copy completion failed");
    goto cleanup;
  }
  if (memcmp(host_destination.mapping, source_bytes, sizeof(source_bytes)) != 0) {
    result = failf("daemon live region copy", "destination bytes mismatch");
    goto cleanup;
  }

  (void)fprintf(stdout,
                "cdev qualification: daemon CDEV_BIND + registered-memory region COPY: PASS\n");
  result = 0;

cleanup:
  mf_client_payload_close_v1(&argument_payload);
  mf_client_payload_close_v1(&host_destination);
  mf_client_payload_close_v1(&host_source);
  if (connected) {
    mf_client_session_close_v1(&session);
  }
  if (child > 0) {
    (void)kill(child, SIGTERM);
    (void)wait_for_child(child);
  }
  if (directory != NULL) {
    (void)unlink(socket_path);
    (void)rmdir(directory);
  }
  return result;
}

static int expect_errno(int fd, unsigned long request, void* argument, int expected,
                        const char* step) {
  errno = 0;
  if (ioctl(fd, request, argument) != -1 || errno != expected) {
    char detail[96];
    (void)snprintf(detail, sizeof(detail), "expected errno %d, got %d", expected, errno);
    return failf(step, detail);
  }
  return 0;
}

static int validate_ring_mapping(const mf_ring_header_v1* submission,
                                 const mf_ring_header_v1* completion,
                                 const mf_cdev_session_v0* session) {
  if (submission == NULL || completion == NULL || session == NULL ||
      submission->metadata.magic != MF_SHARED_RING_MAGIC ||
      completion->metadata.magic != MF_SHARED_RING_MAGIC ||
      submission->metadata.abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      completion->metadata.abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      submission->metadata.capacity != MF_CDEV_RING_CAPACITY_V0 ||
      completion->metadata.capacity != MF_CDEV_RING_CAPACITY_V0 ||
      submission->metadata.queue_id != session->queue_id ||
      completion->metadata.queue_id != session->queue_id + UINT64_C(1) ||
      submission->metadata.mapping_size == 0U ||
      (submission->metadata.mapping_size % MF_CDEV_TEST_PAGE_SIZE) != 0U ||
      submission->metadata.mapping_size != session->submission.mapping_size ||
      completion->metadata.mapping_size != session->completion.mapping_size ||
      submission->metadata.queue_generation != session->device_generation ||
      completion->metadata.queue_generation != session->device_generation ||
      !mf_registry_view_id_equal_v1(submission->metadata.registry_view_id,
                                    session->registry_view_id) ||
      !mf_registry_view_id_equal_v1(completion->metadata.registry_view_id,
                                    session->registry_view_id)) {
    return failf("validate ring mapping", "metadata mismatch");
  }
  return 0;
}

int main(void) {
  const char* device_path = getenv("METAFLUX_CDEV_PATH");
  const char* control_path = getenv("METAFLUX_CDEV_CONTROL_PATH");
  mf_cdev_session_v0 session = {.device_fd = -1};
  mf_cdev_memory_v0 payload = {.device_fd = -1};
  mf_cdev_memory_v0 registered_memory = {.device_fd = -1};
  mf_uapi_worker_lease_v0 lease;
  mf_uapi_negotiate_v0 control_negotiate;
  mf_uapi_memory_v0 query;
  mf_uapi_memory_v0 stale_memory;
  mf_uapi_memory_v0 unregister_check;
  mf_uapi_negotiate_v0 malformed_negotiate;
  uint64_t unknown_ioctl_argument = 0U;
  struct pollfd poll_descriptor;
  mf_ring_header_v1* control_queue = MAP_FAILED;
  uint8_t* control_payload = MAP_FAILED;
  uint8_t* registered_base = NULL;
  int control_fd = -1;
  int kick_eventfd = -1;
  int completion_eventfd = -1;
  uint64_t registered_handle = 0U;
  int result_code = 1;
  int status;
  int open_session = 0;

  if (device_path == NULL)
    device_path = MF_CDEV_DEFAULT_PATH_V0;
  if (control_path == NULL)
    control_path = MF_CDEV_DEFAULT_CONTROL_PATH_V0;

  status = mf_cdev_session_open_v0(device_path, &session);
  if (status == MF_SHARED_NOT_SUPPORTED) {
    (void)fprintf(stdout, "cdev qualification: SKIP: %s is not available\n", device_path);
    return MF_CDEV_TEST_SKIP;
  }
  if (status == MF_SHARED_PERMISSION_DENIED) {
    (void)fprintf(stdout,
                  "cdev qualification: SKIP: %s requires elevated driver live privileges\n",
                  device_path);
    return MF_CDEV_TEST_SKIP;
  }
  if (status != MF_SHARED_SUCCESS) {
    (void)fprintf(stderr, "cdev qualification: session open status %d\n", status);
    return result_code;
  }
  open_session = 1;

  if (session.device_generation == 0U || session.queue_id == 0U || session.mapping == NULL ||
      session.mapping_size == 0U || validate_ring_mapping(session.submission.header,
                                                           session.completion.header,
                                                           &session) != 0)
    goto cleanup;

  poll_descriptor.fd = mf_cdev_borrow_fd_v0(&session);
  poll_descriptor.events = POLLIN;
  poll_descriptor.revents = 0;
  if (poll(&poll_descriptor, 1, 0) != 0 ||
      (poll_descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0U) {
    (void)failf("poll online queue", "queue reported an error or hangup");
    goto cleanup;
  }

  control_fd = open(control_path, O_RDWR | O_CLOEXEC);
  if (control_fd < 0) {
    (void)failf("open control device", strerror(errno));
    goto cleanup;
  }
  kick_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  completion_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (kick_eventfd < 0 || completion_eventfd < 0) {
    (void)failf("create lease eventfds", strerror(errno));
    goto cleanup;
  }

  (void)memset(&control_negotiate, 0, sizeof(control_negotiate));
  control_negotiate.struct_size = sizeof(control_negotiate);
  control_negotiate.version = MF_UAPI_VERSION_V0;
  control_negotiate.required_features = MF_UAPI_FEATURE_QUEUE_MMAP_V0;
  if (ioctl(control_fd, MF_UAPI_IOCTL_NEGOTIATE, &control_negotiate) != 0) {
    (void)failf("negotiate control device", strerror(errno));
    goto cleanup;
  }
  if (control_negotiate.struct_size != sizeof(control_negotiate) ||
      control_negotiate.version != MF_UAPI_VERSION_V0 ||
      (control_negotiate.required_features & MF_UAPI_FEATURE_QUEUE_MMAP_V0) == 0U ||
      control_negotiate.registry_view_daemon != session.registry_view_id.daemon_incarnation ||
      control_negotiate.registry_view_serial != session.registry_view_id.view_serial ||
      control_negotiate.device_generation != session.device_generation ||
      (control_negotiate.dma_width != UINT32_C(32) &&
       control_negotiate.dma_width != UINT32_C(64)) ||
      control_negotiate.dma_alignment != MF_CDEV_TEST_PAGE_SIZE) {
    (void)failf("validate control negotiation", "returned control view does not match data queue");
    goto cleanup;
  }

  (void)memset(&lease, 0, sizeof(lease));
  lease.struct_size = sizeof(lease);
  lease.daemon_incarnation = session.registry_view_id.daemon_incarnation;
  lease.registry_view_serial = session.registry_view_id.view_serial;
  lease.device_generation = session.device_generation;
  lease.kick_eventfd = kick_eventfd;
  lease.completion_eventfd = completion_eventfd;
  errno = 0;
  if (ioctl(control_fd, MF_UAPI_IOCTL_WORKER_LEASE, &lease) != 0) {
    if (errno == EBUSY) {
      (void)fprintf(stdout, "cdev qualification: SKIP: worker lease is busy\n");
      result_code = MF_CDEV_TEST_SKIP;
    } else {
      (void)failf("acquire worker lease", strerror(errno));
    }
    goto cleanup;
  }
  if (lease.device_generation != session.device_generation || lease.lease_id != session.queue_id ||
      lease.queue_mmap_offset != 0U || lease.queue_mapping_size != session.mapping_size ||
      lease.kick_eventfd != kick_eventfd || lease.completion_eventfd != completion_eventfd) {
    (void)failf("validate worker lease", "returned lease does not match data queue");
    goto cleanup;
  }
  (void)close(kick_eventfd);
  kick_eventfd = -1;
  (void)close(completion_eventfd);
  completion_eventfd = -1;

  control_queue = mmap(NULL, (size_t)lease.queue_mapping_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, control_fd, (off_t)lease.queue_mmap_offset);
  if (control_queue == MAP_FAILED) {
    (void)failf("map queue through lease", strerror(errno));
    goto cleanup;
  }
  if (validate_ring_mapping(control_queue,
                            (mf_ring_header_v1*)((uint8_t*)control_queue +
                                                 session.submission.mapping_size),
                            &session) != 0)
    goto cleanup;

  status = mf_cdev_memory_alloc_v0(&session, MF_CDEV_TEST_PAGE_SIZE,
                                   MF_CDEV_TEST_PAGE_SIZE, &payload);
  if (status != MF_SHARED_SUCCESS) {
    (void)fprintf(stderr, "cdev qualification: payload allocation status %d\n", status);
    goto cleanup;
  }
  if (payload.mapping == NULL || payload.byte_count < MF_CDEV_TEST_PAGE_SIZE ||
      payload.generation != session.device_generation)
    goto cleanup;
  (void)memset(payload.mapping, 0xA5, (size_t)payload.byte_count);

  (void)memset(&query, 0, sizeof(query));
  query.struct_size = sizeof(query);
  query.fd = -1;
  if (ioctl(control_fd, MF_UAPI_IOCTL_MEMORY_QUERY, &query) != 0) {
    (void)failf("query payload through lease", strerror(errno));
    goto cleanup;
  }
  if (query.handle != payload.handle || query.generation != payload.generation ||
      query.byte_count != payload.byte_count || query.offset == 0U ||
      (query.offset % MF_CDEV_TEST_PAGE_SIZE) != 0U || query.fd != -1) {
    (void)failf("validate payload query", "returned payload does not match allocation");
    goto cleanup;
  }
  control_payload = mmap(NULL, (size_t)query.byte_count, PROT_READ | PROT_WRITE, MAP_SHARED,
                         control_fd, (off_t)query.offset);
  if (control_payload == MAP_FAILED || control_payload[0] != UINT8_C(0xA5) ||
      control_payload[query.byte_count - 1U] != UINT8_C(0xA5)) {
    (void)failf("map payload through lease", "control mapping did not observe payload");
    goto cleanup;
  }

  if (posix_memalign((void**)&registered_base, MF_CDEV_TEST_PAGE_SIZE,
                     MF_CDEV_TEST_PAGE_SIZE * 2U) != 0) {
    (void)failf("allocate registered range", "posix_memalign failed");
    goto cleanup;
  }
  (void)memset(registered_base, 0x3C, MF_CDEV_TEST_PAGE_SIZE * 2U);
  status = mf_cdev_memory_register_v0(
      &session, registered_base + 128U, MF_CDEV_TEST_REGISTER_BYTES,
      MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0 | MF_CDEV_MEMORY_REGISTER_FLAG_WRITE_V0,
      &registered_memory);
  if (status == MF_SHARED_NOT_SUPPORTED) {
    (void)fprintf(stdout,
                  "cdev qualification: SKIP: registered-memory DMA target is unavailable\n");
    result_code = MF_CDEV_TEST_SKIP;
    goto cleanup;
  }
  if (status != MF_SHARED_SUCCESS) {
    (void)fprintf(stderr, "cdev qualification: registered-memory status %d\n", status);
    goto cleanup;
  }
  if (registered_memory.handle < UINT64_C(3) || registered_memory.handle > UINT64_C(6) ||
      registered_memory.generation != session.device_generation ||
      registered_memory.byte_count != MF_CDEV_TEST_REGISTER_BYTES) {
    (void)failf("validate registered range", "returned handle or range mismatch");
    goto cleanup;
  }
  registered_handle = registered_memory.handle;
  mf_cdev_memory_close_v0(&registered_memory);
  (void)memset(&unregister_check, 0, sizeof(unregister_check));
  unregister_check.struct_size = sizeof(unregister_check);
  unregister_check.handle = registered_handle;
  unregister_check.generation = session.device_generation;
  unregister_check.fd = -1;
  if (expect_errno(session.device_fd, MF_UAPI_IOCTL_MEMORY_REGISTER, &unregister_check, ENOENT,
                   "registered range unregister") != 0)
    goto cleanup;

  if (expect_errno(session.device_fd, _IO('M', 127), &unknown_ioctl_argument, ENOTTY,
                   "unknown ioctl") != 0)
    goto cleanup;

  (void)memset(&stale_memory, 0, sizeof(stale_memory));
  stale_memory.struct_size = sizeof(stale_memory);
  stale_memory.generation = session.device_generation == UINT64_MAX
                                ? UINT64_C(1)
                                : session.device_generation + UINT64_C(1);
  stale_memory.byte_count = MF_CDEV_TEST_PAGE_SIZE;
  stale_memory.alignment = MF_CDEV_TEST_PAGE_SIZE;
  stale_memory.fd = -1;
  if (expect_errno(session.device_fd, MF_UAPI_IOCTL_MEMORY_ALLOC, &stale_memory, EINVAL,
                   "stale generation allocation") != 0)
    goto cleanup;

  (void)memset(&malformed_negotiate, 0, sizeof(malformed_negotiate));
  malformed_negotiate.struct_size = sizeof(malformed_negotiate);
  malformed_negotiate.version = MF_UAPI_VERSION_V0;
  malformed_negotiate.required_features = MF_UAPI_FEATURE_QUEUE_MMAP_V0;
  malformed_negotiate.reserved[0] = UINT8_C(1);
  if (expect_errno(control_fd, MF_UAPI_IOCTL_NEGOTIATE, &malformed_negotiate, EINVAL,
                   "reserved-byte rejection") != 0)
    goto cleanup;

  /* Closing the data owner must leave the leased control mappings readable as tombstones. */
  mf_cdev_session_close_v0(&session);
  open_session = 0;
  if (control_queue == MAP_FAILED || control_queue->metadata.magic != MF_SHARED_RING_MAGIC ||
      control_queue->metadata.queue_generation == 0U || control_payload == MAP_FAILED ||
      control_payload[0] != UINT8_C(0xA5)) {
    (void)failf("owner-close tombstone", "leased VMA became unreadable");
    goto cleanup;
  }

  /*
   * Release the kernel-side data owner before the daemon proof so the static
   * cdev fixture can grant a fresh worker lease and payload arena.
   */
  if (control_payload != MAP_FAILED) {
    (void)munmap(control_payload, (size_t)query.byte_count);
    control_payload = MAP_FAILED;
  }
  if (control_queue != MAP_FAILED) {
    (void)munmap(control_queue, (size_t)lease.queue_mapping_size);
    control_queue = MAP_FAILED;
  }
  if (control_fd >= 0) {
    (void)close(control_fd);
    control_fd = -1;
  }
  mf_cdev_memory_close_v0(&payload);
  if (kick_eventfd >= 0) {
    (void)close(kick_eventfd);
    kick_eventfd = -1;
  }
  if (completion_eventfd >= 0) {
    (void)close(completion_eventfd);
    completion_eventfd = -1;
  }

  result_code = prove_daemon_cdev_add_copy();

cleanup:
  if (registered_memory.handle != 0U)
    mf_cdev_memory_close_v0(&registered_memory);
  free(registered_base);
  if (open_session)
    mf_cdev_session_close_v0(&session);
  mf_cdev_memory_close_v0(&payload);
  if (control_payload != MAP_FAILED)
    (void)munmap(control_payload, (size_t)query.byte_count);
  if (control_queue != MAP_FAILED)
    (void)munmap(control_queue, (size_t)lease.queue_mapping_size);
  if (kick_eventfd >= 0)
    (void)close(kick_eventfd);
  if (completion_eventfd >= 0)
    (void)close(completion_eventfd);
  if (control_fd >= 0)
    (void)close(control_fd);
  if (result_code == 0)
    (void)fprintf(stdout, "cdev qualification: PASS\n");
  return result_code;
}
