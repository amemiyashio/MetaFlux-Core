#ifndef METAFLUX_NVML_PROVIDER_H
#define METAFLUX_NVML_PROVIDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_NVML_PROVIDER_API __attribute__((visibility("default")))
#else
#define MF_NVML_PROVIDER_API
#endif

MF_NVML_PROVIDER_API uint32_t mf_nvml_provider_bootstrap_abi_version(void);

#ifdef __cplusplus
}
#endif

#endif
