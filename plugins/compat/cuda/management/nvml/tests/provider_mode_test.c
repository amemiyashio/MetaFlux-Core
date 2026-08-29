#define _GNU_SOURCE
#define METAFLUX_CUDA_ABI_INTERNAL 1
#define METAFLUX_NVML_ABI_INTERNAL 1
#define METAFLUX_PROVIDER_TESTING 1

#include "metaflux/cuda/provider.h"
#include "metaflux/nvml/provider.h"

#include "passthrough_internal.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/memfd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MF_MODE_MANAGED_DEVICE_COUNT UINT32_C(3)

typedef struct mf_mode_environment {
  char root[PATH_MAX];
  char missing_config[PATH_MAX];
  char proc_version[PATH_MAX];
  char pid_namespace[PATH_MAX];
  char mount_namespace[PATH_MAX];
  char install_root[PATH_MAX];
  char socket_path[PATH_MAX];
  const char* cuda_provider;
  const char* nvml_provider;
  const char* vendor_cuda;
  const char* vendor_nvml;
} mf_mode_environment;

typedef struct mf_mode_registry_fixture {
  int fd;
  void* mapping;
  uint64_t size;
  mf_registry_view_id_v1 view_id;
  mf_client_ring_v1 submission;
  mf_client_ring_v1 completion;
} mf_mode_registry_fixture;

typedef struct mf_cuda_api {
  CUresult (*init)(unsigned int flags);
  CUresult (*driver_get_version)(int* version);
  CUresult (*device_get_count)(int* count);
  CUresult (*device_get)(CUdevice* device, int ordinal);
  CUresult (*device_get_name)(char* name, int length, CUdevice device);
  CUresult (*device_get_uuid)(CUuuid* uuid, CUdevice device);
  CUresult (*get_proc_address_v2)(const char* symbol, void** function, int cuda_version,
                                  cuuint64_t flags, CUdriverProcAddressQueryResult* symbol_status);
  CUresult (*launch_kernel_ptsz)(CUfunction function, unsigned int grid_x, unsigned int grid_y,
                                 unsigned int grid_z, unsigned int block_x, unsigned int block_y,
                                 unsigned int block_z, unsigned int shared_memory_bytes,
                                 CUstream stream, void** kernel_parameters, void** extra);
  CUresult (*copy_htod_ptds)(CUdeviceptr destination, const void* source, size_t bytes);
  int (*install_transport)(const mf_cuda_provider_test_transport_v1* transport);
  int (*install_policy)(const mf_cuda_passthrough_policy_v1* policy);
  void (*force_dirty)(uint32_t enabled);
  int (*snapshot)(mf_cuda_provider_test_mode_snapshot_v1* snapshot);
  uint64_t (*call_count)(const char* symbol);
  uint32_t (*validate_surface)(void);
  void (*reset)(void);
} mf_cuda_api;

typedef struct mf_nvml_api {
  nvmlReturn_t (*init)(void);
  nvmlReturn_t (*init_v2)(void);
  nvmlReturn_t (*init_with_flags)(unsigned int flags);
  nvmlReturn_t (*shutdown)(void);
  const char* (*error_string)(nvmlReturn_t result);
  nvmlReturn_t (*system_driver_version)(char* version, unsigned int length);
  nvmlReturn_t (*device_get_count)(unsigned int* count);
  nvmlReturn_t (*device_get_count_v2)(unsigned int* count);
  nvmlReturn_t (*device_get_handle)(unsigned int index, nvmlDevice_t* device);
  nvmlReturn_t (*device_get_handle_v2)(unsigned int index, nvmlDevice_t* device);
  nvmlReturn_t (*device_get_name)(nvmlDevice_t device, char* name, unsigned int length);
  nvmlReturn_t (*device_get_uuid)(nvmlDevice_t device, char* uuid, unsigned int length);
  nvmlReturn_t (*device_get_temperature)(nvmlDevice_t device, nvmlTemperatureSensors_t sensor,
                                         unsigned int* temperature);
  int (*install_transport)(const mf_nvml_provider_test_transport_v1* transport);
  int (*install_policy)(const mf_cuda_passthrough_policy_v1* policy);
  void (*force_dirty)(uint32_t enabled);
  int (*snapshot)(mf_nvml_provider_test_mode_snapshot_v1* snapshot);
  uint64_t (*call_count)(const char* symbol);
  uint32_t (*validate_surface)(void);
  void (*reset)(void);
} mf_nvml_api;

#define MF_CHECK(condition)                                                                        \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      (void)fprintf(stderr, "%s:%d: check failed: %s\n", __func__, __LINE__, #condition);          \
      return -1;                                                                                   \
    }                                                                                              \
  } while (0)

static int mf_join_path(char output[PATH_MAX], const char* directory, const char* name) {
  const int written = snprintf(output, PATH_MAX, "%s/%s", directory, name);
  return written > 0 && written < PATH_MAX ? 0 : -1;
}

static int mf_write_text(const char* path, const char* text) {
  const size_t length = strlen(text);
  size_t offset = 0;
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return -1;
  }
  while (offset < length) {
    const ssize_t result = write(fd, text + offset, length - offset);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      (void)close(fd);
      return -1;
    }
    offset += (size_t)result;
  }
  return close(fd);
}

