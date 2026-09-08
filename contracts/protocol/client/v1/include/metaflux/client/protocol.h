#ifndef METAFLUX_CLIENT_PROTOCOL_H
#define METAFLUX_CLIENT_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_CLIENT_PROTOCOL_ABI_VERSION_1 UINT32_C(1)
#define MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 UINT32_C(64)
#define MF_CLIENT_PROTOCOL_MAGIC_V1 UINT32_C(0x3143464d)
#define MF_CLIENT_PROCESS_SNAPSHOT_MAGIC_V1 UINT32_C(0x3153504d)
#define MF_CLIENT_PROCESS_SNAPSHOT_VERSION_V1 UINT16_C(1)
#define MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 UINT16_C(64)
#define MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1 UINT32_C(128)
#define MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1 UINT32_C(64)
#define MF_CLIENT_PROCESS_NAME_SIZE_V1 UINT32_C(64)
#define MF_CLIENT_KERNEL_REQUEST_MAGIC_V1 UINT32_C(0x31524b4d)
#define MF_CLIENT_KERNEL_REQUEST_VERSION_V1 UINT16_C(1)
#define MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 UINT16_C(64)

#define MF_CLIENT_MESSAGE_NEGOTIATE_REQUEST_V1 UINT16_C(1)
#define MF_CLIENT_MESSAGE_NEGOTIATE_RESPONSE_V1 UINT16_C(2)
#define MF_CLIENT_MESSAGE_CONTROL_REQUEST_V1 UINT16_C(3)
#define MF_CLIENT_MESSAGE_CONTROL_RESPONSE_V1 UINT16_C(4)

#define MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1 UINT16_C(1)
#define MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1 UINT16_C(2)
#define MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1 UINT16_C(3)
#define MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1 UINT16_C(4)
#define MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1 UINT16_C(5)
#define MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1 UINT16_C(6)
#define MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1 UINT16_C(7)
#define MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1 UINT16_C(8)
#define MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1 UINT16_C(9)
#define MF_CLIENT_CONTROL_PROCESS_SNAPSHOT_V1 UINT16_C(10)
#define MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1 UINT16_C(11)
#define MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1 UINT16_C(12)
#define MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1 UINT16_C(13)
#define MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1 UINT16_C(14)
#define MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1 UINT16_C(15)
#define MF_CLIENT_CONTROL_CDEV_BIND_V1 UINT16_C(16)
#define MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1 UINT16_C(17)

#define MF_CLIENT_CONTROL_OK UINT32_C(0)
#define MF_CLIENT_CONTROL_MALFORMED UINT32_C(1)
#define MF_CLIENT_CONTROL_STALE_GENERATION UINT32_C(2)
#define MF_CLIENT_CONTROL_NOT_FOUND UINT32_C(3)
#define MF_CLIENT_CONTROL_INVALID_ARGUMENT UINT32_C(4)
#define MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED UINT32_C(5)
#define MF_CLIENT_CONTROL_UNSUPPORTED UINT32_C(6)
#define MF_CLIENT_CONTROL_INTERNAL_ERROR UINT32_C(7)
#define MF_CLIENT_CONTROL_NO_PERMISSION UINT32_C(8)

#define MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD (UINT16_C(1) << 0U)
#define MF_CLIENT_CONTROL_FLAG_READ (UINT16_C(1) << 1U)
#define MF_CLIENT_CONTROL_FLAG_WRITE (UINT16_C(1) << 2U)
#define MF_CLIENT_CONTROL_FLAG_PTX (UINT16_C(1) << 3U)
#define MF_CLIENT_CONTROL_KNOWN_FLAGS                                                              \
  (MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |                               \
   MF_CLIENT_CONTROL_FLAG_WRITE | MF_CLIENT_CONTROL_FLAG_PTX)

#define MF_CLIENT_NEGOTIATION_OK UINT32_C(0)
#define MF_CLIENT_NEGOTIATION_MALFORMED UINT32_C(1)
#define MF_CLIENT_NEGOTIATION_UNSUPPORTED_VERSION UINT32_C(2)
#define MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY UINT32_C(3)
#define MF_CLIENT_NEGOTIATION_INCOMPATIBLE_VIEW UINT32_C(4)

#define MF_CLIENT_CAP_SHARED_DEVICE_V1 (UINT64_C(1) << 0U)
#define MF_CLIENT_CAP_MEMFD_RING_V1 (UINT64_C(1) << 1U)
#define MF_CLIENT_CAP_FUTEX_DOORBELL_V1 (UINT64_C(1) << 2U)
#define MF_CLIENT_CAP_TIMELINE_V1 (UINT64_C(1) << 3U)
#define MF_CLIENT_CAP_TELEMETRY_V1 (UINT64_C(1) << 4U)
#define MF_CLIENT_CAP_PROCESS_SNAPSHOT_V1 (UINT64_C(1) << 5U)
#define MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1 (UINT64_C(1) << 6U)
#define MF_CLIENT_CAP_COPY_REGION_V1 (UINT64_C(1) << 7U)
#define MF_CLIENT_CAP_POLICY_SETTERS_V1 (UINT64_C(1) << 8U)
#define MF_CLIENT_CAP_DIRECT_HOST_COPY_V1 (UINT64_C(1) << 9U)
#define MF_CLIENT_CAP_CDEV_BINDING_V1 (UINT64_C(1) << 10U)
#define MF_CLIENT_CAP_KERNEL_REQUEST_V1 (UINT64_C(1) << 11U)

#define MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1 UINT32_C(1)
#define MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_ADD_I32_V1 UINT32_C(1)
#define MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_MUL_I32_V1 UINT32_C(2)
#define MF_CLIENT_KERNEL_REQUEST_OPERATION_ABI_VERSION_V1 UINT32_C(1)
#define MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1 UINT32_C(2)
#define MF_CLIENT_KERNEL_REQUEST_LIFETIME_MODULE_LOAD_V1 UINT32_C(1)

#define MF_CLIENT_PROCESS_KIND_COMPUTE_V1 (UINT32_C(1) << 0U)
#define MF_CLIENT_PROCESS_KIND_GRAPHICS_V1 (UINT32_C(1) << 1U)
#define MF_CLIENT_PROCESS_KIND_MPS_V1 (UINT32_C(1) << 2U)
#define MF_CLIENT_PROCESS_KNOWN_KINDS_V1                                                           \
  (MF_CLIENT_PROCESS_KIND_COMPUTE_V1 | MF_CLIENT_PROCESS_KIND_GRAPHICS_V1 |                        \
   MF_CLIENT_PROCESS_KIND_MPS_V1)

#define MF_CLIENT_FLAG_JOIN_EXISTING_VIEW_V1 (UINT32_C(1) << 0U)
#define MF_CLIENT_KNOWN_FLAGS_V1 MF_CLIENT_FLAG_JOIN_EXISTING_VIEW_V1

typedef struct mf_client_negotiation_request_v1 {
  uint8_t bytes[MF_CLIENT_PROTOCOL_WIRE_SIZE_V1];
} mf_client_negotiation_request_v1;

typedef struct mf_client_negotiation_response_v1 {
  uint8_t bytes[MF_CLIENT_PROTOCOL_WIRE_SIZE_V1];
} mf_client_negotiation_response_v1;

typedef struct mf_client_control_request_v1 {
  uint8_t bytes[MF_CLIENT_PROTOCOL_WIRE_SIZE_V1];
} mf_client_control_request_v1;

typedef struct mf_client_control_response_v1 {
  uint8_t bytes[MF_CLIENT_PROTOCOL_WIRE_SIZE_V1];
} mf_client_control_response_v1;

typedef struct mf_client_process_snapshot_header_v1 {
  uint8_t bytes[MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1];
} mf_client_process_snapshot_header_v1;

typedef struct mf_client_process_snapshot_row_wire_v1 {
  uint8_t bytes[MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1];
} mf_client_process_snapshot_row_wire_v1;

typedef struct mf_client_kernel_request_v1 {
  uint8_t bytes[MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1];
} mf_client_kernel_request_v1;

static inline uint16_t mf_client_load_le16_v1(const uint8_t* bytes) {
  return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static inline uint32_t mf_client_load_le32_v1(const uint8_t* bytes) {
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
         ((uint32_t)bytes[3] << 24U);
}

static inline uint64_t mf_client_load_le64_v1(const uint8_t* bytes) {
  return (uint64_t)mf_client_load_le32_v1(bytes) |
         ((uint64_t)mf_client_load_le32_v1(bytes + 4) << 32U);
}

static inline void mf_client_store_le16_v1(uint8_t* bytes, uint16_t value) {
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8U);
}

static inline void mf_client_store_le32_v1(uint8_t* bytes, uint32_t value) {
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8U);
  bytes[2] = (uint8_t)(value >> 16U);
  bytes[3] = (uint8_t)(value >> 24U);
}

static inline void mf_client_store_le64_v1(uint8_t* bytes, uint64_t value) {
  mf_client_store_le32_v1(bytes, (uint32_t)value);
  mf_client_store_le32_v1(bytes + 4, (uint32_t)(value >> 32U));
}

