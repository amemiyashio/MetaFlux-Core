#define METAFLUX_NVML_ABI_INTERNAL 1
#include "metaflux/nvml/provider.h"

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

#define MF_NVML_INIT_FLAG_NO_GPUS UINT32_C(1)

typedef struct mf_nvml_dispatch_state {
  atomic_flag lock;
  mf_cuda_passthrough_pair_v1* pair;
  pid_t owner_pid;
  mf_cuda_runtime_mode_v1 requested_mode;
  mf_cuda_selected_runtime_v1 selected_runtime;
  mf_cuda_passthrough_status_v1 mode_status;
  mf_cuda_passthrough_status_v1 selector_status;
  uint32_t mode_frozen;
  uint32_t atfork_registered;
  uint32_t vendor_reference_count;
  uint32_t active_vendor_calls;
  uint32_t release_pending;
#if defined(METAFLUX_PROVIDER_TESTING)
  mf_cuda_passthrough_policy_v1 test_policy;
  uint32_t test_policy_installed;
  uint32_t force_dirty_rollback;
#endif
} mf_nvml_dispatch_state;

typedef struct mf_nvml_managed_transaction {
  nvmlReturn_t result;
  unsigned int flags;
  uint32_t prepared;
} mf_nvml_managed_transaction;

static mf_nvml_dispatch_state mf_nvml_dispatch_global = {.lock = ATOMIC_FLAG_INIT,
                                                         .owner_pid = (pid_t)-1};

#define MF_NVML_INTERNAL(name)
#define MF_NVML_SYMBOL(name, target, status, route, result, parameters, arguments)                 \
  extern result mf_nvml_managed_##name parameters;
#include "../symbols.def"
#undef MF_NVML_SYMBOL
#undef MF_NVML_INTERNAL

extern void mf_nvml_provider_managed_rollback_v1(void);
extern int32_t mf_nvml_provider_managed_is_pristine_v1(void);
#if defined(METAFLUX_PROVIDER_TESTING)
extern void mf_nvml_provider_test_reset_managed_v1(void);
#endif

static void mf_nvml_dispatch_lock(void) {
  while (atomic_flag_test_and_set_explicit(&mf_nvml_dispatch_global.lock, memory_order_acquire)) {
  }
}

static void mf_nvml_dispatch_unlock(void) {
  atomic_flag_clear_explicit(&mf_nvml_dispatch_global.lock, memory_order_release);
}

static void mf_nvml_dispatch_after_fork_child(void) {
  atomic_flag_clear_explicit(&mf_nvml_dispatch_global.lock, memory_order_release);
}

static nvmlReturn_t mf_nvml_selector_error(mf_cuda_passthrough_status_v1 status) {
  switch (status) {
  case MF_CUDA_PASSTHROUGH_SUCCESS:
    return NVML_SUCCESS;
  case MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT:
  case MF_CUDA_PASSTHROUGH_INVALID_MODE:
    return NVML_ERROR_INVALID_ARGUMENT;
  case MF_CUDA_PASSTHROUGH_STALE:
    return NVML_ERROR_GPU_IS_LOST;
  case MF_CUDA_PASSTHROUGH_NOT_FOUND:
  case MF_CUDA_PASSTHROUGH_LOAD_FAILED:
    return NVML_ERROR_DRIVER_NOT_LOADED;
  case MF_CUDA_PASSTHROUGH_POLICY_REJECTED:
  case MF_CUDA_PASSTHROUGH_MALFORMED_CONFIG:
  case MF_CUDA_PASSTHROUGH_MALFORMED_ELF:
  case MF_CUDA_PASSTHROUGH_BUILD_MISMATCH:
    return NVML_ERROR_LIBRARY_NOT_FOUND;
  case MF_CUDA_PASSTHROUGH_SYMBOL_MISSING:
    return NVML_ERROR_FUNCTION_NOT_FOUND;
  case MF_CUDA_PASSTHROUGH_PARTIAL_STATE:
    return NVML_ERROR_INVALID_STATE;
  case MF_CUDA_PASSTHROUGH_SYSTEM_ERROR:
  default:
    return NVML_ERROR_UNKNOWN;
  }
}

