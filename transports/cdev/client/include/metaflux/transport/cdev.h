#ifndef METAFLUX_TRANSPORT_CDEV_H
#define METAFLUX_TRANSPORT_CDEV_H

#include <stdint.h>

#include "metaflux/client/fastpath.h"
#include "metaflux/uapi/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF_CDEV_DEFAULT_PATH_V0 "/dev/metaflux0"
#define MF_CDEV_DEFAULT_CONTROL_PATH_V0 "/dev/metafluxctl"
#define MF_CDEV_RING_CAPACITY_V0 UINT32_C(256)
#define MF_CDEV_PAYLOAD_OFFSET_V0 UINT64_C(16384)
#define MF_CDEV_PAYLOAD_MMAP_OFFSET_V0 UINT64_C(8192)
#define MF_CDEV_PAYLOAD_MAX_SIZE_V0 UINT64_C(67108864)
#define MF_CDEV_MEMORY_KIND_ALLOC_V0 UINT32_C(0)
#define MF_CDEV_MEMORY_KIND_REGISTERED_V0 UINT32_C(1)
#define MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0 MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0
#define MF_CDEV_MEMORY_REGISTER_FLAG_WRITE_V0 MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0
#define MF_CDEV_MEMORY_REGISTER_KNOWN_FLAGS_V0 MF_UAPI_MEMORY_REGISTER_KNOWN_FLAGS_V0

typedef struct mf_cdev_session_v0 {
  void* mapping;
  uint64_t mapping_size;
  mf_client_ring_v1 submission;
  mf_client_ring_v1 completion;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t device_generation;
  uint64_t queue_id;
  int32_t device_fd;
  uint32_t negotiated_features;
  uint32_t reserved;
  int32_t submission_eventfd;
  int32_t completion_eventfd;
} mf_cdev_session_v0;

typedef struct mf_cdev_memory_v0 {
  void* mapping;
  uint64_t mapping_size;
  uint64_t byte_count;
  uint64_t handle;
  uint64_t generation;
  int32_t device_fd;
  /* Client metadata; reserved is retained as a source-compatible alias. */
  union {
    uint32_t kind;
    uint32_t reserved;
  };
} mf_cdev_memory_v0;

typedef struct mf_cdev_copy_v0 {
  uint64_t destination_offset;
  uint64_t source_offset;
  uint64_t byte_count;
} mf_cdev_copy_v0;

/* Open, negotiate, and map one generation-bound local cdev queue. */
mf_shared_status_v1 mf_cdev_session_open_v0(const char* device_path,
                                             mf_cdev_session_v0* out_session);

/* Open a queue and attach caller-owned eventfds to its generation. */
mf_shared_status_v1 mf_cdev_session_open_with_eventfds_v0(const char* device_path,
                                                          int32_t submission_eventfd,
                                                          int32_t completion_eventfd,
                                                          mf_cdev_session_v0* out_session);

mf_shared_status_v1 mf_cdev_session_open_default_v0(mf_cdev_session_v0* out_session);

void mf_cdev_session_close_v0(mf_cdev_session_v0* session);

/* Allocate and map the generation-bound driver payload arena. */
mf_shared_status_v1 mf_cdev_memory_alloc_v0(mf_cdev_session_v0* session,
                                            uint64_t byte_count,
                                            uint64_t alignment,
                                            mf_cdev_memory_v0* out_memory);

/* Pin a caller-owned range for long-term device access and retain its registration handle. */
mf_shared_status_v1 mf_cdev_memory_register_v0(mf_cdev_session_v0* session,
                                               void* address,
                                               uint64_t byte_count,
                                               uint32_t flags,
                                               mf_cdev_memory_v0* out_memory);

void mf_cdev_memory_close_v0(mf_cdev_memory_v0* memory);

/* Encode a copy descriptor without submitting it. Offsets address the mapped payload arena. */
mf_shared_status_v1 mf_cdev_copy_descriptor_v0(uint64_t request_id,
                                               uint64_t generation,
                                               const mf_cdev_copy_v0* copy,
                                               mf_ring_descriptor_v1* out_descriptor);

mf_shared_status_v1 mf_cdev_submit_copy_v0(mf_cdev_session_v0* session, uint64_t request_id,
                                           const mf_cdev_copy_v0* copy);

mf_shared_status_v1 mf_cdev_wait_v0(mf_cdev_session_v0* session, uint64_t timeline,
                                    uint64_t timeout_ns);

mf_shared_status_v1 mf_cdev_try_consume_completion_v0(mf_cdev_session_v0* session,
                                                      mf_ring_descriptor_v1* out_descriptor);

int32_t mf_cdev_borrow_fd_v0(const mf_cdev_session_v0* session);

#ifdef __cplusplus
}
#endif

#endif