static int mf_environment_create(mf_mode_environment* environment, char** argv) {
  char template_path[] = "/tmp/metaflux-provider-mode-XXXXXX";
  char* root = (char*)0;
  (void)memset(environment, 0, sizeof(*environment));
  root = mkdtemp(template_path);
  if (root == (char*)0 || strlen(root) >= sizeof(environment->root)) {
    return -1;
  }
  (void)memcpy(environment->root, root, strlen(root) + (size_t)1);
  if (mf_join_path(environment->missing_config, root, "missing.conf") != 0 ||
      mf_join_path(environment->proc_version, root, "nvidia-version") != 0 ||
      mf_join_path(environment->pid_namespace, root, "pid-namespace") != 0 ||
      mf_join_path(environment->mount_namespace, root, "mount-namespace") != 0 ||
      mf_join_path(environment->install_root, root, "install-root") != 0 ||
      mf_join_path(environment->socket_path, root, "missing.sock") != 0 ||
      mkdir(environment->install_root, S_IRWXU) != 0 ||
      mf_write_text(environment->proc_version,
                    "NVRM version: NVIDIA UNIX x86_64 Kernel Module  777.42.01  Test Build\n") !=
          0 ||
      mf_write_text(environment->pid_namespace, "pid-a\n") != 0 ||
      mf_write_text(environment->mount_namespace, "mount-a\n") != 0) {
    return -1;
  }
  environment->cuda_provider = argv[1];
  environment->nvml_provider = argv[2];
  environment->vendor_cuda = argv[3];
  environment->vendor_nvml = argv[4];
  return 0;
}

static void mf_environment_destroy(mf_mode_environment* environment) {
  (void)unlink(environment->proc_version);
  (void)unlink(environment->pid_namespace);
  (void)unlink(environment->mount_namespace);
  (void)rmdir(environment->install_root);
  (void)rmdir(environment->root);
}

static void mf_initialize_policy(const mf_mode_environment* environment, const char* provider_self,
                                 mf_cuda_passthrough_policy_v1* policy) {
  mf_cuda_passthrough_test_policy_init_v1(policy);
  policy->trusted_uid = (uint32_t)geteuid();
  policy->enforce_trusted_ancestors = UINT32_C(0);
  policy->config_path = environment->missing_config;
  policy->proc_version_path = environment->proc_version;
  policy->pid_namespace_path = environment->pid_namespace;
  policy->mount_namespace_path = environment->mount_namespace;
  policy->install_root = environment->install_root;
  policy->provider_self_path = provider_self;
  policy->default_pair_count = UINT32_C(1);
  policy->default_cuda_paths[0] = environment->vendor_cuda;
  policy->default_nvml_paths[0] = environment->vendor_nvml;
  policy->default_cuda_paths[1] = (const char*)0;
  policy->default_nvml_paths[1] = (const char*)0;
}

static int mf_load_symbol(void* library, const char* name, void* output, size_t output_size) {
  void* address = (void*)0;
  const char* error = (const char*)0;
  if (output_size != sizeof(address)) {
    return -1;
  }
  (void)dlerror();
  address = dlsym(library, name);
  error = dlerror();
  if (error != (const char*)0 || address == (void*)0) {
    (void)fprintf(stderr, "dlsym(%s): %s\n", name, error == (const char*)0 ? "null symbol" : error);
    return -1;
  }
  (void)memcpy(output, &address, sizeof(address));
  return 0;
}

#define MF_LOAD(api, library, field, symbol)                                                       \
  do {                                                                                             \
    if (mf_load_symbol((library), (symbol), &(api)->field, sizeof((api)->field)) != 0) {           \
      return -1;                                                                                   \
    }                                                                                              \
  } while (0)

static int mf_load_cuda_api(void* library, mf_cuda_api* api) {
  (void)memset(api, 0, sizeof(*api));
  MF_LOAD(api, library, init, "cuInit");
  MF_LOAD(api, library, driver_get_version, "cuDriverGetVersion");
  MF_LOAD(api, library, device_get_count, "cuDeviceGetCount");
  MF_LOAD(api, library, device_get, "cuDeviceGet");
  MF_LOAD(api, library, device_get_name, "cuDeviceGetName");
  MF_LOAD(api, library, device_get_uuid, "cuDeviceGetUuid");
  MF_LOAD(api, library, get_proc_address_v2, "cuGetProcAddress_v2");
  MF_LOAD(api, library, launch_kernel_ptsz, "cuLaunchKernel_ptsz");
  MF_LOAD(api, library, copy_htod_ptds, "cuMemcpyHtoD_v2_ptds");
  MF_LOAD(api, library, install_transport, "mf_cuda_provider_test_install_transport_v1");
  MF_LOAD(api, library, install_policy, "mf_cuda_provider_test_install_passthrough_policy_v1");
  MF_LOAD(api, library, force_dirty, "mf_cuda_provider_test_force_dirty_rollback_v1");
  MF_LOAD(api, library, snapshot, "mf_cuda_provider_test_get_mode_snapshot_v1");
  MF_LOAD(api, library, call_count, "mf_cuda_provider_test_vendor_call_count_v1");
  MF_LOAD(api, library, validate_surface, "mf_cuda_provider_test_validate_vendor_surface_v1");
  MF_LOAD(api, library, reset, "mf_cuda_provider_test_reset_v1");
  return 0;
}

static int mf_load_nvml_api(void* library, mf_nvml_api* api) {
  (void)memset(api, 0, sizeof(*api));
  MF_LOAD(api, library, init, "nvmlInit");
  MF_LOAD(api, library, init_v2, "nvmlInit_v2");
  MF_LOAD(api, library, init_with_flags, "nvmlInitWithFlags");
  MF_LOAD(api, library, shutdown, "nvmlShutdown");
  MF_LOAD(api, library, error_string, "nvmlErrorString");
  MF_LOAD(api, library, system_driver_version, "nvmlSystemGetDriverVersion");
  MF_LOAD(api, library, device_get_count, "nvmlDeviceGetCount");
  MF_LOAD(api, library, device_get_count_v2, "nvmlDeviceGetCount_v2");
  MF_LOAD(api, library, device_get_handle, "nvmlDeviceGetHandleByIndex");
  MF_LOAD(api, library, device_get_handle_v2, "nvmlDeviceGetHandleByIndex_v2");
  MF_LOAD(api, library, device_get_name, "nvmlDeviceGetName");
  MF_LOAD(api, library, device_get_uuid, "nvmlDeviceGetUUID");
  MF_LOAD(api, library, device_get_temperature, "nvmlDeviceGetTemperature");
  MF_LOAD(api, library, install_transport, "mf_nvml_provider_test_install_transport_v1");
  MF_LOAD(api, library, install_policy, "mf_nvml_provider_test_install_passthrough_policy_v1");
  MF_LOAD(api, library, force_dirty, "mf_nvml_provider_test_force_dirty_rollback_v1");
  MF_LOAD(api, library, snapshot, "mf_nvml_provider_test_get_mode_snapshot_v1");
  MF_LOAD(api, library, call_count, "mf_nvml_provider_test_vendor_call_count_v1");
  MF_LOAD(api, library, validate_surface, "mf_nvml_provider_test_validate_vendor_surface_v1");
  MF_LOAD(api, library, reset, "mf_nvml_provider_test_reset_v1");
  return 0;
}