static inline void mf_client_zero_wire_v1(uint8_t* bytes) {
  uint32_t index = 0;
  for (index = 0; index < MF_CLIENT_PROTOCOL_WIRE_SIZE_V1; ++index) {
    bytes[index] = UINT8_C(0);
  }
}

static inline void mf_client_zero_bytes_v1(uint8_t* bytes, uint32_t byte_count) {
  uint32_t index = 0;
  for (index = 0; index < byte_count; ++index) {
    bytes[index] = UINT8_C(0);
  }
}

static inline int mf_client_reserved_is_zero_v1(const uint8_t* bytes, uint32_t begin, uint32_t end);

static inline void mf_client_kernel_request_init_v1(mf_client_kernel_request_v1* request,
                                                    uint32_t profile, uint32_t operation,
                                                    uint32_t kernel_ir_schema_version,
                                                    uint64_t total_size) {
  mf_client_zero_bytes_v1(request->bytes, MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1);
  mf_client_store_le32_v1(request->bytes + 0, MF_CLIENT_KERNEL_REQUEST_MAGIC_V1);
  mf_client_store_le16_v1(request->bytes + 4, MF_CLIENT_KERNEL_REQUEST_VERSION_V1);
  mf_client_store_le16_v1(request->bytes + 6, MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1);
  mf_client_store_le64_v1(request->bytes + 8, total_size);
  mf_client_store_le32_v1(request->bytes + 16, profile);
  mf_client_store_le32_v1(request->bytes + 20, operation);
  mf_client_store_le32_v1(request->bytes + 24, MF_CLIENT_KERNEL_REQUEST_OPERATION_ABI_VERSION_V1);
  mf_client_store_le32_v1(request->bytes + 28, kernel_ir_schema_version);
  mf_client_store_le32_v1(request->bytes + 32, MF_CLIENT_KERNEL_REQUEST_LIFETIME_MODULE_LOAD_V1);
  mf_client_store_le64_v1(request->bytes + 40, MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1);
  mf_client_store_le64_v1(request->bytes + 48,
                          total_size >= MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1
                              ? total_size - MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1
                              : UINT64_C(0));
}

static inline uint64_t
mf_client_kernel_request_payload_offset_v1(const mf_client_kernel_request_v1* request) {
  return mf_client_load_le64_v1(request->bytes + 40);
}

static inline uint64_t
mf_client_kernel_request_payload_size_v1(const mf_client_kernel_request_v1* request) {
  return mf_client_load_le64_v1(request->bytes + 48);
}

static inline uint32_t mf_client_kernel_request_validate_v1(const uint8_t* payload,
                                                            uint64_t byte_count) {
  const mf_client_kernel_request_v1* request = (const mf_client_kernel_request_v1*)payload;
  if (payload == (const uint8_t*)0 || byte_count < MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 ||
      mf_client_load_le32_v1(request->bytes + 0) != MF_CLIENT_KERNEL_REQUEST_MAGIC_V1 ||
      mf_client_load_le16_v1(request->bytes + 4) != MF_CLIENT_KERNEL_REQUEST_VERSION_V1 ||
      mf_client_load_le16_v1(request->bytes + 6) != MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 ||
      mf_client_load_le64_v1(request->bytes + 8) != byte_count ||
      mf_client_load_le32_v1(request->bytes + 16) != MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1 ||
      (mf_client_load_le32_v1(request->bytes + 20) !=
           MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_ADD_I32_V1 &&
       mf_client_load_le32_v1(request->bytes + 20) !=
           MF_CLIENT_KERNEL_REQUEST_OPERATION_ELEMENTWISE_MUL_I32_V1) ||
      mf_client_load_le32_v1(request->bytes + 24) !=
          MF_CLIENT_KERNEL_REQUEST_OPERATION_ABI_VERSION_V1 ||
      mf_client_load_le32_v1(request->bytes + 28) !=
          MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1 ||
      mf_client_load_le32_v1(request->bytes + 32) !=
          MF_CLIENT_KERNEL_REQUEST_LIFETIME_MODULE_LOAD_V1 ||
      mf_client_kernel_request_payload_offset_v1(request) !=
          MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 ||
      mf_client_kernel_request_payload_size_v1(request) == UINT64_C(0) ||
      mf_client_kernel_request_payload_size_v1(request) !=
          byte_count - MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 ||
      !mf_client_reserved_is_zero_v1(request->bytes, UINT32_C(36), UINT32_C(40)) ||
      !mf_client_reserved_is_zero_v1(request->bytes, UINT32_C(56),
                                     MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1)) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  return MF_CLIENT_CONTROL_OK;
}

