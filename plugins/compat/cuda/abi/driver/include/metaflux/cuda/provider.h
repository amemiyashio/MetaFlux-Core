#ifndef METAFLUX_CUDA_PROVIDER_H
#define METAFLUX_CUDA_PROVIDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_CUDA_PROVIDER_API __attribute__((visibility("default")))
#else
#define MF_CUDA_PROVIDER_API
#endif

MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_bootstrap_abi_version(void);

#ifdef __cplusplus
}
#endif

#endif
