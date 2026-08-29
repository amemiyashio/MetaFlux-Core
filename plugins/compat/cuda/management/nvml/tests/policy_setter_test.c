#define _GNU_SOURCE

#include "metaflux/client/fastpath.h"
#include "metaflux/nvml/abi.h"

#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void short_pause(void) {
  const struct timespec duration = {.tv_sec = 0, .tv_nsec = 10000000};
  (void)nanosleep(&duration, (struct timespec*)0);
}

static int connect_observer(const char* socket_path, mf_client_session_v1* session) {
  uint32_t attempt = 0;
  for (attempt = 0; attempt < UINT32_C(500); ++attempt) {
    if (mf_client_observer_connect_v1(socket_path, session) == MF_SHARED_SUCCESS) {
      return 1;
    }
    short_pause();
  }
  return 0;
}

static int read_fence(mf_client_session_v1* observer, mf_client_fence_snapshot_v1* fence) {
  mf_virtual_device_identity_v1 identity;
  mf_generation_handle_v1 handle;
  if (mf_client_registry_identity_v1(&observer->registry, UINT32_C(0), &identity) !=
          MF_SHARED_SUCCESS ||
      mf_client_registry_make_handle_v1(&observer->registry, UINT32_C(0), UINT64_C(1), UINT64_C(1),
                                        MF_OBJECT_TYPE_CONTEXT, &handle) != MF_SHARED_SUCCESS ||
      mf_client_registry_validate_device_v1(&observer->registry, &handle, fence) !=
          MF_SHARED_SUCCESS) {
    return 0;
  }
  return fence->identity_record_id == identity.identity_record_id;
}

static int set_policy(mf_client_session_v1* observer, uint16_t opcode, uint64_t value,
                      uint32_t expected_status, uint64_t* out_lifecycle_sequence) {
  mf_virtual_device_identity_v1 identity;
  mf_client_control_response_v1 response;
  if (mf_client_registry_identity_v1(&observer->registry, UINT32_C(0), &identity) !=
          MF_SHARED_SUCCESS ||
      mf_client_session_control_v1(observer, opcode, UINT16_C(0), identity.identity_record_id,
                                   value, -1, &response, (int32_t*)0) != MF_SHARED_SUCCESS ||
      mf_client_load_le32_v1(response.bytes + 12) != expected_status) {
    return 0;
  }
  if (expected_status == MF_CLIENT_CONTROL_OK) {
    if (mf_client_load_le64_v1(response.bytes + 48) != identity.identity_record_id ||
        mf_client_load_le64_v1(response.bytes + 56) == UINT64_C(0)) {
      return 0;
    }
    if (out_lifecycle_sequence != (uint64_t*)0) {
      *out_lifecycle_sequence = mf_client_load_le64_v1(response.bytes + 56);
    }
  }
  return 1;
}

