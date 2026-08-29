#define _GNU_SOURCE

#include "metaflux/nvml/provider.h"

#include <linux/memfd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct mf_nvml_registry_fixture {
  int fd;
  void* mapping;
  uint64_t size;
  mf_registry_view_id_v1 view_id;
} mf_nvml_registry_fixture;

typedef struct mf_nvml_init_thread_args {
  atomic_uint* failure;
} mf_nvml_init_thread_args;

static void* mf_nvml_init_thread(void* context) {
  mf_nvml_init_thread_args* args = (mf_nvml_init_thread_args*)context;
  uint32_t iteration = 0;
  for (iteration = 0; iteration < UINT32_C(64); ++iteration) {
    unsigned int count = 0;
    if (nvmlInit_v2() != NVML_SUCCESS || nvmlDeviceGetCount_v2(&count) != NVML_SUCCESS ||
        count != UINT32_C(1) || nvmlShutdown() != NVML_SUCCESS) {
      atomic_store_explicit(args->failure, UINT32_C(1), memory_order_release);
      break;
    }
  }
  return (void*)0;
}

static int mf_nvml_run_init_threads(void) {
  mf_nvml_init_thread_args args;
  pthread_t threads[4];
  atomic_uint failure;
  uint32_t created = 0;
  uint32_t index = 0;
  int result = 0;
  atomic_init(&failure, UINT32_C(0));
  args.failure = &failure;
  for (index = 0; index < (uint32_t)(sizeof(threads) / sizeof(threads[0])); ++index) {
    if (pthread_create(&threads[index], (const pthread_attr_t*)0, mf_nvml_init_thread, &args) !=
        0) {
      result = -1;
      break;
    }
    created += UINT32_C(1);
  }
  for (index = 0; index < created; ++index) {
    if (pthread_join(threads[index], (void**)0) != 0) {
      result = -1;
    }
  }
  return result == 0 && atomic_load_explicit(&failure, memory_order_acquire) == UINT32_C(0) ? 0
                                                                                            : -1;
}

static int mf_nvml_fixture_create(mf_nvml_registry_fixture* fixture, uint32_t device_count) {
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
  uint32_t index = 0;
  (void)memset(fixture, 0, sizeof(*fixture));
  fixture->fd = -1;
  fixture->view_id.daemon_incarnation = UINT64_C(17);
  fixture->view_id.view_serial = UINT64_C(23) + (uint64_t)device_count;
  fixture->size = sizeof(mf_shared_registry_header_v1) + sizeof(mf_view_admission_control_v1) +
                  sizeof(mf_registry_view_control_v1) +
                  ((uint64_t)device_count * sizeof(mf_virtual_device_identity_v1)) +
                  ((uint64_t)device_count * sizeof(mf_device_admission_control_v1)) +
                  ((uint64_t)device_count * sizeof(mf_virtual_device_lifecycle_fence_v1)) +
                  sizeof(mf_telemetry_control_v1) +
                  (UINT64_C(2) * (uint64_t)device_count * sizeof(mf_virtual_device_telemetry_v1));
  fixture->fd = memfd_create("metaflux-nvml-test-registry", MFD_CLOEXEC);
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
  header->device_count = device_count;
  header->telemetry_row_count = device_count;
  header->view_admission_offset = offset;
  view_admission = (mf_view_admission_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += sizeof(*view_admission);
  header->view_control_offset = offset;
  view_control = (mf_registry_view_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += sizeof(*view_control);
  header->identities_offset = offset;
  identities = (mf_virtual_device_identity_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)device_count * sizeof(*identities);
  header->device_admission_offset = offset;
  device_admissions = (mf_device_admission_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)device_count * sizeof(*device_admissions);
  header->lifecycle_fences_offset = offset;
  fences = (mf_virtual_device_lifecycle_fence_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)device_count * sizeof(*fences);
  header->telemetry_control_offset = offset;
  telemetry_control = (mf_telemetry_control_v1*)((uint8_t*)fixture->mapping + offset);
  offset += sizeof(*telemetry_control);
  header->telemetry_bank0_offset = offset;
  bank0 = (mf_virtual_device_telemetry_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)device_count * sizeof(*bank0);
  header->telemetry_bank1_offset = offset;
  bank1 = (mf_virtual_device_telemetry_v1*)((uint8_t*)fixture->mapping + offset);
  offset += (uint64_t)device_count * sizeof(*bank1);

  view_admission->registry_view_id = fixture->view_id;
  view_admission->state_generation = mf_view_admission_pack_v1(UINT64_C(1), MF_VIEW_ADMISSION_OPEN);
  view_control->registry_view_id = fixture->view_id;
  view_control->gate_state = MF_VIEW_GATE_OPEN;
  telemetry_control->snapshot_sequence = UINT64_C(1);
  telemetry_control->active_bank_state =
      mf_telemetry_bank_state_pack_v1(UINT32_C(0), MF_TELEMETRY_STATE_READY);
  telemetry_control->row_count = device_count;
  for (index = 0; index < device_count; ++index) {
    uint32_t uuid_index = 0;
    identities[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    for (uuid_index = 0; uuid_index < UINT32_C(16); ++uuid_index) {
      identities[index].gpu_uuid[uuid_index] =
          (uint8_t)((index * UINT32_C(16)) + uuid_index + UINT32_C(1));
    }
    if (device_count == UINT32_C(1)) {
      (void)memcpy(identities[index].display_name, "MetaFlux Test Device",
                   sizeof("MetaFlux Test Device"));
    } else {
      (void)snprintf((char*)identities[index].display_name, sizeof(identities[index].display_name),
                     "MetaFlux Test Device %u", index);
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
    fences[index].policy_bits = MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1 |
                                (MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_PROCESS_V1
                                 << MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1);
    fences[index].device_state = MF_DEVICE_STATE_ONLINE;
    bank0[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    bank0[index].observed_lifecycle_sequence = UINT64_C(1);
    bank0[index].committed_work_items = UINT64_C(11);
    bank0[index].completed_work_items = UINT64_C(10);
    bank0[index].active_time_ns = UINT64_C(25000000);
    bank0[index].memory_active_time_ns = UINT64_C(75000000);
    bank0[index].memory_used_bytes = UINT64_C(134217728);
    bank0[index].memory_capacity_bytes = UINT64_C(1073741824);
    bank0[index].sample_time_ns = UINT64_C(100000000);
    bank1[index] = bank0[index];
  }
  return offset == fixture->size ? 0 : -1;
}

static void mf_nvml_fixture_destroy(mf_nvml_registry_fixture* fixture) {
  if (fixture->mapping != (void*)0) {
    (void)munmap(fixture->mapping, (size_t)fixture->size);
  }
  if (fixture->fd >= 0) {
    (void)close(fixture->fd);
  }
}

static int mf_nvml_test_device_count(uint32_t expected_count) {
  mf_nvml_registry_fixture fixture;
  mf_nvml_provider_test_transport_v1 transport;
  unsigned int count = UINT32_C(77);
  uint32_t index = 0;
  int initialized = 0;
  int result = -1;
  if (mf_nvml_fixture_create(&fixture, expected_count) != 0) {
    return -1;
  }
  (void)memset(&transport, 0, sizeof(transport));
  transport.registry_fd = fixture.fd;
  transport.registry_view_id = fixture.view_id;
  transport.process_data_available = UINT32_C(1);
  if (mf_nvml_provider_test_install_transport_v1(&transport) != 0 ||
      nvmlInit_v2() != NVML_SUCCESS) {
    goto cleanup;
  }
  initialized = 1;
  if (nvmlDeviceGetCount_v2(&count) != NVML_SUCCESS || count != expected_count) {
    goto cleanup;
  }
  for (index = 0; index < expected_count; ++index) {
    nvmlDevice_t device = (nvmlDevice_t)0;
    nvmlDevice_t found = (nvmlDevice_t)0;
    unsigned int actual_index = UINT32_MAX;
    char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
    if (nvmlDeviceGetHandleByIndex_v2(index, &device) != NVML_SUCCESS ||
        nvmlDeviceGetIndex(device, &actual_index) != NVML_SUCCESS || actual_index != index ||
        nvmlDeviceGetUUID(device, uuid, sizeof(uuid)) != NVML_SUCCESS ||
        nvmlDeviceGetHandleByUUID(uuid, &found) != NVML_SUCCESS || found != device) {
      goto cleanup;
    }
  }
  {
    nvmlDevice_t invalid = (nvmlDevice_t)0;
    if (nvmlDeviceGetHandleByIndex_v2(expected_count, &invalid) != NVML_ERROR_INVALID_ARGUMENT) {
      goto cleanup;
    }
  }
  result = 0;

cleanup:
  if (initialized != 0) {
    (void)nvmlShutdown();
  }
  mf_nvml_provider_test_reset_v1();
  mf_nvml_fixture_destroy(&fixture);
  return result;
}

int main(void) {
  mf_nvml_registry_fixture fixture;
  const mf_nvml_provider_test_process_v1 processes[] = {
      {0, 101, UINT64_C(4096), UINT32_MAX, UINT32_MAX, MF_NVML_PROVIDER_PROCESS_COMPUTE, 0},
      {0, 202, UINT64_C(8192), UINT32_MAX, UINT32_MAX,
       MF_NVML_PROVIDER_PROCESS_COMPUTE | MF_NVML_PROVIDER_PROCESS_MPS, 0}};
  mf_nvml_provider_test_transport_v1 transport;
  nvmlDevice_t device = (nvmlDevice_t)0;
  nvmlDevice_t found = (nvmlDevice_t)0;
  nvmlEventSet_t event_set = (nvmlEventSet_t)0;
  nvmlEventData_t event_data;
  const uint8_t export_table_id[16] = {UINT8_C(0xc4), UINT8_C(0xfe), UINT8_C(0x3e), UINT8_C(0x6c),
                                       UINT8_C(0xc9), UINT8_C(0x8f), UINT8_C(0x6c), UINT8_C(0x4e),
                                       UINT8_C(0xa3), UINT8_C(0x27), UINT8_C(0xee), UINT8_C(0x69),
                                       UINT8_C(0x6e), UINT8_C(0x12), UINT8_C(0xf7), UINT8_C(0xc4)};
  const uint8_t unknown_export_table_id[16] = {UINT8_C(0)};
  const void* export_table = (const void*)0;
  unsigned int count = 0;
  unsigned int sentinel = UINT32_C(77);
  char name[NVML_DEVICE_NAME_BUFFER_SIZE];
  char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
  char driver_version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
  char interface_version[NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE];
  int cuda_driver_version = 0;
  nvmlMemory_t memory;
  nvmlMemory_v2_t memory_v2;
  nvmlPciInfo_t pci;
  nvmlPciInfoExt_t pci_ext;
  nvmlUtilization_t utilization = {77, 77};
  nvmlEnableState_t persistence_mode = NVML_FEATURE_ENABLED;
  nvmlEnableState_t display_mode = NVML_FEATURE_ENABLED;
  nvmlComputeMode_t compute_mode = NVML_COMPUTEMODE_PROHIBITED;
  nvmlDriverModel_t driver_model = NVML_DRIVER_WDDM;
  nvmlEnableState_t auto_boost = NVML_FEATURE_ENABLED;
  nvmlFieldValue_t field_value = {UINT32_C(17),
                                  UINT32_C(23),
                                  INT64_C(77),
                                  INT64_C(77),
                                  NVML_VALUE_TYPE_DOUBLE,
                                  NVML_SUCCESS,
                                  {.ullVal = UINT64_C(77)}};
  _Alignas(max_align_t) unsigned char opaque_output[64];
  unsigned int current_mig_mode = UINT32_C(77);
  unsigned int pending_mig_mode = UINT32_C(77);
  unsigned int is_mig_device = UINT32_C(77);
  nvmlProcessInfo_t process_infos[2];
  nvmlProcessInfo_t short_info = {77, 77, 77, 77};
  if (nvmlInit_v2() != NVML_ERROR_DRIVER_NOT_LOADED ||
      mf_nvml_fixture_create(&fixture, UINT32_C(1)) != 0) {
    return 1;
  }
  (void)memset(&transport, 0, sizeof(transport));
  transport.registry_fd = fixture.fd;
  transport.registry_view_id = fixture.view_id;
  transport.processes = processes;
  transport.process_count = (uint32_t)(sizeof(processes) / sizeof(processes[0]));
  transport.process_data_available = UINT32_C(1);
  if (mf_nvml_provider_test_install_transport_v1(&transport) != 0 ||
      nvmlInit_v2() != NVML_SUCCESS || nvmlInit() != NVML_SUCCESS ||
      nvmlDeviceGetCount_v2(&count) != NVML_SUCCESS || count != UINT32_C(1) ||
      nvmlDeviceGetHandleByIndex_v2(0, &device) != NVML_SUCCESS ||
      nvmlDeviceGetName(device, name, sizeof(name)) != NVML_SUCCESS ||
      strcmp(name, "MetaFlux Test Device") != 0 ||
      nvmlDeviceGetUUID(device, uuid, sizeof(uuid)) != NVML_SUCCESS ||
      nvmlDeviceGetHandleByUUID(uuid, &found) != NVML_SUCCESS || found != device ||
      nvmlDeviceGetHandleByPciBusId_v2("0000:03:04.0", &found) != NVML_SUCCESS || found != device ||
      nvmlSystemGetDriverVersion(driver_version, sizeof(driver_version)) != NVML_SUCCESS ||
      strcmp(driver_version, "610.43.02") != 0 ||
      nvmlSystemGetNVMLVersion(interface_version, sizeof(interface_version)) != NVML_SUCCESS ||
      strcmp(interface_version, "13.610.43.02") != 0 ||
      nvmlSystemGetCudaDriverVersion_v2(&cuda_driver_version) != NVML_SUCCESS ||
      cuda_driver_version != 13030) {
    return 2;
  }
  if (nvmlInternalGetExportTable((const void**)0, export_table_id) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlInternalGetExportTable(&export_table, unknown_export_table_id) !=
          NVML_ERROR_NOT_SUPPORTED ||
      export_table != (const void*)0 ||
      nvmlInternalGetExportTable(&export_table, export_table_id) != NVML_SUCCESS ||
      export_table == (const void*)0 || *(const uint64_t*)export_table != UINT64_C(0x948) ||
      nvmlEventSetCreate(&event_set) != NVML_SUCCESS || event_set == (nvmlEventSet_t)0 ||
      nvmlDeviceRegisterEvents(device, UINT64_C(1), event_set) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlEventSetWait_v2(event_set, &event_data, UINT32_C(0)) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlEventSetFree(event_set) != NVML_SUCCESS ||
      nvmlEventSetFree(event_set) != NVML_ERROR_INVALID_ARGUMENT) {
    return 12;
  }
  sentinel = UINT32_C(77);
  if (nvmlDeviceGetAccountingBufferSize(device, &sentinel) != NVML_ERROR_NOT_SUPPORTED ||
      sentinel != UINT32_C(77) ||
      nvmlDeviceGetAccountingBufferSize(device, (unsigned int*)0) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetPcieThroughput(device, NVML_PCIE_UTIL_COUNT, &sentinel) !=
          NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetPcieThroughput(device, NVML_PCIE_UTIL_TX_BYTES, &sentinel) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetDriverModel_v2(device, &driver_model, (nvmlDriverModel_t*)0) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetDriverModel_v2(device, (nvmlDriverModel_t*)0, &driver_model) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetDriverModel_v2(device, (nvmlDriverModel_t*)0, (nvmlDriverModel_t*)0) !=
          NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetAutoBoostedClocksEnabled(device, &auto_boost, (nvmlEnableState_t*)0) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetC2cModeInfoV(device, (nvmlC2cModeInfo_v1_t*)opaque_output) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceWorkloadPowerProfileGetCurrentProfiles(
          device, (nvmlWorkloadPowerProfileCurrentProfiles_t*)opaque_output) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetFieldValues(device, 1, &field_value) != NVML_SUCCESS ||
      field_value.fieldId != UINT32_C(17) || field_value.scopeId != UINT32_C(23) ||
      field_value.timestamp != INT64_C(0) || field_value.latencyUsec != INT64_C(0) ||
      field_value.valueType != NVML_VALUE_TYPE_COUNT ||
      field_value.nvmlReturn != NVML_ERROR_NOT_SUPPORTED ||
      field_value.value.ullVal != UINT64_C(0) ||
      nvmlDeviceGetFieldValues(device, 0, &field_value) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetRetiredPages(device, NVML_PAGE_RETIREMENT_CAUSE_MULTIPLE_SINGLE_BIT_ECC_ERRORS,
                                &sentinel, (unsigned long long*)0) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetApplicationsClock(device, NVML_CLOCK_GRAPHICS, &sentinel) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetApplicationsClock(device, (nvmlClockType_t)99, &sentinel) !=
          NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetDriverModel(device, (nvmlDriverModel_t*)0, &driver_model) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetMemoryErrorCounter(
          device, NVML_MEMORY_ERROR_TYPE_CORRECTED, NVML_VOLATILE_ECC, NVML_MEMORY_LOCATION_DRAM,
          (unsigned long long*)opaque_output) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetRemappedRows(device, &sentinel, &sentinel, &sentinel, &sentinel) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetRemappedRows(device, &sentinel, (unsigned int*)0, &sentinel, &sentinel) !=
          NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetSupportedClocksThrottleReasons(device, (unsigned long long*)opaque_output) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetAccountingPids(device, &sentinel, (unsigned int*)0) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetAccountingPids(device, (unsigned int*)0, &sentinel) !=
          NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetSupportedMemoryClocks(device, &sentinel, (unsigned int*)0) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetPowerState(device, (nvmlPstates_t*)opaque_output) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetPowerManagementLimitConstraints(device, &sentinel, (unsigned int*)0) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetPowerManagementLimitConstraints(device, (unsigned int*)0, &sentinel) !=
          NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetPowerManagementLimitConstraints(device, (unsigned int*)0, (unsigned int*)0) !=
          NVML_ERROR_INVALID_ARGUMENT) {
    return 14;
  }
  if (nvmlDeviceGetHandleByPciBusId_v2("10000:03:04.0", &found) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetHandleByPciBusId_v2("0000:100:04.0", &found) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetHandleByPciBusId_v2("0000:03:20.0", &found) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceGetHandleByPciBusId_v2("0000:03:04.8", &found) != NVML_ERROR_INVALID_ARGUMENT ||
      strcmp(nvmlErrorString(NVML_ERROR_INSUFFICIENT_POWER), "Insufficient Power") != 0 ||
      strcmp(nvmlErrorString(NVML_ERROR_GPU_NOT_FOUND), "GPU Not Found") != 0) {
    return 11;
  }
  (void)memset(&pci, UINT8_C(0xa5), sizeof(pci));
  if (nvmlDeviceGetPciInfo(device, &pci) != NVML_SUCCESS ||
      nvmlDeviceGetPciInfo_v2(device, &pci) != NVML_SUCCESS ||
      nvmlDeviceGetPciInfo_v3(device, &pci) != NVML_SUCCESS ||
      strcmp(pci.busIdLegacy, "0000:03:04.0") != 0 || strcmp(pci.busId, "00000000:03:04.0") != 0 ||
      pci.domain != UINT32_C(0) || pci.bus != UINT32_C(3) || pci.device != UINT32_C(4) ||
      pci.pciDeviceId != UINT32_C(0) || pci.pciSubSystemId != UINT32_C(0)) {
    return 15;
  }
  (void)memset(&pci_ext, 0, sizeof(pci_ext));
  if (nvmlDeviceGetPciInfoExt(device, &pci_ext) != NVML_ERROR_ARGUMENT_VERSION_MISMATCH) {
    return 16;
  }
  pci_ext.version = nvmlPciInfoExt_v1;
  if (nvmlDeviceGetPciInfoExt(device, &pci_ext) != NVML_SUCCESS ||
      pci_ext.version != nvmlPciInfoExt_v1 || strcmp(pci_ext.busId, "00000000:03:04.0") != 0 ||
      pci_ext.domain != UINT32_C(0) || pci_ext.bus != UINT32_C(3) ||
      pci_ext.device != UINT32_C(4) || pci_ext.pciDeviceId != UINT32_C(0) ||
      pci_ext.pciSubSystemId != UINT32_C(0) || pci_ext.baseClass != UINT32_C(0) ||
      pci_ext.subClass != UINT32_C(0)) {
    return 17;
  }
  if (nvmlDeviceGetMemoryInfo(device, &memory) != NVML_SUCCESS ||
      memory.total != UINT64_C(1073741824) || memory.used != UINT64_C(134217728) ||
      memory.free != UINT64_C(939524096)) {
    return 3;
  }
  (void)memset(&memory_v2, 0, sizeof(memory_v2));
  memory_v2.version = UINT32_C(2);
  if (nvmlDeviceGetMemoryInfo_v2(device, &memory_v2) != NVML_ERROR_ARGUMENT_VERSION_MISMATCH) {
    return 4;
  }
  memory_v2.version = nvmlMemory_v2;
  if (nvmlDeviceGetMemoryInfo_v2(device, &memory_v2) != NVML_SUCCESS ||
      memory_v2.reserved != UINT64_C(0) || memory_v2.used != memory.used ||
      nvmlDeviceGetUtilizationRates(device, &utilization) != NVML_SUCCESS ||
      utilization.gpu != UINT32_C(25) || utilization.memory != UINT32_C(75) ||
      nvmlDeviceGetFanSpeed(device, &sentinel) != NVML_ERROR_NOT_SUPPORTED ||
      sentinel != UINT32_C(77)) {
    return 5;
  }
  count = UINT32_C(77);
  if (nvmlDeviceGetPersistenceMode(device, &persistence_mode) != NVML_SUCCESS ||
      persistence_mode != NVML_FEATURE_ENABLED ||
      nvmlDeviceSetPersistenceMode(device, (nvmlEnableState_t)2) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceSetPersistenceMode(device, NVML_FEATURE_DISABLED) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetDisplayActive(device, &display_mode) != NVML_SUCCESS ||
      display_mode != NVML_FEATURE_DISABLED ||
      nvmlDeviceGetComputeMode(device, &compute_mode) != NVML_SUCCESS ||
      compute_mode != NVML_COMPUTEMODE_EXCLUSIVE_PROCESS ||
      nvmlDeviceSetComputeMode(device, (nvmlComputeMode_t)4) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlDeviceSetComputeMode(device, NVML_COMPUTEMODE_DEFAULT) != NVML_ERROR_NOT_SUPPORTED ||
      nvmlDeviceGetMigMode(device, &current_mig_mode, &pending_mig_mode) != NVML_SUCCESS ||
      current_mig_mode != UINT32_C(0) || pending_mig_mode != UINT32_C(0) ||
      nvmlDeviceIsMigDeviceHandle(device, &is_mig_device) != NVML_SUCCESS ||
      is_mig_device != UINT32_C(0) ||
      nvmlDeviceGetMaxMigDeviceCount(device, &count) != NVML_SUCCESS || count != UINT32_C(0)) {
    return 13;
  }
  count = UINT32_C(0);
  if (nvmlDeviceGetComputeRunningProcesses_v3(device, &count, (nvmlProcessInfo_t*)0) !=
          NVML_ERROR_INSUFFICIENT_SIZE ||
      count != UINT32_C(2)) {
    return 6;
  }
  count = UINT32_C(1);
  if (nvmlDeviceGetComputeRunningProcesses_v3(device, &count, &short_info) !=
          NVML_ERROR_INSUFFICIENT_SIZE ||
      count != UINT32_C(2) || short_info.pid != UINT32_C(77)) {
    return 7;
  }
  count = UINT32_C(2);
  if (nvmlDeviceGetComputeRunningProcesses_v3(device, &count, process_infos) != NVML_SUCCESS ||
      count != UINT32_C(2) || process_infos[0].pid != UINT32_C(101) ||
      process_infos[1].usedGpuMemory != UINT64_C(8192)) {
    return 8;
  }
  count = UINT32_C(0);
  if (nvmlDeviceGetGraphicsRunningProcesses_v3(device, &count, (nvmlProcessInfo_t*)0) !=
          NVML_SUCCESS ||
      count != UINT32_C(0) || nvmlShutdown() != NVML_SUCCESS ||
      nvmlDeviceGetIndex(device, &count) != NVML_SUCCESS || nvmlShutdown() != NVML_SUCCESS ||
      nvmlShutdown() != NVML_ERROR_UNINITIALIZED) {
    return 9;
  }
  if (mf_nvml_run_init_threads() != 0 || nvmlInit_v2() != NVML_SUCCESS ||
      nvmlDeviceGetIndex(device, &count) != NVML_ERROR_INVALID_ARGUMENT ||
      nvmlShutdown() != NVML_SUCCESS) {
    return 10;
  }
  mf_nvml_provider_test_reset_v1();
  mf_nvml_fixture_destroy(&fixture);
  if (mf_nvml_test_device_count(UINT32_C(0)) != 0 || mf_nvml_test_device_count(UINT32_C(3)) != 0) {
    return 18;
  }
  return 0;
}