static mf_cuda_passthrough_status_v1 mf_nvml_managed_prepare(void* context, uint64_t* out_ticket) {
  mf_nvml_managed_transaction* transaction = (mf_nvml_managed_transaction*)context;
  if (transaction == (mf_nvml_managed_transaction*)0 || out_ticket == (uint64_t*)0) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  *out_ticket = UINT64_C(0);
  transaction->result = mf_nvml_managed_nvmlInitWithFlags(transaction->flags);
  if (transaction->result != NVML_SUCCESS) {
    return MF_CUDA_PASSTHROUGH_LOAD_FAILED;
  }
  transaction->prepared = UINT32_C(1);
  *out_ticket = UINT64_C(1);
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static mf_cuda_passthrough_status_v1 mf_nvml_managed_commit(void* context, uint64_t ticket) {
  const mf_nvml_managed_transaction* transaction = (const mf_nvml_managed_transaction*)context;
  return transaction != (const mf_nvml_managed_transaction*)0 &&
                 transaction->prepared != UINT32_C(0) && ticket == UINT64_C(1)
             ? MF_CUDA_PASSTHROUGH_SUCCESS
             : MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
}

static mf_cuda_passthrough_status_v1 mf_nvml_managed_rollback(void* context, uint64_t ticket) {
  mf_nvml_managed_transaction* transaction = (mf_nvml_managed_transaction*)context;
  if (transaction == (mf_nvml_managed_transaction*)0 ||
      (ticket != UINT64_C(0) && ticket != UINT64_C(1))) {
    return MF_CUDA_PASSTHROUGH_INVALID_ARGUMENT;
  }
  mf_nvml_provider_managed_rollback_v1();
  transaction->prepared = UINT32_C(0);
  return MF_CUDA_PASSTHROUGH_SUCCESS;
}

static int32_t mf_nvml_managed_is_pristine(void* context) {
  (void)context;
#if defined(METAFLUX_PROVIDER_TESTING)
  if (mf_nvml_dispatch_global.force_dirty_rollback != UINT32_C(0)) {
    return INT32_C(0);
  }
#endif
  return mf_nvml_provider_managed_is_pristine_v1();
}

static mf_cuda_passthrough_status_v1 mf_nvml_freeze_mode_locked(void) {
  const char* configured = (const char*)0;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  if (mf_nvml_dispatch_global.mode_frozen != UINT32_C(0)) {
    return mf_nvml_dispatch_global.mode_status;
  }
  configured = getenv("METAFLUX_MODE");
  if (configured == (const char*)0) {
    configured = "auto";
  }
  status = mf_cuda_runtime_mode_parse_v1(configured, &mf_nvml_dispatch_global.requested_mode);
  mf_nvml_dispatch_global.mode_frozen = UINT32_C(1);
  mf_nvml_dispatch_global.owner_pid = getpid();
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS &&
      mf_nvml_dispatch_global.atfork_registered == UINT32_C(0)) {
    if (pthread_atfork((void (*)(void))0, (void (*)(void))0, mf_nvml_dispatch_after_fork_child) !=
        0) {
      status = MF_CUDA_PASSTHROUGH_SYSTEM_ERROR;
    } else {
      mf_nvml_dispatch_global.atfork_registered = UINT32_C(1);
    }
  }
  mf_nvml_dispatch_global.mode_status = status;
  mf_nvml_dispatch_global.selector_status = status;
  return status;
}

static mf_cuda_passthrough_status_v1
mf_nvml_select_locked(mf_nvml_managed_transaction* transaction) {
  mf_cuda_managed_callbacks_v1 callbacks;
  mf_cuda_passthrough_pair_v1* pair = (mf_cuda_passthrough_pair_v1*)0;
  mf_cuda_selected_runtime_v1 selected = MF_CUDA_SELECTED_NONE_V1;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
  (void)memset(&callbacks, 0, sizeof(callbacks));
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.abi_version = MF_CUDA_PASSTHROUGH_ABI_VERSION_V1;
  callbacks.context = transaction;
  callbacks.prepare = mf_nvml_managed_prepare;
  callbacks.commit = mf_nvml_managed_commit;
  callbacks.rollback = mf_nvml_managed_rollback;
  callbacks.is_pristine = mf_nvml_managed_is_pristine;
#if defined(METAFLUX_PROVIDER_TESTING)
  if (mf_nvml_dispatch_global.test_policy_installed != UINT32_C(0)) {
    status = mf_cuda_runtime_select_with_policy_v1(mf_nvml_dispatch_global.requested_mode,
                                                   &callbacks, &mf_nvml_dispatch_global.test_policy,
                                                   &selected, &pair);
  } else {
    status = mf_cuda_runtime_select_v1(mf_nvml_dispatch_global.requested_mode, &callbacks,
                                       &selected, &pair);
  }
#else
  status = mf_cuda_runtime_select_v1(mf_nvml_dispatch_global.requested_mode, &callbacks, &selected,
                                     &pair);
#endif
  mf_nvml_dispatch_global.selector_status = status;
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    mf_nvml_dispatch_global.selected_runtime = selected;
    mf_nvml_dispatch_global.pair = pair;
  }
  return status;
}

