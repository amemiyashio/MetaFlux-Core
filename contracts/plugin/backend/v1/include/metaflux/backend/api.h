#ifndef METAFLUX_BACKEND_API_H
#define METAFLUX_BACKEND_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_BACKEND_ABI_VERSION_1 UINT32_C(1)

#define MF_BACKEND_SUCCESS INT32_C(0)
#define MF_BACKEND_INVALID_ARGUMENT INT32_C(1)
#define MF_BACKEND_UNSUPPORTED INT32_C(2)
#define MF_BACKEND_OUT_OF_MEMORY INT32_C(3)
#define MF_BACKEND_DEVICE_LOST INT32_C(4)
#define MF_BACKEND_TIMEOUT INT32_C(5)
#define MF_BACKEND_BUSY INT32_C(6)
#define MF_BACKEND_COMPILATION_FAILED INT32_C(7)
#define MF_BACKEND_INTERNAL_ERROR INT32_C(8)

#define MF_BACKEND_CAP_COMPILE (UINT64_C(1) << 0U)
#define MF_BACKEND_CAP_COPY (UINT64_C(1) << 1U)
#define MF_BACKEND_CAP_EVENTS (UINT64_C(1) << 2U)
#define MF_BACKEND_CAP_CANCELLATION (UINT64_C(1) << 3U)
#define MF_BACKEND_CAP_METRICS (UINT64_C(1) << 4U)
#define MF_BACKEND_CAP_POLICY (UINT64_C(1) << 5U)

typedef int32_t mf_backend_status_v1;
typedef uint64_t mf_backend_instance_v1;
typedef uint64_t mf_backend_context_v1;
typedef uint64_t mf_backend_queue_v1;
typedef uint64_t mf_backend_memory_v1;
typedef uint64_t mf_backend_module_v1;
typedef uint64_t mf_backend_event_v1;

typedef struct mf_backend_extension_header_v1 {
  uint32_t extension_id;
  uint32_t struct_size;
  const void* next;
} mf_backend_extension_header_v1;

typedef struct mf_backend_api_header_v1 {
  uint32_t abi_version;
  uint32_t struct_size;
  uint64_t capabilities;
  const void* extensions;
} mf_backend_api_header_v1;

typedef struct mf_backend_host_api_v1 {
  uint32_t abi_version;
  uint32_t struct_size;
  void* host_context;
  void (*log)(void* host_context, uint32_t level, const char* message, uint64_t message_size);
  const void* extensions;
} mf_backend_host_api_v1;

typedef struct mf_backend_device_info_v1 {
  uint32_t struct_size;
  uint32_t backend_device_index;
  uint64_t capability_bits;
  uint64_t memory_capacity_bytes;
  uint32_t virtual_compute_capability;
  uint32_t numa_node;
  uint8_t backend_uuid[16];
  uint8_t display_name[64];
  uint64_t reserved[4];
} mf_backend_device_info_v1;

typedef struct mf_backend_compile_request_v1 {
  uint32_t struct_size;
  uint32_t input_kind;
  const uint8_t* input_bytes;
  uint64_t input_size;
  const uint8_t* options_bytes;
  uint64_t options_size;
  uint64_t capability_profile;
  const void* extensions;
} mf_backend_compile_request_v1;

typedef struct mf_backend_launch_v1 {
  uint32_t struct_size;
  uint32_t flags;
  mf_backend_module_v1 module;
  uint64_t kernel_id;
  const uint8_t* argument_bytes;
  uint64_t argument_size;
  uint32_t grid[3];
  uint32_t block[3];
  uint32_t dynamic_shared_bytes;
  uint32_t reserved_word;
} mf_backend_launch_v1;

typedef struct mf_backend_copy_v1 {
  uint32_t struct_size;
  uint32_t flags;
  mf_backend_memory_v1 destination;
  uint64_t destination_offset;
  mf_backend_memory_v1 source;
  uint64_t source_offset;
  uint64_t byte_count;
  uint64_t reserved[2];
} mf_backend_copy_v1;

typedef struct mf_backend_metrics_v1 {
  uint32_t struct_size;
  uint32_t state;
  uint64_t committed_work_items;
  uint64_t completed_work_items;
  uint64_t active_time_ns;
  uint64_t memory_used_bytes;
  uint64_t sample_time_ns;
  uint64_t reserved[3];
} mf_backend_metrics_v1;

