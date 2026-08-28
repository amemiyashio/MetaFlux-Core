#ifndef METAFLUX_BACKEND_API_H
#define METAFLUX_BACKEND_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_BACKEND_ABI_VERSION_1 UINT32_C(1)

typedef struct mf_backend_api_header_v1 {
  uint32_t abi_version;
  uint32_t struct_size;
  uint64_t capabilities;
  const void* extensions;
} mf_backend_api_header_v1;

typedef struct mf_backend_api_v1 {
  mf_backend_api_header_v1 header;
} mf_backend_api_v1;

typedef const mf_backend_api_v1* (*mf_backend_get_api_v1_fn)(void);

#ifdef __cplusplus
}
#endif

#endif
