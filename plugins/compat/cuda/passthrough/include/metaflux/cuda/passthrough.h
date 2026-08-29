#ifndef METAFLUX_CUDA_PASSTHROUGH_H
#define METAFLUX_CUDA_PASSTHROUGH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_CUDA_PASSTHROUGH_ABI_VERSION_V1 UINT32_C(1)

typedef uint32_t mf_cuda_passthrough_status_v1;

#define MF_CUDA_PASSTHROUGH_SUCCESS UINT32_C(0)
#define MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT UINT32_C(1)
#define MF_CUDA_PASSTHROUGH_INVALID_MODE UINT32_C(2)
#define MF_CUDA_PASSTHROUGH_NOT_FOUND UINT32_C(3)
#define MF_CUDA_PASSTHROUGH_POLICY_REJECTED UINT32_C(4)
#define MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG UINT32_C(5)
#define MF_CUDA_PASSTHROUGH_MALFORMED_ELF UINT32_C(6)
#define MF_CUDA_PASSTHROUGH_BUILD_MISMATCH UINT32_C(7)
#define MF_CUDA_PASSTHROUGH_SYMBOL_MISSING UINT32_C(8)
#define MF_CUDA_PASSTHROUGH_LOAD_FAILED UINT32_C(9)
#define MF_CUDA_PASSTHROUGH_STALE UINT32_C(10)
#define MF_CUDA_PASSTHROUGH_PARTIAL_STATE UINT32_C(11)
#define MF_CUDA_PASSTHROUGH_SYSTEM_ERROR UINT32_C(12)

typedef uint32_t mf_cuda_runtime_mode_v1;

#define MF_CUDA_RUNTIME_MODE_MANAGED_V1 UINT32_C(1)
#define MF_CUDA_RUNTIME_MODE_PASSTHROUGH_V1 UINT32_C(2)
#define MF_CUDA_RUNTIME_MODE_AUTO_V1 UINT32_C(3)

typedef uint32_t mf_cuda_selected_runtime_v1;

#define MF_CUDA_SELECTED_NONE_V1 UINT32_C(0)
#define MF_CUDA_SELECTED_MANAGED_V1 UINT32_C(1)
#define MF_CUDA_SELECTED_PASSTHROUGH_V1 UINT32_C(2)

typedef uint32_t mf_cuda_vendor_library_v1;

#define MF_CUDA_VENDOR_LIBRARY_CUDA_V1 UINT32_C(1)
#define MF_CUDA_VENDOR_LIBRARY_NVML_V1 UINT32_C(2)

typedef struct mf_cuda_passthrough_pair_v1 mf_cuda_passthrough_pair_v1;

typedef struct mf_cuda_passthrough_bootstrap_v1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void* cu_init;
  void* cu_driver_get_version;
  void* cu_get_proc_address;
  void* nvml_init_v2;
  void* nvml_shutdown;
  void* nvml_system_get_driver_version;
} mf_cuda_passthrough_bootstrap_v1;

/*
 * prepare and commit may create managed state. rollback must be idempotent and
 * accept the ticket value left by prepare, including zero after early failure.
 * rollback followed by is_pristine must prove that no managed state is visible
 * before auto mode may activate the vendor pair. A successful commit selects
 * managed mode.
 */
typedef struct mf_cuda_managed_callbacks_v1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void* context;
  mf_cuda_passthrough_status_v1 (*prepare)(void* context, uint64_t* out_ticket);
  mf_cuda_passthrough_status_v1 (*commit)(void* context, uint64_t ticket);
  mf_cuda_passthrough_status_v1 (*rollback)(void* context, uint64_t ticket);
  int32_t (*is_pristine)(void* context);
  uint64_t reserved[2];
} mf_cuda_managed_callbacks_v1;

mf_cuda_passthrough_status_v1 mf_cuda_runtime_mode_parse_v1(const char* text,
                                                            mf_cuda_runtime_mode_v1* out_mode);

/*
 * Uses only D0013 production defaults: trusted UID 0 and fixed paths. Success
 * transfers one pair to the caller; every failure stores null in out_pair.
 */
mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_load_v1(mf_cuda_passthrough_pair_v1** out_pair);

/* Release must not race another operation on pair. Null is accepted. */
void mf_cuda_passthrough_pair_release_v1(mf_cuda_passthrough_pair_v1* pair);

mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_validate_current_v1(const mf_cuda_passthrough_pair_v1* pair);

const char* mf_cuda_passthrough_pair_cuda_path_v1(const mf_cuda_passthrough_pair_v1* pair);

const char* mf_cuda_passthrough_pair_nvml_path_v1(const mf_cuda_passthrough_pair_v1* pair);

const char* mf_cuda_passthrough_pair_driver_build_v1(const mf_cuda_passthrough_pair_v1* pair);

int64_t mf_cuda_passthrough_pair_namespace_id_v1(const mf_cuda_passthrough_pair_v1* pair);

const mf_cuda_passthrough_bootstrap_v1*
mf_cuda_passthrough_pair_bootstrap_v1(const mf_cuda_passthrough_pair_v1* pair);

/*
 * Returned paths, build text, bootstrap storage, and symbols are borrowed for
 * the pair lifetime. A null symbol_version requests the default symbol; an
 * empty version is invalid. Lookup clears out_symbol on every non-success.
 */
mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_lookup_v1(const mf_cuda_passthrough_pair_v1* pair,
                                   mf_cuda_vendor_library_v1 library, const char* symbol_name,
                                   const char* symbol_version, void** out_symbol);

/*
 * On success, managed selection returns a null pair. Passthrough selection
 * transfers one owned pair to the caller. All failure paths return a null pair.
 */
mf_cuda_passthrough_status_v1 mf_cuda_runtime_select_v1(mf_cuda_runtime_mode_v1 requested_mode,
                                                        const mf_cuda_managed_callbacks_v1* managed,
                                                        mf_cuda_selected_runtime_v1* out_selected,
                                                        mf_cuda_passthrough_pair_v1** out_pair);

#ifdef __cplusplus
}
#endif

#endif
