#define _GNU_SOURCE

#include "metaflux/transport/cdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum {
  MF_CDEV_TEST_SKIP = 77,
  MF_CDEV_TEST_PAGE_SIZE = 4096,
  MF_CDEV_TEST_REGISTER_BYTES = 1024,
};

static int failf(const char* step, const char* detail) {
  (void)fprintf(stderr, "cdev qualification: FAIL: %s (%s)\n", step, detail);
  return 1;
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

  result_code = 0;

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
