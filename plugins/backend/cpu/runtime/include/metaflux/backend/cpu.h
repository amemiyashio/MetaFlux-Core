#ifndef METAFLUX_BACKEND_CPU_H
#define METAFLUX_BACKEND_CPU_H

#include "metaflux/backend/api.h"

#ifdef __cplusplus
extern "C" {
#endif

const mf_backend_api_v1* mf_cpu_backend_get_api_v1(void);

#ifdef __cplusplus
}
#endif

#endif
