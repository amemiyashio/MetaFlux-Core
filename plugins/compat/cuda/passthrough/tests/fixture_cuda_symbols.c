#define METAFLUX_CUDA_ABI_INTERNAL 1
#include "metaflux/cuda/abi.h"

extern void mf_fixture_cuda_record_call(const char* name);

#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments)                        \
  __attribute__((weak)) CUresult name parameters {                                                 \
    mf_fixture_cuda_record_call(#name);                                                            \
    return CUDA_ERROR_NOT_SUPPORTED;                                                               \
  }
#include "../../abi/driver/symbols.def"
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL
