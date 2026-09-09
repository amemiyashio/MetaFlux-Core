#include "metaflux/client/protocol.h"

#include <stdint.h>

_Static_assert(sizeof(mf_client_negotiation_request_v1) == 64, "request wire size");
_Static_assert(sizeof(mf_client_negotiation_response_v1) == 64, "response wire size");
_Static_assert(sizeof(mf_client_control_request_v1) == 64, "control request wire size");
_Static_assert(sizeof(mf_client_control_response_v1) == 64, "control response wire size");
_Static_assert(sizeof(mf_client_process_snapshot_header_v1) == 64, "snapshot header wire size");
_Static_assert(sizeof(mf_client_process_snapshot_row_wire_v1) == 128, "snapshot row wire size");
_Static_assert(sizeof(mf_client_kernel_request_v1) == 64, "kernel request header wire size");
_Static_assert(MF_CLIENT_CAP_COPY_REGION_V1 == (UINT64_C(1) << 7U), "copy-region bit");
_Static_assert(MF_CLIENT_CAP_POLICY_SETTERS_V1 == (UINT64_C(1) << 8U), "policy-setter bit");
_Static_assert(MF_CLIENT_CAP_DIRECT_HOST_COPY_V1 == (UINT64_C(1) << 9U), "direct-copy bit");
_Static_assert(MF_CLIENT_CAP_CDEV_BINDING_V1 == (UINT64_C(1) << 10U), "cdev-binding bit");
_Static_assert(MF_CLIENT_CAP_KERNEL_REQUEST_V1 == (UINT64_C(1) << 11U), "kernel-request bit");
_Static_assert(MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1 == UINT16_C(13),
               "persistence setter opcode");
_Static_assert(MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1 == UINT16_C(14),
               "compute setter opcode");
_Static_assert(MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1 == UINT16_C(15),
               "host address-space opcode");
_Static_assert(MF_CLIENT_CONTROL_CDEV_BIND_V1 == UINT16_C(16), "cdev-binding opcode");
_Static_assert(MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1 == UINT16_C(17),
               "kernel-request opcode");
_Static_assert(MF_CLIENT_KERNEL_REQUEST_OPERATION_CONCAT_U32_V1 == UINT32_C(27),
               "concat-u32 operation");

