#ifndef METAFLUX_BACKEND_CPU_H
#define METAFLUX_BACKEND_CPU_H

#include "metaflux/backend/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The CPU backend accepts canonical Kernel IR as a loadable module artifact.
 * This is an internal backend contract; CUDA/PTX parsing remains owned by the
 * compatibility layer and daemon before the artifact reaches this API. */
#define MF_CPU_BACKEND_ARGUMENT_BLOCK_MAGIC_V1 UINT32_C(0x3141424d)
#define MF_CPU_BACKEND_ARGUMENT_BLOCK_VERSION_V1 UINT32_C(1)
#define MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1 UINT32_C(1)
#define MF_CPU_BACKEND_ARGUMENT_KIND_U32_V1 UINT32_C(2)
#define MF_CPU_BACKEND_ARGUMENT_KIND_F32_V1 UINT32_C(3)
#define MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1 UINT32_C(1)
#define MF_CPU_BACKEND_ARGUMENT_BUFFER_WRITE_V1 UINT32_C(2)
#define MF_CPU_BACKEND_ARGUMENT_BUFFER_KNOWN_FLAGS_V1                                              \
  (MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1 | MF_CPU_BACKEND_ARGUMENT_BUFFER_WRITE_V1)
#define MF_CPU_BACKEND_MAX_ARGUMENTS_V1 UINT32_C(64)

typedef struct mf_cpu_backend_argument_block_header_v1 {
  uint32_t magic;
  uint32_t version;
  uint32_t header_size;
  uint32_t entry_size;
  uint32_t entry_count;
  uint32_t reserved_word;
  uint64_t total_size;
  uint64_t reserved[2];
} mf_cpu_backend_argument_block_header_v1;

typedef struct mf_cpu_backend_argument_v1 {
  uint32_t kind;
  uint32_t flags;
  uint64_t memory;
  uint64_t offset;
  uint64_t byte_count;
  uint64_t value;
} mf_cpu_backend_argument_v1;

const mf_backend_api_v1* mf_cpu_backend_get_api_v1(void);

/* Import one caller-owned host range as a CPU backend memory handle.
 * The range remains caller-owned and must outlive the returned handle. */
mf_backend_status_v1 mf_cpu_backend_import_host_memory_v1(mf_backend_instance_v1 instance,
                                                          mf_backend_context_v1 context,
                                                          void* address, uint64_t byte_count,
                                                          mf_backend_memory_v1* out_memory);

#ifdef __cplusplus
}
#endif

#endif
