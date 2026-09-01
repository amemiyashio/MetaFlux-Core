#include <metaflux/transport/vfio_user_guest.h>

#include <stdint.h>
#include <string.h>

static uint64_t next_word(uint64_t* state) {
  *state ^= *state << 7U;
  *state ^= *state >> 9U;
  *state ^= *state << 8U;
  return *state;
}

static void fill_bytes(uint8_t* bytes, size_t count, uint64_t* state) {
  size_t index;
  for (index = 0U; index < count; ++index) {
    if ((index & 7U) == 0U) {
      (void)next_word(state);
    }
    bytes[index] = (uint8_t)(*state >> ((index & 7U) * 8U));
  }
}

static void no_op_doorbell(void* context, uint32_t value) {
  (void)context;
  (void)value;
}

static int status_is_defined(mf_shared_status_v1 status) {
  switch (status) {
  case MF_SHARED_SUCCESS:
  case MF_SHARED_WOULD_BLOCK:
  case MF_SHARED_TIMEOUT:
  case MF_SHARED_INTERRUPTED:
  case MF_SHARED_RETRY:
  case MF_SHARED_STALE_HANDLE:
  case MF_SHARED_DEVICE_LOST:
  case MF_SHARED_TERMINAL_VIEW:
  case MF_SHARED_INVALID_ARGUMENT:
  case MF_SHARED_MALFORMED:
  case MF_SHARED_OVERFLOW:
  case MF_SHARED_RESOURCE_EXHAUSTED:
  case MF_SHARED_SYSTEM_ERROR:
  case MF_SHARED_NOT_SUPPORTED:
  case MF_SHARED_PERMISSION_DENIED:
    return 1;
  }
  return 0;
}

static int fuzz_packet_entrypoints(void) {
  uint8_t packet[MF_VFIO_USER_MAX_PACKET_SIZE_V0];
  uint8_t encoded[MF_VFIO_USER_MAX_PACKET_SIZE_V0];
  mf_transport_completion_v0 completion;
  mf_vfio_user_get_info_reply_v0 info;
  mf_transport_negotiate_v0 negotiation;
  mf_vfio_user_dma_map_v0 map;
  mf_vfio_user_dma_unmap_v0 unmap;
  uint32_t output_size;
  uint64_t state = UINT64_C(0x7d4e9a3182c5f607);
  uint32_t seed;

  for (seed = 0U; seed < 4096U; ++seed) {
    const uint32_t size = seed % (MF_VFIO_USER_MAX_PACKET_SIZE_V0 + 1U);
    fill_bytes(packet, sizeof(packet), &state);
    if (!status_is_defined(mf_vfio_user_guest_decode_completion_v0(
            packet, size, next_word(&state), (uint16_t)next_word(&state), &completion)) ||
        !status_is_defined(mf_vfio_user_guest_decode_get_info_v0(
            packet, size, next_word(&state), &info)) ||
        !status_is_defined(mf_vfio_user_guest_decode_negotiate_v0(
            packet, size, next_word(&state), &negotiation))) {
      return 1;
    }

    fill_bytes((uint8_t*)&negotiation, sizeof(negotiation), &state);
    fill_bytes((uint8_t*)&map, sizeof(map), &state);
    fill_bytes((uint8_t*)&unmap, sizeof(unmap), &state);
    output_size = UINT32_MAX;
    if (!status_is_defined(mf_vfio_user_guest_encode_get_info_v0(
            next_word(&state), encoded, seed % (sizeof(encoded) + 1U), &output_size)) ||
        !status_is_defined(mf_vfio_user_guest_encode_negotiate_v0(
            next_word(&state), &negotiation, encoded, size, &output_size)) ||
        !status_is_defined(mf_vfio_user_guest_encode_dma_map_v0(
            next_word(&state), &map, encoded, size, &output_size)) ||
        !status_is_defined(mf_vfio_user_guest_encode_dma_unmap_v0(
            next_word(&state), &unmap, (uint16_t)next_word(&state), encoded, size,
            &output_size))) {
      return 2;
    }
  }
  return 0;
}

static int fuzz_ring_entrypoints(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(41), UINT64_C(43)};
  mf_client_ring_v1 submission_owner;
  mf_client_ring_v1 completion_owner;
  mf_vfio_user_guest_ring_v0 guest;
  mf_ring_descriptor_v1 descriptor;
  mf_ring_descriptor_v1 consumed;
  uint8_t payload[32];
  uint64_t state = UINT64_C(0x1029384756abcdef);
  uint32_t index;
  mf_shared_status_v1 status;

  (void)memset(&submission_owner, 0, sizeof(submission_owner));
  (void)memset(&completion_owner, 0, sizeof(completion_owner));
  (void)memset(&guest, 0, sizeof(guest));
  submission_owner.owned_fd = -1;
  completion_owner.owned_fd = -1;
  guest.submission.owned_fd = -1;
  guest.completion.owned_fd = -1;
  if (mf_client_ring_create_v1(2U, view_id, MF_CLIENT_SUBMISSION_QUEUE_ID_V1, 9U,
                               &submission_owner) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(2U, view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1, 9U,
                               &completion_owner) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_attach_v0(
          mf_client_ring_borrow_fd_v1(&submission_owner),
          mf_client_ring_borrow_fd_v1(&completion_owner), view_id, 9U, payload, sizeof(payload),
          no_op_doorbell, NULL, 1U, &guest) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 1;
  }

  for (index = 0U; index < 4096U; ++index) {
    fill_bytes((uint8_t*)&descriptor, sizeof(descriptor), &state);
    status = mf_vfio_user_guest_ring_submit_v0(&guest, &descriptor);
    if (!status_is_defined(status)) {
      mf_vfio_user_guest_ring_close_v0(&guest);
      mf_client_ring_close_v1(&completion_owner);
      mf_client_ring_close_v1(&submission_owner);
      return 2;
    }
    if (status == MF_SHARED_SUCCESS &&
        mf_client_ring_try_consume_v1(&submission_owner, &consumed) != MF_SHARED_SUCCESS) {
      mf_vfio_user_guest_ring_close_v0(&guest);
      mf_client_ring_close_v1(&completion_owner);
      mf_client_ring_close_v1(&submission_owner);
      return 3;
    }
    if (!status_is_defined(mf_vfio_user_guest_ring_payload_contains_v0(
            &guest, next_word(&state), next_word(&state))) ||
        !status_is_defined(mf_vfio_user_guest_ring_arm_completion_v0(
            &guest, next_word(&state)))) {
      mf_vfio_user_guest_ring_close_v0(&guest);
      mf_client_ring_close_v1(&completion_owner);
      mf_client_ring_close_v1(&submission_owner);
      return 4;
    }
  }
  mf_vfio_user_guest_ring_close_v0(&guest);
  mf_client_ring_close_v1(&completion_owner);
  mf_client_ring_close_v1(&submission_owner);
  return 0;
}

int main(void) {
  const int packet_result = fuzz_packet_entrypoints();
  const int ring_result = packet_result == 0 ? fuzz_ring_entrypoints() : 1;
  return packet_result == 0 && ring_result == 0 ? 0 : 1;
}
