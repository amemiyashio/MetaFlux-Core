#define METAFLUX_CUDA_ABI_INTERNAL 1
#include "metaflux/cuda/provider.h"

#include "metaflux/cuda/passthrough.h"
#if defined(METAFLUX_PROVIDER_TESTING)
#include "passthrough_internal.h"
#endif

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct mf_cuda_dispatch_state {
  atomic_flag lock;
  atomic_uint_fast64_t lock_acquisitions;
  mf_cuda_passthrough_pair_v1* pair;
  _Atomic(pid_t) owner_pid;
  mf_cuda_runtime_mode_v1 requested_mode;
  _Atomic(mf_cuda_selected_runtime_v1) selected_runtime;
  mf_cuda_passthrough_status_v1 mode_status;
  mf_cuda_passthrough_status_v1 selector_status;
  uint32_t mode_frozen;
  uint32_t atfork_registered;
#if defined(METAFLUX_PROVIDER_TESTING)
  mf_cuda_passthrough_policy_v1 test_policy;
  uint32_t test_policy_installed;
  uint32_t force_dirty_rollback;
#endif
} mf_cuda_dispatch_state;

typedef struct mf_cuda_managed_transaction {
  CUresult result;
  uint32_t prepared;
  uint32_t attempted;
} mf_cuda_managed_transaction;

static mf_cuda_dispatch_state mf_cuda_dispatch_global = {.lock = ATOMIC_FLAG_INIT,
                                                         .owner_pid = (pid_t)-1};

#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments)                        \
  extern CUresult mf_cuda_managed_##name parameters;
#include "../symbols.def"
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL

extern void mf_cuda_provider_managed_rollback_v1(void);
extern int32_t mf_cuda_provider_managed_is_pristine_v1(void);
#if defined(METAFLUX_PROVIDER_TESTING)
extern void mf_cuda_provider_test_reset_managed_v1(void);
#endif

static void mf_cuda_dispatch_lock(void) {
  while (atomic_flag_test_and_set_explicit(&mf_cuda_dispatch_global.lock, memory_order_acquire)) {
  }
  (void)atomic_fetch_add_explicit(&mf_cuda_dispatch_global.lock_acquisitions, UINT64_C(1),
                                  memory_order_relaxed);
}

static void mf_cuda_dispatch_unlock(void) {
  atomic_flag_clear_explicit(&mf_cuda_dispatch_global.lock, memory_order_release);
}

static void mf_cuda_dispatch_after_fork_child(void) {
  atomic_flag_clear_explicit(&mf_cuda_dispatch_global.lock, memory_order_release);
}

static CUresult mf_cuda_selector_error(mf_cuda_passthrough_status_v1 status) {
  switch (status) {
  case MF_CUDA_PASSTHROUGH_SUCCESS:
    return CUDA_SUCCESS;
  case MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT:
  case MF_CUDA_PASSTHROUGH_INVALID_MODE:
    return CUDA_ERROR_INVALID_VALUE;
  case MF_CUDA_PASSTHROUGH_STALE:
    return CUDA_ERROR_DEINITIALIZED;
  case MF_CUDA_PASSTHROUGH_NOT_FOUND:
  case MF_CUDA_PASSTHROUGH_POLICY_REJECTED:
  case MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG:
  case MF_CUDA_PASSTHROUGH_MALFORMED_ELF:
  case MF_CUDA_PASSTHROUGH_BUILD_MISMATCH:
  case MF_CUDA_PASSTHROUGH_SYMBOL_MISSING:
  case MF_CUDA_PASSTHROUGH_LOAD_FAILED:
    return CUDA_ERROR_SYSTEM_NOT_READY;
  case MF_CUDA_PASSTHROUGH_PARTIAL_STATE:
  case MF_CUDA_PASSTHROUGH_SYSTEM_ERROR:
  default:
    return CUDA_ERROR_UNKNOWN;
  }
}