int main(int argc, char** argv) {
  char directory_template[] = "/tmp/metaflux-nvml-policy-XXXXXX";
  char socket_path[PATH_MAX];
  char* directory = (char*)0;
  mf_client_session_v1 observer;
  mf_client_session_v1 fresh_observer;
  mf_client_fence_snapshot_v1 fence;
  nvmlDevice_t device = (nvmlDevice_t)0;
  nvmlEnableState_t persistence = NVML_FEATURE_ENABLED;
  nvmlComputeMode_t compute = NVML_COMPUTEMODE_PROHIBITED;
  uint64_t sequence = 0;
  uint64_t repeated_sequence = 0;
  pid_t daemon = -1;
  int observer_connected = 0;
  int fresh_observer_connected = 0;
  int nvml_initialized = 0;
  int result = 1;
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
  if ((observer.negotiated_capabilities & MF_CLIENT_CAP_POLICY_SETTERS_V1) == UINT64_C(0) ||
      setenv("METAFLUX_MODE", "managed", 1) != 0 ||
      setenv("METAFLUX_SOCKET", socket_path, 1) != 0 || nvmlInit_v2() != NVML_SUCCESS) {
    goto cleanup;
  }
  nvml_initialized = 1;
  if (nvmlDeviceGetHandleByIndex_v2(UINT32_C(0), &device) != NVML_SUCCESS ||
      nvmlDeviceGetPersistenceMode(device, &persistence) != NVML_SUCCESS ||
      persistence != NVML_FEATURE_DISABLED ||
      nvmlDeviceGetComputeMode(device, &compute) != NVML_SUCCESS ||
      compute != NVML_COMPUTEMODE_DEFAULT || !read_fence(&observer, &fence) ||
      fence.policy_bits != UINT64_C(0)) {
    goto cleanup;
  }

  if (nvmlDeviceSetPersistenceMode(device, (nvmlEnableState_t)2) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceSetComputeMode(device, (nvmlComputeMode_t)4) != NVML_ERROR_INVALID_ARGUMENT ||
      !read_fence(&observer, &fence) || fence.lifecycle_sequence != UINT64_C(1) ||
      fence.policy_bits != UINT64_C(0)) {
    goto cleanup;
  }
  if (nvmlDeviceSetPersistenceMode(device, NVML_FEATURE_ENABLED) != NVML_SUCCESS ||
      nvmlDeviceGetPersistenceMode(device, &persistence) != NVML_SUCCESS ||
      persistence != NVML_FEATURE_ENABLED || !read_fence(&observer, &fence) ||
      (fence.policy_bits & MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1) == UINT64_C(0)) {
    goto cleanup;
  }
  sequence = fence.lifecycle_sequence;
  if (nvmlDeviceSetPersistenceMode(device, NVML_FEATURE_ENABLED) != NVML_SUCCESS ||
      !read_fence(&observer, &fence) || fence.lifecycle_sequence != sequence) {
    goto cleanup;
  }

  if (!set_policy(&observer, MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1,
                  MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_THREAD_V1, MF_CLIENT_CONTROL_OK,
                  &sequence) ||
      !read_fence(&observer, &fence) || fence.lifecycle_sequence != sequence ||
      ((fence.policy_bits & MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1) >>
       MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1) !=
          MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_THREAD_V1 ||
      nvmlDeviceGetComputeMode(device, &compute) != NVML_SUCCESS ||
      compute != NVML_COMPUTEMODE_EXCLUSIVE_THREAD) {
    goto cleanup;
  }
  if (!set_policy(&observer, MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1, UINT64_C(4),
                  MF_CLIENT_CONTROL_INVALID_ARGUMENT, (uint64_t*)0) ||
      !read_fence(&observer, &fence) || fence.lifecycle_sequence != sequence) {
    goto cleanup;
  }

  if (nvmlDeviceSetComputeMode(device, NVML_COMPUTEMODE_EXCLUSIVE_PROCESS) != NVML_SUCCESS ||
      nvmlDeviceGetComputeMode(device, &compute) != NVML_SUCCESS ||
      compute != NVML_COMPUTEMODE_EXCLUSIVE_PROCESS ||
      !connect_observer(socket_path, &fresh_observer)) {
    goto cleanup;
  }
  fresh_observer_connected = 1;
  if (!read_fence(&fresh_observer, &fence) ||
      (fence.policy_bits & MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1) == UINT64_C(0) ||
      ((fence.policy_bits & MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1) >>
       MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1) !=
          MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_PROCESS_V1 ||
      !set_policy(&fresh_observer, MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1,
                  MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_PROCESS_V1, MF_CLIENT_CONTROL_OK,
                  &repeated_sequence) ||
      repeated_sequence != fence.lifecycle_sequence) {
    goto cleanup;
  }
  result = 0;

cleanup:
  if (nvml_initialized != 0) {
    if (nvmlShutdown() != NVML_SUCCESS) {
      result = 1;
    }
  }
  if (fresh_observer_connected != 0) {
    mf_client_session_close_v1(&fresh_observer);
  }
  if (observer_connected != 0) {
    mf_client_session_close_v1(&observer);
  }
  if (daemon > 0) {
    (void)kill(daemon, SIGTERM);
    (void)waitpid(daemon, (int*)0, 0);
  }
  (void)unsetenv("METAFLUX_MODE");
  (void)unsetenv("METAFLUX_SOCKET");
  (void)unlink(socket_path);
  (void)rmdir(directory);
  return result;
}
