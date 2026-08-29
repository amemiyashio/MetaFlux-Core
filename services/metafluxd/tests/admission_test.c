#define _GNU_SOURCE

#include "metaflux/client/fastpath.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_UID_SESSION_LIMIT UINT32_C(16)
#define TEST_WAIT_ATTEMPTS UINT32_C(1000)
#define TEST_FAST_WAIT_ATTEMPTS UINT32_C(100)

static void short_pause(void) {
  struct timespec remaining = {.tv_sec = 0, .tv_nsec = 5000000};
  while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
  }
}

static int wait_for_child(pid_t child) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < TEST_WAIT_ATTEMPTS; ++attempt) {
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
  return 0;
}

static int connect_normal_with_retry(const char* path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < TEST_WAIT_ATTEMPTS; ++attempt) {
    if (mf_client_session_connect_v1(path, session) == MF_SHARED_SUCCESS) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int connect_silent(const char* path) {
  const size_t path_length = strlen(path);
  struct sockaddr_un address;
  socklen_t address_length = 0;
  int socket_fd = -1;
  if (path_length == 0U || path_length >= sizeof(address.sun_path)) {
    return -1;
  }
  (void)memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  (void)memcpy(address.sun_path, path, path_length + 1U);
  address_length = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_length + 1U);
  socket_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (socket_fd < 0 || connect(socket_fd, (const struct sockaddr*)&address, address_length) != 0) {
    if (socket_fd >= 0) {
      (void)close(socket_fd);
    }
    return -1;
  }
  return socket_fd;
}

static int count_tasks(pid_t daemon, size_t* out_count) {
  char task_path[64];
  struct dirent* entry = (struct dirent*)0;
  DIR* directory = (DIR*)0;
  size_t count = 0;
  if (snprintf(task_path, sizeof(task_path), "/proc/%ld/task", (long)daemon) <= 0) {
    return 0;
  }
  directory = opendir(task_path);
  if (directory == (DIR*)0) {
    return 0;
  }
  errno = 0;
  while ((entry = readdir(directory)) != (struct dirent*)0) {
    if (entry->d_name[0] != '.') {
      ++count;
    }
  }
  if (errno != 0 || closedir(directory) != 0 || count == 0U) {
    return 0;
  }
  *out_count = count;
  return 1;
}

static int wait_for_task_count(pid_t daemon, size_t expected, uint32_t maximum_attempts) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < maximum_attempts; ++attempt) {
    size_t actual = 0;
    if (!count_tasks(daemon, &actual)) {
      return 0;
    }
    if (actual == expected) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int wait_for_close_without_extra_worker(int socket_fd, pid_t daemon, size_t maximum_tasks) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < TEST_FAST_WAIT_ATTEMPTS; ++attempt) {
    struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN, .revents = 0};
    size_t task_count = 0;
    int poll_result = -1;
    if (!count_tasks(daemon, &task_count) || task_count > maximum_tasks) {
      return 0;
    }
    do {
      poll_result = poll(&descriptor, 1U, 5);
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

static int wait_for_close(int socket_fd) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < TEST_WAIT_ATTEMPTS; ++attempt) {
    struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN, .revents = 0};
    int poll_result = -1;
    do {
      poll_result = poll(&descriptor, 1U, 5);
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

static int context_round_trip(mf_client_session_v1* session) {
  mf_client_control_response_v1 response;
  if (mf_client_session_control_v1(session, MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1, UINT16_C(0),
                                   MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1), -1, &response,
                                   (int32_t*)0) != MF_SHARED_SUCCESS ||
      mf_client_load_le32_v1(response.bytes + 12) != MF_CLIENT_CONTROL_OK) {
    return 0;
  }
  return mf_client_session_control_v1(session, MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1, UINT16_C(0),
                                      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, UINT64_C(1), -1, &response,
                                      (int32_t*)0) == MF_SHARED_SUCCESS &&
         mf_client_load_le32_v1(response.bytes + 12) == MF_CLIENT_CONTROL_OK;
}

static int open_silent_set(const char* path, int* sockets, uint32_t count) {
  uint32_t index = 0;
  for (index = 0; index < count; ++index) {
    sockets[index] = connect_silent(path);
    if (sockets[index] < 0) {
      return 0;
    }
  }
  return 1;
}

static int close_timed_out_set(int* sockets, uint32_t count) {
  uint32_t index = 0;
  for (index = 0; index < count; ++index) {
    if (sockets[index] < 0 || !wait_for_close(sockets[index])) {
      return 0;
    }
    (void)close(sockets[index]);
    sockets[index] = -1;
  }
  return 1;
}