static mf_cuda_passthrough_status_v1 mf_cuda_managed_prepare(void* context, uint64_t* out_ticket) {
  mf_cuda_managed_transaction* transaction = (mf_cuda_managed_transaction*)context;
  if (transaction == (mf_cuda_managed_transaction*)0 || out_ticket == (uint64_t*)0) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_ticket = UINT64_C(0);
  transaction->attempted = UINT32_C(1);
  transaction->result = mf_cuda_managed_cuInit(UINT32_C(0));
  if (transaction->result != CUDA_SUCCESS) {
    return MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  }
  transaction->prepared = UINT32_C(1);
  *out_ticket = UINT64_C(1);
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static mf_cuda_passthrough_status_v1 mf_cuda_managed_commit(void* context, uint64_t ticket) {
  const mf_cuda_managed_transaction* transaction = (const mf_cuda_managed_transaction*)context;
  return transaction != (const mf_cuda_managed_transaction*)0 &&
                 transaction->prepared != UINT32_C(0) && ticket == UINT64_C(1)
             ? MF_CUDA_PASSTHROUGH_SUCCESS
             : MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
}

static mf_cuda_passthrough_status_v1 mf_cuda_managed_rollback(void* context, uint64_t ticket) {
  mf_cuda_managed_transaction* transaction = (mf_cuda_managed_transaction*)context;
  if (transaction == (mf_cuda_managed_transaction*)0 ||
      (ticket != UINT64_C(0) && ticket != UINT64_C(1))) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  mf_cuda_provider_managed_rollback_v1();
  transaction->prepared = UINT32_C(0);
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static int32_t mf_cuda_managed_is_pristine(void* context) {
  (void)context;
#if defined(METAFLUX_PROVIDER_TESTING)
  if (mf_cuda_dispatch_global.force_dirty_rollback != UINT32_C(0)) {
    return INT32_C(0);
  }
#endif
  return mf_cuda_provider_managed_is_pristine_v1();
}

static mf_cuda_passthrough_status_v1 mf_cuda_freeze_mode_locked(void) {
  const char* configured = (const char*)0;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  if (mf_cuda_dispatch_global.mode_frozen != UINT32_C(0)) {
    return mf_cuda_dispatch_global.mode_status;
  }
  configured = getenv("METAFLUX_MODE");
  if (configured == (const char*)0) {
    configured = "auto";
  }
  status = mf_cuda_runtime_mode_parse_v1(configured, &mf_cuda_dispatch_global.requested_mode);
  mf_cuda_dispatch_global.mode_frozen = UINT32_C(1);
  atomic_store_explicit(&mf_cuda_dispatch_global.owner_pid, getpid(), memory_order_relaxed);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS &&
      mf_cuda_dispatch_global.atfork_registered == UINT32_C(0)) {
    if (pthread_atfork((void (*)(void))0, (void (*)(void))0, mf_cuda_dispatch_after_fork_child) !=
        0) {
      status = MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    } else {
      mf_cuda_dispatch_global.atfork_registered = UINT32_C(1);
    }
  }
  mf_cuda_dispatch_global.mode_status = status;
  mf_cuda_dispatch_global.selector_status = status;
  return status;
}

static mf_cuda_passthrough_status_v1
mf_cuda_select_locked(mf_cuda_managed_transaction* transaction) {
  mf_cuda_managed_callbacks_v1 callbacks;
  mf_cuda_passthrough_pair_v1* pair = (mf_cuda_passthrough_pair_v1*)0;
  mf_cuda_selected_runtime_v1 selected = MF_CUDA_SELECTED_NONE_V1;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  (void)memset(&callbacks, 0, sizeof(callbacks));
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.abi_version = MF_CUDA_PASSTHROUGH_ABI_VERSION_V1;
  callbacks.context = transaction;
  callbacks.prepare = mf_cuda_managed_prepare;
  callbacks.commit = mf_cuda_managed_commit;
  callbacks.rollback = mf_cuda_managed_rollback;
  callbacks.is_pristine = mf_cuda_managed_is_pristine;
#if defined(METAFLUX_PROVIDER_TESTING)
  if (mf_cuda_dispatch_global.test_policy_installed != UINT32_C(0)) {
    status = mf_cuda_runtime_select_with_policy_v1(mf_cuda_dispatch_global.requested_mode,
                                                   &callbacks, &mf_cuda_dispatch_global.test_policy,
                                                   &selected, &pair);
  } else {
    status = mf_cuda_runtime_select_v1(mf_cuda_dispatch_global.requested_mode, &callbacks,
                                       &selected, &pair);
  }
#else
  status = mf_cuda_runtime_select_v1(mf_cuda_dispatch_global.requested_mode, &callbacks, &selected,
                                     &pair);
#endif
  mf_cuda_dispatch_global.selector_status = status;
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    mf_cuda_dispatch_global.pair = pair;
    atomic_store_explicit(&mf_cuda_dispatch_global.selected_runtime, selected,
                          memory_order_release);
  }
  return status;
}

static CUresult mf_cuda_lookup_vendor_locked(const char* symbol, void** out_symbol) {
  const mf_cuda_passthrough_status_v1 status = mf_cuda_passthrough_pair_lookup_v1(
      mf_cuda_dispatch_global.pair, MF_CUDA_VENDOR_LIBRARY_CUDA_V1, symbol, "libcuda.so.1",
      out_symbol);
  return mf_cuda_selector_error(status);
}

static CUresult mf_cuda_dispatch_init_v1(unsigned int flags) {
  mf_cuda_managed_transaction transaction = {
      .result = CUDA_ERROR_UNKNOWN, .prepared = UINT32_C(0), .attempted = UINT32_C(0)};
  mf_cuda_selected_runtime_v1 selected = MF_CUDA_SELECTED_NONE_V1;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  void* symbol = (void*)0;
  CUresult result = CUDA_SUCCESS;
  CUresult (*vendor_init)(unsigned int) = (CUresult (*)(unsigned int))0;
  if (flags != UINT32_C(0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  selected = atomic_load_explicit(&mf_cuda_dispatch_global.selected_runtime, memory_order_acquire);
  if (selected == MF_CUDA_SELECTED_MANAGED_V1) {
    return atomic_load_explicit(&mf_cuda_dispatch_global.owner_pid, memory_order_relaxed) ==
                   getpid()
               ? mf_cuda_managed_cuInit(flags)
               : CUDA_ERROR_DEINITIALIZED;
  }
  mf_cuda_dispatch_lock();
  status = mf_cuda_freeze_mode_locked();
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    mf_cuda_dispatch_unlock();
    return mf_cuda_selector_error(status);
  }
  if (atomic_load_explicit(&mf_cuda_dispatch_global.owner_pid, memory_order_relaxed) != getpid()) {
    mf_cuda_dispatch_unlock();
    return CUDA_ERROR_DEINITIALIZED;
  }
  selected = atomic_load_explicit(&mf_cuda_dispatch_global.selected_runtime, memory_order_relaxed);
  if (selected == MF_CUDA_SELECTED_MANAGED_V1) {
    mf_cuda_dispatch_unlock();
    return mf_cuda_managed_cuInit(flags);
  }
  if (selected == MF_CUDA_SELECTED_NONE_V1) {
    status = mf_cuda_select_locked(&transaction);
    if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
      const mf_cuda_runtime_mode_v1 requested = mf_cuda_dispatch_global.requested_mode;
      mf_cuda_dispatch_unlock();
      return requested == MF_CUDA_RUNTIME_MODE_MANAGED_V1 && transaction.attempted != UINT32_C(0)
                 ? transaction.result
                 : mf_cuda_selector_error(status);
    }
    selected =
        atomic_load_explicit(&mf_cuda_dispatch_global.selected_runtime, memory_order_relaxed);
    if (selected == MF_CUDA_SELECTED_MANAGED_V1) {
      result = transaction.result;
      mf_cuda_dispatch_unlock();
      return result;
    }
  }
  result = mf_cuda_lookup_vendor_locked("cuInit", &symbol);
  mf_cuda_dispatch_unlock();
  if (result != CUDA_SUCCESS) {
    return result;
  }
  _Static_assert(sizeof(vendor_init) == sizeof(symbol),
                 "POSIX function and data pointers must match");
  (void)memcpy(&vendor_init, &symbol, sizeof(vendor_init));
  return vendor_init(flags);
}

static CUresult mf_cuda_dispatch_resolve_v1(const char* symbol, void** out_symbol,
                                            uint32_t* out_vendor) {
  mf_cuda_selected_runtime_v1 selected = MF_CUDA_SELECTED_NONE_V1;
  CUresult result = CUDA_SUCCESS;
  *out_symbol = (void*)0;
  *out_vendor = UINT32_C(0);
  selected = atomic_load_explicit(&mf_cuda_dispatch_global.selected_runtime, memory_order_acquire);
  if (selected == MF_CUDA_SELECTED_MANAGED_V1) {
    return atomic_load_explicit(&mf_cuda_dispatch_global.owner_pid, memory_order_relaxed) ==
                   getpid()
               ? CUDA_SUCCESS
               : CUDA_ERROR_DEINITIALIZED;
  }
  mf_cuda_dispatch_lock();
  if (mf_cuda_dispatch_global.mode_frozen != UINT32_C(0) &&
      atomic_load_explicit(&mf_cuda_dispatch_global.owner_pid, memory_order_relaxed) != getpid()) {
    result = CUDA_ERROR_DEINITIALIZED;
  } else if (atomic_load_explicit(&mf_cuda_dispatch_global.selected_runtime,
                                  memory_order_relaxed) == MF_CUDA_SELECTED_PASSTHROUGH_V1) {
    result = mf_cuda_lookup_vendor_locked(symbol, out_symbol);
    if (result == CUDA_SUCCESS) {
      *out_vendor = UINT32_C(1);
    }
  }
  mf_cuda_dispatch_unlock();
  return result;
}