static mf_cuda_passthrough_status_v1 mf_nvml_reload_pair_locked(void) {
  mf_cuda_passthrough_pair_v1* pair = (mf_cuda_passthrough_pair_v1*)0;
  mf_cuda_passthrough_status_v1 status = MF_CUDA_PASSTHROUGH_SUCCESS;
#if defined(METAFLUX_PROVIDER_TESTING)
  if (mf_nvml_dispatch_global.test_policy_installed != UINT32_C(0)) {
    status =
        mf_cuda_passthrough_pair_load_with_policy_v1(&mf_nvml_dispatch_global.test_policy, &pair);
  } else {
    status = mf_cuda_passthrough_pair_load_v1(&pair);
  }
#else
  status = mf_cuda_passthrough_pair_load_v1(&pair);
#endif
  mf_nvml_dispatch_global.selector_status = status;
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    mf_nvml_dispatch_global.pair = pair;
  }
  return status;
}

static nvmlReturn_t mf_nvml_vendor_acquire_locked(const char* symbol, void** out_symbol) {
  const mf_cuda_passthrough_status_v1 status = mf_cuda_passthrough_pair_lookup_v1(
      mf_nvml_dispatch_global.pair, MF_CUDA_VENDOR_LIBRARY_NVML_V1, symbol, "libnvidia-ml.so.1",
      out_symbol);
  if (status == MF_CUDA_PASSTHROUGH_SUCCESS) {
    mf_nvml_dispatch_global.active_vendor_calls += UINT32_C(1);
  }
  return mf_nvml_selector_error(status);
}

static void mf_nvml_release_pair_if_idle_locked(mf_cuda_passthrough_pair_v1** out_pair) {
  *out_pair = (mf_cuda_passthrough_pair_v1*)0;
  if (mf_nvml_dispatch_global.release_pending != UINT32_C(0) &&
      mf_nvml_dispatch_global.active_vendor_calls == UINT32_C(0)) {
    *out_pair = mf_nvml_dispatch_global.pair;
    mf_nvml_dispatch_global.pair = (mf_cuda_passthrough_pair_v1*)0;
    mf_nvml_dispatch_global.release_pending = UINT32_C(0);
  }
}

static void mf_nvml_vendor_call_complete(void) {
  mf_cuda_passthrough_pair_v1* release = (mf_cuda_passthrough_pair_v1*)0;
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.active_vendor_calls != UINT32_C(0)) {
    mf_nvml_dispatch_global.active_vendor_calls -= UINT32_C(1);
  }
  mf_nvml_release_pair_if_idle_locked(&release);
  mf_nvml_dispatch_unlock();
  mf_cuda_passthrough_pair_release_v1(release);
}

static nvmlReturn_t mf_nvml_dispatch_init_locked(const char* symbol, unsigned int flags,
                                                 void** out_symbol, uint32_t* out_managed) {
  mf_nvml_managed_transaction transaction = {
      .result = NVML_ERROR_UNKNOWN, .flags = flags, .prepared = UINT32_C(0)};
  mf_cuda_passthrough_status_v1 status = mf_nvml_freeze_mode_locked();
  *out_symbol = (void*)0;
  *out_managed = UINT32_C(0);
  if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
    return mf_nvml_selector_error(status);
  }
  if (mf_nvml_dispatch_global.owner_pid != getpid()) {
    return NVML_ERROR_GPU_IS_LOST;
  }
  if (mf_nvml_dispatch_global.selected_runtime == MF_CUDA_SELECTED_MANAGED_V1) {
    *out_managed = UINT32_C(1);
    return NVML_SUCCESS;
  }
  if (mf_nvml_dispatch_global.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1 &&
      mf_nvml_dispatch_global.pair == (mf_cuda_passthrough_pair_v1*)0) {
    status = mf_nvml_reload_pair_locked();
    if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
      return mf_nvml_selector_error(status);
    }
  }
  if (mf_nvml_dispatch_global.selected_runtime == MF_CUDA_SELECTED_NONE_V1) {
    status = mf_nvml_select_locked(&transaction);
    if (status != MF_CUDA_PASSTHROUGH_SUCCESS) {
      return mf_nvml_dispatch_global.requested_mode == MF_CUDA_RUNTIME_MODE_MANAGED_V1 &&
                     transaction.result != NVML_ERROR_UNKNOWN
                 ? transaction.result
                 : mf_nvml_selector_error(status);
    }
    if (mf_nvml_dispatch_global.selected_runtime == MF_CUDA_SELECTED_MANAGED_V1) {
      return transaction.result;
    }
  }
  if (mf_nvml_dispatch_global.vendor_reference_count == UINT32_MAX) {
    return NVML_ERROR_UNKNOWN;
  }
  return mf_nvml_vendor_acquire_locked(symbol, out_symbol);
}