static inline int mf_client_reserved_is_zero_v1(const uint8_t* bytes, uint32_t begin,
                                                uint32_t end) {
  uint32_t index = 0;
  for (index = begin; index < end; ++index) {
    if (bytes[index] != UINT8_C(0)) {
      return 0;
    }
  }
  return 1;
}

static inline void mf_client_negotiation_request_init_v1(
    mf_client_negotiation_request_v1* request, uint16_t minimum_version, uint16_t maximum_version,
    uint64_t required_capabilities, uint64_t optional_capabilities, uint32_t flags) {
  mf_client_zero_wire_v1(request->bytes);
  mf_client_store_le32_v1(request->bytes + 0, MF_CLIENT_PROTOCOL_MAGIC_V1);
  mf_client_store_le16_v1(request->bytes + 4, MF_CLIENT_MESSAGE_NEGOTIATE_REQUEST_V1);
  mf_client_store_le16_v1(request->bytes + 6, (uint16_t)MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le32_v1(request->bytes + 8, MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le16_v1(request->bytes + 12, minimum_version);
  mf_client_store_le16_v1(request->bytes + 14, maximum_version);
  mf_client_store_le64_v1(request->bytes + 16, required_capabilities);
  mf_client_store_le64_v1(request->bytes + 24, optional_capabilities);
  mf_client_store_le32_v1(request->bytes + 32, flags);
}

static inline uint32_t
mf_client_negotiation_request_validate_v1(const mf_client_negotiation_request_v1* request) {
  if (request == (const mf_client_negotiation_request_v1*)0) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  if (mf_client_load_le32_v1(request->bytes + 0) != MF_CLIENT_PROTOCOL_MAGIC_V1 ||
      mf_client_load_le16_v1(request->bytes + 4) != MF_CLIENT_MESSAGE_NEGOTIATE_REQUEST_V1 ||
      mf_client_load_le16_v1(request->bytes + 6) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      mf_client_load_le32_v1(request->bytes + 8) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  if (mf_client_load_le16_v1(request->bytes + 12) == UINT16_C(0) ||
      mf_client_load_le16_v1(request->bytes + 12) > mf_client_load_le16_v1(request->bytes + 14) ||
      (mf_client_load_le32_v1(request->bytes + 32) & ~MF_CLIENT_KNOWN_FLAGS_V1) != 0U ||
      !mf_client_reserved_is_zero_v1(request->bytes, UINT32_C(36),
                                     MF_CLIENT_PROTOCOL_WIRE_SIZE_V1)) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  return MF_CLIENT_NEGOTIATION_OK;
}

static inline void
mf_client_negotiation_response_init_v1(mf_client_negotiation_response_v1* response, uint32_t status,
                                       uint32_t selected_version, uint64_t negotiated_capabilities,
                                       uint32_t shared_layout_version, uint32_t ring_layout_version,
                                       uint64_t daemon_incarnation, uint64_t view_serial) {
  mf_client_zero_wire_v1(response->bytes);
  mf_client_store_le32_v1(response->bytes + 0, MF_CLIENT_PROTOCOL_MAGIC_V1);
  mf_client_store_le16_v1(response->bytes + 4, MF_CLIENT_MESSAGE_NEGOTIATE_RESPONSE_V1);
  mf_client_store_le16_v1(response->bytes + 6, (uint16_t)MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le32_v1(response->bytes + 8, MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le32_v1(response->bytes + 12, status);
  mf_client_store_le32_v1(response->bytes + 16, selected_version);
  mf_client_store_le64_v1(response->bytes + 24, negotiated_capabilities);
  mf_client_store_le32_v1(response->bytes + 32, shared_layout_version);
  mf_client_store_le32_v1(response->bytes + 36, ring_layout_version);
  mf_client_store_le64_v1(response->bytes + 40, daemon_incarnation);
  mf_client_store_le64_v1(response->bytes + 48, view_serial);
}

static inline uint32_t
mf_client_negotiation_response_validate_v1(const mf_client_negotiation_response_v1* response) {
  uint32_t status = 0;
  if (response == (const mf_client_negotiation_response_v1*)0) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  status = mf_client_load_le32_v1(response->bytes + 12);
  if (mf_client_load_le32_v1(response->bytes + 0) != MF_CLIENT_PROTOCOL_MAGIC_V1 ||
      mf_client_load_le16_v1(response->bytes + 4) != MF_CLIENT_MESSAGE_NEGOTIATE_RESPONSE_V1 ||
      mf_client_load_le16_v1(response->bytes + 6) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      mf_client_load_le32_v1(response->bytes + 8) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      status > MF_CLIENT_NEGOTIATION_INCOMPATIBLE_VIEW ||
      mf_client_load_le32_v1(response->bytes + 20) != UINT32_C(0) ||
      !mf_client_reserved_is_zero_v1(response->bytes, UINT32_C(56),
                                     MF_CLIENT_PROTOCOL_WIRE_SIZE_V1)) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  if (status == MF_CLIENT_NEGOTIATION_OK &&
      (mf_client_load_le32_v1(response->bytes + 16) == UINT32_C(0) ||
       mf_client_load_le32_v1(response->bytes + 32) == UINT32_C(0) ||
       mf_client_load_le32_v1(response->bytes + 36) == UINT32_C(0) ||
       mf_client_load_le64_v1(response->bytes + 40) == UINT64_C(0) ||
       mf_client_load_le64_v1(response->bytes + 48) == UINT64_C(0))) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  return MF_CLIENT_NEGOTIATION_OK;
}

static inline uint32_t
mf_client_negotiate_v1(const mf_client_negotiation_request_v1* request,
                       uint16_t runtime_minimum_version, uint16_t runtime_maximum_version,
                       uint64_t runtime_capabilities, uint32_t shared_layout_version,
                       uint32_t ring_layout_version, uint64_t daemon_incarnation,
                       uint64_t view_serial, mf_client_negotiation_response_v1* response) {
  uint16_t client_minimum_version = 0;
  uint16_t client_maximum_version = 0;
  uint16_t selected_version = 0;
  uint64_t required_capabilities = 0;
  uint64_t optional_capabilities = 0;
  uint32_t status = mf_client_negotiation_request_validate_v1(request);

  if (response == (mf_client_negotiation_response_v1*)0) {
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }
  if (status != MF_CLIENT_NEGOTIATION_OK || runtime_minimum_version == UINT16_C(0) ||
      runtime_minimum_version > runtime_maximum_version || shared_layout_version == UINT32_C(0) ||
      ring_layout_version == UINT32_C(0)) {
    mf_client_negotiation_response_init_v1(response, MF_CLIENT_NEGOTIATION_MALFORMED, UINT32_C(0),
                                           UINT64_C(0), UINT32_C(0), UINT32_C(0), UINT64_C(0),
                                           UINT64_C(0));
    return MF_CLIENT_NEGOTIATION_MALFORMED;
  }

  client_minimum_version = mf_client_load_le16_v1(request->bytes + 12);
  client_maximum_version = mf_client_load_le16_v1(request->bytes + 14);
  selected_version = client_maximum_version < runtime_maximum_version ? client_maximum_version
                                                                      : runtime_maximum_version;
  if (selected_version < client_minimum_version || selected_version < runtime_minimum_version) {
    mf_client_negotiation_response_init_v1(response, MF_CLIENT_NEGOTIATION_UNSUPPORTED_VERSION,
                                           UINT32_C(0), UINT64_C(0), UINT32_C(0), UINT32_C(0),
                                           UINT64_C(0), UINT64_C(0));
    return MF_CLIENT_NEGOTIATION_UNSUPPORTED_VERSION;
  }

  required_capabilities = mf_client_load_le64_v1(request->bytes + 16);
  optional_capabilities = mf_client_load_le64_v1(request->bytes + 24);
  if ((required_capabilities & ~runtime_capabilities) != UINT64_C(0)) {
    mf_client_negotiation_response_init_v1(response, MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY,
                                           UINT32_C(0), UINT64_C(0), UINT32_C(0), UINT32_C(0),
                                           UINT64_C(0), UINT64_C(0));
    return MF_CLIENT_NEGOTIATION_UNSUPPORTED_CAPABILITY;
  }

  mf_client_negotiation_response_init_v1(
      response, MF_CLIENT_NEGOTIATION_OK, (uint32_t)selected_version,
      required_capabilities | (optional_capabilities & runtime_capabilities), shared_layout_version,
      ring_layout_version, daemon_incarnation, view_serial);
  return MF_CLIENT_NEGOTIATION_OK;
}

static inline void
mf_client_control_request_init_v1(mf_client_control_request_v1* request, uint16_t opcode,
                                  uint16_t flags, uint64_t request_id, uint64_t daemon_incarnation,
                                  uint64_t view_serial, uint64_t object_id, uint64_t argument) {
  mf_client_zero_wire_v1(request->bytes);
  mf_client_store_le32_v1(request->bytes + 0, MF_CLIENT_PROTOCOL_MAGIC_V1);
  mf_client_store_le16_v1(request->bytes + 4, MF_CLIENT_MESSAGE_CONTROL_REQUEST_V1);
  mf_client_store_le16_v1(request->bytes + 6, (uint16_t)MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le32_v1(request->bytes + 8, MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le16_v1(request->bytes + 12, opcode);
  mf_client_store_le16_v1(request->bytes + 14, flags);
  mf_client_store_le64_v1(request->bytes + 24, request_id);
  mf_client_store_le64_v1(request->bytes + 32, daemon_incarnation);
  mf_client_store_le64_v1(request->bytes + 40, view_serial);
  mf_client_store_le64_v1(request->bytes + 48, object_id);
  mf_client_store_le64_v1(request->bytes + 56, argument);
}

static inline uint32_t
mf_client_control_request_validate_v1(const mf_client_control_request_v1* request) {
  uint16_t opcode = 0;
  uint16_t flags = 0;
  if (request == (const mf_client_control_request_v1*)0 ||
      mf_client_load_le32_v1(request->bytes + 0) != MF_CLIENT_PROTOCOL_MAGIC_V1 ||
      mf_client_load_le16_v1(request->bytes + 4) != MF_CLIENT_MESSAGE_CONTROL_REQUEST_V1 ||
      mf_client_load_le16_v1(request->bytes + 6) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      mf_client_load_le32_v1(request->bytes + 8) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      !mf_client_reserved_is_zero_v1(request->bytes, UINT32_C(16), UINT32_C(24))) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  opcode = mf_client_load_le16_v1(request->bytes + 12);
  flags = mf_client_load_le16_v1(request->bytes + 14);
  if (opcode < MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1 ||
      opcode > MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1 ||
      (flags & (uint16_t)~MF_CLIENT_CONTROL_KNOWN_FLAGS) != UINT16_C(0) ||
      mf_client_load_le64_v1(request->bytes + 24) == UINT64_C(0) ||
      mf_client_load_le64_v1(request->bytes + 32) == UINT64_C(0) ||
      mf_client_load_le64_v1(request->bytes + 40) == UINT64_C(0)) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  return MF_CLIENT_CONTROL_OK;
}

static inline uint32_t mf_client_process_snapshot_size_v1(uint32_t row_count,
                                                          uint64_t* out_byte_count) {
  if (out_byte_count == (uint64_t*)0) {
    return MF_CLIENT_CONTROL_INVALID_ARGUMENT;
  }
  if (row_count > MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1) {
    return MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED;
  }
  *out_byte_count = (uint64_t)MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 +
                    ((uint64_t)row_count * (uint64_t)MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1);
  return MF_CLIENT_CONTROL_OK;
}

static inline void
mf_client_process_snapshot_header_init_v1(mf_client_process_snapshot_header_v1* header,
                                          uint64_t revision, uint32_t row_count) {
  uint64_t total_size = 0;
  mf_client_zero_bytes_v1(header->bytes, MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1);
  (void)mf_client_process_snapshot_size_v1(row_count, &total_size);
  mf_client_store_le32_v1(header->bytes + 0, MF_CLIENT_PROCESS_SNAPSHOT_MAGIC_V1);
  mf_client_store_le16_v1(header->bytes + 4, MF_CLIENT_PROCESS_SNAPSHOT_VERSION_V1);
  mf_client_store_le16_v1(header->bytes + 6, MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1);
  mf_client_store_le64_v1(header->bytes + 8, total_size);
  mf_client_store_le64_v1(header->bytes + 16, revision);
  mf_client_store_le32_v1(header->bytes + 24, row_count);
  mf_client_store_le32_v1(header->bytes + 28, MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1);
  mf_client_store_le32_v1(header->bytes + 36, MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1);
}

static inline void mf_client_process_snapshot_row_init_v1(
    mf_client_process_snapshot_row_wire_v1* row, uint32_t pid, uint32_t kinds,
    uint64_t process_start_time_ticks, uint64_t identity_record_id, uint64_t device_generation,
    uint64_t used_memory_bytes, const uint8_t* process_name, uint32_t process_name_length) {
  uint32_t index = 0;
  mf_client_zero_bytes_v1(row->bytes, MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1);
  mf_client_store_le32_v1(row->bytes + 0, pid);
  mf_client_store_le32_v1(row->bytes + 4, kinds);
  mf_client_store_le64_v1(row->bytes + 8, process_start_time_ticks);
  mf_client_store_le64_v1(row->bytes + 16, identity_record_id);
  mf_client_store_le64_v1(row->bytes + 24, device_generation);
  mf_client_store_le64_v1(row->bytes + 32, used_memory_bytes);
  mf_client_store_le32_v1(row->bytes + 40, process_name_length);
  if (process_name != (const uint8_t*)0 && process_name_length < MF_CLIENT_PROCESS_NAME_SIZE_V1) {
    for (index = 0; index < process_name_length; ++index) {
      row->bytes[48U + index] = process_name[index];
    }
  }
}

static inline const mf_client_process_snapshot_header_v1*
mf_client_process_snapshot_header_v1_at(const uint8_t* payload) {
  return (const mf_client_process_snapshot_header_v1*)payload;
}

static inline const mf_client_process_snapshot_row_wire_v1*
mf_client_process_snapshot_row_v1_at(const uint8_t* payload, uint32_t index) {
  return (const mf_client_process_snapshot_row_wire_v1*)(payload +
                                                         MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 +
                                                         ((uint64_t)index *
                                                          MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1));
}

static inline mf_client_process_snapshot_row_wire_v1*
mf_client_process_snapshot_mutable_row_v1_at(uint8_t* payload, uint32_t index) {
  return (mf_client_process_snapshot_row_wire_v1*)(payload +
                                                   MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 +
                                                   ((uint64_t)index *
                                                    MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1));
}

static inline uint64_t mf_client_process_snapshot_revision_v1(const uint8_t* payload) {
  return mf_client_load_le64_v1(payload + 16);
}

static inline uint32_t mf_client_process_snapshot_count_v1(const uint8_t* payload) {
  return mf_client_load_le32_v1(payload + 24);
}

static inline uint32_t
mf_client_process_snapshot_row_pid_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le32_v1(row->bytes + 0);
}

static inline uint32_t
mf_client_process_snapshot_row_kinds_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le32_v1(row->bytes + 4);
}

static inline uint64_t
mf_client_process_snapshot_row_start_time_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le64_v1(row->bytes + 8);
}

static inline uint64_t
mf_client_process_snapshot_row_identity_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le64_v1(row->bytes + 16);
}

static inline uint64_t
mf_client_process_snapshot_row_generation_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le64_v1(row->bytes + 24);
}

static inline uint64_t
mf_client_process_snapshot_row_used_memory_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le64_v1(row->bytes + 32);
}

static inline uint32_t
mf_client_process_snapshot_row_name_length_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return mf_client_load_le32_v1(row->bytes + 40);
}

