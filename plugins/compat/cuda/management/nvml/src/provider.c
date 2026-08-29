#include "managed-renames.h"

#define METAFLUX_NVML_ABI_INTERNAL 1
#include "metaflux/nvml/provider.h"

#include "metaflux/client/fastpath.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MF_NVML_PROCESS_CAPACITY UINT32_C(64)
#define MF_NVML_EVENT_SET_CAPACITY UINT32_C(16)
#define MF_NVML_EXPORT_TABLE_SIZE UINT64_C(0x948)
#define MF_NVML_EXPORT_SLOT_COUNT ((MF_NVML_EXPORT_TABLE_SIZE - UINT64_C(8)) / UINT64_C(8))
#define MF_NVML_EXPORT_PROCESS_NAME_SLOT UINT32_C(80)
#define MF_NVML_EXPORT_COMPUTE_PROCESSES_SLOT UINT32_C(212)
#define MF_NVML_EXPORT_GRAPHICS_PROCESSES_SLOT UINT32_C(213)
#define MF_NVML_EXPORT_MPS_PROCESSES_SLOT UINT32_C(214)
#define MF_NVML_EXPORT_PROCESS_NAME_CAPACITY UINT32_C(4096)
#define MF_NVML_HANDLE_TAG UINT64_C(0x4e56000000000000)
#define MF_NVML_HANDLE_TAG_MASK UINT64_C(0xffff000000000000)
#define MF_NVML_HANDLE_INDEX_MASK UINT64_C(0x0000ffff00000000)
#define MF_NVML_HANDLE_GENERATION_MASK UINT64_C(0x00000000ffffffff)
#define MF_NVML_EVENT_SET_TAG UINT64_C(0x4556000000000000)
#define MF_NVML_INIT_FLAG_NO_GPUS UINT32_C(1)
#if !defined(MF_NVML_TARGET_DRIVER_VERSION)
#define MF_NVML_TARGET_DRIVER_VERSION "610.43.02"
#endif
#if !defined(MF_NVML_TARGET_INTERFACE_VERSION)
#define MF_NVML_TARGET_INTERFACE_VERSION "13.610.43.02"
#endif
#if !defined(MF_NVML_TARGET_CUDA_DRIVER_VERSION)
#define MF_NVML_TARGET_CUDA_DRIVER_VERSION 13030
#endif

#define MF_NVML_PROCESS_COMPUTE UINT32_C(1)
#define MF_NVML_PROCESS_GRAPHICS UINT32_C(2)
#define MF_NVML_PROCESS_MPS UINT32_C(4)

typedef struct mf_nvml_process_row {
  uint32_t device_index;
  uint32_t pid;
  uint64_t used_gpu_memory;
  uint32_t gpu_instance_id;
  uint32_t compute_instance_id;
  uint32_t kinds;
  uint32_t reserved;
} mf_nvml_process_row;

typedef struct mf_nvml_event_set_record {
  uint32_t generation;
  uint32_t active;
} mf_nvml_event_set_record;

typedef nvmlReturn_t (*mf_nvml_export_function)(void);

typedef struct mf_nvml_export_process_info {
  uint32_t pid;
  uint32_t reserved;
  uint64_t used_gpu_memory;
  uint32_t gpu_instance_id;
  uint32_t compute_instance_id;
  char process_name[MF_NVML_EXPORT_PROCESS_NAME_CAPACITY];
} mf_nvml_export_process_info;

typedef struct mf_nvml_export_table {
  uint64_t size;
  mf_nvml_export_function slots[MF_NVML_EXPORT_SLOT_COUNT];
} mf_nvml_export_table;

static const uint8_t mf_nvml_stock_export_table_id[16] = {
    UINT8_C(0xc4), UINT8_C(0xfe), UINT8_C(0x3e), UINT8_C(0x6c), UINT8_C(0xc9), UINT8_C(0x8f),
    UINT8_C(0x6c), UINT8_C(0x4e), UINT8_C(0xa3), UINT8_C(0x27), UINT8_C(0xee), UINT8_C(0x69),
    UINT8_C(0x6e), UINT8_C(0x12), UINT8_C(0xf7), UINT8_C(0xc4)};

static mf_nvml_export_table mf_nvml_stock_export_table;
static uint32_t mf_nvml_stock_export_table_initialized;

static nvmlReturn_t mf_nvml_export_not_supported(void) { return NVML_ERROR_NOT_SUPPORTED; }
static nvmlReturn_t mf_nvml_export_compute_processes(nvmlDevice_t device, unsigned int* count,
                                                     mf_nvml_export_process_info* infos);
static nvmlReturn_t mf_nvml_export_graphics_processes(nvmlDevice_t device, unsigned int* count,
                                                      mf_nvml_export_process_info* infos);
static nvmlReturn_t mf_nvml_export_mps_processes(nvmlDevice_t device, unsigned int* count,
                                                 mf_nvml_export_process_info* infos);

_Static_assert(sizeof(mf_nvml_export_table) == MF_NVML_EXPORT_TABLE_SIZE,
               "selected nvidia-smi export-table ABI size");
_Static_assert(sizeof(mf_nvml_export_process_info) == UINT64_C(0x1018),
               "selected nvidia-smi process-record ABI size");

typedef struct mf_nvml_transport {
  int32_t registry_fd;
  mf_registry_view_id_v1 view_id;
  uint32_t configured;
  uint32_t process_data_available;
} mf_nvml_transport;

typedef struct mf_nvml_state {
  atomic_flag lock;
  mf_nvml_transport transport;
  mf_client_session_v1 session;
  mf_client_registry_v1 registry;
  mf_nvml_process_row processes[MF_NVML_PROCESS_CAPACITY];
  mf_nvml_event_set_record event_sets[MF_NVML_EVENT_SET_CAPACITY];
  uint32_t process_count;
  uint32_t reference_count;
  uint32_t initialization_generation;
} mf_nvml_state;

static mf_nvml_state mf_nvml_global = {.lock = ATOMIC_FLAG_INIT,
                                       .session = {.registry = {.owned_fd = -1},
                                                   .submission = {.owned_fd = -1},
                                                   .completion = {.owned_fd = -1},
                                                   .socket_fd = -1},
                                       .registry = {.owned_fd = -1}};

static void mf_nvml_lock(void) {
  while (atomic_flag_test_and_set_explicit(&mf_nvml_global.lock, memory_order_acquire)) {
  }
}

static void mf_nvml_unlock(void) {
  atomic_flag_clear_explicit(&mf_nvml_global.lock, memory_order_release);
}

static nvmlReturn_t mf_nvml_require_locked(void);

static nvmlReturn_t mf_nvml_status(mf_shared_status_v1 status) {
  switch (status) {
  case MF_SHARED_SUCCESS:
    return NVML_SUCCESS;
  case MF_SHARED_WOULD_BLOCK:
  case MF_SHARED_TIMEOUT:
  case MF_SHARED_INTERRUPTED:
  case MF_SHARED_RETRY:
    return NVML_ERROR_NOT_READY;
  case MF_SHARED_STALE_HANDLE:
  case MF_SHARED_DEVICE_LOST:
  case MF_SHARED_TERMINAL_VIEW:
    return NVML_ERROR_GPU_IS_LOST;
  case MF_SHARED_INVALID_ARGUMENT:
  case MF_SHARED_MALFORMED:
  case MF_SHARED_OVERFLOW:
    return NVML_ERROR_INVALID_ARGUMENT;
  case MF_SHARED_RESOURCE_EXHAUSTED:
    return NVML_ERROR_MEMORY;
  case MF_SHARED_NOT_SUPPORTED:
    return NVML_ERROR_NOT_SUPPORTED;
  case MF_SHARED_PERMISSION_DENIED:
    return NVML_ERROR_NO_PERMISSION;
  case MF_SHARED_SYSTEM_ERROR:
  default:
    return NVML_ERROR_UNKNOWN;
  }
}

static uint32_t mf_nvml_next_generation(uint32_t generation) {
  generation += UINT32_C(1);
  return generation == UINT32_C(0) ? UINT32_C(1) : generation;
}

static nvmlDevice_t mf_nvml_make_device(uint32_t index) {
  const uint64_t token = MF_NVML_HANDLE_TAG | ((uint64_t)(index + UINT32_C(1)) << 32U) |
                         (uint64_t)mf_nvml_global.initialization_generation;
  return (nvmlDevice_t)(uintptr_t)token;
}

static int mf_nvml_decode_device(nvmlDevice_t device, uint32_t* index, uint32_t* generation) {
  const uint64_t token = (uint64_t)(uintptr_t)device;
  const uint64_t encoded_index = (token & MF_NVML_HANDLE_INDEX_MASK) >> 32U;
  if ((token & MF_NVML_HANDLE_TAG_MASK) != MF_NVML_HANDLE_TAG || encoded_index == UINT64_C(0) ||
      encoded_index > UINT64_C(65535) || (token & MF_NVML_HANDLE_GENERATION_MASK) == UINT64_C(0)) {
    return 0;
  }
  *index = (uint32_t)(encoded_index - UINT64_C(1));
  *generation = (uint32_t)(token & MF_NVML_HANDLE_GENERATION_MASK);
  return 1;
}

static nvmlEventSet_t mf_nvml_make_event_set(uint32_t index, uint32_t generation) {
  const uint64_t token =
      MF_NVML_EVENT_SET_TAG | ((uint64_t)(index + UINT32_C(1)) << 32U) | (uint64_t)generation;
  return (nvmlEventSet_t)(uintptr_t)token;
}