static nvmlReturn_t mf_nvml_dispatch_init_zero_v1(const char* symbol) {
  void* address = (void*)0;
  uint32_t managed = UINT32_C(0);
  nvmlReturn_t result = NVML_SUCCESS;
  nvmlReturn_t (*vendor_init)(void) = (nvmlReturn_t (*)(void))0;
  mf_nvml_dispatch_lock();
  result = mf_nvml_dispatch_init_locked(symbol, UINT32_C(0), &address, &managed);
  mf_nvml_dispatch_unlock();
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (managed != UINT32_C(0)) {
    return mf_nvml_managed_nvmlInitWithFlags(UINT32_C(0));
  }
  if (address == (void*)0) {
    return NVML_SUCCESS;
  }
  _Static_assert(sizeof(vendor_init) == sizeof(address),
                 "POSIX function and data pointers must match");
  (void)memcpy(&vendor_init, &address, sizeof(vendor_init));
  result = vendor_init();
  mf_nvml_dispatch_lock();
  if (result == NVML_SUCCESS) {
    mf_nvml_dispatch_global.vendor_reference_count += UINT32_C(1);
  }
  mf_nvml_dispatch_unlock();
  mf_nvml_vendor_call_complete();
  return result;
}

static nvmlReturn_t mf_nvml_dispatch_init_flags_v1(const char* symbol, unsigned int flags) {
  void* address = (void*)0;
  uint32_t managed = UINT32_C(0);
  nvmlReturn_t result = NVML_SUCCESS;
  nvmlReturn_t (*vendor_init)(unsigned int) = (nvmlReturn_t (*)(unsigned int))0;
  if ((flags & ~(unsigned int)MF_NVML_INIT_FLAG_NO_GPUS) != UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  mf_nvml_dispatch_lock();
  result = mf_nvml_dispatch_init_locked(symbol, flags, &address, &managed);
  mf_nvml_dispatch_unlock();
  if (result != NVML_SUCCESS) {
    return result;
  }
  if (managed != UINT32_C(0)) {
    return mf_nvml_managed_nvmlInitWithFlags(flags);
  }
  if (address == (void*)0) {
    return NVML_SUCCESS;
  }
  _Static_assert(sizeof(vendor_init) == sizeof(address),
                 "POSIX function and data pointers must match");
  (void)memcpy(&vendor_init, &address, sizeof(vendor_init));
  result = vendor_init(flags);
  mf_nvml_dispatch_lock();
  if (result == NVML_SUCCESS) {
    mf_nvml_dispatch_global.vendor_reference_count += UINT32_C(1);
  }
  mf_nvml_dispatch_unlock();
  mf_nvml_vendor_call_complete();
  return result;
}

static nvmlReturn_t mf_nvml_dispatch_resolve_v1(const char* symbol, void** out_symbol,
                                                uint32_t* out_vendor) {
  nvmlReturn_t result = NVML_SUCCESS;
  *out_symbol = (void*)0;
  *out_vendor = UINT32_C(0);
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.mode_frozen != UINT32_C(0) &&
      mf_nvml_dispatch_global.owner_pid != getpid()) {
    result = NVML_ERROR_GPU_IS_LOST;
  } else if (mf_nvml_dispatch_global.selected_runtime == MF_CUDA_SELECTED_PASSTHROUGH_V1) {
    if (mf_nvml_dispatch_global.pair == (mf_cuda_passthrough_pair_v1*)0) {
      result = NVML_ERROR_UNINITIALIZED;
    } else {
      result = mf_nvml_vendor_acquire_locked(symbol, out_symbol);
      if (result == NVML_SUCCESS) {
        *out_vendor = UINT32_C(1);
      }
    }
  }
  mf_nvml_dispatch_unlock();
  return result;
}