#if defined(METAFLUX_PROVIDER_TESTING)
int mf_cuda_provider_test_install_passthrough_policy_v1(
    const mf_cuda_passthrough_policy_v1* policy) {
  if (policy == (const mf_cuda_passthrough_policy_v1*)0) {
    return -1;
  }
  mf_cuda_dispatch_lock();
  if (mf_cuda_dispatch_global.mode_frozen != UINT32_C(0)) {
    mf_cuda_dispatch_unlock();
    return -1;
  }
  mf_cuda_dispatch_global.test_policy = *policy;
  mf_cuda_dispatch_global.test_policy_installed = UINT32_C(1);
  mf_cuda_dispatch_unlock();
  return 0;
}

void mf_cuda_provider_test_force_dirty_rollback_v1(uint32_t enabled) {
  mf_cuda_dispatch_lock();
  mf_cuda_dispatch_global.force_dirty_rollback = enabled == UINT32_C(0) ? UINT32_C(0) : UINT32_C(1);
  mf_cuda_dispatch_unlock();
}

static void mf_cuda_snapshot_string(char* output, size_t capacity, const char* input) {
  if (capacity == (size_t)0) {
    return;
  }
  if (input == (const char*)0) {
    output[0] = '\0';
    return;
  }
  (void)snprintf(output, capacity, "%s", input);
}

int mf_cuda_provider_test_get_mode_snapshot_v1(mf_cuda_provider_test_mode_snapshot_v1* snapshot) {
  if (snapshot == (mf_cuda_provider_test_mode_snapshot_v1*)0) {
    return -1;
  }
  (void)memset(snapshot, 0, sizeof(*snapshot));
  snapshot->namespace_id = INT64_C(-1);
  mf_cuda_dispatch_lock();
  snapshot->mode_frozen = mf_cuda_dispatch_global.mode_frozen;
  snapshot->requested_mode = mf_cuda_dispatch_global.requested_mode;
  snapshot->selected_runtime = (uint32_t)atomic_load_explicit(
      &mf_cuda_dispatch_global.selected_runtime, memory_order_relaxed);
  snapshot->selector_status = mf_cuda_dispatch_global.selector_status;
  if (mf_cuda_dispatch_global.pair != (mf_cuda_passthrough_pair_v1*)0) {
    snapshot->namespace_id = mf_cuda_passthrough_pair_namespace_id_v1(mf_cuda_dispatch_global.pair);
    mf_cuda_snapshot_string(snapshot->driver_build, sizeof(snapshot->driver_build),
                            mf_cuda_passthrough_pair_driver_build_v1(mf_cuda_dispatch_global.pair));
    mf_cuda_snapshot_string(snapshot->cuda_path, sizeof(snapshot->cuda_path),
                            mf_cuda_passthrough_pair_cuda_path_v1(mf_cuda_dispatch_global.pair));
    mf_cuda_snapshot_string(snapshot->nvml_path, sizeof(snapshot->nvml_path),
                            mf_cuda_passthrough_pair_nvml_path_v1(mf_cuda_dispatch_global.pair));
  }
  mf_cuda_dispatch_unlock();
  return 0;
}

uint64_t mf_cuda_provider_test_vendor_call_count_v1(const char* symbol) {
  void* address = (void*)0;
  uint64_t (*call_count)(const char*) = (uint64_t (*)(const char*))0;
  uint64_t result = UINT64_MAX;
  mf_cuda_dispatch_lock();
  if (mf_cuda_dispatch_global.pair != (mf_cuda_passthrough_pair_v1*)0 &&
      mf_cuda_passthrough_pair_lookup_v1(
          mf_cuda_dispatch_global.pair, MF_CUDA_VENDOR_LIBRARY_CUDA_V1,
          "mf_fixture_cuda_call_count", (const char*)0, &address) == MF_CUDA_PASSTHROUGH_SUCCESS) {
    _Static_assert(sizeof(call_count) == sizeof(address),
                   "POSIX function and data pointers must match");
    (void)memcpy(&call_count, &address, sizeof(call_count));
  }
  mf_cuda_dispatch_unlock();
  if (call_count != (uint64_t (*)(const char*))0) {
    result = call_count(symbol);
  }
  return result;
}

uint32_t mf_cuda_provider_test_validate_vendor_surface_v1(void) {
  static const char* const symbols[] = {
#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments) #name,
#include "../symbols.def"
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL
  };
  size_t index = 0;
  uint32_t count = UINT32_C(0);
  mf_cuda_dispatch_lock();
  if (mf_cuda_dispatch_global.pair != (mf_cuda_passthrough_pair_v1*)0) {
    for (index = 0; index < sizeof(symbols) / sizeof(symbols[0]); ++index) {
      void* address = (void*)0;
      if (mf_cuda_passthrough_pair_lookup_v1(
              mf_cuda_dispatch_global.pair, MF_CUDA_VENDOR_LIBRARY_CUDA_V1, symbols[index],
              "libcuda.so.1", &address) != MF_CUDA_PASSTHROUGH_SUCCESS) {
        count = UINT32_C(0);
        break;
      }
      count += UINT32_C(1);
    }
  }
  mf_cuda_dispatch_unlock();
  return count;
}

uint64_t mf_cuda_provider_test_dispatch_lock_count_v1(void) {
  return (uint64_t)atomic_load_explicit(&mf_cuda_dispatch_global.lock_acquisitions,
                                        memory_order_relaxed);
}

void mf_cuda_provider_test_reset_v1(void) {
  mf_cuda_passthrough_pair_v1* pair = (mf_cuda_passthrough_pair_v1*)0;
  mf_cuda_dispatch_lock();
  pair = mf_cuda_dispatch_global.pair;
  mf_cuda_dispatch_global.pair = (mf_cuda_passthrough_pair_v1*)0;
  atomic_store_explicit(&mf_cuda_dispatch_global.owner_pid, (pid_t)-1, memory_order_relaxed);
  mf_cuda_dispatch_global.requested_mode = UINT32_C(0);
  atomic_store_explicit(&mf_cuda_dispatch_global.selected_runtime, MF_CUDA_SELECTED_NONE_V1,
                        memory_order_release);
  mf_cuda_dispatch_global.mode_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  mf_cuda_dispatch_global.selector_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  mf_cuda_dispatch_global.mode_frozen = UINT32_C(0);
  mf_cuda_dispatch_global.force_dirty_rollback = UINT32_C(0);
  mf_cuda_dispatch_global.test_policy_installed = UINT32_C(0);
  (void)memset(&mf_cuda_dispatch_global.test_policy, 0,
               sizeof(mf_cuda_dispatch_global.test_policy));
  mf_cuda_dispatch_unlock();
  mf_cuda_passthrough_pair_release_v1(pair);
  mf_cuda_provider_test_reset_managed_v1();
}
#endif

#define MF_CUDA_DEFINE_REGULAR(name, parameters, arguments)                                        \
  CUresult name parameters {                                                                       \
    void* mf_vendor_address = (void*)0;                                                            \
    uint32_t mf_vendor_selected = UINT32_C(0);                                                     \
    CUresult mf_dispatch_result =                                                                  \
        mf_cuda_dispatch_resolve_v1(#name, &mf_vendor_address, &mf_vendor_selected);               \
    CUresult(*mf_vendor_function) parameters = (CUresult(*) parameters)0;                          \
    if (mf_dispatch_result != CUDA_SUCCESS) {                                                      \
      return mf_dispatch_result;                                                                   \
    }                                                                                              \
    if (mf_vendor_selected == UINT32_C(0)) {                                                       \
      return mf_cuda_managed_##name arguments;                                                     \
    }                                                                                              \
    _Static_assert(sizeof(mf_vendor_function) == sizeof(mf_vendor_address),                        \
                   "POSIX function and data pointers must match");                                 \
    (void)memcpy(&mf_vendor_function, &mf_vendor_address, sizeof(mf_vendor_function));             \
    return mf_vendor_function arguments;                                                           \
  }

#define MF_CUDA_DEFINE_INIT(name, parameters, arguments)                                           \
  CUresult name parameters { return mf_cuda_dispatch_init_v1 arguments; }

#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments)                        \
  MF_CUDA_DEFINE_##route(name, parameters, arguments)
#include "../symbols.def"
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL
#undef MF_CUDA_DEFINE_INIT
#undef MF_CUDA_DEFINE_REGULAR
