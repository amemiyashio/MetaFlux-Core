#ifndef METAFLUX_BACKEND_CPU_H
#define METAFLUX_BACKEND_CPU_H

#include "metaflux/backend/api.h"

#ifdef __cplusplus
extern "C" {
#endif

const mf_backend_api_v1* mf_cpu_backend_get_api_v1(void);

/* Import one caller-owned host range as a CPU backend memory handle.
 * The range remains caller-owned and must outlive the returned handle. */
mf_backend_status_v1 mf_cpu_backend_import_host_memory_v1(mf_backend_instance_v1 instance,
                                                          mf_backend_context_v1 context,
                                                          void* address,
                                                          uint64_t byte_count,
                                                          mf_backend_memory_v1* out_memory);

#ifdef __cplusplus
}
#endif

#endif
