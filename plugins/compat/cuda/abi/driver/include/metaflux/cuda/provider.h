#ifndef METAFLUX_CUDA_PROVIDER_H
#define METAFLUX_CUDA_PROVIDER_H

#include <stdint.h>

#include "metaflux/cuda/abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_CUDA_PROVIDER_API __attribute__((visibility("default")))
#else
#define MF_CUDA_PROVIDER_API
#endif

MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_bootstrap_abi_version(void);

/* Pending-arange read-through bridge (see provider_stubs.c). */
void mf_arange_pending_register(unsigned long long pointer, unsigned int count,
                                unsigned int kind, const void* functor);
void mf_arange_pending_invalidate(unsigned long long pointer);
int mf_arange_pending_read(unsigned long long source, void* host, size_t bytes);


#if defined(METAFLUX_PROVIDER_TESTING)
#include "metaflux/client/fastpath.h"
#include "metaflux/client/protocol.h"

typedef struct mf_cuda_passthrough_policy_v1 mf_cuda_passthrough_policy_v1;

typedef mf_shared_status_v1 (*mf_cuda_provider_test_control_fn_v1)(
    void* context, const mf_client_control_request_v1* request, const uint8_t* payload,
    uint64_t payload_size, mf_client_control_response_v1* response);
typedef mf_shared_status_v1 (*mf_cuda_provider_test_read_object_fn_v1)(
    const void* context, uint64_t object_id, uint64_t object_generation, uint64_t offset,
    uint8_t* bytes, uint64_t byte_count);

typedef struct mf_cuda_provider_test_transport_v1 {
  int32_t registry_fd;
  int32_t submission_fd;
  int32_t completion_fd;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t submission_queue_id;
  uint64_t submission_queue_generation;
  uint64_t completion_queue_id;
  uint64_t completion_queue_generation;
  uint64_t runtime_context_id;
  uint64_t runtime_event_id;
  uint64_t runtime_event_generation;
  uint64_t runtime_add_kernel_id;
  uint64_t negotiated_capabilities;
  void* control_context;
  mf_cuda_provider_test_control_fn_v1 control;
  mf_cuda_provider_test_read_object_fn_v1 read_object;
} mf_cuda_provider_test_transport_v1;

typedef struct mf_cuda_provider_test_mode_snapshot_v1 {
  uint32_t mode_frozen;
  uint32_t requested_mode;
  uint32_t selected_runtime;
  uint32_t selector_status;
  int64_t namespace_id;
  char driver_build[64];
  char cuda_path[4096];
  char nvml_path[4096];
} mf_cuda_provider_test_mode_snapshot_v1;

typedef struct mf_cuda_provider_test_path_counters_v1 {
  uint64_t dispatch_lock_acquisitions;
  uint64_t global_lock_acquisitions;
  uint64_t queue_gate_acquisitions;
  uint64_t heap_allocation_attempts;
} mf_cuda_provider_test_path_counters_v1;

#define MF_CUDA_PROVIDER_TEST_COPY_HOST_TO_DEVICE UINT32_C(1)
#define MF_CUDA_PROVIDER_TEST_COPY_DEVICE_TO_HOST UINT32_C(2)
#define MF_CUDA_PROVIDER_TEST_COPY_DEVICE_TO_DEVICE UINT32_C(3)
#define MF_CUDA_PROVIDER_TEST_MATERIALIZED_POINTER UINT32_C(0x80000000)

MF_CUDA_PROVIDER_API int
mf_cuda_provider_test_install_transport_v1(const mf_cuda_provider_test_transport_v1* transport);
MF_CUDA_PROVIDER_API int mf_cuda_provider_test_set_direct_registration_v1(
    uint32_t local_unavailable, mf_shared_status_v1 transport_status, uint32_t control_status);
MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_test_managed_is_pristine_v1(void);
MF_CUDA_PROVIDER_API uint64_t mf_cuda_provider_test_process_view_revision_v1(void);
MF_CUDA_PROVIDER_API int
mf_cuda_provider_test_install_passthrough_policy_v1(const mf_cuda_passthrough_policy_v1* policy);
MF_CUDA_PROVIDER_API void mf_cuda_provider_test_force_dirty_rollback_v1(uint32_t enabled);
MF_CUDA_PROVIDER_API int
mf_cuda_provider_test_get_mode_snapshot_v1(mf_cuda_provider_test_mode_snapshot_v1* snapshot);
MF_CUDA_PROVIDER_API uint64_t mf_cuda_provider_test_vendor_call_count_v1(const char* symbol);
MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_test_validate_vendor_surface_v1(void);
MF_CUDA_PROVIDER_API int mf_cuda_provider_test_set_next_request_v1(uint64_t next_request);
MF_CUDA_PROVIDER_API int mf_cuda_provider_test_set_primary_refcount_v1(CUdevice device,
                                                                       uint32_t refcount);
MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_test_pending_count_v1(void);
MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_test_pending_capacity_v1(void);
MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_test_async_error_count_v1(void);
MF_CUDA_PROVIDER_API uint32_t mf_cuda_provider_test_active_waiter_count_v1(void);
MF_CUDA_PROVIDER_API void
mf_cuda_provider_test_get_path_counters_v1(mf_cuda_provider_test_path_counters_v1* counters);
MF_CUDA_PROVIDER_API void mf_cuda_provider_test_reset_v1(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