static nvmlReturn_t mf_nvml_dispatch_shutdown_v1(void) {
  void* address = (void*)0;
  nvmlReturn_t result = NVML_SUCCESS;
  nvmlReturn_t (*vendor_shutdown)(void) = (nvmlReturn_t (*)(void))0;
  mf_cuda_passthrough_pair_v1* release = (mf_cuda_passthrough_pair_v1*)0;
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.mode_frozen != UINT32_C(0) &&
      mf_nvml_dispatch_global.owner_pid != getpid()) {
    mf_nvml_dispatch_unlock();
    return NVML_ERROR_GPU_IS_LOST;
  }
  if (mf_nvml_dispatch_global.selected_runtime != MF_CUDA_SELECTED_PASSTHROUGH_V1) {
    mf_nvml_dispatch_unlock();
    return mf_nvml_managed_nvmlShutdown();
  }
  if (mf_nvml_dispatch_global.pair == (mf_cuda_passthrough_pair_v1*)0 ||
      mf_nvml_dispatch_global.vendor_reference_count == UINT32_C(0)) {
    mf_nvml_dispatch_unlock();
    return NVML_ERROR_UNINITIALIZED;
  }
  result = mf_nvml_vendor_acquire_locked("nvmlShutdown", &address);
  mf_nvml_dispatch_unlock();
  if (result != NVML_SUCCESS) {
    return result;
  }
  _Static_assert(sizeof(vendor_shutdown) == sizeof(address),
                 "POSIX function and data pointers must match");
  (void)memcpy(&vendor_shutdown, &address, sizeof(vendor_shutdown));
  result = vendor_shutdown();
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.active_vendor_calls != UINT32_C(0)) {
    mf_nvml_dispatch_global.active_vendor_calls -= UINT32_C(1);
  }
  if (result == NVML_SUCCESS && mf_nvml_dispatch_global.vendor_reference_count != UINT32_C(0)) {
    mf_nvml_dispatch_global.vendor_reference_count -= UINT32_C(1);
    if (mf_nvml_dispatch_global.vendor_reference_count == UINT32_C(0)) {
      mf_nvml_dispatch_global.release_pending = UINT32_C(1);
    }
  }
  mf_nvml_release_pair_if_idle_locked(&release);
  mf_nvml_dispatch_unlock();
  mf_cuda_passthrough_pair_release_v1(release);
  return result;
}

#if defined(METAFLUX_PROVIDER_TESTING)
int mf_nvml_provider_test_install_passthrough_policy_v1(
    const mf_cuda_passthrough_policy_v1* policy) {
  if (policy == (const mf_cuda_passthrough_policy_v1*)0) {
    return -1;
  }
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.mode_frozen != UINT32_C(0)) {
    mf_nvml_dispatch_unlock();
    return -1;
  }
  mf_nvml_dispatch_global.test_policy = *policy;
  mf_nvml_dispatch_global.test_policy_installed = UINT32_C(1);
  mf_nvml_dispatch_unlock();
  return 0;
}

void mf_nvml_provider_test_force_dirty_rollback_v1(uint32_t enabled) {
  mf_nvml_dispatch_lock();
  mf_nvml_dispatch_global.force_dirty_rollback = enabled == UINT32_C(0) ? UINT32_C(0) : UINT32_C(1);
  mf_nvml_dispatch_unlock();
}

static void mf_nvml_snapshot_string(char* output, size_t capacity, const char* input) {
  if (capacity == (size_t)0) {
    return;
  }
  if (input == (const char*)0) {
    output[0] = '\0';
    return;
  }
  (void)snprintf(output, capacity, "%s", input);
}