static inline const uint8_t*
mf_client_process_snapshot_row_name_v1(const mf_client_process_snapshot_row_wire_v1* row) {
  return row->bytes + 48;
}

static inline uint32_t mf_client_process_snapshot_validate_v1(const uint8_t* payload,
                                                              uint64_t byte_count) {
  const mf_client_process_snapshot_header_v1* header =
      mf_client_process_snapshot_header_v1_at(payload);
  uint64_t expected_size = 0;
  uint32_t row_count = 0;
  uint32_t row_index = 0;
  if (payload == (const uint8_t*)0 || byte_count < MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  row_count = mf_client_load_le32_v1(header->bytes + 24);
  if (mf_client_load_le32_v1(header->bytes + 0) != MF_CLIENT_PROCESS_SNAPSHOT_MAGIC_V1 ||
      mf_client_load_le16_v1(header->bytes + 4) != MF_CLIENT_PROCESS_SNAPSHOT_VERSION_V1 ||
      mf_client_load_le16_v1(header->bytes + 6) != MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1 ||
      mf_client_load_le64_v1(header->bytes + 16) == UINT64_C(0) ||
      mf_client_load_le32_v1(header->bytes + 28) != MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1 ||
      mf_client_load_le32_v1(header->bytes + 32) != UINT32_C(0) ||
      mf_client_load_le32_v1(header->bytes + 36) != MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1 ||
      !mf_client_reserved_is_zero_v1(header->bytes, UINT32_C(40),
                                     MF_CLIENT_PROCESS_SNAPSHOT_HEADER_SIZE_V1) ||
      mf_client_process_snapshot_size_v1(row_count, &expected_size) != MF_CLIENT_CONTROL_OK ||
      expected_size != byte_count || mf_client_load_le64_v1(header->bytes + 8) != byte_count) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  for (row_index = 0; row_index < row_count; ++row_index) {
    const mf_client_process_snapshot_row_wire_v1* row =
        mf_client_process_snapshot_row_v1_at(payload, row_index);
    const uint32_t pid = mf_client_process_snapshot_row_pid_v1(row);
    const uint32_t kinds = mf_client_process_snapshot_row_kinds_v1(row);
    const uint32_t name_length = mf_client_process_snapshot_row_name_length_v1(row);
    const uint8_t* name = mf_client_process_snapshot_row_name_v1(row);
    uint32_t name_index = 0;
    uint32_t previous = 0;
    if (pid == UINT32_C(0) || kinds == UINT32_C(0) ||
        (kinds & ~MF_CLIENT_PROCESS_KNOWN_KINDS_V1) != UINT32_C(0) ||
        mf_client_process_snapshot_row_start_time_v1(row) == UINT64_C(0) ||
        mf_client_process_snapshot_row_identity_v1(row) == UINT64_C(0) ||
        mf_client_process_snapshot_row_generation_v1(row) == UINT64_C(0) ||
        name_length == UINT32_C(0) || name_length >= MF_CLIENT_PROCESS_NAME_SIZE_V1 ||
        !mf_client_reserved_is_zero_v1(row->bytes, UINT32_C(44), UINT32_C(48)) ||
        !mf_client_reserved_is_zero_v1(row->bytes, UINT32_C(112),
                                       MF_CLIENT_PROCESS_SNAPSHOT_ROW_SIZE_V1)) {
      return MF_CLIENT_CONTROL_MALFORMED;
    }
    for (name_index = 0; name_index < name_length; ++name_index) {
      if (name[name_index] == UINT8_C(0)) {
        return MF_CLIENT_CONTROL_MALFORMED;
      }
    }
    for (name_index = name_length; name_index < MF_CLIENT_PROCESS_NAME_SIZE_V1; ++name_index) {
      if (name[name_index] != UINT8_C(0)) {
        return MF_CLIENT_CONTROL_MALFORMED;
      }
    }
    for (previous = 0; previous < row_index; ++previous) {
      if (mf_client_process_snapshot_row_pid_v1(
              mf_client_process_snapshot_row_v1_at(payload, previous)) == pid) {
        return MF_CLIENT_CONTROL_MALFORMED;
      }
    }
  }
  return MF_CLIENT_CONTROL_OK;
}

static inline void mf_client_control_response_init_v1(mf_client_control_response_v1* response,
                                                      uint32_t status, uint32_t flags,
                                                      uint64_t request_id,
                                                      uint64_t daemon_incarnation,
                                                      uint64_t view_serial, uint64_t object_id,
                                                      uint64_t object_generation) {
  mf_client_zero_wire_v1(response->bytes);
  mf_client_store_le32_v1(response->bytes + 0, MF_CLIENT_PROTOCOL_MAGIC_V1);
  mf_client_store_le16_v1(response->bytes + 4, MF_CLIENT_MESSAGE_CONTROL_RESPONSE_V1);
  mf_client_store_le16_v1(response->bytes + 6, (uint16_t)MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le32_v1(response->bytes + 8, MF_CLIENT_PROTOCOL_WIRE_SIZE_V1);
  mf_client_store_le32_v1(response->bytes + 12, status);
  mf_client_store_le32_v1(response->bytes + 16, flags);
  mf_client_store_le64_v1(response->bytes + 24, request_id);
  mf_client_store_le64_v1(response->bytes + 32, daemon_incarnation);
  mf_client_store_le64_v1(response->bytes + 40, view_serial);
  mf_client_store_le64_v1(response->bytes + 48, object_id);
  mf_client_store_le64_v1(response->bytes + 56, object_generation);
}

static inline uint32_t
mf_client_control_response_validate_v1(const mf_client_control_response_v1* response) {
  uint32_t status = 0;
  if (response == (const mf_client_control_response_v1*)0 ||
      mf_client_load_le32_v1(response->bytes + 0) != MF_CLIENT_PROTOCOL_MAGIC_V1 ||
      mf_client_load_le16_v1(response->bytes + 4) != MF_CLIENT_MESSAGE_CONTROL_RESPONSE_V1 ||
      mf_client_load_le16_v1(response->bytes + 6) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      mf_client_load_le32_v1(response->bytes + 8) != MF_CLIENT_PROTOCOL_WIRE_SIZE_V1 ||
      mf_client_load_le32_v1(response->bytes + 20) != UINT32_C(0) ||
      mf_client_load_le64_v1(response->bytes + 24) == UINT64_C(0) ||
      mf_client_load_le64_v1(response->bytes + 32) == UINT64_C(0) ||
      mf_client_load_le64_v1(response->bytes + 40) == UINT64_C(0)) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  status = mf_client_load_le32_v1(response->bytes + 12);
  if (status > MF_CLIENT_CONTROL_NO_PERMISSION ||
      (status == MF_CLIENT_CONTROL_OK &&
       (mf_client_load_le64_v1(response->bytes + 48) == UINT64_C(0) ||
        mf_client_load_le64_v1(response->bytes + 56) == UINT64_C(0)))) {
    return MF_CLIENT_CONTROL_MALFORMED;
  }
  return MF_CLIENT_CONTROL_OK;
}

#ifdef __cplusplus
}
#endif

#endif