static int mf_nvml_decode_event_set(nvmlEventSet_t set, uint32_t* index, uint32_t* generation) {
  const uint64_t token = (uint64_t)(uintptr_t)set;
  const uint64_t encoded_index = (token & MF_NVML_HANDLE_INDEX_MASK) >> 32U;
  if ((token & MF_NVML_HANDLE_TAG_MASK) != MF_NVML_EVENT_SET_TAG || encoded_index == UINT64_C(0) ||
      encoded_index > MF_NVML_EVENT_SET_CAPACITY ||
      (token & MF_NVML_HANDLE_GENERATION_MASK) == UINT64_C(0)) {
    return 0;
  }
  *index = (uint32_t)(encoded_index - UINT64_C(1));
  *generation = (uint32_t)(token & MF_NVML_HANDLE_GENERATION_MASK);
  return 1;
}

static nvmlReturn_t mf_nvml_validate_event_set_locked(nvmlEventSet_t set) {
  uint32_t index = 0;
  uint32_t generation = 0;
  nvmlReturn_t result = mf_nvml_require_locked();
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (!mf_nvml_decode_event_set(set, &index, &generation) ||
      mf_nvml_global.event_sets[index].active == UINT32_C(0) ||
      mf_nvml_global.event_sets[index].generation != generation) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_nvml_require_locked(void) {
  return mf_nvml_global.reference_count == UINT32_C(0) ? NVML_ERROR_UNINITIALIZED : NVML_SUCCESS;
}

static nvmlReturn_t mf_nvml_validate_device_locked(nvmlDevice_t device, uint32_t* out_index,
                                                   mf_virtual_device_identity_v1* out_identity,
                                                   mf_client_fence_snapshot_v1* out_fence) {
  uint32_t index = 0;
  uint32_t generation = 0;
  mf_generation_handle_v1 handle;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  nvmlReturn_t result = mf_nvml_require_locked();
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (!mf_nvml_decode_device(device, &index, &generation) ||
      generation != mf_nvml_global.initialization_generation ||
      index >= mf_client_registry_device_count_v1(&mf_nvml_global.registry)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  status = mf_client_registry_make_handle_v1(&mf_nvml_global.registry, index,
                                             (uint64_t)index + UINT64_C(1), UINT64_C(1),
                                             MF_OBJECT_TYPE_CONTEXT, &handle);
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_registry_validate_device_v1(&mf_nvml_global.registry, &handle, out_fence);
  }
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_registry_identity_v1(&mf_nvml_global.registry, index, out_identity);
  }
  if (status == MF_SHARED_SUCCESS) {
    *out_index = index;
  }
  return mf_nvml_status(status);
}

static nvmlReturn_t mf_nvml_control_status(uint32_t status) {
  switch (status) {
  case MF_CLIENT_CONTROL_OK:
    return NVML_SUCCESS;
  case MF_CLIENT_CONTROL_STALE_GENERATION:
  case MF_CLIENT_CONTROL_NOT_FOUND:
    return NVML_ERROR_GPU_IS_LOST;
  case MF_CLIENT_CONTROL_INVALID_ARGUMENT:
  case MF_CLIENT_CONTROL_MALFORMED:
    return NVML_ERROR_INVALID_ARGUMENT;
  case MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED:
    return NVML_ERROR_MEMORY;
  case MF_CLIENT_CONTROL_UNSUPPORTED:
    return NVML_ERROR_NOT_SUPPORTED;
  case MF_CLIENT_CONTROL_NO_PERMISSION:
    return NVML_ERROR_NO_PERMISSION;
  case MF_CLIENT_CONTROL_INTERNAL_ERROR:
  default:
    return NVML_ERROR_UNKNOWN;
  }
}

static nvmlReturn_t mf_nvml_set_policy_locked(nvmlDevice_t device, uint16_t opcode,
                                              uint64_t argument, uint64_t policy_mask,
                                              uint64_t policy_value) {
  mf_client_control_response_v1 response;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  uint32_t index = 0;
  nvmlReturn_t result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (mf_nvml_global.session.socket_fd < 0 || (mf_nvml_global.session.negotiated_capabilities &
                                               MF_CLIENT_CAP_POLICY_SETTERS_V1) == UINT64_C(0)) {
    return NVML_ERROR_NOT_SUPPORTED;
  }
  {
    const mf_shared_status_v1 status = mf_client_session_control_v1(
        &mf_nvml_global.session, opcode, UINT16_C(0), identity.identity_record_id, argument, -1,
        &response, (int32_t*)0);
    if (status != MF_SHARED_SUCCESS) {
      return mf_nvml_status(status);
    }
  }
  result = mf_nvml_control_status(mf_client_load_le32_v1(response.bytes + 12));
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (mf_client_load_le64_v1(response.bytes + 48) != identity.identity_record_id ||
      mf_client_load_le64_v1(response.bytes + 56) == UINT64_C(0)) {
    return NVML_ERROR_UNKNOWN;
  }
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (fence.lifecycle_sequence < mf_client_load_le64_v1(response.bytes + 56) ||
      (fence.policy_bits & policy_mask) != policy_value) {
    return NVML_ERROR_UNKNOWN;
  }
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_nvml_read_telemetry_locked(uint32_t index,
                                                  mf_client_telemetry_snapshot_v1* out_snapshot) {
  mf_generation_handle_v1 handle;
  mf_shared_status_v1 status = mf_client_registry_make_handle_v1(
      &mf_nvml_global.registry, index, (uint64_t)index + UINT64_C(1), UINT64_C(1),
      MF_OBJECT_TYPE_CONTEXT, &handle);
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_registry_read_telemetry_v1(&mf_nvml_global.registry, &handle, out_snapshot);
  }
  return mf_nvml_status(status);
}

static nvmlReturn_t mf_nvml_copy_string(char* output, unsigned int length, const char* value) {
  const size_t required = strlen(value) + (size_t)1;
  if (output == (char*)0 || length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  if (required > (size_t)length) {
    return NVML_ERROR_INSUFFICIENT_SIZE;
  }
  (void)memcpy(output, value, required);
  return NVML_SUCCESS;
}

static void mf_nvml_format_uuid(const uint8_t bytes[16], char output[37]) {
  (void)snprintf(output, (size_t)37,
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                 "%02x%02x%02x%02x%02x%02x",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                 bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14],
                 bytes[15]);
}

static nvmlReturn_t mf_nvml_stub_device(nvmlDevice_t device) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  return mf_nvml_validate_device_locked(device, &index, &identity, &fence);
}

static void mf_nvml_take_session_registry_locked(void) {
  mf_nvml_global.registry = mf_nvml_global.session.registry;
  (void)memset(&mf_nvml_global.session.registry, 0, sizeof(mf_nvml_global.session.registry));
  mf_nvml_global.session.registry.owned_fd = -1;
}

static void mf_nvml_close_locked(void) {
  uint32_t index = 0;
  if (mf_nvml_global.session.socket_fd >= 0) {
    mf_client_session_close_v1(&mf_nvml_global.session);
  }
  if (mf_nvml_global.registry.mapping != (void*)0) {
    mf_client_registry_close_v1(&mf_nvml_global.registry);
  }
  for (index = UINT32_C(0); index < MF_NVML_EVENT_SET_CAPACITY; ++index) {
    mf_nvml_global.event_sets[index].active = UINT32_C(0);
  }
}

uint32_t mf_nvml_provider_bootstrap_abi_version(void) {
  return mf_client_fastpath_bootstrap_abi_version();
}

void mf_nvml_provider_managed_rollback_v1(void) {
  mf_nvml_lock();
  mf_nvml_global.reference_count = UINT32_C(0);
  mf_nvml_close_locked();
  mf_nvml_unlock();
}

int32_t mf_nvml_provider_managed_is_pristine_v1(void) {
  int32_t pristine = INT32_C(0);
  mf_nvml_lock();
  pristine = mf_nvml_global.reference_count == UINT32_C(0) &&
                     mf_nvml_global.session.socket_fd < 0 &&
                     mf_nvml_global.registry.mapping == (void*)0
                 ? INT32_C(1)
                 : INT32_C(0);
  mf_nvml_unlock();
  return pristine;
}

#if defined(METAFLUX_PROVIDER_TESTING)
int mf_nvml_provider_test_install_transport_v1(
    const mf_nvml_provider_test_transport_v1* transport) {
  uint32_t index = 0;
  if (transport == (const mf_nvml_provider_test_transport_v1*)0 || transport->registry_fd < 0 ||
      transport->registry_view_id.daemon_incarnation == UINT64_C(0) ||
      transport->registry_view_id.view_serial == UINT64_C(0) ||
      transport->process_count > MF_NVML_PROCESS_CAPACITY ||
      (transport->process_count != UINT32_C(0) &&
       transport->processes == (const mf_nvml_provider_test_process_v1*)0)) {
    return -1;
  }
  mf_nvml_lock();
  if (mf_nvml_global.reference_count != UINT32_C(0)) {
    mf_nvml_unlock();
    return -1;
  }
  mf_nvml_global.transport.registry_fd = transport->registry_fd;
  mf_nvml_global.transport.view_id = transport->registry_view_id;
  mf_nvml_global.transport.configured = UINT32_C(1);
  mf_nvml_global.transport.process_data_available = transport->process_data_available;
  mf_nvml_global.process_count = transport->process_count;
  (void)memset(mf_nvml_global.processes, 0, sizeof(mf_nvml_global.processes));
  for (index = 0; index < transport->process_count; ++index) {
    mf_nvml_global.processes[index].device_index = transport->processes[index].device_index;
    mf_nvml_global.processes[index].pid = transport->processes[index].pid;
    mf_nvml_global.processes[index].used_gpu_memory = transport->processes[index].used_gpu_memory;
    mf_nvml_global.processes[index].gpu_instance_id = transport->processes[index].gpu_instance_id;
    mf_nvml_global.processes[index].compute_instance_id =
        transport->processes[index].compute_instance_id;
    mf_nvml_global.processes[index].kinds = transport->processes[index].kinds;
  }
  mf_nvml_unlock();
  return 0;
}

