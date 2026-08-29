#include <cuda.h>
#include <cudaTypedefs.h>
#include <nvml.h>

#if !defined(CUDA_VERSION) || CUDA_VERSION < 12020
#error "M0001 requires CUDA Driver headers from toolkit 12.2 or newer"
#endif

#if !defined(NVML_DEVICE_UUID_BUFFER_SIZE)
#error "NVML UUID buffer contract is missing"
#endif

#if defined(__cplusplus)
#define METAFLUX_HEADER_STATIC_ASSERT static_assert
#else
#define METAFLUX_HEADER_STATIC_ASSERT _Static_assert
#endif

METAFLUX_HEADER_STATIC_ASSERT(sizeof(CUuuid) == 16, "CUDA UUID ABI changed");
METAFLUX_HEADER_STATIC_ASSERT(sizeof(nvmlPciInfo_t) > 0, "NVML PCI identity ABI is missing");
METAFLUX_HEADER_STATIC_ASSERT(NVML_DEVICE_UUID_BUFFER_SIZE >= 80, "NVML UUID buffer is too small");

int metaflux_nvidia_header_probe(void) {
  return (int)(sizeof(&cuInit) + sizeof(&cuGetProcAddress) + sizeof(&nvmlInit_v2) +
               sizeof(&nvmlDeviceGetCount_v2));
}

#undef METAFLUX_HEADER_STATIC_ASSERT