typedef struct mf_backend_policy_v1 {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t memory_quota_bytes;
  uint64_t execution_quota_ns;
  uint64_t policy_bits;
  uint64_t reserved[3];
} mf_backend_policy_v1;

typedef struct mf_backend_api_v1 {
  mf_backend_api_header_v1 header;
  mf_backend_status_v1 (*create_instance)(const mf_backend_host_api_v1* host_api,
                                          mf_backend_instance_v1* out_instance);
  void (*destroy_instance)(mf_backend_instance_v1 instance);
  mf_backend_status_v1 (*enumerate_devices)(mf_backend_instance_v1 instance, uint32_t* inout_count,
                                            mf_backend_device_info_v1* devices);
  mf_backend_status_v1 (*compile)(mf_backend_instance_v1 instance,
                                  const mf_backend_compile_request_v1* request,
                                  uint8_t* artifact_bytes, uint64_t* inout_artifact_size);
  mf_backend_status_v1 (*load_module)(mf_backend_instance_v1 instance, uint32_t device_index,
                                      const uint8_t* artifact_bytes, uint64_t artifact_size,
                                      mf_backend_module_v1* out_module);
  void (*unload_module)(mf_backend_instance_v1 instance, mf_backend_module_v1 module);
  mf_backend_status_v1 (*create_context)(mf_backend_instance_v1 instance, uint32_t device_index,
                                         mf_backend_context_v1* out_context);
  void (*destroy_context)(mf_backend_instance_v1 instance, mf_backend_context_v1 context);
  mf_backend_status_v1 (*create_queue)(mf_backend_instance_v1 instance,
                                       mf_backend_context_v1 context,
                                       mf_backend_queue_v1* out_queue);
  void (*destroy_queue)(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue);
  mf_backend_status_v1 (*allocate_memory)(mf_backend_instance_v1 instance,
                                          mf_backend_context_v1 context, uint64_t byte_count,
                                          uint64_t alignment, mf_backend_memory_v1* out_memory);
  void (*free_memory)(mf_backend_instance_v1 instance, mf_backend_memory_v1 memory);
  mf_backend_status_v1 (*submit)(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                                 const mf_backend_launch_v1* launch,
                                 mf_backend_event_v1 completion_event);
  mf_backend_status_v1 (*copy)(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                               const mf_backend_copy_v1* copy,
                               mf_backend_event_v1 completion_event);
  mf_backend_status_v1 (*create_event)(mf_backend_instance_v1 instance,
                                       mf_backend_context_v1 context,
                                       mf_backend_event_v1* out_event);
  void (*destroy_event)(mf_backend_instance_v1 instance, mf_backend_event_v1 event);
  mf_backend_status_v1 (*query_event)(mf_backend_instance_v1 instance, mf_backend_event_v1 event,
                                      uint32_t* out_complete);
  mf_backend_status_v1 (*wait_event)(mf_backend_instance_v1 instance, mf_backend_event_v1 event,
                                     uint64_t timeout_ns);
  mf_backend_status_v1 (*synchronize_queue)(mf_backend_instance_v1 instance,
                                            mf_backend_queue_v1 queue, uint64_t timeout_ns);
  mf_backend_status_v1 (*cancel_queue)(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue);
  mf_backend_status_v1 (*read_metrics)(mf_backend_instance_v1 instance, uint32_t device_index,
                                       mf_backend_metrics_v1* metrics);
  mf_backend_status_v1 (*set_policy)(mf_backend_instance_v1 instance, uint32_t device_index,
                                     const mf_backend_policy_v1* policy);
} mf_backend_api_v1;

typedef const mf_backend_api_v1* (*mf_backend_get_api_v1_fn)(void);

static inline mf_backend_status_v1 mf_backend_api_validate_v1(const mf_backend_api_v1* api,
                                                              uint32_t required_size,
                                                              uint64_t required_capabilities) {
  if (api == (const mf_backend_api_v1*)0 || api->header.abi_version != MF_BACKEND_ABI_VERSION_1 ||
      api->header.struct_size < required_size ||
      (api->header.capabilities & required_capabilities) != required_capabilities) {
    return MF_BACKEND_UNSUPPORTED;
  }
  return MF_BACKEND_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif
