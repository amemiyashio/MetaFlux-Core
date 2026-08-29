#define METAFLUX_NVML_ABI_INTERNAL 1
#include "metaflux/nvml/abi.h"

extern void mf_fixture_nvml_record_call(const char* name);

#define MF_FIXTURE_NVML_REGULAR(name, result, parameters)                                          \
  __attribute__((weak)) result name parameters {                                                   \
    mf_fixture_nvml_record_call(#name);                                                            \
    return NVML_ERROR_NOT_SUPPORTED;                                                               \
  }
#define MF_FIXTURE_NVML_INIT_ZERO(name, result, parameters)                                        \
  MF_FIXTURE_NVML_REGULAR(name, result, parameters)
#define MF_FIXTURE_NVML_INIT_FLAGS(name, result, parameters)                                       \
  MF_FIXTURE_NVML_REGULAR(name, result, parameters)
#define MF_FIXTURE_NVML_SHUTDOWN(name, result, parameters)                                         \
  MF_FIXTURE_NVML_REGULAR(name, result, parameters)
#define MF_FIXTURE_NVML_ERROR_STRING(name, result, parameters)                                     \
  __attribute__((weak)) result name parameters {                                                   \
    mf_fixture_nvml_record_call(#name);                                                            \
    return "Vendor Error";                                                                         \
  }
#define MF_NVML_INTERNAL(name)
#define MF_NVML_SYMBOL(name, target, status, route, result, parameters, arguments)                 \
  MF_FIXTURE_NVML_##route(name, result, parameters)
#include "../../management/nvml/symbols.def"
#undef MF_NVML_SYMBOL
#undef MF_NVML_INTERNAL
#undef MF_FIXTURE_NVML_ERROR_STRING
#undef MF_FIXTURE_NVML_SHUTDOWN
#undef MF_FIXTURE_NVML_INIT_FLAGS
#undef MF_FIXTURE_NVML_INIT_ZERO
#undef MF_FIXTURE_NVML_REGULAR
