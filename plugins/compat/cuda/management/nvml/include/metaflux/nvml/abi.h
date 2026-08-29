#ifndef METAFLUX_NVML_ABI_H
#define METAFLUX_NVML_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_NVML_ABI_API __attribute__((visibility("default")))
#else
#define MF_NVML_ABI_API
#endif

#define NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE 32
#define NVML_DEVICE_PCI_BUS_ID_BUFFER_V2_SIZE 16
#define NVML_DEVICE_UUID_BUFFER_SIZE 80
#define NVML_DEVICE_NAME_BUFFER_SIZE 64
#define NVML_DEVICE_SERIAL_BUFFER_SIZE 30
#define NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE 80
#define NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE 80
#define NVML_API_VERSION 13
#define NVML_API_VERSION_STR "13"
#define NVML_VALUE_NOT_AVAILABLE (-1)
#define NVML_STRUCT_VERSION(data, version_number)                                                  \
  ((unsigned int)(sizeof(nvml##data##_v##version_number##_t) |                                     \
                  ((unsigned int)(version_number) << 24U)))

typedef struct nvmlDevice_st* nvmlDevice_t;
typedef struct nvmlEventSet_st* nvmlEventSet_t;

typedef struct nvmlEventData_st {
  nvmlDevice_t device;
  unsigned long long eventType;
  unsigned long long eventData;
  unsigned int gpuInstanceId;
  unsigned int computeInstanceId;
} nvmlEventData_t;

typedef enum nvmlReturn_enum {
  NVML_SUCCESS = 0,
  NVML_ERROR_UNINITIALIZED = 1,
  NVML_ERROR_INVALID_ARGUMENT = 2,
  NVML_ERROR_NOT_SUPPORTED = 3,
  NVML_ERROR_NO_PERMISSION = 4,
  NVML_ERROR_ALREADY_INITIALIZED = 5,
  NVML_ERROR_NOT_FOUND = 6,
  NVML_ERROR_INSUFFICIENT_SIZE = 7,
  NVML_ERROR_INSUFFICIENT_POWER = 8,
  NVML_ERROR_DRIVER_NOT_LOADED = 9,
  NVML_ERROR_TIMEOUT = 10,
  NVML_ERROR_IRQ_ISSUE = 11,
  NVML_ERROR_LIBRARY_NOT_FOUND = 12,
  NVML_ERROR_FUNCTION_NOT_FOUND = 13,
  NVML_ERROR_CORRUPTED_INFOROM = 14,
  NVML_ERROR_GPU_IS_LOST = 15,
  NVML_ERROR_RESET_REQUIRED = 16,
  NVML_ERROR_OPERATING_SYSTEM = 17,
  NVML_ERROR_LIB_RM_VERSION_MISMATCH = 18,
  NVML_ERROR_IN_USE = 19,
  NVML_ERROR_MEMORY = 20,
  NVML_ERROR_NO_DATA = 21,
  NVML_ERROR_VGPU_ECC_NOT_SUPPORTED = 22,
  NVML_ERROR_INSUFFICIENT_RESOURCES = 23,
  NVML_ERROR_FREQ_NOT_SUPPORTED = 24,
  NVML_ERROR_ARGUMENT_VERSION_MISMATCH = 25,
  NVML_ERROR_DEPRECATED = 26,
  NVML_ERROR_NOT_READY = 27,
  NVML_ERROR_GPU_NOT_FOUND = 28,
  NVML_ERROR_INVALID_STATE = 29,
  NVML_ERROR_UNKNOWN = 999
} nvmlReturn_t;

typedef enum nvmlEnableState_enum {
  NVML_FEATURE_DISABLED = 0,
  NVML_FEATURE_ENABLED = 1
} nvmlEnableState_t;

typedef enum nvmlTemperatureSensors_enum { NVML_TEMPERATURE_GPU = 0 } nvmlTemperatureSensors_t;
typedef enum nvmlMemoryErrorType_enum {
  NVML_MEMORY_ERROR_TYPE_CORRECTED = 0,
  NVML_MEMORY_ERROR_TYPE_UNCORRECTED = 1
} nvmlMemoryErrorType_t;
typedef enum nvmlEccCounterType_enum {
  NVML_VOLATILE_ECC = 0,
  NVML_AGGREGATE_ECC = 1
} nvmlEccCounterType_t;
typedef enum nvmlClockType_enum {
  NVML_CLOCK_GRAPHICS = 0,
  NVML_CLOCK_SM = 1,
  NVML_CLOCK_MEM = 2,
  NVML_CLOCK_VIDEO = 3
} nvmlClockType_t;
typedef enum nvmlPstates_enum { NVML_PSTATE_0 = 0, NVML_PSTATE_UNKNOWN = 32 } nvmlPstates_t;
typedef enum nvmlComputeMode_enum {
  NVML_COMPUTEMODE_DEFAULT = 0,
  NVML_COMPUTEMODE_EXCLUSIVE_THREAD = 1,
  NVML_COMPUTEMODE_PROHIBITED = 2,
  NVML_COMPUTEMODE_EXCLUSIVE_PROCESS = 3
} nvmlComputeMode_t;
typedef enum nvmlBrandType_enum { NVML_BRAND_UNKNOWN = 0 } nvmlBrandType_t;
typedef enum nvmlDeviceArchitecture_enum { NVML_DEVICE_ARCH_UNKNOWN = 0 } nvmlDeviceArchitecture_t;
typedef enum nvmlPcieUtilCounter_enum {
  NVML_PCIE_UTIL_TX_BYTES = 0,
  NVML_PCIE_UTIL_RX_BYTES = 1,
  NVML_PCIE_UTIL_COUNT = 2
} nvmlPcieUtilCounter_t;
typedef enum nvmlClockId_enum {
  NVML_CLOCK_ID_CURRENT = 0,
  NVML_CLOCK_ID_APP_CLOCK_TARGET = 1,
  NVML_CLOCK_ID_APP_CLOCK_DEFAULT = 2,
  NVML_CLOCK_ID_CUSTOMER_BOOST_MAX = 3,
  NVML_CLOCK_ID_COUNT = 4
} nvmlClockId_t;
typedef enum nvmlTemperatureThresholds_enum {
  NVML_TEMPERATURE_THRESHOLD_SHUTDOWN = 0,
  NVML_TEMPERATURE_THRESHOLD_SLOWDOWN = 1,
  NVML_TEMPERATURE_THRESHOLD_MEM_MAX = 2,
  NVML_TEMPERATURE_THRESHOLD_GPU_MAX = 3,
  NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MIN = 4,
  NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_CURR = 5,
  NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MAX = 6,
  NVML_TEMPERATURE_THRESHOLD_GPS_CURR = 7,
  NVML_TEMPERATURE_THRESHOLD_COUNT = 8
} nvmlTemperatureThresholds_t;
typedef enum nvmlGpuOperationMode_enum {
  NVML_GOM_ALL_ON = 0,
  NVML_GOM_COMPUTE = 1,
  NVML_GOM_LOW_DP = 2
} nvmlGpuOperationMode_t;
typedef enum nvmlInforomObject_enum {
  NVML_INFOROM_OEM = 0,
  NVML_INFOROM_ECC = 1,
  NVML_INFOROM_POWER = 2,
  NVML_INFOROM_DEN = 3,
  NVML_INFOROM_COUNT = 4
} nvmlInforomObject_t;
typedef enum nvmlDriverModel_enum {
  NVML_DRIVER_WDDM = 0,
  NVML_DRIVER_WDM = 1,
  NVML_DRIVER_MCDM = 2
} nvmlDriverModel_t;
typedef enum nvmlPageRetirementCause_enum {
  NVML_PAGE_RETIREMENT_CAUSE_MULTIPLE_SINGLE_BIT_ECC_ERRORS = 0,
  NVML_PAGE_RETIREMENT_CAUSE_DOUBLE_BIT_ECC_ERROR = 1,
  NVML_PAGE_RETIREMENT_CAUSE_COUNT = 2
} nvmlPageRetirementCause_t;
typedef enum nvmlGpuVirtualizationMode_enum {
  NVML_GPU_VIRTUALIZATION_MODE_NONE = 0,
  NVML_GPU_VIRTUALIZATION_MODE_PASSTHROUGH = 1,
  NVML_GPU_VIRTUALIZATION_MODE_VGPU = 2,
  NVML_GPU_VIRTUALIZATION_MODE_HOST_VGPU = 3,
  NVML_GPU_VIRTUALIZATION_MODE_HOST_VSGA = 4
} nvmlGpuVirtualizationMode_t;
typedef enum nvmlHostVgpuMode_enum {
  NVML_HOST_VGPU_MODE_NON_SRIOV = 0,
  NVML_HOST_VGPU_MODE_SRIOV = 1
} nvmlHostVgpuMode_t;
typedef enum nvmlMemoryLocation_enum {
  NVML_MEMORY_LOCATION_L1_CACHE = 0,
  NVML_MEMORY_LOCATION_L2_CACHE = 1,
  NVML_MEMORY_LOCATION_DRAM = 2,
  NVML_MEMORY_LOCATION_DEVICE_MEMORY = 2,
  NVML_MEMORY_LOCATION_REGISTER_FILE = 3,
  NVML_MEMORY_LOCATION_TEXTURE_MEMORY = 4,
  NVML_MEMORY_LOCATION_TEXTURE_SHM = 5,
  NVML_MEMORY_LOCATION_CBU = 6,
  NVML_MEMORY_LOCATION_SRAM = 7,
  NVML_MEMORY_LOCATION_COUNT = 8
} nvmlMemoryLocation_t;
typedef enum nvmlValueType_enum {
  NVML_VALUE_TYPE_DOUBLE = 0,
  NVML_VALUE_TYPE_UNSIGNED_INT = 1,
  NVML_VALUE_TYPE_UNSIGNED_LONG = 2,
  NVML_VALUE_TYPE_UNSIGNED_LONG_LONG = 3,
  NVML_VALUE_TYPE_SIGNED_LONG_LONG = 4,
  NVML_VALUE_TYPE_SIGNED_INT = 5,
  NVML_VALUE_TYPE_COUNT = 6
} nvmlValueType_t;

typedef union nvmlValue_st {
  double dVal;
  int siVal;
  unsigned int uiVal;
  unsigned long ulVal;
  unsigned long long ullVal;
  signed long long sllVal;
} nvmlValue_t;

typedef struct nvmlFieldValue_st {
  unsigned int fieldId;
  unsigned int scopeId;
  long long timestamp;
  long long latencyUsec;
  nvmlValueType_t valueType;
  nvmlReturn_t nvmlReturn;
  nvmlValue_t value;
} nvmlFieldValue_t;

typedef struct nvmlC2cModeInfo_v1_st nvmlC2cModeInfo_v1_t;
typedef struct nvmlDeviceAddressingMode_v1_st nvmlDeviceAddressingMode_t;
typedef struct nvmlBBXTimeData_v1_st nvmlBBXTimeData_v1_t;
typedef struct nvmlMarginTemperature_v1_st nvmlMarginTemperature_t;
typedef struct nvmlDramEncryptionInfo_v1_st nvmlDramEncryptionInfo_t;
typedef struct nvmlFBCStats_st nvmlFBCStats_t;
typedef struct nvmlBridgeChipHierarchy_st nvmlBridgeChipHierarchy_t;
typedef struct nvmlGpuFabricInfo_v3_st nvmlGpuFabricInfoV_t;
typedef struct nvmlGpuFabricInfo_st nvmlGpuFabricInfo_t;
typedef struct nvmlPdi_v1_st nvmlPdi_t;
typedef struct nvmlVgpuHeterogeneousMode_v1_st nvmlVgpuHeterogeneousMode_t;
typedef struct nvmlGridLicensableFeatures_st nvmlGridLicensableFeatures_t;
typedef struct nvmlDeviceCapabilities_v1_st nvmlDeviceCapabilities_t;
typedef struct nvmlRemappedRowsInfo_v2_st nvmlRemappedRowsInfo_v2_t;
typedef struct nvmlWorkloadPowerProfileCurrentProfiles_v1_st
    nvmlWorkloadPowerProfileCurrentProfiles_t;

typedef struct nvmlPciInfo_st {
  char busIdLegacy[NVML_DEVICE_PCI_BUS_ID_BUFFER_V2_SIZE];
  unsigned int domain;
  unsigned int bus;
  unsigned int device;
  unsigned int pciDeviceId;
  unsigned int pciSubSystemId;
  char busId[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE];
} nvmlPciInfo_t;

typedef struct nvmlPciInfoExt_v1_st {
  unsigned int version;
  unsigned int domain;
  unsigned int bus;
  unsigned int device;
  unsigned int pciDeviceId;
  unsigned int pciSubSystemId;
  unsigned int baseClass;
  unsigned int subClass;
  char busId[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE];
} nvmlPciInfoExt_v1_t;
typedef nvmlPciInfoExt_v1_t nvmlPciInfoExt_t;
#define nvmlPciInfoExt_v1 NVML_STRUCT_VERSION(PciInfoExt, 1)

typedef struct nvmlTemperature_v1_st {
  unsigned int version;
  nvmlTemperatureSensors_t sensorType;
  int temperature;
} nvmlTemperature_t;

typedef struct nvmlUtilization_st {
  unsigned int gpu;
  unsigned int memory;
} nvmlUtilization_t;

typedef struct nvmlMemory_st {
  unsigned long long total;
  unsigned long long free;
  unsigned long long used;
} nvmlMemory_t;

typedef struct nvmlMemory_v2_st {
  unsigned int version;
  unsigned long long total;
  unsigned long long reserved;
  unsigned long long free;
  unsigned long long used;
} nvmlMemory_v2_t;

#define nvmlMemory_v2 NVML_STRUCT_VERSION(Memory, 2)

typedef struct nvmlBAR1Memory_st {
  unsigned long long bar1Total;
  unsigned long long bar1Free;
  unsigned long long bar1Used;
} nvmlBAR1Memory_t;

typedef struct nvmlProcessInfo_v1_st {
  unsigned int pid;
  unsigned long long usedGpuMemory;
} nvmlProcessInfo_v1_t;

typedef struct nvmlProcessInfo_v2_st {
  unsigned int pid;
  unsigned long long usedGpuMemory;
  unsigned int gpuInstanceId;
  unsigned int computeInstanceId;
} nvmlProcessInfo_v2_t, nvmlProcessInfo_t;

#if defined(__cplusplus)
#define MF_NVML_ABI_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define MF_NVML_ABI_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlPciInfo_t) == 68, "nvmlPciInfo_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(offsetof(nvmlPciInfo_t, busId) == 36, "nvmlPciInfo_t busId ABI offset");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlUtilization_t) == 8, "nvmlUtilization_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlMemory_t) == 24, "nvmlMemory_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlMemory_v2_t) == 40, "nvmlMemory_v2_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(offsetof(nvmlMemory_v2_t, total) == 8,
                          "nvmlMemory_v2_t total ABI offset");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlProcessInfo_v1_t) == 16, "nvmlProcessInfo_v1_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlProcessInfo_v2_t) == 24, "nvmlProcessInfo_v2_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlPcieUtilCounter_t) == sizeof(int),
                          "nvmlPcieUtilCounter_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlClockId_t) == sizeof(int), "nvmlClockId_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlTemperatureThresholds_t) == sizeof(int),
                          "nvmlTemperatureThresholds_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlGpuOperationMode_t) == sizeof(int),
                          "nvmlGpuOperationMode_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlInforomObject_t) == sizeof(int),
                          "nvmlInforomObject_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlDriverModel_t) == sizeof(int), "nvmlDriverModel_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlPageRetirementCause_t) == sizeof(int),
                          "nvmlPageRetirementCause_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlGpuVirtualizationMode_t) == sizeof(int),
                          "nvmlGpuVirtualizationMode_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlHostVgpuMode_t) == sizeof(int), "nvmlHostVgpuMode_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlMemoryLocation_t) == sizeof(int),
                          "nvmlMemoryLocation_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlValueType_t) == sizeof(int), "nvmlValueType_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlValue_t) == 8, "nvmlValue_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(sizeof(nvmlFieldValue_t) == 40, "nvmlFieldValue_t ABI size");
MF_NVML_ABI_STATIC_ASSERT(offsetof(nvmlFieldValue_t, nvmlReturn) == 28,
                          "nvmlFieldValue_t status ABI offset");
MF_NVML_ABI_STATIC_ASSERT(offsetof(nvmlFieldValue_t, value) == 32,
                          "nvmlFieldValue_t value ABI offset");
#undef MF_NVML_ABI_STATIC_ASSERT

MF_NVML_ABI_API nvmlReturn_t nvmlInit(void);
MF_NVML_ABI_API nvmlReturn_t nvmlInit_v2(void);
MF_NVML_ABI_API nvmlReturn_t nvmlInitWithFlags(unsigned int flags);
MF_NVML_ABI_API nvmlReturn_t nvmlInternalGetExportTable(const void** export_table,
                                                        const void* export_table_id);
MF_NVML_ABI_API nvmlReturn_t nvmlShutdown(void);
MF_NVML_ABI_API const char* nvmlErrorString(nvmlReturn_t result);
MF_NVML_ABI_API nvmlReturn_t nvmlSystemGetDriverVersion(char* version, unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlSystemGetNVMLVersion(char* version, unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlSystemGetCudaDriverVersion(int* version);
MF_NVML_ABI_API nvmlReturn_t nvmlSystemGetCudaDriverVersion_v2(int* version);
MF_NVML_ABI_API nvmlReturn_t nvmlSystemGetProcessName(unsigned int pid, char* name,
                                                      unsigned int length);

MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetCount(unsigned int* count);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int* count);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t* device);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index,
                                                           nvmlDevice_t* device);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetHandleByUUID(const char* uuid, nvmlDevice_t* device);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetHandleByPciBusId(const char* pci_bus_id,
                                                           nvmlDevice_t* device);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetHandleByPciBusId_v2(const char* pci_bus_id,
                                                              nvmlDevice_t* device);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetIndex(nvmlDevice_t device, unsigned int* index);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char* name,
                                               unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetUUID(nvmlDevice_t device, char* uuid,
                                               unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t* pci);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPciInfo_v2(nvmlDevice_t device, nvmlPciInfo_t* pci);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPciInfo_v3(nvmlDevice_t device, nvmlPciInfo_t* pci);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPciInfoExt(nvmlDevice_t device, nvmlPciInfoExt_t* pci);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t* memory);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(nvmlDevice_t device,
                                                        nvmlMemory_v2_t* memory);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t device,
                                                           nvmlUtilization_t* utilization);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetCudaComputeCapability(nvmlDevice_t device, int* major,
                                                                int* minor);

MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetComputeRunningProcesses(nvmlDevice_t device,
                                                                  unsigned int* count,
                                                                  nvmlProcessInfo_v1_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetComputeRunningProcesses_v2(nvmlDevice_t device,
                                                                     unsigned int* count,
                                                                     nvmlProcessInfo_v2_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetComputeRunningProcesses_v3(nvmlDevice_t device,
                                                                     unsigned int* count,
                                                                     nvmlProcessInfo_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses(nvmlDevice_t device,
                                                                   unsigned int* count,
                                                                   nvmlProcessInfo_v1_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses_v2(nvmlDevice_t device,
                                                                      unsigned int* count,
                                                                      nvmlProcessInfo_v2_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses_v3(nvmlDevice_t device,
                                                                      unsigned int* count,
                                                                      nvmlProcessInfo_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMPSComputeRunningProcesses(nvmlDevice_t device,
                                                                     unsigned int* count,
                                                                     nvmlProcessInfo_v1_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMPSComputeRunningProcesses_v2(
    nvmlDevice_t device, unsigned int* count, nvmlProcessInfo_v2_t* infos);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMPSComputeRunningProcesses_v3(nvmlDevice_t device,
                                                                        unsigned int* count,
                                                                        nvmlProcessInfo_t* infos);

MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetBrand(nvmlDevice_t device, nvmlBrandType_t* brand);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetSerial(nvmlDevice_t device, char* serial,
                                                 unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMinorNumber(nvmlDevice_t device,
                                                      unsigned int* minor_number);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetVbiosVersion(nvmlDevice_t device, char* version,
                                                       unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPersistenceMode(nvmlDevice_t device,
                                                          nvmlEnableState_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceSetPersistenceMode(nvmlDevice_t device,
                                                          nvmlEnableState_t mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDisplayMode(nvmlDevice_t device, nvmlEnableState_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDisplayActive(nvmlDevice_t device,
                                                        nvmlEnableState_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int* speed);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetTemperature(nvmlDevice_t device,
                                                      nvmlTemperatureSensors_t sensor,
                                                      unsigned int* temperature);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetTemperatureV(nvmlDevice_t device,
                                                       nvmlTemperature_t* temperature);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPerformanceState(nvmlDevice_t device,
                                                           nvmlPstates_t* state);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int* milliwatts);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPowerManagementMode(nvmlDevice_t device,
                                                              nvmlEnableState_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPowerManagementLimit(nvmlDevice_t device,
                                                               unsigned int* milliwatts);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetEnforcedPowerLimit(nvmlDevice_t device,
                                                             unsigned int* milliwatts);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPowerManagementDefaultLimit(nvmlDevice_t device,
                                                                      unsigned int* milliwatts);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPowerManagementLimitConstraints(
    nvmlDevice_t device, unsigned int* minimum_milliwatts, unsigned int* maximum_milliwatts);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetClockInfo(nvmlDevice_t device, nvmlClockType_t type,
                                                    unsigned int* mhz);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetComputeMode(nvmlDevice_t device, nvmlComputeMode_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceSetComputeMode(nvmlDevice_t device, nvmlComputeMode_t mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetBAR1MemoryInfo(nvmlDevice_t device,
                                                         nvmlBAR1Memory_t* memory);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetArchitecture(nvmlDevice_t device,
                                                       nvmlDeviceArchitecture_t* architecture);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMigMode(nvmlDevice_t device, unsigned int* current,
                                                  unsigned int* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMaxMigDeviceCount(nvmlDevice_t device,
                                                            unsigned int* count);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceIsMigDeviceHandle(nvmlDevice_t device,
                                                         unsigned int* is_mig_device);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetTotalEccErrors(nvmlDevice_t device,
                                                         nvmlMemoryErrorType_t error_type,
                                                         nvmlEccCounterType_t counter_type,
                                                         unsigned long long* count);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetEccMode(nvmlDevice_t device, nvmlEnableState_t* current,
                                                  nvmlEnableState_t* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetC2cModeInfoV(nvmlDevice_t device,
                                                       nvmlC2cModeInfo_v1_t* c2c_mode_info);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetAddressingMode(nvmlDevice_t device,
                                                         nvmlDeviceAddressingMode_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetBoardPartNumber(nvmlDevice_t device, char* part_number,
                                                          unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetInforomVersion(nvmlDevice_t device,
                                                         nvmlInforomObject_t object, char* version,
                                                         unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetInforomImageVersion(nvmlDevice_t device, char* version,
                                                              unsigned int length);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetLastBBXFlushTime(nvmlDevice_t device,
                                                           unsigned long long* timestamp,
                                                           unsigned long* duration_us);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetBBXTimeData_v1(nvmlDevice_t device,
                                                         nvmlBBXTimeData_v1_t* time_data);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMaxPcieLinkGeneration(nvmlDevice_t device,
                                                                unsigned int* generation);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGpuMaxPcieLinkGeneration(nvmlDevice_t device,
                                                                   unsigned int* generation);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMaxPcieLinkWidth(nvmlDevice_t device,
                                                           unsigned int* width);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetCurrPcieLinkGeneration(nvmlDevice_t device,
                                                                 unsigned int* generation);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetCurrPcieLinkWidth(nvmlDevice_t device,
                                                            unsigned int* width);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPcieThroughput(nvmlDevice_t device,
                                                         nvmlPcieUtilCounter_t counter,
                                                         unsigned int* value);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPcieReplayCounter(nvmlDevice_t device,
                                                            unsigned int* value);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMaxClockInfo(nvmlDevice_t device, nvmlClockType_t type,
                                                       unsigned int* clock);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetClock(nvmlDevice_t device, nvmlClockType_t clock_type,
                                                nvmlClockId_t clock_id, unsigned int* clock_mhz);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetAutoBoostedClocksEnabled(
    nvmlDevice_t device, nvmlEnableState_t* enabled, nvmlEnableState_t* default_enabled);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetTemperatureThreshold(
    nvmlDevice_t device, nvmlTemperatureThresholds_t threshold_type, unsigned int* temperature);
MF_NVML_ABI_API nvmlReturn_t
nvmlDeviceGetMarginTemperature(nvmlDevice_t device, nvmlMarginTemperature_t* margin_temperature);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetSupportedClocksEventReasons(nvmlDevice_t device,
                                                                      unsigned long long* reasons);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGpuOperationMode(nvmlDevice_t device,
                                                           nvmlGpuOperationMode_t* current,
                                                           nvmlGpuOperationMode_t* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDramEncryptionMode(nvmlDevice_t device,
                                                             nvmlDramEncryptionInfo_t* current,
                                                             nvmlDramEncryptionInfo_t* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetBoardId(nvmlDevice_t device, unsigned int* board_id);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMultiGpuBoard(nvmlDevice_t device,
                                                        unsigned int* multi_gpu);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetEncoderUtilization(nvmlDevice_t device,
                                                             unsigned int* utilization,
                                                             unsigned int* sampling_period_us);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetEncoderStats(nvmlDevice_t device,
                                                       unsigned int* session_count,
                                                       unsigned int* average_fps,
                                                       unsigned int* average_latency);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDecoderUtilization(nvmlDevice_t device,
                                                             unsigned int* utilization,
                                                             unsigned int* sampling_period_us);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetJpgUtilization(nvmlDevice_t device,
                                                         unsigned int* utilization,
                                                         unsigned int* sampling_period_us);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetOfaUtilization(nvmlDevice_t device,
                                                         unsigned int* utilization,
                                                         unsigned int* sampling_period_us);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetFBCStats(nvmlDevice_t device, nvmlFBCStats_t* stats);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDriverModel_v2(nvmlDevice_t device,
                                                         nvmlDriverModel_t* current,
                                                         nvmlDriverModel_t* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetBridgeChipInfo(nvmlDevice_t device,
                                                         nvmlBridgeChipHierarchy_t* hierarchy);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGpuFabricInfoV(nvmlDevice_t device,
                                                         nvmlGpuFabricInfoV_t* fabric_info);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetConfComputeProtectedMemoryUsage(nvmlDevice_t device,
                                                                          nvmlMemory_t* memory);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGspFirmwareVersion(nvmlDevice_t device, char* version);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetAccountingMode(nvmlDevice_t device,
                                                         nvmlEnableState_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetAccountingBufferSize(nvmlDevice_t device,
                                                               unsigned int* buffer_size);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetRetiredPages(nvmlDevice_t device,
                                                       nvmlPageRetirementCause_t cause,
                                                       unsigned int* page_count,
                                                       unsigned long long* addresses);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetRetiredPagesPendingStatus(nvmlDevice_t device,
                                                                    nvmlEnableState_t* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPdi(nvmlDevice_t device, nvmlPdi_t* pdi);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetFieldValues(nvmlDevice_t device, int value_count,
                                                      nvmlFieldValue_t* values);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetVirtualizationMode(nvmlDevice_t device,
                                                             nvmlGpuVirtualizationMode_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetHostVgpuMode(nvmlDevice_t device,
                                                       nvmlHostVgpuMode_t* mode);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetVgpuHeterogeneousMode(nvmlDevice_t device,
                                                                nvmlVgpuHeterogeneousMode_t* mode);
MF_NVML_ABI_API nvmlReturn_t
nvmlDeviceGetGridLicensableFeatures_v4(nvmlDevice_t device, nvmlGridLicensableFeatures_t* features);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetCapabilities(nvmlDevice_t device,
                                                       nvmlDeviceCapabilities_t* capabilities);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetRemappedRows_v2(nvmlDevice_t device,
                                                          nvmlRemappedRowsInfo_v2_t* information);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceWorkloadPowerProfileGetCurrentProfiles(
    nvmlDevice_t device, nvmlWorkloadPowerProfileCurrentProfiles_t* current_profiles);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetApplicationsClock(nvmlDevice_t device,
                                                            nvmlClockType_t clock_type,
                                                            unsigned int* clock_mhz);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDefaultApplicationsClock(nvmlDevice_t device,
                                                                   nvmlClockType_t clock_type,
                                                                   unsigned int* clock_mhz);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetDriverModel(nvmlDevice_t device,
                                                      nvmlDriverModel_t* current,
                                                      nvmlDriverModel_t* pending);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetGpuFabricInfo(nvmlDevice_t device,
                                                        nvmlGpuFabricInfo_t* fabric_info);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetMemoryErrorCounter(nvmlDevice_t device,
                                                             nvmlMemoryErrorType_t error_type,
                                                             nvmlEccCounterType_t counter_type,
                                                             nvmlMemoryLocation_t location_type,
                                                             unsigned long long* count);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetModuleId(nvmlDevice_t device, unsigned int* module_id);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetRemappedRows(nvmlDevice_t device,
                                                       unsigned int* corrected_rows,
                                                       unsigned int* uncorrected_rows,
                                                       unsigned int* pending,
                                                       unsigned int* failure_occurred);
MF_NVML_ABI_API nvmlReturn_t
nvmlDeviceGetSupportedClocksThrottleReasons(nvmlDevice_t device, unsigned long long* reasons);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetAccountingPids(nvmlDevice_t device, unsigned int* count,
                                                         unsigned int* pids);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetPowerState(nvmlDevice_t device, nvmlPstates_t* state);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetSupportedMemoryClocks(nvmlDevice_t device,
                                                                unsigned int* count,
                                                                unsigned int* clocks_mhz);
MF_NVML_ABI_API nvmlReturn_t nvmlEventSetCreate(nvmlEventSet_t* set);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceRegisterEvents(nvmlDevice_t device,
                                                      unsigned long long event_types,
                                                      nvmlEventSet_t set);
MF_NVML_ABI_API nvmlReturn_t nvmlDeviceGetSupportedEventTypes(nvmlDevice_t device,
                                                              unsigned long long* event_types);
MF_NVML_ABI_API nvmlReturn_t nvmlEventSetWait(nvmlEventSet_t set, nvmlEventData_t* data,
                                              unsigned int timeout_ms);
MF_NVML_ABI_API nvmlReturn_t nvmlEventSetWait_v2(nvmlEventSet_t set, nvmlEventData_t* data,
                                                 unsigned int timeout_ms);
MF_NVML_ABI_API nvmlReturn_t nvmlEventSetFree(nvmlEventSet_t set);

#if !defined(METAFLUX_NVML_ABI_INTERNAL)
#define nvmlInit nvmlInit_v2
#define nvmlDeviceGetPciInfo nvmlDeviceGetPciInfo_v3
#define nvmlDeviceGetCount nvmlDeviceGetCount_v2
#define nvmlDeviceGetHandleByIndex nvmlDeviceGetHandleByIndex_v2
#define nvmlDeviceGetHandleByPciBusId nvmlDeviceGetHandleByPciBusId_v2
#define nvmlDeviceGetComputeRunningProcesses nvmlDeviceGetComputeRunningProcesses_v3
#define nvmlDeviceGetGraphicsRunningProcesses nvmlDeviceGetGraphicsRunningProcesses_v3
#define nvmlDeviceGetMPSComputeRunningProcesses nvmlDeviceGetMPSComputeRunningProcesses_v3
#endif

#ifdef __cplusplus
}
#endif

#endif