int main(int argc, char** argv) {
  char directory_template[] = "/tmp/metaflux-admission-XXXXXX";
  char socket_path[PATH_MAX];
  int silent[TEST_UID_SESSION_LIMIT];
  mf_client_session_v1 probe;
  mf_client_session_v1 survivor;
  mf_client_session_v1 replacement;
  pid_t daemon = -1;
  char* directory = (char*)0;
  size_t baseline_tasks = 0;
  size_t probe_tasks = 0;
  uint32_t silent_count = 0;
  int probe_connected = 0;
  int survivor_connected = 0;
  int replacement_connected = 0;
  int rejected = -1;
  int result = 1;
  uint32_t index = 0;

  for (index = 0; index < TEST_UID_SESSION_LIMIT; ++index) {
    silent[index] = -1;
  }
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
  if (daemon <= 0 || !connect_normal_with_retry(socket_path, &probe)) {
    goto cleanup;
  }
  probe_connected = 1;
  if (!count_tasks(daemon, &probe_tasks) || probe_tasks < 2U) {
    goto cleanup;
  }
  mf_client_session_close_v1(&probe);
  probe_connected = 0;
  baseline_tasks = probe_tasks - 1U;
  if (!wait_for_task_count(daemon, baseline_tasks, TEST_FAST_WAIT_ATTEMPTS)) {
    goto cleanup;
  }

  silent_count = TEST_UID_SESSION_LIMIT;
  if (!open_silent_set(socket_path, silent, silent_count) ||
      !wait_for_task_count(daemon, baseline_tasks + silent_count, TEST_FAST_WAIT_ATTEMPTS)) {
    goto cleanup;
  }
  rejected = connect_silent(socket_path);
  if (rejected < 0 ||
      !wait_for_close_without_extra_worker(rejected, daemon, baseline_tasks + silent_count)) {
    goto cleanup;
  }
  (void)close(rejected);
  rejected = -1;
  if (!close_timed_out_set(silent, silent_count) ||
      !wait_for_task_count(daemon, baseline_tasks, TEST_WAIT_ATTEMPTS) ||
      !connect_normal_with_retry(socket_path, &survivor)) {
    goto cleanup;
  }
  silent_count = 0;
  survivor_connected = 1;

  silent_count = TEST_UID_SESSION_LIMIT - UINT32_C(1);
  if (!open_silent_set(socket_path, silent, silent_count) ||
      !wait_for_task_count(daemon, baseline_tasks + TEST_UID_SESSION_LIMIT,
                           TEST_FAST_WAIT_ATTEMPTS)) {
    goto cleanup;
  }
  rejected = connect_silent(socket_path);
  if (rejected < 0 ||
      !wait_for_close_without_extra_worker(rejected, daemon,
                                           baseline_tasks + TEST_UID_SESSION_LIMIT) ||
      !context_round_trip(&survivor)) {
    goto cleanup;
  }
  (void)close(rejected);
  rejected = -1;
  if (!close_timed_out_set(silent, silent_count) ||
      !wait_for_task_count(daemon, baseline_tasks + 1U, TEST_WAIT_ATTEMPTS) ||
      !connect_normal_with_retry(socket_path, &replacement) || !context_round_trip(&survivor)) {
    goto cleanup;
  }
  silent_count = 0;
  replacement_connected = 1;
  result = 0;

cleanup:
  if (rejected >= 0) {
    (void)close(rejected);
  }
  for (index = 0; index < TEST_UID_SESSION_LIMIT; ++index) {
    if (silent[index] >= 0) {
      (void)close(silent[index]);
    }
  }
  if (replacement_connected) {
    mf_client_session_close_v1(&replacement);
  }
  if (survivor_connected) {
    mf_client_session_close_v1(&survivor);
  }
  if (probe_connected) {
    mf_client_session_close_v1(&probe);
  }
  if (daemon > 0) {
    if (kill(daemon, SIGTERM) != 0 || !wait_for_child(daemon)) {
      (void)kill(daemon, SIGKILL);
      (void)waitpid(daemon, (int*)0, 0);
      result = 1;
    }
  }
  (void)unlink(socket_path);
  (void)rmdir(directory);
  if (result != 0) {
    (void)fprintf(stderr, "pre-negotiation admission regression failed\n");
  }
  return result;
}