int mf_nvml_provider_test_get_mode_snapshot_v1(mf_nvml_provider_test_mode_snapshot_v1* snapshot) {
  if (snapshot == (mf_nvml_provider_test_mode_snapshot_v1*)0) {
    return -1;
  }
  (void)memset(snapshot, 0, sizeof(*snapshot));
  snapshot->namespace_id = INT64_C(-1);
  mf_nvml_dispatch_lock();
  snapshot->mode_frozen = mf_nvml_dispatch_global.mode_frozen;
  snapshot->requested_mode = mf_nvml_dispatch_global.requested_mode;
  snapshot->selected_runtime = mf_nvml_dispatch_global.selected_runtime;
  snapshot->selector_status = mf_nvml_dispatch_global.selector_status;
  if (mf_nvml_dispatch_global.pair != (mf_cuda_passthrough_pair_v1*)0) {
    snapshot->namespace_id = mf_cuda_passthrough_pair_namespace_id_v1(mf_nvml_dispatch_global.pair);
    mf_nvml_snapshot_string(snapshot->driver_build, sizeof(snapshot->driver_build),
                            mf_cuda_passthrough_pair_driver_build_v1(mf_nvml_dispatch_global.pair));
    mf_nvml_snapshot_string(snapshot->cuda_path, sizeof(snapshot->cuda_path),
                            mf_cuda_passthrough_pair_cuda_path_v1(mf_nvml_dispatch_global.pair));
    mf_nvml_snapshot_string(snapshot->nvml_path, sizeof(snapshot->nvml_path),
                            mf_cuda_passthrough_pair_nvml_path_v1(mf_nvml_dispatch_global.pair));
  }
  mf_nvml_dispatch_unlock();
  return 0;
}

uint64_t mf_nvml_provider_test_vendor_call_count_v1(const char* symbol) {
  void* address = (void*)0;
  uint64_t (*call_count)(const char*) = (uint64_t (*)(const char*))0;
  uint64_t result = UINT64_MAX;
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.pair != (mf_cuda_passthrough_pair_v1*)0 &&
      mf_cuda_passthrough_pair_lookup_v1(
          mf_nvml_dispatch_global.pair, MF_CUDA_VENDOR_LIBRARY_NVML_V1,
          "mf_fixture_nvml_call_count", (const char*)0, &address) == MF_CUDA_PASSTHROUGH_SUCCESS) {
    _Static_assert(sizeof(call_count) == sizeof(address),
                   "POSIX function and data pointers must match");
    (void)memcpy(&call_count, &address, sizeof(call_count));
  }
  mf_nvml_dispatch_unlock();
  if (call_count != (uint64_t (*)(const char*))0) {
    result = call_count(symbol);
  }
  return result;
}

uint32_t mf_nvml_provider_test_validate_vendor_surface_v1(void) {
  static const char* const symbols[] = {
#define MF_NVML_INTERNAL(name)
#define MF_NVML_SYMBOL(name, target, status, route, result, parameters, arguments) #name,
#include "../symbols.def"
#undef MF_NVML_SYMBOL
#undef MF_NVML_INTERNAL
  };
  size_t index = 0;
  uint32_t count = UINT32_C(0);
  mf_nvml_dispatch_lock();
  if (mf_nvml_dispatch_global.pair != (mf_cuda_passthrough_pair_v1*)0) {
    for (index = 0; index < sizeof(symbols) / sizeof(symbols[0]); ++index) {
      void* address = (void*)0;
      if (mf_cuda_passthrough_pair_lookup_v1(
              mf_nvml_dispatch_global.pair, MF_CUDA_VENDOR_LIBRARY_NVML_V1, symbols[index],
              "libnvidia-ml.so.1", &address) != MF_CUDA_PASSTHROUGH_SUCCESS) {
        count = UINT32_C(0);
        break;
      }
      count += UINT32_C(1);
    }
  }
  mf_nvml_dispatch_unlock();
  return count;
}