static int mf_registry_fixture_create(mf_mode_registry_fixture* fixture) {
  uint64_t offset = sizeof(mf_shared_registry_header_v1);
  mf_shared_registry_header_v1* header = (mf_shared_registry_header_v1*)0;
  mf_view_admission_control_v1* view_admission = (mf_view_admission_control_v1*)0;
  mf_registry_view_control_v1* view_control = (mf_registry_view_control_v1*)0;
  mf_virtual_device_identity_v1* identities = (mf_virtual_device_identity_v1*)0;
  mf_device_admission_control_v1* device_admissions = (mf_device_admission_control_v1*)0;
  mf_virtual_device_lifecycle_fence_v1* fences = (mf_virtual_device_lifecycle_fence_v1*)0;
  mf_telemetry_control_v1* telemetry_control = (mf_telemetry_control_v1*)0;
  mf_virtual_device_telemetry_v1* bank0 = (mf_virtual_device_telemetry_v1*)0;
  mf_virtual_device_telemetry_v1* bank1 = (mf_virtual_device_telemetry_v1*)0;
  uint32_t index = UINT32_C(0);
  (void)memset(fixture, 0, sizeof(*fixture));
  fixture->fd = -1;
  fixture->submission.owned_fd = -1;
  fixture->completion.owned_fd = -1;
  fixture->view_id.daemon_incarnation = UINT64_C(41);
  fixture->view_id.view_serial = UINT64_C(43);
  fixture->size =
      sizeof(mf_shared_registry_header_v1) + sizeof(mf_view_admission_control_v1) +
      sizeof(mf_registry_view_control_v1) +
      ((uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(mf_virtual_device_identity_v1)) +
      ((uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(mf_device_admission_control_v1)) +
      ((uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(mf_virtual_device_lifecycle_fence_v1)) +
      sizeof(mf_telemetry_control_v1) +
      (UINT64_C(2) * (uint64_t)MF_MODE_MANAGED_DEVICE_COUNT *
       sizeof(mf_virtual_device_telemetry_v1));
  fixture->fd = memfd_create("metaflux-provider-mode-registry", MFD_CLOEXEC);
  if (fixture->fd < 0 || ftruncate(fixture->fd, (off_t)fixture->size) != 0) {
    return -1;
  }
  fixture->mapping =
      mmap((void*)0, (size_t)fixture->size, PROT_READ | PROT_WRITE, MAP_SHARED, fixture->fd, 0);
  if (fixture->mapping == MAP_FAILED) {
    fixture->mapping = (void*)0;
    return -1;
  }
  (void)memset(fixture->mapping, 0, (size_t)fixture->size);
  header = (mf_shared_registry_header_v1*)fixture->mapping;
  header->magic = MF_SHARED_REGISTRY_MAGIC;
  header->abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  header->header_size = (uint32_t)sizeof(*header);
  header->total_size = fixture->size;
  header->registry_view_id = fixture->view_id;
  header->process_view_revision = UINT64_C(1);
  header->device_count = MF_MODE_MANAGED_DEVICE_COUNT;
  header->telemetry_row_count = MF_MODE_MANAGED_DEVICE_COUNT;
  header->view_admission_offset = offset;
  view_admission = (mf_view_admission_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += sizeof(*view_admission);
  header->view_control_offset = offset;
  view_control = (mf_registry_view_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += sizeof(*view_control);
  header->identities_offset = offset;
  identities = (mf_virtual_device_identity_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(*identities);
  header->device_admission_offset = offset;
  device_admissions = (mf_device_admission_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(*device_admissions);
  header->lifecycle_fences_offset = offset;
  fences = (mf_virtual_device_lifecycle_fence_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(*fences);
  header->telemetry_control_offset = offset;
  telemetry_control = (mf_telemetry_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += sizeof(*telemetry_control);
  header->telemetry_bank0_offset = offset;
  bank0 = (mf_virtual_device_telemetry_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(*bank0);
  header->telemetry_bank1_offset = offset;
  bank1 = (mf_virtual_device_telemetry_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)MF_MODE_MANAGED_DEVICE_COUNT * sizeof(*bank1);

  view_admission->registry_view_id = fixture->view_id;
  view_admission->state_generation = mf_view_admission_pack_v1(UINT64_C(1), MF_VIEW_ADMISSION_OPEN);
  view_control->registry_view_id = fixture->view_id;
  view_control->gate_state = MF_VIEW_GATE_OPEN;
  telemetry_control->snapshot_sequence = UINT64_C(1);
  telemetry_control->active_bank_state =
      mf_telemetry_bank_state_pack_v1(UINT32_C(0), MF_TELEMETRY_STATE_READY);
  telemetry_control->row_count = MF_MODE_MANAGED_DEVICE_COUNT;
  for (index = UINT32_C(0); index < MF_MODE_MANAGED_DEVICE_COUNT; ++index) {
    uint32_t uuid_index = UINT32_C(0);
    identities[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    for (uuid_index = UINT32_C(0); uuid_index < UINT32_C(16); ++uuid_index) {
      identities[index].gpu_uuid[uuid_index] = (uint8_t)((index * UINT32_C(16)) + uuid_index);
    }
    if (index == UINT32_C(0)) {
      (void)memcpy(identities[index].display_name, "MetaFlux Managed Fixture",
                   sizeof("MetaFlux Managed Fixture"));
    } else {
      (void)snprintf((char*)identities[index].display_name, sizeof(identities[index].display_name),
                     "MetaFlux Managed Fixture %u", index);
    }
    identities[index].committed_generation = UINT64_C(1);
    identities[index].virtual_compute_capability = UINT32_C(80);
    identities[index].pci_domain = UINT32_C(0);
    identities[index].pci_bus = UINT32_C(3) + index;
    identities[index].pci_device = UINT32_C(4);
    identities[index].pci_function = UINT32_C(0);
    device_admissions[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    device_admissions[index].state_generation_tag =
        mf_device_admission_pack_v1(index + UINT32_C(1), MF_DEVICE_ADMISSION_OPEN, UINT32_C(1));
    fences[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    fences[index].lifecycle_sequence = UINT64_C(1);
    fences[index].epoch = UINT64_C(1);
    fences[index].effective_quota_bytes = UINT64_C(1073741824);
    fences[index].device_state = MF_DEVICE_STATE_ONLINE;
    bank0[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    bank0[index].observed_lifecycle_sequence = UINT64_C(1);
    bank0[index].active_time_ns = UINT64_C(20000000);
    bank0[index].memory_active_time_ns = UINT64_C(10000000);
    bank0[index].memory_capacity_bytes = UINT64_C(1073741824);
    bank0[index].sample_time_ns = UINT64_C(100000000);
    bank1[index] = bank0[index];
  }
  if (offset != fixture->size ||
      mf_client_ring_create_v1(UINT32_C(16), fixture->view_id, MF_CLIENT_SUBMISSION_QUEUE_ID_V1,
                               MF_CLIENT_QUEUE_GENERATION_V1,
                               &fixture->submission) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(UINT32_C(16), fixture->view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1,
                               MF_CLIENT_QUEUE_GENERATION_V1,
                               &fixture->completion) != MF_SHARED_SUCCESS) {
    return -1;
  }
  return 0;
}

static void mf_registry_fixture_destroy(mf_mode_registry_fixture* fixture) {
  mf_client_ring_close_v1(&fixture->completion);
  mf_client_ring_close_v1(&fixture->submission);
  if (fixture->mapping != (void*)0) {
    (void)munmap(fixture->mapping, (size_t)fixture->size);
  }
  if (fixture->fd >= 0) {
    (void)close(fixture->fd);
  }
}

static mf_shared_status_v1 mf_control_not_supported(void* context,
                                                    const mf_client_control_request_v1* request,
                                                    const uint8_t* payload, uint64_t payload_size,
                                                    mf_client_control_response_v1* response) {
  (void)context;
  (void)request;
  (void)payload;
  (void)payload_size;
  (void)response;
  return MF_SHARED_NOT_SUPPORTED;
}

static mf_shared_status_v1 mf_read_not_supported(const void* context, uint64_t object_id,
                                                 uint64_t object_generation, uint64_t offset,
                                                 uint8_t* bytes, uint64_t byte_count) {
  (void)context;
  (void)object_id;
  (void)object_generation;
  (void)offset;
  (void)bytes;
  (void)byte_count;
  return MF_SHARED_NOT_SUPPORTED;
}

static int mf_install_managed_transports(const mf_mode_registry_fixture* fixture,
                                         const mf_cuda_api* cuda, const mf_nvml_api* nvml) {
  mf_cuda_provider_test_transport_v1 cuda_transport;
  mf_nvml_provider_test_transport_v1 nvml_transport;
  (void)memset(&cuda_transport, 0, sizeof(cuda_transport));
  cuda_transport.registry_fd = fixture->fd;
  cuda_transport.submission_fd = mf_client_ring_borrow_fd_v1(&fixture->submission);
  cuda_transport.completion_fd = mf_client_ring_borrow_fd_v1(&fixture->completion);
  cuda_transport.registry_view_id = fixture->view_id;
  cuda_transport.submission_queue_id = MF_CLIENT_SUBMISSION_QUEUE_ID_V1;
  cuda_transport.submission_queue_generation = MF_CLIENT_QUEUE_GENERATION_V1;
  cuda_transport.completion_queue_id = MF_CLIENT_COMPLETION_QUEUE_ID_V1;
  cuda_transport.completion_queue_generation = MF_CLIENT_QUEUE_GENERATION_V1;
  cuda_transport.runtime_context_id = MF_CLIENT_RUNTIME_CONTEXT_ID_V1;
  cuda_transport.runtime_event_id = MF_CLIENT_RUNTIME_EVENT_ID_V1;
  cuda_transport.runtime_event_generation = MF_CLIENT_RUNTIME_EVENT_GENERATION_V1;
  cuda_transport.runtime_add_kernel_id = MF_CLIENT_RUNTIME_ADD_KERNEL_ID_V1;
  cuda_transport.control = mf_control_not_supported;
  cuda_transport.read_object = mf_read_not_supported;
  (void)memset(&nvml_transport, 0, sizeof(nvml_transport));
  nvml_transport.registry_fd = fixture->fd;
  nvml_transport.registry_view_id = fixture->view_id;
  nvml_transport.process_data_available = UINT32_C(1);
  return cuda->install_transport(&cuda_transport) == 0 &&
                 nvml->install_transport(&nvml_transport) == 0
             ? 0
             : -1;
}

static int mf_install_policies(const mf_mode_environment* environment, const mf_cuda_api* cuda,
                               const mf_nvml_api* nvml) {
  mf_cuda_passthrough_policy_v1 cuda_policy;
  mf_cuda_passthrough_policy_v1 nvml_policy;
  mf_initialize_policy(environment, environment->cuda_provider, &cuda_policy);
  mf_initialize_policy(environment, environment->nvml_provider, &nvml_policy);
  return cuda->install_policy(&cuda_policy) == 0 && nvml->install_policy(&nvml_policy) == 0 ? 0
                                                                                            : -1;
}

static void mf_reset(const mf_cuda_api* cuda, const mf_nvml_api* nvml) {
  nvml->reset();
  cuda->reset();
}

static int mf_test_managed_failure(const mf_mode_environment* environment, const mf_cuda_api* cuda,
                                   const mf_nvml_api* nvml) {
  mf_cuda_provider_test_mode_snapshot_v1 cuda_snapshot;
  mf_nvml_provider_test_mode_snapshot_v1 nvml_snapshot;
  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "managed", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_ERROR_SYSTEM_NOT_READY);
  MF_CHECK(nvml->init_v2() == NVML_ERROR_DRIVER_NOT_LOADED);
  MF_CHECK(cuda->snapshot(&cuda_snapshot) == 0 && nvml->snapshot(&nvml_snapshot) == 0);
  MF_CHECK(cuda_snapshot.requested_mode == MF_CUDA_RUNTIME_MODE_MANAGED_V1 &&
           cuda_snapshot.selected_runtime == MF_CUDA_SELECTED_NONE_V1);
  MF_CHECK(nvml_snapshot.requested_mode == MF_CUDA_RUNTIME_MODE_MANAGED_V1 &&
           nvml_snapshot.selected_runtime == MF_CUDA_SELECTED_NONE_V1);
  return 0;
}

static int mf_test_invalid_mode(const mf_cuda_api* cuda, const mf_nvml_api* nvml) {
  mf_cuda_provider_test_mode_snapshot_v1 cuda_snapshot;
  mf_nvml_provider_test_mode_snapshot_v1 nvml_snapshot;
  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "Auto", 1) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_ERROR_INVALID_VALUE);
  MF_CHECK(nvml->init_v2() == NVML_ERROR_INVALID_ARGUMENT);
  MF_CHECK(setenv("METAFLUX_MODE", "passthrough", 1) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_ERROR_INVALID_VALUE);
  MF_CHECK(nvml->init_v2() == NVML_ERROR_INVALID_ARGUMENT);
  MF_CHECK(cuda->snapshot(&cuda_snapshot) == 0 && nvml->snapshot(&nvml_snapshot) == 0);
  MF_CHECK(cuda_snapshot.mode_frozen == UINT32_C(1) &&
           cuda_snapshot.selector_status == MF_CUDA_PASSTHROUGH_INVALID_MODE &&
           cuda_snapshot.selected_runtime == MF_CUDA_SELECTED_NONE_V1);
  MF_CHECK(nvml_snapshot.mode_frozen == UINT32_C(1) &&
           nvml_snapshot.selector_status == MF_CUDA_PASSTHROUGH_INVALID_MODE &&
           nvml_snapshot.selected_runtime == MF_CUDA_SELECTED_NONE_V1);
  return 0;
}

static int mf_test_auto_managed(const mf_mode_environment* environment,
                                const mf_mode_registry_fixture* fixture, const mf_cuda_api* cuda,
                                const mf_nvml_api* nvml) {
  mf_cuda_provider_test_mode_snapshot_v1 cuda_snapshot;
  mf_nvml_provider_test_mode_snapshot_v1 nvml_snapshot;
  int cuda_count = 0;
  unsigned int nvml_count = UINT32_C(0);
  uint32_t index = UINT32_C(0);
  mf_reset(cuda, nvml);
  MF_CHECK(unsetenv("METAFLUX_MODE") == 0 && unsetenv("CUDA_VISIBLE_DEVICES") == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  MF_CHECK(mf_install_managed_transports(fixture, cuda, nvml) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_SUCCESS && nvml->init_v2() == NVML_SUCCESS);
  MF_CHECK(cuda->device_get_count(&cuda_count) == CUDA_SUCCESS &&
           cuda_count == (int)MF_MODE_MANAGED_DEVICE_COUNT);
  MF_CHECK(nvml->device_get_count_v2(&nvml_count) == NVML_SUCCESS &&
           nvml_count == MF_MODE_MANAGED_DEVICE_COUNT);
  for (index = UINT32_C(0); index < MF_MODE_MANAGED_DEVICE_COUNT; ++index) {
    CUdevice cuda_device = -1;
    CUuuid cuda_uuid;
    nvmlDevice_t nvml_device = (nvmlDevice_t)0;
    char cuda_name[64];
    char expected_name[64];
    char expected_uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
    char nvml_name[NVML_DEVICE_NAME_BUFFER_SIZE];
    char nvml_uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
    uint32_t uuid_index = UINT32_C(0);
    const uint32_t base = index * UINT32_C(16);
    if (index == UINT32_C(0)) {
      (void)snprintf(expected_name, sizeof(expected_name), "MetaFlux Managed Fixture");
    } else {
      (void)snprintf(expected_name, sizeof(expected_name), "MetaFlux Managed Fixture %u", index);
    }
    (void)snprintf(expected_uuid, sizeof(expected_uuid),
                   "GPU-%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                   "%02x%02x%02x%02x%02x%02x",
                   base, base + UINT32_C(1), base + UINT32_C(2), base + UINT32_C(3),
                   base + UINT32_C(4), base + UINT32_C(5), base + UINT32_C(6), base + UINT32_C(7),
                   base + UINT32_C(8), base + UINT32_C(9), base + UINT32_C(10), base + UINT32_C(11),
                   base + UINT32_C(12), base + UINT32_C(13), base + UINT32_C(14),
                   base + UINT32_C(15));
    MF_CHECK(cuda->device_get(&cuda_device, (int)index) == CUDA_SUCCESS &&
             cuda->device_get_name(cuda_name, (int)sizeof(cuda_name), cuda_device) ==
                 CUDA_SUCCESS &&
             cuda->device_get_uuid(&cuda_uuid, cuda_device) == CUDA_SUCCESS);
    MF_CHECK(nvml->device_get_handle_v2(index, &nvml_device) == NVML_SUCCESS &&
             nvml->device_get_name(nvml_device, nvml_name, sizeof(nvml_name)) == NVML_SUCCESS &&
             nvml->device_get_uuid(nvml_device, nvml_uuid, sizeof(nvml_uuid)) == NVML_SUCCESS);
    MF_CHECK(strcmp(cuda_name, expected_name) == 0 && strcmp(cuda_name, nvml_name) == 0 &&
             strcmp(nvml_uuid, expected_uuid) == 0);
    for (uuid_index = UINT32_C(0); uuid_index < UINT32_C(16); ++uuid_index) {
      MF_CHECK((unsigned char)cuda_uuid.bytes[uuid_index] == (unsigned char)(base + uuid_index));
    }
  }
  MF_CHECK(cuda->snapshot(&cuda_snapshot) == 0 && nvml->snapshot(&nvml_snapshot) == 0);
  MF_CHECK(cuda_snapshot.requested_mode == MF_CUDA_RUNTIME_MODE_AUTO_V1 &&
           cuda_snapshot.selected_runtime == MF_CUDA_SELECTED_MANAGED_V1 &&
           cuda_snapshot.cuda_path[0] == '\0');
  MF_CHECK(nvml_snapshot.requested_mode == MF_CUDA_RUNTIME_MODE_AUTO_V1 &&
           nvml_snapshot.selected_runtime == MF_CUDA_SELECTED_MANAGED_V1 &&
           nvml_snapshot.cuda_path[0] == '\0');
  MF_CHECK(nvml->shutdown() == NVML_SUCCESS);
  return 0;
}

static int mf_test_auto_managed_empty_cuda_filter(const mf_mode_environment* environment,
                                                  const mf_mode_registry_fixture* fixture,
                                                  const mf_cuda_api* cuda,
                                                  const mf_nvml_api* nvml) {
  nvmlDevice_t nvml_device = (nvmlDevice_t)0;
  int cuda_count = -1;
  unsigned int nvml_count = UINT32_C(0);
  char nvml_name[NVML_DEVICE_NAME_BUFFER_SIZE];
  mf_reset(cuda, nvml);
  MF_CHECK(unsetenv("METAFLUX_MODE") == 0 && setenv("CUDA_VISIBLE_DEVICES", "", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  MF_CHECK(mf_install_managed_transports(fixture, cuda, nvml) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_SUCCESS && nvml->init_v2() == NVML_SUCCESS);
  MF_CHECK(cuda->device_get_count(&cuda_count) == CUDA_SUCCESS && cuda_count == 0);
  MF_CHECK(nvml->device_get_count_v2(&nvml_count) == NVML_SUCCESS &&
           nvml_count == MF_MODE_MANAGED_DEVICE_COUNT);
  MF_CHECK(nvml->device_get_handle_v2(UINT32_C(0), &nvml_device) == NVML_SUCCESS &&
           nvml->device_get_name(nvml_device, nvml_name, sizeof(nvml_name)) == NVML_SUCCESS &&
           strcmp(nvml_name, "MetaFlux Managed Fixture") == 0);
  MF_CHECK(nvml->shutdown() == NVML_SUCCESS && unsetenv("CUDA_VISIBLE_DEVICES") == 0);
  return 0;
}

static int mf_wait_for_fork(const mf_cuda_api* cuda, const mf_nvml_api* nvml) {
  pid_t child = fork();
  int status = 0;
  if (child < 0) {
    return -1;
  }
  if (child == 0) {
    int version = 0;
    unsigned int count = UINT32_C(0);
    const int valid = cuda->driver_get_version(&version) == CUDA_ERROR_DEINITIALIZED &&
                      nvml->device_get_count_v2(&count) == NVML_ERROR_GPU_IS_LOST;
    _exit(valid != 0 ? 0 : 1);
  }
  if (waitpid(child, &status, 0) != child) {
    return -1;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int mf_test_passthrough(const mf_mode_environment* environment, const mf_cuda_api* cuda,
                               const mf_nvml_api* nvml) {
  mf_cuda_provider_test_mode_snapshot_v1 cuda_snapshot;
  mf_nvml_provider_test_mode_snapshot_v1 nvml_snapshot;
  CUdevice cuda_device = -1;
  CUuuid cuda_uuid;
  nvmlDevice_t nvml_device = (nvmlDevice_t)0;
  int cuda_count = 0;
  int cuda_version = 0;
  unsigned int nvml_count = UINT32_C(0);
  unsigned int temperature = UINT32_C(0);
  char cuda_name[64];
  char nvml_name[NVML_DEVICE_NAME_BUFFER_SIZE];
  char nvml_uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
  char driver_version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
  void* address = (void*)0;
  CUdriverProcAddressQueryResult query = CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND;
  CUresult cuda_init_result = CUDA_SUCCESS;
  CUresult (*launch)(CUfunction function, unsigned int grid_x, unsigned int grid_y,
                     unsigned int grid_z, unsigned int block_x, unsigned int block_y,
                     unsigned int block_z, unsigned int shared_memory_bytes, CUstream stream,
                     void** kernel_parameters, void** extra) = (void*)0;
  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "passthrough", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  cuda_init_result = cuda->init(UINT32_C(0));
  if (cuda_init_result != CUDA_SUCCESS) {
    (void)cuda->snapshot(&cuda_snapshot);
    (void)fprintf(stderr, "CUDA passthrough init=%d selector=%u requested=%u selected=%u\n",
                  cuda_init_result, cuda_snapshot.selector_status, cuda_snapshot.requested_mode,
                  cuda_snapshot.selected_runtime);
  }
  MF_CHECK(cuda_init_result == CUDA_SUCCESS);
  MF_CHECK(nvml->init_v2() == NVML_SUCCESS);
  MF_CHECK(cuda->validate_surface() == UINT32_C(77));
  MF_CHECK(nvml->validate_surface() == UINT32_C(131));
  MF_CHECK(cuda->driver_get_version(&cuda_version) == CUDA_SUCCESS && cuda_version == 12070);
  MF_CHECK(cuda->device_get_count(&cuda_count) == CUDA_SUCCESS && cuda_count == 1);
  MF_CHECK(cuda->device_get(&cuda_device, 0) == CUDA_SUCCESS &&
           cuda->device_get_name(cuda_name, (int)sizeof(cuda_name), cuda_device) == CUDA_SUCCESS &&
           cuda->device_get_uuid(&cuda_uuid, cuda_device) == CUDA_SUCCESS);
  MF_CHECK(nvml->device_get_count(&nvml_count) == NVML_SUCCESS && nvml_count == UINT32_C(1));
  MF_CHECK(nvml->device_get_count_v2(&nvml_count) == NVML_SUCCESS && nvml_count == UINT32_C(1));
  MF_CHECK(nvml->device_get_handle(UINT32_C(0), &nvml_device) == NVML_SUCCESS &&
           nvml->device_get_handle_v2(UINT32_C(0), &nvml_device) == NVML_SUCCESS &&
           nvml->device_get_name(nvml_device, nvml_name, sizeof(nvml_name)) == NVML_SUCCESS &&
           nvml->device_get_uuid(nvml_device, nvml_uuid, sizeof(nvml_uuid)) == NVML_SUCCESS);
  MF_CHECK(nvml->system_driver_version(driver_version, sizeof(driver_version)) == NVML_SUCCESS &&
           strcmp(driver_version, "777.42.01") == 0);
  MF_CHECK(strcmp(cuda_name, "MetaFlux Vendor Fixture") == 0 && strcmp(cuda_name, nvml_name) == 0 &&
           strcmp(nvml_uuid, "GPU-00010203-0405-0607-0809-0a0b0c0d0e0f") == 0 &&
           (unsigned char)cuda_uuid.bytes[15] == UINT8_C(15));
  MF_CHECK(nvml->device_get_temperature(nvml_device, NVML_TEMPERATURE_GPU, &temperature) ==
           NVML_ERROR_NOT_SUPPORTED);
  MF_CHECK(nvml->error_string(NVML_SUCCESS) != (const char*)0 &&
           strcmp(nvml->error_string(NVML_SUCCESS), "Vendor Success") == 0);
  MF_CHECK(cuda->copy_htod_ptds(UINT64_C(0), (const void*)0, (size_t)0) ==
           CUDA_ERROR_NOT_SUPPORTED);
  MF_CHECK(cuda->launch_kernel_ptsz((CUfunction)0, UINT32_C(0), UINT32_C(0), UINT32_C(0),
                                    UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), (CUstream)0,
                                    (void**)0, (void**)0) == CUDA_ERROR_NOT_SUPPORTED);
  MF_CHECK(cuda->get_proc_address_v2("cuLaunchKernel", &address, 12070,
                                     CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM,
                                     &query) == CUDA_SUCCESS &&
           address != (void*)0 && query == CU_GET_PROC_ADDRESS_SUCCESS);
  (void)memcpy(&launch, &address, sizeof(launch));
  MF_CHECK(launch((CUfunction)0, UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0),
                  UINT32_C(0), UINT32_C(0), (CUstream)0, (void**)0,
                  (void**)0) == CUDA_ERROR_NOT_SUPPORTED);
  MF_CHECK(cuda->call_count("cuLaunchKernel_ptsz") == UINT64_C(2) &&
           cuda->call_count("cuMemcpyHtoD_v2_ptds") == UINT64_C(1) &&
           cuda->call_count("cuGetProcAddress_v2") == UINT64_C(1));
  MF_CHECK(nvml->call_count("nvmlDeviceGetCount") == UINT64_C(1) &&
           nvml->call_count("nvmlDeviceGetCount_v2") == UINT64_C(1) &&
           nvml->call_count("nvmlDeviceGetHandleByIndex") == UINT64_C(1) &&
           nvml->call_count("nvmlDeviceGetHandleByIndex_v2") == UINT64_C(1));
  MF_CHECK(cuda->snapshot(&cuda_snapshot) == 0 && nvml->snapshot(&nvml_snapshot) == 0);
  MF_CHECK(cuda_snapshot.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1 &&
           nvml_snapshot.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1 &&
           cuda_snapshot.namespace_id > INT64_C(0) && nvml_snapshot.namespace_id > INT64_C(0) &&
           strcmp(cuda_snapshot.driver_build, nvml_snapshot.driver_build) == 0 &&
           strcmp(cuda_snapshot.cuda_path, nvml_snapshot.cuda_path) == 0 &&
           strcmp(cuda_snapshot.nvml_path, nvml_snapshot.nvml_path) == 0);
  MF_CHECK(setenv("METAFLUX_MODE", "managed", 1) == 0);
  MF_CHECK(cuda->driver_get_version(&cuda_version) == CUDA_SUCCESS &&
           nvml->device_get_count_v2(&nvml_count) == NVML_SUCCESS);
  MF_CHECK(mf_wait_for_fork(cuda, nvml) == 0);
  return 0;
}

static int mf_test_auto_fail_open_and_dirty(const mf_mode_environment* environment,
                                            const mf_cuda_api* cuda, const mf_nvml_api* nvml) {
  mf_cuda_provider_test_mode_snapshot_v1 cuda_snapshot;
  mf_nvml_provider_test_mode_snapshot_v1 nvml_snapshot;
  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "auto", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_SUCCESS && nvml->init_v2() == NVML_SUCCESS);
  MF_CHECK(cuda->snapshot(&cuda_snapshot) == 0 && nvml->snapshot(&nvml_snapshot) == 0);
  MF_CHECK(cuda_snapshot.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1 &&
           nvml_snapshot.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1);

  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "auto", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  cuda->force_dirty(UINT32_C(1));
  nvml->force_dirty(UINT32_C(1));
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_ERROR_UNKNOWN);
  MF_CHECK(nvml->init_v2() == NVML_ERROR_INVALID_STATE);
  MF_CHECK(cuda->snapshot(&cuda_snapshot) == 0 && nvml->snapshot(&nvml_snapshot) == 0);
  MF_CHECK(cuda_snapshot.selected_runtime == MF_CUDA_SELECTED_NONE_V1 &&
           cuda_snapshot.selector_status == MF_CUDA_PASSTHROUGH_PARTIAL_STATE &&
           cuda_snapshot.cuda_path[0] == '\0' && cuda->call_count("cuInit") == UINT64_MAX);
  MF_CHECK(nvml_snapshot.selected_runtime == MF_CUDA_SELECTED_NONE_V1 &&
           nvml_snapshot.selector_status == MF_CUDA_PASSTHROUGH_PARTIAL_STATE &&
           nvml_snapshot.cuda_path[0] == '\0' && nvml->call_count("nvmlInit_v2") == UINT64_MAX);
  return 0;
}

static int mf_test_stale_pair(const mf_mode_environment* environment, const mf_cuda_api* cuda,
                              const mf_nvml_api* nvml) {
  int version = 0;
  unsigned int count = UINT32_C(0);
  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "passthrough", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  MF_CHECK(cuda->init(UINT32_C(0)) == CUDA_SUCCESS && nvml->init_v2() == NVML_SUCCESS);
  MF_CHECK(mf_write_text(environment->proc_version,
                         "NVRM version: NVIDIA UNIX Kernel Module  777.42.02  Changed\n") == 0);
  MF_CHECK(cuda->driver_get_version(&version) == CUDA_ERROR_DEINITIALIZED);
  MF_CHECK(nvml->device_get_count_v2(&count) == NVML_ERROR_GPU_IS_LOST);
  MF_CHECK(mf_write_text(
               environment->proc_version,
               "NVRM version: NVIDIA UNIX x86_64 Kernel Module  777.42.01  Test Build\n") == 0);
  return 0;
}

static int mf_test_nvml_final_teardown(const mf_mode_environment* environment,
                                       const mf_cuda_api* cuda, const mf_nvml_api* nvml) {
  mf_nvml_provider_test_mode_snapshot_v1 snapshot;
  unsigned int count = UINT32_C(0);
  mf_reset(cuda, nvml);
  MF_CHECK(setenv("METAFLUX_MODE", "passthrough", 1) == 0);
  MF_CHECK(mf_install_policies(environment, cuda, nvml) == 0);
  MF_CHECK(nvml->init() == NVML_SUCCESS && nvml->init_with_flags(UINT32_C(0)) == NVML_SUCCESS);
  MF_CHECK(nvml->shutdown() == NVML_SUCCESS);
  MF_CHECK(nvml->call_count("nvmlShutdown") == UINT64_C(1));
  MF_CHECK(nvml->device_get_count_v2(&count) == NVML_SUCCESS && count == UINT32_C(1));
  MF_CHECK(nvml->shutdown() == NVML_SUCCESS);
  MF_CHECK(nvml->snapshot(&snapshot) == 0 &&
           snapshot.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1 &&
           snapshot.namespace_id == INT64_C(-1) && snapshot.cuda_path[0] == '\0');
  MF_CHECK(nvml->device_get_count_v2(&count) == NVML_ERROR_UNINITIALIZED);
  MF_CHECK(nvml->init_v2() == NVML_SUCCESS && nvml->device_get_count_v2(&count) == NVML_SUCCESS &&
           count == UINT32_C(1) && nvml->shutdown() == NVML_SUCCESS);
  return 0;
}

int main(int argc, char** argv) {
  mf_mode_environment environment;
  mf_mode_registry_fixture fixture;
  mf_cuda_api cuda;
  mf_nvml_api nvml;
  void* cuda_library = (void*)0;
  void* nvml_library = (void*)0;
  int result = 1;
  if (argc != 5 || mf_environment_create(&environment, argv) != 0) {
    (void)fprintf(stderr, "usage: %s CUDA_PROVIDER NVML_PROVIDER VENDOR_CUDA VENDOR_NVML\n",
                  argc > 0 ? argv[0] : "provider_mode_test");
    return 2;
  }
  if (setenv("METAFLUX_SOCKET", environment.socket_path, 1) != 0 ||
      mf_registry_fixture_create(&fixture) != 0) {
    (void)fprintf(stderr, "fixture initialization failed\n");
    mf_environment_destroy(&environment);
    return 3;
  }
  cuda_library = dlopen(environment.cuda_provider, RTLD_NOW | RTLD_LOCAL);
  nvml_library = dlopen(environment.nvml_provider, RTLD_NOW | RTLD_LOCAL);
  if (cuda_library == (void*)0 || nvml_library == (void*)0) {
    (void)fprintf(stderr, "provider dlopen failed: %s\n", dlerror());
    goto cleanup;
  }
  if (mf_load_cuda_api(cuda_library, &cuda) != 0 || mf_load_nvml_api(nvml_library, &nvml) != 0) {
    goto cleanup;
  }
  if (mf_test_managed_failure(&environment, &cuda, &nvml) != 0 ||
      mf_test_invalid_mode(&cuda, &nvml) != 0 ||
      mf_test_auto_managed(&environment, &fixture, &cuda, &nvml) != 0 ||
      mf_test_auto_managed_empty_cuda_filter(&environment, &fixture, &cuda, &nvml) != 0 ||
      mf_test_passthrough(&environment, &cuda, &nvml) != 0 ||
      mf_test_auto_fail_open_and_dirty(&environment, &cuda, &nvml) != 0 ||
      mf_test_stale_pair(&environment, &cuda, &nvml) != 0 ||
      mf_test_nvml_final_teardown(&environment, &cuda, &nvml) != 0) {
    goto cleanup_loaded;
  }
  result = 0;

cleanup_loaded:
  mf_reset(&cuda, &nvml);
cleanup:
  if (nvml_library != (void*)0) {
    (void)dlclose(nvml_library);
  }
  if (cuda_library != (void*)0) {
    (void)dlclose(cuda_library);
  }
  mf_registry_fixture_destroy(&fixture);
  mf_environment_destroy(&environment);
  return result;
}
