#ifndef METAFLUX_RUNTIME_HOST_FIXTURE_H
#define METAFLUX_RUNTIME_HOST_FIXTURE_H

#include <stdint.h>

#include "metaflux/client/protocol.h"
#include "metaflux/shared/device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF_HOST_FIXTURE_SUBMISSION_QUEUE_ID UINT64_C(1)
#define MF_HOST_FIXTURE_COMPLETION_QUEUE_ID UINT64_C(2)
#define MF_HOST_FIXTURE_QUEUE_GENERATION UINT64_C(1)
#define MF_HOST_FIXTURE_CONTEXT_ID UINT64_C(1)
#define MF_HOST_FIXTURE_EVENT_ID UINT64_C(1)
#define MF_HOST_FIXTURE_EVENT_GENERATION UINT64_C(1)
#define MF_HOST_FIXTURE_KERNEL_ADD_I32 UINT64_C(1)

typedef struct mf_host_fixture_add_arguments_v1 {
  uint64_t destination_id;
  uint64_t destination_generation;
  uint64_t left_id;
  uint64_t left_generation;
  uint64_t right_id;
  uint64_t right_generation;
  uint64_t element_count;
  uint64_t reserved;
} mf_host_fixture_add_arguments_v1;

typedef struct mf_host_fixture_v1 mf_host_fixture_v1;

mf_shared_status_v1 mf_host_fixture_create_v1(mf_host_fixture_v1** out_fixture);
void mf_host_fixture_destroy_v1(mf_host_fixture_v1* fixture);

mf_registry_view_id_v1 mf_host_fixture_view_id_v1(const mf_host_fixture_v1* fixture);
int32_t mf_host_fixture_registry_fd_v1(const mf_host_fixture_v1* fixture);
int32_t mf_host_fixture_submission_fd_v1(const mf_host_fixture_v1* fixture);
int32_t mf_host_fixture_completion_fd_v1(const mf_host_fixture_v1* fixture);

mf_shared_status_v1 mf_host_fixture_control_v1(mf_host_fixture_v1* fixture,
                                               const mf_client_control_request_v1* request,
                                               const uint8_t* payload, uint64_t payload_size,
                                               mf_client_control_response_v1* response);

mf_shared_status_v1 mf_host_fixture_write_object_v1(mf_host_fixture_v1* fixture, uint64_t object_id,
                                                    uint64_t object_generation, uint64_t offset,
                                                    const uint8_t* bytes, uint64_t byte_count);

mf_shared_status_v1 mf_host_fixture_read_object_v1(const mf_host_fixture_v1* fixture,
                                                   uint64_t object_id, uint64_t object_generation,
                                                   uint64_t offset, uint8_t* bytes,
                                                   uint64_t byte_count);

mf_shared_status_v1 mf_host_fixture_pump_once_v1(mf_host_fixture_v1* fixture);

#ifdef __cplusplus
}
#endif

#endif