void mf_nvml_provider_test_reset_managed_v1(void) {
  mf_nvml_lock();
  mf_nvml_close_locked();
  (void)memset(&mf_nvml_global.transport, 0, sizeof(mf_nvml_global.transport));
  (void)memset(mf_nvml_global.processes, 0, sizeof(mf_nvml_global.processes));
  mf_nvml_global.process_count = UINT32_C(0);
  mf_nvml_global.reference_count = UINT32_C(0);
  mf_nvml_unlock();
}
#endif

nvmlReturn_t nvmlInitWithFlags(unsigned int flags) {
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if ((flags & ~(unsigned int)MF_NVML_INIT_FLAG_NO_GPUS) != UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  if (mf_nvml_global.reference_count != UINT32_C(0)) {
    if (mf_nvml_global.reference_count == UINT32_MAX) {
      mf_nvml_unlock();
      return NVML_ERROR_UNKNOWN;
    }
    mf_nvml_global.reference_count += UINT32_C(1);
    mf_nvml_unlock();
    return NVML_SUCCESS;
  }
  if (mf_nvml_global.transport.configured != UINT32_C(0)) {
    status =
        mf_client_registry_attach_v1(mf_nvml_global.transport.registry_fd,
                                     mf_nvml_global.transport.view_id, &mf_nvml_global.registry);
  } else {
    status = mf_client_observer_connect_default_v1(&mf_nvml_global.session);
    if (status == MF_SHARED_SUCCESS) {
      mf_nvml_take_session_registry_locked();
    }
  }
  if (status == MF_SHARED_SUCCESS) {
    mf_nvml_global.initialization_generation =
        mf_nvml_next_generation(mf_nvml_global.initialization_generation);
    mf_nvml_global.reference_count = UINT32_C(1);
  }
  mf_nvml_unlock();
  return status == MF_SHARED_SYSTEM_ERROR ? NVML_ERROR_DRIVER_NOT_LOADED : mf_nvml_status(status);
}

nvmlReturn_t nvmlInit_v2(void) { return nvmlInitWithFlags(UINT32_C(0)); }

nvmlReturn_t nvmlInit(void) { return nvmlInit_v2(); }

nvmlReturn_t nvmlInternalGetExportTable(const void** export_table, const void* export_table_id) {
  size_t slot = 0;
  nvmlReturn_t (*process_name)(unsigned int, char*, unsigned int) = nvmlSystemGetProcessName;
  nvmlReturn_t (*compute_processes)(nvmlDevice_t, unsigned int*, mf_nvml_export_process_info*) =
      mf_nvml_export_compute_processes;
  nvmlReturn_t (*graphics_processes)(nvmlDevice_t, unsigned int*, mf_nvml_export_process_info*) =
      mf_nvml_export_graphics_processes;
  nvmlReturn_t (*mps_processes)(nvmlDevice_t, unsigned int*, mf_nvml_export_process_info*) =
      mf_nvml_export_mps_processes;
  if (export_table == (const void**)0 || export_table_id == (const void*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  *export_table = (const void*)0;
  if (memcmp(export_table_id, mf_nvml_stock_export_table_id,
             sizeof(mf_nvml_stock_export_table_id)) != 0) {
    return NVML_ERROR_NOT_SUPPORTED;
  }
  mf_nvml_lock();
  if (mf_nvml_stock_export_table_initialized == UINT32_C(0)) {
    mf_nvml_stock_export_table.size = MF_NVML_EXPORT_TABLE_SIZE;
    for (slot = (size_t)2; slot < (size_t)MF_NVML_EXPORT_SLOT_COUNT; ++slot) {
      mf_nvml_stock_export_table.slots[slot] = mf_nvml_export_not_supported;
    }
    _Static_assert(sizeof(process_name) == sizeof(mf_nvml_stock_export_table.slots[0]),
                   "NVML export-table function pointers must have one representation");
    (void)memcpy(&mf_nvml_stock_export_table.slots[MF_NVML_EXPORT_PROCESS_NAME_SLOT], &process_name,
                 sizeof(process_name));
    _Static_assert(sizeof(compute_processes) == sizeof(mf_nvml_stock_export_table.slots[0]),
                   "NVML export-table function pointers must have one representation");
    (void)memcpy(&mf_nvml_stock_export_table.slots[MF_NVML_EXPORT_COMPUTE_PROCESSES_SLOT],
                 &compute_processes, sizeof(compute_processes));
    _Static_assert(sizeof(graphics_processes) == sizeof(mf_nvml_stock_export_table.slots[0]),
                   "NVML export-table function pointers must have one representation");
    _Static_assert(sizeof(mps_processes) == sizeof(mf_nvml_stock_export_table.slots[0]),
                   "NVML export-table function pointers must have one representation");
    (void)memcpy(&mf_nvml_stock_export_table.slots[MF_NVML_EXPORT_GRAPHICS_PROCESSES_SLOT],
                 &graphics_processes, sizeof(graphics_processes));
    (void)memcpy(&mf_nvml_stock_export_table.slots[MF_NVML_EXPORT_MPS_PROCESSES_SLOT],
                 &mps_processes, sizeof(mps_processes));
    mf_nvml_stock_export_table_initialized = UINT32_C(1);
  }
  *export_table = &mf_nvml_stock_export_table;
  mf_nvml_unlock();
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlShutdown(void) {
  mf_nvml_lock();
  if (mf_nvml_global.reference_count == UINT32_C(0)) {
    mf_nvml_unlock();
    return NVML_ERROR_UNINITIALIZED;
  }
  mf_nvml_global.reference_count -= UINT32_C(1);
  if (mf_nvml_global.reference_count == UINT32_C(0)) {
    mf_nvml_close_locked();
  }
  mf_nvml_unlock();
  return NVML_SUCCESS;
}

const char* nvmlErrorString(nvmlReturn_t result) {
  switch (result) {
  case NVML_SUCCESS:
    return "Success";
  case NVML_ERROR_UNINITIALIZED:
    return "Uninitialized";
  case NVML_ERROR_INVALID_ARGUMENT:
    return "Invalid Argument";
  case NVML_ERROR_NOT_SUPPORTED:
    return "Not Supported";
  case NVML_ERROR_NO_PERMISSION:
    return "Insufficient Permissions";
  case NVML_ERROR_ALREADY_INITIALIZED:
    return "Already Initialized";
  case NVML_ERROR_NOT_FOUND:
    return "Not Found";
  case NVML_ERROR_INSUFFICIENT_SIZE:
    return "Insufficient Size";
  case NVML_ERROR_INSUFFICIENT_POWER:
    return "Insufficient Power";
  case NVML_ERROR_DRIVER_NOT_LOADED:
    return "Driver Not Loaded";
  case NVML_ERROR_TIMEOUT:
    return "Timeout";
  case NVML_ERROR_IRQ_ISSUE:
    return "Interrupt Request Issue";
  case NVML_ERROR_LIBRARY_NOT_FOUND:
    return "Library Not Found";
  case NVML_ERROR_FUNCTION_NOT_FOUND:
    return "Function Not Found";
  case NVML_ERROR_CORRUPTED_INFOROM:
    return "Corrupted InfoROM";
  case NVML_ERROR_GPU_IS_LOST:
    return "GPU is Lost";
  case NVML_ERROR_RESET_REQUIRED:
    return "Reset Required";
  case NVML_ERROR_OPERATING_SYSTEM:
    return "Operating System Error";
  case NVML_ERROR_LIB_RM_VERSION_MISMATCH:
    return "Driver/library Version Mismatch";
  case NVML_ERROR_IN_USE:
    return "Resource In Use";
  case NVML_ERROR_MEMORY:
    return "Insufficient Memory";
  case NVML_ERROR_NO_DATA:
    return "No Data";
  case NVML_ERROR_VGPU_ECC_NOT_SUPPORTED:
    return "vGPU ECC Not Supported";
  case NVML_ERROR_INSUFFICIENT_RESOURCES:
    return "Insufficient Resources";
  case NVML_ERROR_FREQ_NOT_SUPPORTED:
    return "Frequency Not Supported";
  case NVML_ERROR_ARGUMENT_VERSION_MISMATCH:
    return "Argument Version Mismatch";
  case NVML_ERROR_DEPRECATED:
    return "Deprecated";
  case NVML_ERROR_NOT_READY:
    return "Not Ready";
  case NVML_ERROR_GPU_NOT_FOUND:
    return "GPU Not Found";
  case NVML_ERROR_INVALID_STATE:
    return "Invalid State";
  case NVML_ERROR_UNKNOWN:
  default:
    return "Unknown Error";
  }
}

static nvmlReturn_t mf_nvml_system_string(char* output, unsigned int length, const char* value) {
  nvmlReturn_t result = NVML_SUCCESS;
  mf_nvml_lock();
  result = mf_nvml_require_locked();
  if (result == NVML_SUCCESS) {
    result = mf_nvml_copy_string(output, length, value);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlSystemGetDriverVersion(char* version, unsigned int length) {
  return mf_nvml_system_string(version, length, MF_NVML_TARGET_DRIVER_VERSION);
}

nvmlReturn_t nvmlSystemGetNVMLVersion(char* version, unsigned int length) {
  return mf_nvml_system_string(version, length, MF_NVML_TARGET_INTERFACE_VERSION);
}

nvmlReturn_t nvmlSystemGetCudaDriverVersion_v2(int* version) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (version == (int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_require_locked();
  if (result == NVML_SUCCESS) {
    *version = MF_NVML_TARGET_CUDA_DRIVER_VERSION;
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlSystemGetCudaDriverVersion(int* version) {
  return nvmlSystemGetCudaDriverVersion_v2(version);
}

nvmlReturn_t nvmlSystemGetProcessName(unsigned int pid, char* name, unsigned int length) {
  nvmlReturn_t result = NVML_SUCCESS;
  mf_client_process_snapshot_v1 snapshot = {.owned_fd = -1};
  mf_client_process_snapshot_row_v1 rows[MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1];
  uint32_t count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  uint32_t index = 0;
  if (pid == UINT32_C(0) || name == (char*)0 || length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_require_locked();
  if (result == NVML_SUCCESS && mf_nvml_global.transport.configured != UINT32_C(0)) {
    result = NVML_ERROR_NOT_SUPPORTED;
  }
  if (result == NVML_SUCCESS) {
    result =
        mf_nvml_status(mf_client_process_snapshot_fetch_v1(&mf_nvml_global.session, &snapshot));
  }
  if (result == NVML_SUCCESS) {
    result = mf_nvml_status(mf_client_process_snapshot_fill_v1(&snapshot, &count, rows));
  }
  if (result == NVML_SUCCESS) {
    result = NVML_ERROR_NOT_FOUND;
    for (index = 0; index < count; ++index) {
      if (rows[index].pid == pid) {
        result = mf_nvml_copy_string(name, length, rows[index].process_name);
        break;
      }
    }
  }
  mf_client_process_snapshot_close_v1(&snapshot);
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int* count) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (count == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_require_locked();
  if (result == NVML_SUCCESS) {
    *count = mf_client_registry_device_count_v1(&mf_nvml_global.registry);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetCount(unsigned int* count) { return nvmlDeviceGetCount_v2(count); }

nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t* device) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (device == (nvmlDevice_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_require_locked();
  if (result == NVML_SUCCESS &&
      index >= mf_client_registry_device_count_v1(&mf_nvml_global.registry)) {
    result = NVML_ERROR_INVALID_ARGUMENT;
  }
  if (result == NVML_SUCCESS) {
    *device = mf_nvml_make_device(index);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t* device) {
  return nvmlDeviceGetHandleByIndex_v2(index, device);
}

static nvmlReturn_t mf_nvml_identity(nvmlDevice_t device, uint32_t* index,
                                     mf_virtual_device_identity_v1* identity,
                                     mf_client_fence_snapshot_v1* fence) {
  nvmlReturn_t result = NVML_SUCCESS;
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, index, identity, fence);
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetIndex(nvmlDevice_t device, unsigned int* index) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  uint32_t decoded_index = 0;
  nvmlReturn_t result = NVML_SUCCESS;
  if (index == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  result = mf_nvml_identity(device, &decoded_index, &identity, &fence);
  if (result == NVML_SUCCESS) {
    *index = decoded_index;
  }
  return result;
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char* name, unsigned int length) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  uint32_t index = 0;
  char value[sizeof(identity.display_name) + (size_t)1];
  size_t source_length = 0;
  nvmlReturn_t result = mf_nvml_identity(device, &index, &identity, &fence);
  if (result != NVML_SUCCESS) {
    return result;
  }
  source_length = 0;
  while (source_length < sizeof(identity.display_name) &&
         identity.display_name[source_length] != UINT8_C(0)) {
    value[source_length] = (char)identity.display_name[source_length];
    ++source_length;
  }
  value[source_length] = '\0';
  return mf_nvml_copy_string(name, length, value);
}

nvmlReturn_t nvmlDeviceGetUUID(nvmlDevice_t device, char* uuid, unsigned int length) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  uint32_t index = 0;
  char raw[37];
  char value[41];
  nvmlReturn_t result = mf_nvml_identity(device, &index, &identity, &fence);
  if (result != NVML_SUCCESS) {
    return result;
  }
  mf_nvml_format_uuid(identity.gpu_uuid, raw);
  (void)snprintf(value, sizeof(value), "GPU-%s", raw);
  return mf_nvml_copy_string(uuid, length, value);
}

nvmlReturn_t nvmlDeviceGetHandleByUUID(const char* uuid, nvmlDevice_t* device) {
  unsigned int count = 0;
  unsigned int index = 0;
  char candidate[NVML_DEVICE_UUID_BUFFER_SIZE];
  nvmlReturn_t result = NVML_SUCCESS;
  if (uuid == (const char*)0 || device == (nvmlDevice_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  result = nvmlDeviceGetCount_v2(&count);
  if (result != NVML_SUCCESS) {
    return result;
  }
  for (index = 0; index < count; ++index) {
    nvmlDevice_t candidate_device = (nvmlDevice_t)0;
    result = nvmlDeviceGetHandleByIndex_v2(index, &candidate_device);
    if (result == NVML_SUCCESS) {
      result = nvmlDeviceGetUUID(candidate_device, candidate, sizeof(candidate));
    }
    if (result == NVML_SUCCESS && strcmp(uuid, candidate) == 0) {
      *device = candidate_device;
      return NVML_SUCCESS;
    }
  }
  return NVML_ERROR_NOT_FOUND;
}

static int mf_nvml_parse_pci(const char* value, unsigned int* domain, unsigned int* bus,
                             unsigned int* device, unsigned int* function) {
  int consumed = 0;
  if (sscanf(value, "%x:%x:%x.%x%n", domain, bus, device, function, &consumed) == 4 &&
      value[consumed] == '\0' && *domain <= UINT32_C(0xffff) && *bus <= UINT32_C(0xff) &&
      *device <= UINT32_C(0x1f) && *function <= UINT32_C(7)) {
    return 1;
  }
  *domain = UINT32_C(0);
  *bus = UINT32_C(0);
  *device = UINT32_C(0);
  *function = UINT32_C(0);
  if (sscanf(value, "%x:%x.%x%n", bus, device, function, &consumed) == 3 &&
      value[consumed] == '\0' && *bus <= UINT32_C(0xff) && *device <= UINT32_C(0x1f) &&
      *function <= UINT32_C(7)) {
    return 1;
  }
  return 0;
}

nvmlReturn_t nvmlDeviceGetHandleByPciBusId_v2(const char* pci_bus_id, nvmlDevice_t* device) {
  unsigned int domain = 0;
  unsigned int bus = 0;
  unsigned int pci_device = 0;
  unsigned int function = 0;
  unsigned int count = 0;
  unsigned int index = 0;
  nvmlReturn_t result = NVML_SUCCESS;
  if (pci_bus_id == (const char*)0 || device == (nvmlDevice_t*)0 ||
      !mf_nvml_parse_pci(pci_bus_id, &domain, &bus, &pci_device, &function)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  result = nvmlDeviceGetCount_v2(&count);
  if (result != NVML_SUCCESS) {
    return result;
  }
  for (index = 0; index < count; ++index) {
    mf_virtual_device_identity_v1 identity;
    mf_client_fence_snapshot_v1 fence;
    nvmlDevice_t candidate = (nvmlDevice_t)0;
    uint32_t decoded_index = 0;
    result = nvmlDeviceGetHandleByIndex_v2(index, &candidate);
    if (result == NVML_SUCCESS) {
      result = mf_nvml_identity(candidate, &decoded_index, &identity, &fence);
    }
    if (result == NVML_SUCCESS && identity.pci_domain == domain && identity.pci_bus == bus &&
        identity.pci_device == pci_device && identity.pci_function == function) {
      *device = candidate;
      return NVML_SUCCESS;
    }
  }
  return NVML_ERROR_NOT_FOUND;
}

nvmlReturn_t nvmlDeviceGetHandleByPciBusId(const char* pci_bus_id, nvmlDevice_t* device) {
  return nvmlDeviceGetHandleByPciBusId_v2(pci_bus_id, device);
}

static nvmlReturn_t mf_nvml_pci_info(nvmlDevice_t device, nvmlPciInfo_t* pci) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  nvmlReturn_t result = NVML_SUCCESS;
  if (pci == (nvmlPciInfo_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS &&
      (identity.pci_domain > UINT32_C(0xffff) || identity.pci_bus > UINT32_C(0xff) ||
       identity.pci_device > UINT32_C(0x1f) || identity.pci_function > UINT32_C(7))) {
    result = NVML_ERROR_UNKNOWN;
  }
  if (result == NVML_SUCCESS) {
    (void)memset(pci, 0, sizeof(*pci));
    pci->domain = identity.pci_domain;
    pci->bus = identity.pci_bus;
    pci->device = identity.pci_device;
    (void)snprintf(pci->busIdLegacy, sizeof(pci->busIdLegacy), "%04x:%02x:%02x.%x",
                   identity.pci_domain, identity.pci_bus, identity.pci_device,
                   identity.pci_function);
    (void)snprintf(pci->busId, sizeof(pci->busId), "%08x:%02x:%02x.%x", identity.pci_domain,
                   identity.pci_bus, identity.pci_device, identity.pci_function);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t* pci) {
  return mf_nvml_pci_info(device, pci);
}

nvmlReturn_t nvmlDeviceGetPciInfo_v2(nvmlDevice_t device, nvmlPciInfo_t* pci) {
  return mf_nvml_pci_info(device, pci);
}

nvmlReturn_t nvmlDeviceGetPciInfo_v3(nvmlDevice_t device, nvmlPciInfo_t* pci) {
  return mf_nvml_pci_info(device, pci);
}

nvmlReturn_t nvmlDeviceGetPciInfoExt(nvmlDevice_t device, nvmlPciInfoExt_t* pci) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  nvmlReturn_t result = NVML_SUCCESS;
  if (pci == (nvmlPciInfoExt_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  if (pci->version != nvmlPciInfoExt_v1) {
    return NVML_ERROR_ARGUMENT_VERSION_MISMATCH;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS &&
      (identity.pci_bus > UINT32_C(0xff) || identity.pci_device > UINT32_C(0x1f) ||
       identity.pci_function > UINT32_C(7))) {
    result = NVML_ERROR_UNKNOWN;
  }
  if (result == NVML_SUCCESS) {
    (void)memset(pci, 0, sizeof(*pci));
    pci->version = nvmlPciInfoExt_v1;
    pci->domain = identity.pci_domain;
    pci->bus = identity.pci_bus;
    pci->device = identity.pci_device;
    (void)snprintf(pci->busId, sizeof(pci->busId), "%08x:%02x:%02x.%x", identity.pci_domain,
                   identity.pci_bus, identity.pci_device, identity.pci_function);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t* memory) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  mf_client_telemetry_snapshot_v1 telemetry;
  nvmlReturn_t result = NVML_SUCCESS;
  if (memory == (nvmlMemory_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS) {
    result = mf_nvml_read_telemetry_locked(index, &telemetry);
  }
  if (result == NVML_SUCCESS && telemetry.memory_used_bytes > telemetry.memory_capacity_bytes) {
    result = NVML_ERROR_UNKNOWN;
  }
  if (result == NVML_SUCCESS) {
    memory->total = (unsigned long long)telemetry.memory_capacity_bytes;
    memory->used = (unsigned long long)telemetry.memory_used_bytes;
    memory->free =
        (unsigned long long)(telemetry.memory_capacity_bytes - telemetry.memory_used_bytes);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(nvmlDevice_t device, nvmlMemory_v2_t* memory) {
  nvmlMemory_t base;
  nvmlReturn_t result = NVML_SUCCESS;
  if (memory == (nvmlMemory_v2_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  if (memory->version != nvmlMemory_v2) {
    return NVML_ERROR_ARGUMENT_VERSION_MISMATCH;
  }
  result = nvmlDeviceGetMemoryInfo(device, &base);
  if (result == NVML_SUCCESS) {
    memory->total = base.total;
    memory->reserved = UINT64_C(0);
    memory->free = base.free;
    memory->used = base.used;
  }
  return result;
}

nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t* utilization) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  mf_client_telemetry_snapshot_v1 telemetry;
  nvmlReturn_t result = NVML_SUCCESS;
  if (utilization == (nvmlUtilization_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS) {
    result = mf_nvml_read_telemetry_locked(index, &telemetry);
  }
  if (result == NVML_SUCCESS) {
    const uint64_t window_ns = UINT64_C(100000000);
    utilization->gpu = telemetry.active_time_ns >= window_ns
                           ? UINT32_C(100)
                           : (unsigned int)(telemetry.active_time_ns / UINT64_C(1000000));
    utilization->memory = telemetry.memory_active_time_ns >= window_ns
                              ? UINT32_C(100)
                              : (unsigned int)(telemetry.memory_active_time_ns / UINT64_C(1000000));
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetCudaComputeCapability(nvmlDevice_t device, int* major, int* minor) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  nvmlReturn_t result = NVML_SUCCESS;
  if (major == (int*)0 || minor == (int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  result = mf_nvml_identity(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS) {
    *major = (int)(identity.virtual_compute_capability / UINT32_C(10));
    *minor = (int)(identity.virtual_compute_capability % UINT32_C(10));
  }
  return result;
}

static void mf_nvml_write_process_info(void* infos, uint32_t output_index,
                                       uint32_t structure_version, uint32_t pid,
                                       uint64_t used_memory) {
  if (structure_version == UINT32_C(1)) {
    nvmlProcessInfo_v1_t* output = (nvmlProcessInfo_v1_t*)infos;
    output[output_index].pid = pid;
    output[output_index].usedGpuMemory = (unsigned long long)used_memory;
  } else {
    nvmlProcessInfo_v2_t* output = (nvmlProcessInfo_v2_t*)infos;
    output[output_index].pid = pid;
    output[output_index].usedGpuMemory = (unsigned long long)used_memory;
    output[output_index].gpuInstanceId = (unsigned int)NVML_VALUE_NOT_AVAILABLE;
    output[output_index].computeInstanceId = (unsigned int)NVML_VALUE_NOT_AVAILABLE;
  }
}

static nvmlReturn_t
mf_nvml_production_processes_locked(const mf_virtual_device_identity_v1* identity,
                                    unsigned int* count, void* infos, uint32_t kind,
                                    uint32_t structure_version) {
  mf_client_process_snapshot_v1 snapshot = {.owned_fd = -1};
  mf_client_process_snapshot_row_v1 rows[MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1];
  const uint32_t capacity = *count;
  uint32_t row_count = MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1;
  uint32_t row_index = 0;
  uint32_t required = 0;
  uint32_t output_index = 0;
  uint32_t snapshot_kind = 0;
  nvmlReturn_t result =
      mf_nvml_status(mf_client_process_snapshot_fetch_v1(&mf_nvml_global.session, &snapshot));
  if (kind == MF_NVML_PROCESS_COMPUTE) {
    snapshot_kind = MF_CLIENT_PROCESS_KIND_COMPUTE_V1;
  } else if (kind == MF_NVML_PROCESS_GRAPHICS) {
    snapshot_kind = MF_CLIENT_PROCESS_KIND_GRAPHICS_V1;
  } else if (kind == MF_NVML_PROCESS_MPS) {
    snapshot_kind = MF_CLIENT_PROCESS_KIND_MPS_V1;
  }
  if (result == NVML_SUCCESS) {
    result = mf_nvml_status(mf_client_process_snapshot_fill_v1(&snapshot, &row_count, rows));
  }
  if (result != NVML_SUCCESS) {
    mf_client_process_snapshot_close_v1(&snapshot);
    return result;
  }
  for (row_index = 0; row_index < row_count; ++row_index) {
    if (rows[row_index].identity_record_id == identity->identity_record_id &&
        rows[row_index].device_generation == identity->committed_generation &&
        (rows[row_index].kinds & snapshot_kind) != UINT32_C(0)) {
      ++required;
    }
  }
  *count = required;
  if (required == UINT32_C(0)) {
    mf_client_process_snapshot_close_v1(&snapshot);
    return NVML_SUCCESS;
  }
  if (capacity < required) {
    mf_client_process_snapshot_close_v1(&snapshot);
    return NVML_ERROR_INSUFFICIENT_SIZE;
  }
  if (infos == (void*)0) {
    mf_client_process_snapshot_close_v1(&snapshot);
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  for (row_index = 0; row_index < row_count; ++row_index) {
    if (rows[row_index].identity_record_id != identity->identity_record_id ||
        rows[row_index].device_generation != identity->committed_generation ||
        (rows[row_index].kinds & snapshot_kind) == UINT32_C(0)) {
      continue;
    }
    mf_nvml_write_process_info(infos, output_index, structure_version, rows[row_index].pid,
                               rows[row_index].used_memory_bytes);
    ++output_index;
  }
  mf_client_process_snapshot_close_v1(&snapshot);
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_nvml_processes_locked(nvmlDevice_t device, unsigned int* count, void* infos,
                                             uint32_t kind, uint32_t structure_version) {
  uint32_t device_index = 0;
  uint32_t row_index = 0;
  uint32_t required = 0;
  uint32_t output_index = 0;
  const uint32_t capacity = count == (unsigned int*)0 ? UINT32_C(0) : *count;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  nvmlReturn_t result = NVML_SUCCESS;
  if (count == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  result = mf_nvml_validate_device_locked(device, &device_index, &identity, &fence);
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (mf_nvml_global.transport.configured == UINT32_C(0)) {
    return mf_nvml_production_processes_locked(&identity, count, infos, kind, structure_version);
  }
  if (mf_nvml_global.transport.process_data_available == UINT32_C(0)) {
    return NVML_ERROR_NOT_SUPPORTED;
  }
  for (row_index = 0; row_index < mf_nvml_global.process_count; ++row_index) {
    const mf_nvml_process_row* row = &mf_nvml_global.processes[row_index];
    if (row->device_index == device_index && (row->kinds & kind) != UINT32_C(0)) {
      required += UINT32_C(1);
    }
  }
  *count = required;
  if (required == UINT32_C(0)) {
    return NVML_SUCCESS;
  }
  if (capacity < required) {
    return NVML_ERROR_INSUFFICIENT_SIZE;
  }
  if (infos == (void*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  for (row_index = 0; row_index < mf_nvml_global.process_count; ++row_index) {
    const mf_nvml_process_row* row = &mf_nvml_global.processes[row_index];
    if (row->device_index != device_index || (row->kinds & kind) == UINT32_C(0)) {
      continue;
    }
    mf_nvml_write_process_info(infos, output_index, structure_version, row->pid,
                               row->used_gpu_memory);
    if (structure_version != UINT32_C(1)) {
      nvmlProcessInfo_v2_t* output = (nvmlProcessInfo_v2_t*)infos;
      output[output_index].gpuInstanceId = row->gpu_instance_id;
      output[output_index].computeInstanceId = row->compute_instance_id;
    }
    output_index += UINT32_C(1);
  }
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_nvml_processes(nvmlDevice_t device, unsigned int* count, void* infos,
                                      uint32_t kind, uint32_t structure_version) {
  nvmlReturn_t result = NVML_SUCCESS;
  mf_nvml_lock();
  result = mf_nvml_processes_locked(device, count, infos, kind, structure_version);
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetComputeRunningProcesses(nvmlDevice_t device, unsigned int* count,
                                                  nvmlProcessInfo_v1_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_COMPUTE, UINT32_C(1));
}

nvmlReturn_t nvmlDeviceGetComputeRunningProcesses_v2(nvmlDevice_t device, unsigned int* count,
                                                     nvmlProcessInfo_v2_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_COMPUTE, UINT32_C(2));
}

nvmlReturn_t nvmlDeviceGetComputeRunningProcesses_v3(nvmlDevice_t device, unsigned int* count,
                                                     nvmlProcessInfo_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_COMPUTE, UINT32_C(2));
}

nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses(nvmlDevice_t device, unsigned int* count,
                                                   nvmlProcessInfo_v1_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_GRAPHICS, UINT32_C(1));
}

nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses_v2(nvmlDevice_t device, unsigned int* count,
                                                      nvmlProcessInfo_v2_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_GRAPHICS, UINT32_C(2));
}

nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses_v3(nvmlDevice_t device, unsigned int* count,
                                                      nvmlProcessInfo_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_GRAPHICS, UINT32_C(2));
}

nvmlReturn_t nvmlDeviceGetMPSComputeRunningProcesses(nvmlDevice_t device, unsigned int* count,
                                                     nvmlProcessInfo_v1_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_MPS, UINT32_C(1));
}

nvmlReturn_t nvmlDeviceGetMPSComputeRunningProcesses_v2(nvmlDevice_t device, unsigned int* count,
                                                        nvmlProcessInfo_v2_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_MPS, UINT32_C(2));
}

nvmlReturn_t nvmlDeviceGetMPSComputeRunningProcesses_v3(nvmlDevice_t device, unsigned int* count,
                                                        nvmlProcessInfo_t* infos) {
  return mf_nvml_processes(device, count, infos, MF_NVML_PROCESS_MPS, UINT32_C(2));
}

typedef nvmlReturn_t (*mf_nvml_process_getter)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*);

static nvmlReturn_t mf_nvml_export_processes(nvmlDevice_t device, unsigned int* count,
                                             mf_nvml_export_process_info* infos,
                                             mf_nvml_process_getter getter) {
  nvmlProcessInfo_t process_infos[MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1];
  unsigned int capacity = 0;
  unsigned int process_count = 0;
  unsigned int index = 0;
  nvmlReturn_t result = NVML_SUCCESS;
  if (count == (unsigned int*)0 || getter == (mf_nvml_process_getter)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  if (infos == (mf_nvml_export_process_info*)0) {
    return getter(device, count, (nvmlProcessInfo_t*)0);
  }
  capacity = *count;
  process_count = capacity > MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1
                      ? MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1
                      : capacity;
  result = getter(device, &process_count, process_infos);
  *count = process_count;
  if (result != NVML_SUCCESS) {
    return result;
  }
  for (index = 0; index < process_count; ++index) {
    (void)memset(&infos[index], 0, sizeof(infos[index]));
    infos[index].pid = process_infos[index].pid;
    infos[index].used_gpu_memory = process_infos[index].usedGpuMemory;
    infos[index].gpu_instance_id = process_infos[index].gpuInstanceId;
    infos[index].compute_instance_id = process_infos[index].computeInstanceId;
    result = nvmlSystemGetProcessName(process_infos[index].pid, infos[index].process_name,
                                      MF_NVML_EXPORT_PROCESS_NAME_CAPACITY);
    if (result != NVML_SUCCESS) {
      return result;
    }
  }
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_nvml_export_compute_processes(nvmlDevice_t device, unsigned int* count,
                                                     mf_nvml_export_process_info* infos) {
  return mf_nvml_export_processes(device, count, infos, nvmlDeviceGetComputeRunningProcesses_v3);
}

static nvmlReturn_t mf_nvml_export_graphics_processes(nvmlDevice_t device, unsigned int* count,
                                                      mf_nvml_export_process_info* infos) {
  return mf_nvml_export_processes(device, count, infos, nvmlDeviceGetGraphicsRunningProcesses_v3);
}

static nvmlReturn_t mf_nvml_export_mps_processes(nvmlDevice_t device, unsigned int* count,
                                                 mf_nvml_export_process_info* infos) {
  return mf_nvml_export_processes(device, count, infos, nvmlDeviceGetMPSComputeRunningProcesses_v3);
}

static nvmlReturn_t mf_nvml_typed_stub(nvmlDevice_t device, const void* output) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (output == (const void*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_stub_device(device);
  mf_nvml_unlock();
  return result == NVML_SUCCESS ? NVML_ERROR_NOT_SUPPORTED : result;
}

nvmlReturn_t nvmlDeviceGetBrand(nvmlDevice_t device, nvmlBrandType_t* brand) {
  return mf_nvml_typed_stub(device, brand);
}

nvmlReturn_t nvmlDeviceGetSerial(nvmlDevice_t device, char* serial, unsigned int length) {
  if (length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, serial);
}

nvmlReturn_t nvmlDeviceGetMinorNumber(nvmlDevice_t device, unsigned int* minor_number) {
  return mf_nvml_typed_stub(device, minor_number);
}

nvmlReturn_t nvmlDeviceGetVbiosVersion(nvmlDevice_t device, char* version, unsigned int length) {
  if (length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, version);
}

nvmlReturn_t nvmlDeviceGetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t* mode) {
  nvmlReturn_t result = NVML_SUCCESS;
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  if (mode == (nvmlEnableState_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS) {
    *mode = (fence.policy_bits & MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1) != UINT64_C(0)
                ? NVML_FEATURE_ENABLED
                : NVML_FEATURE_DISABLED;
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceSetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t mode) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (mode != NVML_FEATURE_DISABLED && mode != NVML_FEATURE_ENABLED) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_set_policy_locked(
      device, MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1, (uint64_t)mode,
      MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1,
      mode == NVML_FEATURE_ENABLED ? MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1 : UINT64_C(0));
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetDisplayMode(nvmlDevice_t device, nvmlEnableState_t* mode) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (mode == (nvmlEnableState_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_stub_device(device);
  if (result == NVML_SUCCESS) {
    *mode = NVML_FEATURE_DISABLED;
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetDisplayActive(nvmlDevice_t device, nvmlEnableState_t* mode) {
  return nvmlDeviceGetDisplayMode(device, mode);
}

nvmlReturn_t nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int* speed) {
  return mf_nvml_typed_stub(device, speed);
}

nvmlReturn_t nvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t sensor,
                                      unsigned int* temperature) {
  if (sensor != NVML_TEMPERATURE_GPU) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, temperature);
}

nvmlReturn_t nvmlDeviceGetTemperatureV(nvmlDevice_t device, nvmlTemperature_t* temperature) {
  return mf_nvml_typed_stub(device, temperature);
}

nvmlReturn_t nvmlDeviceGetPerformanceState(nvmlDevice_t device, nvmlPstates_t* state) {
  return mf_nvml_typed_stub(device, state);
}

nvmlReturn_t nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int* milliwatts) {
  return mf_nvml_typed_stub(device, milliwatts);
}

nvmlReturn_t nvmlDeviceGetPowerManagementMode(nvmlDevice_t device, nvmlEnableState_t* mode) {
  return mf_nvml_typed_stub(device, mode);
}

nvmlReturn_t nvmlDeviceGetPowerManagementLimit(nvmlDevice_t device, unsigned int* milliwatts) {
  return mf_nvml_typed_stub(device, milliwatts);
}

nvmlReturn_t nvmlDeviceGetEnforcedPowerLimit(nvmlDevice_t device, unsigned int* milliwatts) {
  return mf_nvml_typed_stub(device, milliwatts);
}

nvmlReturn_t nvmlDeviceGetPowerManagementDefaultLimit(nvmlDevice_t device,
                                                      unsigned int* milliwatts) {
  return mf_nvml_typed_stub(device, milliwatts);
}

nvmlReturn_t nvmlDeviceGetPowerManagementLimitConstraints(nvmlDevice_t device,
                                                          unsigned int* minimum_milliwatts,
                                                          unsigned int* maximum_milliwatts) {
  if (minimum_milliwatts == (unsigned int*)0 && maximum_milliwatts == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, minimum_milliwatts != (unsigned int*)0 ? minimum_milliwatts
                                                                           : maximum_milliwatts);
}

nvmlReturn_t nvmlDeviceGetClockInfo(nvmlDevice_t device, nvmlClockType_t type, unsigned int* mhz) {
  if (type < NVML_CLOCK_GRAPHICS || type > NVML_CLOCK_VIDEO) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, mhz);
}

nvmlReturn_t nvmlDeviceGetComputeMode(nvmlDevice_t device, nvmlComputeMode_t* mode) {
  nvmlReturn_t result = NVML_SUCCESS;
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  if (mode == (nvmlComputeMode_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  if (result == NVML_SUCCESS) {
    *mode = (nvmlComputeMode_t)((fence.policy_bits & MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1) >>
                                MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceSetComputeMode(nvmlDevice_t device, nvmlComputeMode_t mode) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (mode < NVML_COMPUTEMODE_DEFAULT || mode > NVML_COMPUTEMODE_EXCLUSIVE_PROCESS) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_set_policy_locked(device, MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1,
                                     (uint64_t)mode, MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1,
                                     (uint64_t)mode << MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1);
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetBAR1MemoryInfo(nvmlDevice_t device, nvmlBAR1Memory_t* memory) {
  return mf_nvml_typed_stub(device, memory);
}

nvmlReturn_t nvmlDeviceGetArchitecture(nvmlDevice_t device,
                                       nvmlDeviceArchitecture_t* architecture) {
  return mf_nvml_typed_stub(device, architecture);
}

nvmlReturn_t nvmlDeviceGetMigMode(nvmlDevice_t device, unsigned int* current,
                                  unsigned int* pending) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (current == (unsigned int*)0 || pending == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_stub_device(device);
  if (result == NVML_SUCCESS) {
    *current = UINT32_C(0);
    *pending = UINT32_C(0);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetMaxMigDeviceCount(nvmlDevice_t device, unsigned int* count) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (count == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_stub_device(device);
  if (result == NVML_SUCCESS) {
    *count = UINT32_C(0);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceIsMigDeviceHandle(nvmlDevice_t device, unsigned int* is_mig_device) {
  nvmlReturn_t result = NVML_SUCCESS;
  if (is_mig_device == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_stub_device(device);
  if (result == NVML_SUCCESS) {
    *is_mig_device = UINT32_C(0);
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetTotalEccErrors(nvmlDevice_t device, nvmlMemoryErrorType_t error_type,
                                         nvmlEccCounterType_t counter_type,
                                         unsigned long long* count) {
  if (error_type < NVML_MEMORY_ERROR_TYPE_CORRECTED ||
      error_type > NVML_MEMORY_ERROR_TYPE_UNCORRECTED || counter_type < NVML_VOLATILE_ECC ||
      counter_type > NVML_AGGREGATE_ECC) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, count);
}

nvmlReturn_t nvmlDeviceGetEccMode(nvmlDevice_t device, nvmlEnableState_t* current,
                                  nvmlEnableState_t* pending) {
  if (current == (nvmlEnableState_t*)0 || pending == (nvmlEnableState_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, current);
}

nvmlReturn_t nvmlDeviceGetC2cModeInfoV(nvmlDevice_t device, nvmlC2cModeInfo_v1_t* c2c_mode_info) {
  return mf_nvml_typed_stub(device, c2c_mode_info);
}

nvmlReturn_t nvmlDeviceGetAddressingMode(nvmlDevice_t device, nvmlDeviceAddressingMode_t* mode) {
  return mf_nvml_typed_stub(device, mode);
}

nvmlReturn_t nvmlDeviceGetBoardPartNumber(nvmlDevice_t device, char* part_number,
                                          unsigned int length) {
  if (length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, part_number);
}

nvmlReturn_t nvmlDeviceGetInforomVersion(nvmlDevice_t device, nvmlInforomObject_t object,
                                         char* version, unsigned int length) {
  if (object < NVML_INFOROM_OEM || object >= NVML_INFOROM_COUNT || length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, version);
}

nvmlReturn_t nvmlDeviceGetInforomImageVersion(nvmlDevice_t device, char* version,
                                              unsigned int length) {
  if (length == UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, version);
}

nvmlReturn_t nvmlDeviceGetLastBBXFlushTime(nvmlDevice_t device, unsigned long long* timestamp,
                                           unsigned long* duration_us) {
  if (duration_us == (unsigned long*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, timestamp);
}

nvmlReturn_t nvmlDeviceGetBBXTimeData_v1(nvmlDevice_t device, nvmlBBXTimeData_v1_t* time_data) {
  return mf_nvml_typed_stub(device, time_data);
}

nvmlReturn_t nvmlDeviceGetMaxPcieLinkGeneration(nvmlDevice_t device, unsigned int* generation) {
  return mf_nvml_typed_stub(device, generation);
}

nvmlReturn_t nvmlDeviceGetGpuMaxPcieLinkGeneration(nvmlDevice_t device, unsigned int* generation) {
  return mf_nvml_typed_stub(device, generation);
}

nvmlReturn_t nvmlDeviceGetMaxPcieLinkWidth(nvmlDevice_t device, unsigned int* width) {
  return mf_nvml_typed_stub(device, width);
}

nvmlReturn_t nvmlDeviceGetCurrPcieLinkGeneration(nvmlDevice_t device, unsigned int* generation) {
  return mf_nvml_typed_stub(device, generation);
}

nvmlReturn_t nvmlDeviceGetCurrPcieLinkWidth(nvmlDevice_t device, unsigned int* width) {
  return mf_nvml_typed_stub(device, width);
}

nvmlReturn_t nvmlDeviceGetPcieThroughput(nvmlDevice_t device, nvmlPcieUtilCounter_t counter,
                                         unsigned int* value) {
  if (counter < NVML_PCIE_UTIL_TX_BYTES || counter >= NVML_PCIE_UTIL_COUNT) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, value);
}

nvmlReturn_t nvmlDeviceGetPcieReplayCounter(nvmlDevice_t device, unsigned int* value) {
  return mf_nvml_typed_stub(device, value);
}

nvmlReturn_t nvmlDeviceGetMaxClockInfo(nvmlDevice_t device, nvmlClockType_t type,
                                       unsigned int* clock) {
  if (type < NVML_CLOCK_GRAPHICS || type > NVML_CLOCK_VIDEO) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, clock);
}

nvmlReturn_t nvmlDeviceGetClock(nvmlDevice_t device, nvmlClockType_t clock_type,
                                nvmlClockId_t clock_id, unsigned int* clock_mhz) {
  if (clock_type < NVML_CLOCK_GRAPHICS || clock_type > NVML_CLOCK_VIDEO ||
      clock_id < NVML_CLOCK_ID_CURRENT || clock_id >= NVML_CLOCK_ID_COUNT) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, clock_mhz);
}

nvmlReturn_t nvmlDeviceGetAutoBoostedClocksEnabled(nvmlDevice_t device, nvmlEnableState_t* enabled,
                                                   nvmlEnableState_t* default_enabled) {
  (void)default_enabled;
  return mf_nvml_typed_stub(device, enabled);
}

nvmlReturn_t nvmlDeviceGetTemperatureThreshold(nvmlDevice_t device,
                                               nvmlTemperatureThresholds_t threshold_type,
                                               unsigned int* temperature) {
  if (threshold_type < NVML_TEMPERATURE_THRESHOLD_SHUTDOWN ||
      threshold_type >= NVML_TEMPERATURE_THRESHOLD_COUNT) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, temperature);
}

nvmlReturn_t nvmlDeviceGetMarginTemperature(nvmlDevice_t device,
                                            nvmlMarginTemperature_t* margin_temperature) {
  return mf_nvml_typed_stub(device, margin_temperature);
}

nvmlReturn_t nvmlDeviceGetSupportedClocksEventReasons(nvmlDevice_t device,
                                                      unsigned long long* reasons) {
  return mf_nvml_typed_stub(device, reasons);
}

nvmlReturn_t nvmlDeviceGetGpuOperationMode(nvmlDevice_t device, nvmlGpuOperationMode_t* current,
                                           nvmlGpuOperationMode_t* pending) {
  if (pending == (nvmlGpuOperationMode_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, current);
}

nvmlReturn_t nvmlDeviceGetDramEncryptionMode(nvmlDevice_t device, nvmlDramEncryptionInfo_t* current,
                                             nvmlDramEncryptionInfo_t* pending) {
  if (pending == (nvmlDramEncryptionInfo_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, current);
}

nvmlReturn_t nvmlDeviceGetBoardId(nvmlDevice_t device, unsigned int* board_id) {
  return mf_nvml_typed_stub(device, board_id);
}

nvmlReturn_t nvmlDeviceGetMultiGpuBoard(nvmlDevice_t device, unsigned int* multi_gpu) {
  return mf_nvml_typed_stub(device, multi_gpu);
}

nvmlReturn_t nvmlDeviceGetEncoderUtilization(nvmlDevice_t device, unsigned int* utilization,
                                             unsigned int* sampling_period_us) {
  if (sampling_period_us == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, utilization);
}

nvmlReturn_t nvmlDeviceGetEncoderStats(nvmlDevice_t device, unsigned int* session_count,
                                       unsigned int* average_fps, unsigned int* average_latency) {
  if (average_fps == (unsigned int*)0 || average_latency == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, session_count);
}

nvmlReturn_t nvmlDeviceGetDecoderUtilization(nvmlDevice_t device, unsigned int* utilization,
                                             unsigned int* sampling_period_us) {
  if (sampling_period_us == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, utilization);
}

nvmlReturn_t nvmlDeviceGetJpgUtilization(nvmlDevice_t device, unsigned int* utilization,
                                         unsigned int* sampling_period_us) {
  if (sampling_period_us == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, utilization);
}

nvmlReturn_t nvmlDeviceGetOfaUtilization(nvmlDevice_t device, unsigned int* utilization,
                                         unsigned int* sampling_period_us) {
  if (sampling_period_us == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, utilization);
}

nvmlReturn_t nvmlDeviceGetFBCStats(nvmlDevice_t device, nvmlFBCStats_t* stats) {
  return mf_nvml_typed_stub(device, stats);
}

nvmlReturn_t nvmlDeviceGetDriverModel_v2(nvmlDevice_t device, nvmlDriverModel_t* current,
                                         nvmlDriverModel_t* pending) {
  if (current == (nvmlDriverModel_t*)0 && pending == (nvmlDriverModel_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, current != (nvmlDriverModel_t*)0 ? current : pending);
}

nvmlReturn_t nvmlDeviceGetBridgeChipInfo(nvmlDevice_t device,
                                         nvmlBridgeChipHierarchy_t* hierarchy) {
  return mf_nvml_typed_stub(device, hierarchy);
}

nvmlReturn_t nvmlDeviceGetGpuFabricInfoV(nvmlDevice_t device, nvmlGpuFabricInfoV_t* fabric_info) {
  return mf_nvml_typed_stub(device, fabric_info);
}

nvmlReturn_t nvmlDeviceGetConfComputeProtectedMemoryUsage(nvmlDevice_t device,
                                                          nvmlMemory_t* memory) {
  return mf_nvml_typed_stub(device, memory);
}

nvmlReturn_t nvmlDeviceGetGspFirmwareVersion(nvmlDevice_t device, char* version) {
  return mf_nvml_typed_stub(device, version);
}

nvmlReturn_t nvmlDeviceGetAccountingMode(nvmlDevice_t device, nvmlEnableState_t* mode) {
  return mf_nvml_typed_stub(device, mode);
}

nvmlReturn_t nvmlDeviceGetAccountingBufferSize(nvmlDevice_t device, unsigned int* buffer_size) {
  return mf_nvml_typed_stub(device, buffer_size);
}

nvmlReturn_t nvmlDeviceGetRetiredPages(nvmlDevice_t device, nvmlPageRetirementCause_t cause,
                                       unsigned int* page_count, unsigned long long* addresses) {
  (void)addresses;
  if (cause < NVML_PAGE_RETIREMENT_CAUSE_MULTIPLE_SINGLE_BIT_ECC_ERRORS ||
      cause >= NVML_PAGE_RETIREMENT_CAUSE_COUNT) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, page_count);
}

nvmlReturn_t nvmlDeviceGetRetiredPagesPendingStatus(nvmlDevice_t device,
                                                    nvmlEnableState_t* pending) {
  return mf_nvml_typed_stub(device, pending);
}

nvmlReturn_t nvmlDeviceGetPdi(nvmlDevice_t device, nvmlPdi_t* pdi) {
  return mf_nvml_typed_stub(device, pdi);
}

nvmlReturn_t nvmlDeviceGetFieldValues(nvmlDevice_t device, int value_count,
                                      nvmlFieldValue_t* values) {
  int index = 0;
  nvmlReturn_t result = NVML_SUCCESS;
  if (value_count <= 0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  if (values == (nvmlFieldValue_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_stub_device(device);
  if (result == NVML_SUCCESS) {
    for (index = 0; index < value_count; ++index) {
      values[index].timestamp = 0;
      values[index].latencyUsec = 0;
      values[index].valueType = NVML_VALUE_TYPE_COUNT;
      values[index].nvmlReturn = NVML_ERROR_NOT_SUPPORTED;
      (void)memset(&values[index].value, 0, sizeof(values[index].value));
    }
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceGetVirtualizationMode(nvmlDevice_t device,
                                             nvmlGpuVirtualizationMode_t* mode) {
  return mf_nvml_typed_stub(device, mode);
}

nvmlReturn_t nvmlDeviceGetHostVgpuMode(nvmlDevice_t device, nvmlHostVgpuMode_t* mode) {
  return mf_nvml_typed_stub(device, mode);
}

nvmlReturn_t nvmlDeviceGetVgpuHeterogeneousMode(nvmlDevice_t device,
                                                nvmlVgpuHeterogeneousMode_t* mode) {
  return mf_nvml_typed_stub(device, mode);
}

nvmlReturn_t nvmlDeviceGetGridLicensableFeatures_v4(nvmlDevice_t device,
                                                    nvmlGridLicensableFeatures_t* features) {
  return mf_nvml_typed_stub(device, features);
}

nvmlReturn_t nvmlDeviceGetCapabilities(nvmlDevice_t device,
                                       nvmlDeviceCapabilities_t* capabilities) {
  return mf_nvml_typed_stub(device, capabilities);
}

nvmlReturn_t nvmlDeviceGetRemappedRows_v2(nvmlDevice_t device,
                                          nvmlRemappedRowsInfo_v2_t* information) {
  return mf_nvml_typed_stub(device, information);
}

nvmlReturn_t nvmlDeviceWorkloadPowerProfileGetCurrentProfiles(
    nvmlDevice_t device, nvmlWorkloadPowerProfileCurrentProfiles_t* current_profiles) {
  return mf_nvml_typed_stub(device, current_profiles);
}

nvmlReturn_t nvmlDeviceGetApplicationsClock(nvmlDevice_t device, nvmlClockType_t clock_type,
                                            unsigned int* clock_mhz) {
  if (clock_type < NVML_CLOCK_GRAPHICS || clock_type > NVML_CLOCK_VIDEO) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, clock_mhz);
}

nvmlReturn_t nvmlDeviceGetDefaultApplicationsClock(nvmlDevice_t device, nvmlClockType_t clock_type,
                                                   unsigned int* clock_mhz) {
  return nvmlDeviceGetApplicationsClock(device, clock_type, clock_mhz);
}

nvmlReturn_t nvmlDeviceGetDriverModel(nvmlDevice_t device, nvmlDriverModel_t* current,
                                      nvmlDriverModel_t* pending) {
  return nvmlDeviceGetDriverModel_v2(device, current, pending);
}

nvmlReturn_t nvmlDeviceGetGpuFabricInfo(nvmlDevice_t device, nvmlGpuFabricInfo_t* fabric_info) {
  return mf_nvml_typed_stub(device, fabric_info);
}

nvmlReturn_t nvmlDeviceGetMemoryErrorCounter(nvmlDevice_t device, nvmlMemoryErrorType_t error_type,
                                             nvmlEccCounterType_t counter_type,
                                             nvmlMemoryLocation_t location_type,
                                             unsigned long long* count) {
  if (error_type < NVML_MEMORY_ERROR_TYPE_CORRECTED ||
      error_type > NVML_MEMORY_ERROR_TYPE_UNCORRECTED || counter_type < NVML_VOLATILE_ECC ||
      counter_type > NVML_AGGREGATE_ECC || location_type < NVML_MEMORY_LOCATION_L1_CACHE ||
      location_type >= NVML_MEMORY_LOCATION_COUNT) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, count);
}

nvmlReturn_t nvmlDeviceGetModuleId(nvmlDevice_t device, unsigned int* module_id) {
  return mf_nvml_typed_stub(device, module_id);
}

nvmlReturn_t nvmlDeviceGetRemappedRows(nvmlDevice_t device, unsigned int* corrected_rows,
                                       unsigned int* uncorrected_rows, unsigned int* pending,
                                       unsigned int* failure_occurred) {
  if (uncorrected_rows == (unsigned int*)0 || pending == (unsigned int*)0 ||
      failure_occurred == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  return mf_nvml_typed_stub(device, corrected_rows);
}

nvmlReturn_t nvmlDeviceGetSupportedClocksThrottleReasons(nvmlDevice_t device,
                                                         unsigned long long* reasons) {
  return mf_nvml_typed_stub(device, reasons);
}

nvmlReturn_t nvmlDeviceGetAccountingPids(nvmlDevice_t device, unsigned int* count,
                                         unsigned int* pids) {
  (void)pids;
  return mf_nvml_typed_stub(device, count);
}

nvmlReturn_t nvmlDeviceGetPowerState(nvmlDevice_t device, nvmlPstates_t* state) {
  return nvmlDeviceGetPerformanceState(device, state);
}

nvmlReturn_t nvmlDeviceGetSupportedMemoryClocks(nvmlDevice_t device, unsigned int* count,
                                                unsigned int* clocks_mhz) {
  (void)clocks_mhz;
  return mf_nvml_typed_stub(device, count);
}

nvmlReturn_t nvmlEventSetCreate(nvmlEventSet_t* set) {
  uint32_t index = 0;
  nvmlReturn_t result = NVML_SUCCESS;
  if (set == (nvmlEventSet_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  *set = (nvmlEventSet_t)0;
  mf_nvml_lock();
  result = mf_nvml_require_locked();
  if (result == NVML_SUCCESS) {
    result = NVML_ERROR_INSUFFICIENT_RESOURCES;
    for (index = UINT32_C(0); index < MF_NVML_EVENT_SET_CAPACITY; ++index) {
      if (mf_nvml_global.event_sets[index].active == UINT32_C(0)) {
        mf_nvml_global.event_sets[index].generation =
            mf_nvml_next_generation(mf_nvml_global.event_sets[index].generation);
        mf_nvml_global.event_sets[index].active = UINT32_C(1);
        *set = mf_nvml_make_event_set(index, mf_nvml_global.event_sets[index].generation);
        result = NVML_SUCCESS;
        break;
      }
    }
  }
  mf_nvml_unlock();
  return result;
}

nvmlReturn_t nvmlDeviceRegisterEvents(nvmlDevice_t device, unsigned long long event_types,
                                      nvmlEventSet_t set) {
  uint32_t index = 0;
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  nvmlReturn_t result = NVML_SUCCESS;
  (void)event_types;
  if (set == (nvmlEventSet_t)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_event_set_locked(set);
  if (result == NVML_SUCCESS) {
    result = mf_nvml_validate_device_locked(device, &index, &identity, &fence);
  }
  mf_nvml_unlock();
  return result == NVML_SUCCESS ? NVML_ERROR_NOT_SUPPORTED : result;
}

nvmlReturn_t nvmlDeviceGetSupportedEventTypes(nvmlDevice_t device,
                                              unsigned long long* event_types) {
  return mf_nvml_typed_stub(device, event_types);
}

nvmlReturn_t nvmlEventSetWait_v2(nvmlEventSet_t set, nvmlEventData_t* data,
                                 unsigned int timeout_ms) {
  nvmlReturn_t result = NVML_SUCCESS;
  (void)timeout_ms;
  if (set == (nvmlEventSet_t)0 || data == (nvmlEventData_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_event_set_locked(set);
  mf_nvml_unlock();
  return result == NVML_SUCCESS ? NVML_ERROR_NOT_SUPPORTED : result;
}

nvmlReturn_t nvmlEventSetWait(nvmlEventSet_t set, nvmlEventData_t* data, unsigned int timeout_ms) {
  return nvmlEventSetWait_v2(set, data, timeout_ms);
}

nvmlReturn_t nvmlEventSetFree(nvmlEventSet_t set) {
  uint32_t index = 0;
  uint32_t generation = 0;
  nvmlReturn_t result = NVML_SUCCESS;
  if (set == (nvmlEventSet_t)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_lock();
  result = mf_nvml_validate_event_set_locked(set);
  if (result == NVML_SUCCESS && mf_nvml_decode_event_set(set, &index, &generation)) {
    (void)generation;
    mf_nvml_global.event_sets[index].active = UINT32_C(0);
  }
  mf_nvml_unlock();
  return result;
}