int main(void) {
  mf_client_negotiation_request_v1 request;
  mf_client_negotiation_response_v1 response;
  mf_client_control_request_v1 control_request;
  mf_client_control_response_v1 control_response;
  uint8_t kernel_request_payload[MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 + UINT32_C(1)];
  mf_client_kernel_request_v1* kernel_request =
      (mf_client_kernel_request_v1*)kernel_request_payload;
  uint8_t snapshot[MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 +
                   (UINT32_C(2) * MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1)];
  mf_client_process_snapshot_header_v1* snapshot_header =
      (mf_client_process_snapshot_header_v1*)snapshot;
  mf_client_process_snapshot_row_wire_v1* snapshot_row = (mf_client_process_snapshot_row_wire_v1*)0;
  const uint8_t process_name[] = {'h', 'o', 'l', 'd', 'e', 'r'};
  uint64_t snapshot_size = 0;
  const uint64_t required = MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1;
  const uint64_t optional = MF_CLIENT_CAP_FUTEX_DOORBELL_V1 | MF_CLIENT_CAP_TELEMETRY_V1;
  const uint64_t runtime = required | MF_CLIENT_CAP_FUTEX_DOORBELL_V1;
  const uint64_t live_context = MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1;
  const uint64_t copy_region = MF_CLIENT_CAP_COPY_REGION_V1;
  const uint64_t cdev_binding = MF_CLIENT_CAP_CDEV_BINDING_V1;
  const uint64_t kernel_request_capability = MF_CLIENT_CAP_KERNEL_REQUEST_V1;

  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(2), required, optional,
                                        MF_CLIENT_FLAG_JOIN_EXISTING_VIEW_V1);
  if (request.bytes[0] != UINT8_C(0x4d) || request.bytes[1] != UINT8_C(0x46) ||
      request.bytes[2] != UINT8_C(0x43) || request.bytes[3] != UINT8_C(0x31) ||
      mf_client_load_le16_v1(request.bytes + 12) != UINT16_C(1) ||
      mf_client_load_le16_v1(request.bytes + 14) != UINT16_C(2) ||
      mf_client_load_le64_v1(request.bytes + 16) != required ||
      mf_client_load_le64_v1(request.bytes + 24) != optional ||
      mf_client_negotiation_request_validate_v1(&request) != MF_CLIENT_NEGOTIATION_OK) {
    return 1;
  }

  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime, UINT32_C(1), UINT32_C(1),
                             UINT64_C(0x1122334455667788), UINT64_C(0x8877665544332211),
                             &response) != MF_CLIENT_NEGOTIATION_OK ||
      mf_client_negotiation_response_validate_v1(&response) != MF_CLIENT_NEGOTIATION_OK ||
      mf_client_load_le32_v1(response.bytes + 16) != UINT32_C(1) ||
      mf_client_load_le64_v1(response.bytes + 24) != runtime ||
      mf_client_load_le64_v1(response.bytes + 40) != UINT64_C(0x1122334455667788) ||
      mf_client_load_le64_v1(response.bytes + 48) != UINT64_C(0x8877665544332211)) {
    return 2;
  }

  request.bytes[63] = UINT8_C(1);
  if (mf_client_negotiation_request_validate_v1(&request) != MF_CLIENT_NEGOTIATION_MALFORMED) {
    return 3;
  }
  request.bytes[63] = UINT8_C(0);
  mf_client_negotiation_request_init_v1(&request, UINT16_C(2), UINT16_C(3), required, UINT64_C(0),
                                        UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1),
                             &response) != MF_CLIENT_NEGOTIATION_UNSUPPORTED_VERSION) {
    return 4;
  }
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), UINT64_C(1) << 63U,
                                        UINT64_C(0), UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1),
                             &response) != MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY) {
    return 5;
  }

  /* Old client to new runtime: retain v1 legacy publication semantics. */
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), required, optional,
                                        UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime | live_context,
                             UINT32_C(1), UINT32_C(1), UINT64_C(1), UINT64_C(1),
                             &response) != MF_CLIENT_NEGOTIATION_OK ||
      (mf_client_load_le64_v1(response.bytes + 24) & live_context) != UINT64_C(0)) {
    return 14;
  }

  /* New client to old runtime: reject before control traffic begins. */
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), required | live_context,
                                        optional, UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1),
                             &response) != MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY) {
    return 15;
  }

  /* New client to old runtime: optional COPY_REGION downgrades cleanly. */
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), required | live_context,
                                        optional | copy_region, UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime | live_context,
                             UINT32_C(1), UINT32_C(1), UINT64_C(1), UINT64_C(1),
                             &response) != MF_CLIENT_NEGOTIATION_OK ||
      (mf_client_load_le64_v1(response.bytes + 24) & live_context) == UINT64_C(0) ||
      (mf_client_load_le64_v1(response.bytes + 24) & copy_region) != UINT64_C(0)) {
    return 16;
  }

  /* New client to new runtime: negotiated COPY_REGION authorizes region descriptors. */
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1),
                             runtime | live_context | copy_region, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1), &response) != MF_CLIENT_NEGOTIATION_OK ||
      (mf_client_load_le64_v1(response.bytes + 24) & copy_region) == UINT64_C(0)) {
    return 17;
  }

  /* cdev binding is an initialization capability, not a late transport switch. */
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1), required | cdev_binding,
                                        UINT64_C(0), UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1), &response) !=
      MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY) {
    return 20;
  }
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime | cdev_binding,
                             UINT32_C(1), UINT32_C(1), UINT64_C(1), UINT64_C(1), &response) !=
          MF_CLIENT_NEGOTIATION_OK ||
      (mf_client_load_le64_v1(response.bytes + 24) & cdev_binding) == UINT64_C(0)) {
    return 21;
  }

  /* A profile request is required: it may not silently downgrade to raw artifacts. */
  mf_client_negotiation_request_init_v1(&request, UINT16_C(1), UINT16_C(1),
                                        required | kernel_request_capability, UINT64_C(0),
                                        UINT32_C(0));
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1), runtime, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1),
                             &response) != MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY) {
    return 23;
  }
  if (mf_client_negotiate_v1(&request, UINT16_C(1), UINT16_C(1),
                             runtime | kernel_request_capability, UINT32_C(1), UINT32_C(1),
                             UINT64_C(1), UINT64_C(1), &response) != MF_CLIENT_NEGOTIATION_OK ||
      (mf_client_load_le64_v1(response.bytes + 24) & kernel_request_capability) == UINT64_C(0)) {
    return 24;
  }

  mf_client_control_request_init_v1(&control_request, MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1,
                                    MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX,
                                    UINT64_C(17), UINT64_C(0x1122334455667788),
                                    UINT64_C(0x8877665544332211), UINT64_C(3), UINT64_C(4096));
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le16_v1(control_request.bytes + 12) !=
          MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1 ||
      mf_client_load_le64_v1(control_request.bytes + 24) != UINT64_C(17) ||
      mf_client_load_le64_v1(control_request.bytes + 48) != UINT64_C(3) ||
      mf_client_load_le64_v1(control_request.bytes + 56) != UINT64_C(4096)) {
    return 6;
  }
  mf_client_control_response_init_v1(&control_response, MF_CLIENT_CONTROL_OK, UINT32_C(0),
                                     UINT64_C(17), UINT64_C(0x1122334455667788),
                                     UINT64_C(0x8877665544332211), UINT64_C(91), UINT64_C(4));
  if (mf_client_control_response_validate_v1(&control_response) != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le64_v1(control_response.bytes + 48) != UINT64_C(91) ||
      mf_client_load_le64_v1(control_response.bytes + 56) != UINT64_C(4)) {
    return 7;
  }
  control_request.bytes[19] = UINT8_C(1);
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_MALFORMED) {
    return 8;
  }
  mf_client_control_request_init_v1(&control_request, MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1,
                                    UINT16_C(0), UINT64_C(18), UINT64_C(0x1122334455667788),
                                    UINT64_C(0x8877665544332211), UINT64_C(1), UINT64_C(1));
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_OK) {
    return 13;
  }
  mf_client_control_request_init_v1(&control_request, MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1,
                                    UINT16_C(0), UINT64_C(19), UINT64_C(0x1122334455667788),
                                    UINT64_C(0x8877665544332211), UINT64_C(7), UINT64_C(3));
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_OK) {
    return 18;
  }
  mf_client_control_request_init_v1(
      &control_request, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1, UINT16_C(0), UINT64_C(20),
      UINT64_C(0x1122334455667788), UINT64_C(0x8877665544332211), UINT64_C(7), UINT64_C(1));
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_OK) {
    return 19;
  }
  mf_client_control_request_init_v1(&control_request, MF_CLIENT_CONTROL_CDEV_BIND_V1, UINT16_C(0),
                                    UINT64_C(21), UINT64_C(1), UINT64_C(1),
                                    UINT64_C(1), UINT64_C(1));
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_OK) {
    return 22;
  }
  mf_client_kernel_request_init_v1(kernel_request, MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1,
                                   MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_ADD_I32_V1,
                                   MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1,
                                   sizeof(kernel_request_payload));
  kernel_request_payload[MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1] = UINT8_C(0x7f);
  if (mf_client_kernel_request_validate_v1(
          kernel_request_payload, sizeof(kernel_request_payload)) != MF_CLIENT_CONTROL_OK ||
      mf_client_kernel_request_payload_offset_v1(kernel_request) !=
          MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 ||
      mf_client_kernel_request_payload_size_v1(kernel_request) != UINT64_C(1)) {
    return 25;
  }
  mf_client_kernel_request_init_v1(kernel_request, MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1,
                                   MF_CLIENT_KERNEL_REQUEST_OPERATION_CONCAT_U32_V1,
                                   MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1,
                                   sizeof(kernel_request_payload));
  if (mf_client_kernel_request_validate_v1(
          kernel_request_payload, sizeof(kernel_request_payload)) != MF_CLIENT_CONTROL_OK) {
    return 29;
  }
  mf_client_kernel_request_init_v1(kernel_request, MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1,
                                   MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_ADD_I32_V1,
                                   MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1,
                                   sizeof(kernel_request_payload));
  kernel_request->bytes[56] = UINT8_C(1);
  if (mf_client_kernel_request_validate_v1(
          kernel_request_payload, sizeof(kernel_request_payload)) != MF_CLIENT_CONTROL_MALFORMED) {
    return 26;
  }
  kernel_request->bytes[56] = UINT8_C(0);
  mf_client_store_le32_v1(kernel_request->bytes + 16, UINT32_C(2));
  if (mf_client_kernel_request_validate_v1(
          kernel_request_payload, sizeof(kernel_request_payload)) != MF_CLIENT_CONTROL_MALFORMED) {
    return 27;
  }
  mf_client_kernel_request_init_v1(kernel_request, MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1,
                                   MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_ADD_I32_V1,
                                   MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1,
                                   sizeof(kernel_request_payload));
  kernel_request_payload[MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1] = UINT8_C(0x7f);
  mf_client_control_request_init_v1(&control_request, MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1,
                                    MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD, UINT64_C(22), UINT64_C(1),
                                    UINT64_C(1), UINT64_C(1), sizeof(kernel_request_payload));
  if (mf_client_control_request_validate_v1(&control_request) != MF_CLIENT_CONTROL_OK) {
    return 28;
  }
  mf_client_process_snapshot_header_init_v1(snapshot_header, UINT64_C(9), UINT32_C(2));
  snapshot_row = mf_client_process_snapshot_mutable_row_v1_at(snapshot, UINT32_C(0));
  mf_client_process_snapshot_row_init_v1(
      snapshot_row, UINT32_C(101), MF_CLIENT_PROCESS_KIND_COMPUTE_V1, UINT64_C(1001), UINT64_C(7),
      UINT64_C(3), UINT64_C(4194304), process_name, (uint32_t)sizeof(process_name));
  snapshot_row = mf_client_process_snapshot_mutable_row_v1_at(snapshot, UINT32_C(1));
  mf_client_process_snapshot_row_init_v1(
      snapshot_row, UINT32_C(102), MF_CLIENT_PROCESS_KIND_COMPUTE_V1, UINT64_C(1002), UINT64_C(7),
      UINT64_C(3), UINT64_C(8192), process_name, (uint32_t)sizeof(process_name));
  if (mf_client_process_snapshot_size_v1(UINT32_C(2), &snapshot_size) != MF_CLIENT_CONTROL_OK ||
      snapshot_size != sizeof(snapshot) ||
      mf_client_process_snapshot_validate_v1(snapshot, sizeof(snapshot)) != MF_CLIENT_CONTROL_OK ||
      snapshot[0] != UINT8_C(0x4d) || snapshot[1] != UINT8_C(0x50) ||
      snapshot[2] != UINT8_C(0x53) || snapshot[3] != UINT8_C(0x31) ||
      mf_client_process_snapshot_revision_v1(snapshot) != UINT64_C(9) ||
      mf_client_process_snapshot_count_v1(snapshot) != UINT32_C(2) ||
      mf_client_process_snapshot_row_used_memory_v1(
          mf_client_process_snapshot_row_v1_at(snapshot, UINT32_C(0))) != UINT64_C(4194304)) {
    return 9;
  }
  snapshot_header->bytes[40] = UINT8_C(1);
  if (mf_client_process_snapshot_validate_v1(snapshot, sizeof(snapshot)) !=
      MF_CLIENT_CONTROL_MALFORMED) {
    return 10;
  }
  snapshot_header->bytes[40] = UINT8_C(0);
  snapshot_row = mf_client_process_snapshot_mutable_row_v1_at(snapshot, UINT32_C(1));
  mf_client_store_le32_v1(snapshot_row->bytes + 0, UINT32_C(101));
  if (mf_client_process_snapshot_validate_v1(snapshot, sizeof(snapshot)) !=
      MF_CLIENT_CONTROL_MALFORMED) {
    return 11;
  }
  mf_client_store_le32_v1(snapshot_row->bytes + 0, UINT32_C(102));
  mf_client_store_le32_v1(snapshot_header->bytes + 24, UINT32_MAX);
  mf_client_store_le64_v1(snapshot_header->bytes + 8, UINT64_MAX);
  if (mf_client_process_snapshot_validate_v1(snapshot, sizeof(snapshot)) !=
          MF_CLIENT_CONTROL_MALFORMED ||
      mf_client_process_snapshot_size_v1(UINT32_MAX, &snapshot_size) !=
          MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED) {
    return 12;
  }
  return 0;
}
