#define _GNU_SOURCE

#include "metaflux/client/fastpath.h"

#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MF_NOOP_STRESS_COUNT UINT64_C(1000000)
#define MF_NOOP_CONNECT_ATTEMPTS UINT32_C(500)
#define MF_NOOP_WAIT_NS UINT64_C(10000000)
#define MF_NOOP_DEADLINE_NS UINT64_C(150000000000)

static void short_pause(void) {
  const struct timespec duration = {.tv_sec = 0, .tv_nsec = 10000000};
  (void)nanosleep(&duration, (struct timespec*)0);
}

static uint64_t monotonic_time_ns(void) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 || now.tv_nsec < 0) {
    return UINT64_MAX;
  }
  if ((uint64_t)now.tv_sec > UINT64_MAX / UINT64_C(1000000000)) {
    return UINT64_MAX;
  }
  return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

static uint64_t deadline_after(uint64_t duration_ns) {
  const uint64_t now = monotonic_time_ns();
  return now > UINT64_MAX - duration_ns ? UINT64_MAX : now + duration_ns;
}

static int before_deadline(uint64_t deadline_ns) {
  return monotonic_time_ns() < deadline_ns;
}

static int wait_writable(mf_client_session_v1* session, uint64_t deadline_ns) {
  while (before_deadline(deadline_ns)) {
    const mf_shared_status_v1 status =
        mf_client_ring_wait_writable_v1(&session->submission, MF_NOOP_WAIT_NS);
    if (status == MF_SHARED_SUCCESS || status == MF_SHARED_RETRY ||
        status == MF_SHARED_TIMEOUT || status == MF_SHARED_INTERRUPTED) {
      return 1;
    }
    (void)fprintf(stderr, "submission wait failed status=%d\n", status);
    return 0;
  }
  return 0;
}

static int wait_readable(mf_client_session_v1* session, uint64_t deadline_ns) {
  while (before_deadline(deadline_ns)) {
    const mf_shared_status_v1 status =
        mf_client_ring_wait_readable_v1(&session->completion, MF_NOOP_WAIT_NS);
    if (status == MF_SHARED_SUCCESS || status == MF_SHARED_RETRY ||
        status == MF_SHARED_TIMEOUT || status == MF_SHARED_INTERRUPTED) {
      return 1;
    }
    (void)fprintf(stderr, "completion wait failed status=%d\n", status);
    return 0;
  }
  return 0;
}

static int connect_with_retry(const char* socket_path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < MF_NOOP_CONNECT_ATTEMPTS; ++attempt) {
    (void)memset(session, 0, sizeof(*session));
    session->socket_fd = -1;
    session->registry.owned_fd = -1;
    session->submission.owned_fd = -1;
    session->completion.owned_fd = -1;
    if (mf_client_session_connect_v1(socket_path, session) == MF_SHARED_SUCCESS) {
      return 1;
    }
    mf_client_session_close_v1(session);
    short_pause();
  }
  return 0;
}

static int submit_noop(mf_client_session_v1* session, uint64_t request_id,
                       uint64_t deadline_ns) {
  mf_ring_descriptor_v1 descriptor;
  (void)memset(&descriptor, 0, sizeof(descriptor));
  descriptor.opcode = MF_RING_OPCODE_NOOP;
  descriptor.request_id = request_id;
  descriptor.target_id = MF_CLIENT_RUNTIME_CONTEXT_ID_V1;
  while (before_deadline(deadline_ns)) {
    const mf_shared_status_v1 status =
        mf_client_ring_try_submit_v1(&session->submission, &descriptor);
    if (status == MF_SHARED_SUCCESS) {
      return 1;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      (void)fprintf(stderr, "submission failed status=%d\n", status);
      return 0;
    }
    if (!wait_writable(session, deadline_ns)) {
      return 0;
    }
  }
  return 0;
}

static int consume_noop(mf_client_session_v1* session, uint64_t request_id,
                        uint64_t deadline_ns) {
  mf_client_completion_v1 completion;
  while (before_deadline(deadline_ns)) {
    const mf_shared_status_v1 status =
        mf_client_try_consume_completion_v1(&session->completion, &completion);
    if (status == MF_SHARED_SUCCESS) {
      if (completion.request_id != request_id || completion.status != MF_SHARED_SUCCESS ||
          completion.result_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 ||
          completion.result_generation != UINT64_C(1)) {
        (void)fprintf(stderr,
                      "unexpected completion request=%llu status=%d result=%llu generation=%llu\n",
                      (unsigned long long)completion.request_id, completion.status,
                      (unsigned long long)completion.result_id,
                      (unsigned long long)completion.result_generation);
        return 0;
      }
      return 1;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      (void)fprintf(stderr, "completion consume failed status=%d\n", status);
      return 0;
    }
    if (!wait_readable(session, deadline_ns)) {
      return 0;
    }
  }
  (void)fprintf(stderr, "completion did not arrive for request=%llu\n",
                (unsigned long long)request_id);
  return 0;
}

static int wait_for_child(pid_t child) {
  int status = 0;
  if (waitpid(child, &status, 0) != child) {
    return 0;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char** argv) {
  char directory_template[] = "/tmp/metafluxd-noop-stress-XXXXXX";
  char socket_path[PATH_MAX];
  char* directory = NULL;
  mf_client_session_v1 session;
  pid_t daemon = -1;
  int connected = 0;
  int result = 1;
  uint64_t deadline_ns = 0;
  uint64_t next_request_id = UINT64_C(1);
  uint64_t completed = 0;
  uint64_t consumer_doorbells_before = 0;
  uint64_t consumer_doorbells_after = 0;
  uint64_t started_ns = 0;
  uint64_t batch_capacity = 0;

  if (argc != 2) {
    (void)fprintf(stderr, "usage: %s DAEMON\n", argv[0]);
    return 64;
  }
  directory = mkdtemp(directory_template);
  if (directory == NULL ||
      snprintf(socket_path, sizeof(socket_path), "%s/metafluxd.sock", directory) <= 0) {
    return 1;
  }
  daemon = fork();
  if (daemon == 0) {
    execl(argv[1], argv[1], "--socket", socket_path, (char*)NULL);
    _exit(127);
  }
  if (daemon < 0 || !connect_with_retry(socket_path, &session)) {
    goto cleanup;
  }
  connected = 1;
  if (session.submission.capacity == 0U || session.submission.capacity > UINT32_MAX / 2U) {
    (void)fprintf(stderr, "invalid ring capacity=%u\n", session.submission.capacity);
    goto cleanup;
  }
  batch_capacity = (uint64_t)session.submission.capacity / UINT64_C(2);
  if (batch_capacity == 0U) {
    batch_capacity = UINT64_C(1);
  }
  deadline_ns = deadline_after(MF_NOOP_DEADLINE_NS);
  started_ns = monotonic_time_ns();
  consumer_doorbells_before = mf_client_ring_consumer_doorbells_v1(&session.completion);
  while (completed < MF_NOOP_STRESS_COUNT) {
    const uint64_t batch_end =
        next_request_id > MF_NOOP_STRESS_COUNT - batch_capacity
            ? MF_NOOP_STRESS_COUNT + UINT64_C(1)
            : next_request_id + batch_capacity;
    while (next_request_id < batch_end) {
      if (!submit_noop(&session, next_request_id, deadline_ns)) {
        (void)fprintf(stderr, "NOOP submission failed at request %llu\n",
                      (unsigned long long)next_request_id);
        goto cleanup;
      }
      ++next_request_id;
    }
    while (completed + UINT64_C(1) < next_request_id) {
      const uint64_t request_id = completed + UINT64_C(1);
      if (!consume_noop(&session, request_id, deadline_ns)) {
        (void)fprintf(stderr, "NOOP completion failed at request %llu\n",
                      (unsigned long long)request_id);
        goto cleanup;
      }
      ++completed;
    }
  }
  consumer_doorbells_after = mf_client_ring_consumer_doorbells_v1(&session.completion);
  {
    const uint64_t finished_ns = monotonic_time_ns();
    const uint64_t elapsed_ms = finished_ns >= started_ns ? (finished_ns - started_ns) / UINT64_C(1000000)
                                                         : UINT64_MAX;
    (void)printf("metafluxd-noop-stress: PASS descriptors=%llu elapsed_ms=%llu completion_doorbells=%llu\n",
                 (unsigned long long)MF_NOOP_STRESS_COUNT, (unsigned long long)elapsed_ms,
                 (unsigned long long)(consumer_doorbells_after - consumer_doorbells_before));
  }
  result = 0;

cleanup:
  if (connected) {
    mf_client_session_close_v1(&session);
  }
  if (daemon > 0) {
    if (kill(daemon, SIGTERM) != 0 || !wait_for_child(daemon)) {
      result = 1;
    }
  }
  (void)unlink(socket_path);
  (void)rmdir(directory_template);
  return result;
}