void mf_nvml_provider_test_reset_v1(void) {
  mf_cuda_passthrough_pair_v1* pair = (mf_cuda_passthrough_pair_v1*)0;
  mf_nvml_dispatch_lock();
  pair = mf_nvml_dispatch_global.pair;
  mf_nvml_dispatch_global.pair = (mf_cuda_passthrough_pair_v1*)0;
  mf_nvml_dispatch_global.owner_pid = (pid_t)-1;
  mf_nvml_dispatch_global.requested_mode = UINT32_C(0);
  mf_nvml_dispatch_global.selected_runtime = MF_CUDA_SELECTED_NONE_V1;
  mf_nvml_dispatch_global.mode_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  mf_nvml_dispatch_global.selector_status = MF_CUDA_PASSTHROUGH_SUCCESS;
  mf_nvml_dispatch_global.mode_frozen = UINT32_C(0);
  mf_nvml_dispatch_global.vendor_reference_count = UINT32_C(0);
  mf_nvml_dispatch_global.active_vendor_calls = UINT32_C(0);
  mf_nvml_dispatch_global.release_pending = UINT32_C(0);
  mf_nvml_dispatch_global.force_dirty_rollback = UINT32_C(0);
  mf_nvml_dispatch_global.test_policy_installed = UINT32_C(0);
  (void)memset(&mf_nvml_dispatch_global.test_policy, 0,
               sizeof(mf_nvml_dispatch_global.test_policy));
  mf_nvml_dispatch_unlock();
  mf_cuda_passthrough_pair_release_v1(pair);
  mf_nvml_provider_test_reset_managed_v1();
}
#endif

#define MF_NVML_DEFINE_REGULAR(name, result_type, parameters, arguments)                           \
  result_type name parameters {                                                                    \
    void* symbol = (void*)0;                                                                       \
    uint32_t vendor = UINT32_C(0);                                                                 \
    nvmlReturn_t dispatch = mf_nvml_dispatch_resolve_v1(#name, &symbol, &vendor);                  \
    result_type(*vendor_function) parameters = (result_type(*) parameters)0;                       \
    result_type result;                                                                            \
    if (dispatch != NVML_SUCCESS) {                                                                \
      return (result_type)dispatch;                                                                \
    }                                                                                              \
    if (vendor == UINT32_C(0)) {                                                                   \
      return mf_nvml_managed_##name arguments;                                                     \
    }                                                                                              \
    _Static_assert(sizeof(vendor_function) == sizeof(symbol),                                      \
                   "POSIX function and data pointers must match");                                 \
    (void)memcpy(&vendor_function, &symbol, sizeof(vendor_function));                              \
    result = vendor_function arguments;                                                            \
    mf_nvml_vendor_call_complete();                                                                \
    return result;                                                                                 \
  }

#define MF_NVML_DEFINE_INIT_ZERO(name, result_type, parameters, arguments)                         \
  result_type name parameters { return mf_nvml_dispatch_init_zero_v1(#name); }

#define MF_NVML_DEFINE_INIT_FLAGS(name, result_type, parameters, arguments)                        \
  result_type name parameters { return mf_nvml_dispatch_init_flags_v1(#name, flags); }

#define MF_NVML_DEFINE_SHUTDOWN(name, result_type, parameters, arguments)                          \
  result_type name parameters { return mf_nvml_dispatch_shutdown_v1(); }

#define MF_NVML_DEFINE_ERROR_STRING(name, result_type, parameters, arguments)                      \
  result_type name parameters {                                                                    \
    void* symbol = (void*)0;                                                                       \
    uint32_t vendor = UINT32_C(0);                                                                 \
    const char*(*vendor_function)parameters = (const char*(*)parameters)0;                         \
    const char* text = (const char*)0;                                                             \
    nvmlReturn_t dispatch = mf_nvml_dispatch_resolve_v1(#name, &symbol, &vendor);                  \
    if (dispatch != NVML_SUCCESS || vendor == UINT32_C(0)) {                                       \
      return mf_nvml_managed_##name(dispatch == NVML_SUCCESS ? result : dispatch);                 \
    }                                                                                              \
    _Static_assert(sizeof(vendor_function) == sizeof(symbol),                                      \
                   "POSIX function and data pointers must match");                                 \
    (void)memcpy(&vendor_function, &symbol, sizeof(vendor_function));                              \
    text = vendor_function arguments;                                                              \
    mf_nvml_vendor_call_complete();                                                                \
    return text;                                                                                   \
  }

#define MF_NVML_INTERNAL(name)
#define MF_NVML_SYMBOL(name, target, status, route, result, parameters, arguments)                 \
  MF_NVML_DEFINE_##route(name, result, parameters, arguments)
#include "../symbols.def"
#undef MF_NVML_SYMBOL
#undef MF_NVML_INTERNAL
#undef MF_NVML_DEFINE_ERROR_STRING
#undef MF_NVML_DEFINE_SHUTDOWN
#undef MF_NVML_DEFINE_INIT_FLAGS
#undef MF_NVML_DEFINE_INIT_ZERO
#undef MF_NVML_DEFINE_REGULAR
