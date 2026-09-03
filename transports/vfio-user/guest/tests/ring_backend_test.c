#include <metaflux/transport/vfio_user_guest.h>
#include <string.h>

typedef struct { uint32_t count; uint32_t last_value; } doorbell_counter;

static void record_doorbell(void* ctx, uint32_t value) {
  doorbell_counter* d = (doorbell_counter*)ctx;
  d->count += UINT32_C(1); d->last_value = value;
}

#define CLEANUP(code) do { \
  mf_vfio_user_guest_ring_close_v0(&guest); \
  mf_client_ring_close_v1(&completion_owner); \
  mf_client_ring_close_v1(&submission_owner); \
  return (code); \
} while (0)

static int test_ring_copy_and_completion(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(7), UINT64_C(13)};
  mf_client_ring_v1 submission_owner, completion_owner;
  mf_vfio_user_guest_ring_v0 guest;
  mf_ring_descriptor_v1 desc, completion;
  uint8_t payload[64];
  doorbell_counter doorbell = {0};

  (void)memset(&submission_owner, 0, sizeof(submission_owner));
  (void)memset(&completion_owner, 0, sizeof(completion_owner));
  (void)memset(&guest, 0, sizeof(guest));
  submission_owner.owned_fd = -1; completion_owner.owned_fd = -1;
  guest.submission.owned_fd = -1; guest.completion.owned_fd = -1;

  if (mf_client_ring_create_v1(UINT32_C(4), view_id, MF_CLIENT_SUBMISSION_QUEUE_ID_V1, UINT64_C(5), &submission_owner) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(UINT32_C(4), view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1, UINT64_C(5), &completion_owner) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_attach_v0(mf_client_ring_borrow_fd_v1(&submission_owner),
        mf_client_ring_borrow_fd_v1(&completion_owner), view_id, UINT64_C(5),
        payload, sizeof(payload), record_doorbell, &doorbell, UINT32_C(0x10), &guest) != MF_SHARED_SUCCESS)
    CLEANUP(1);

  /* Submit two COPY descriptors from guest side. */
  (void)memset(&desc, 0, sizeof(desc));
  desc.opcode = MF_RING_OPCODE_COPY; desc.request_id = UINT64_C(10); desc.target_id = UINT64_C(20);
  if (mf_vfio_user_guest_ring_submit_v0(&guest, &desc) != MF_SHARED_SUCCESS || doorbell.count != UINT32_C(1))
    CLEANUP(2);
  desc.request_id = UINT64_C(11); desc.target_id = UINT64_C(21);
  if (mf_vfio_user_guest_ring_submit_v0(&guest, &desc) != MF_SHARED_SUCCESS || doorbell.count != UINT32_C(2))
    CLEANUP(3);

  /* Consume from server side of submission ring; verify ordering. */
  (void)memset(&desc, 0, sizeof(desc));
  if (mf_client_ring_try_consume_v1(&submission_owner, &desc) != MF_SHARED_SUCCESS ||
      desc.request_id != UINT64_C(10) || desc.target_id != UINT64_C(20) || desc.opcode != MF_RING_OPCODE_COPY)
    CLEANUP(4);
  (void)memset(&desc, 0, sizeof(desc));
  if (mf_client_ring_try_consume_v1(&submission_owner, &desc) != MF_SHARED_SUCCESS ||
      desc.request_id != UINT64_C(11) || desc.target_id != UINT64_C(21))
    CLEANUP(5);
  if (mf_client_ring_try_consume_v1(&submission_owner, &desc) != MF_SHARED_WOULD_BLOCK)
    CLEANUP(6);

  /* Produce completions from server side. */
  (void)memset(&completion, 0, sizeof(completion));
  completion.opcode = MF_RING_OPCODE_COMPLETION;
  completion.request_id = UINT64_C(10); completion.target_id = UINT64_C(20);
  completion.arguments[1] = UINT64_C(1);
  if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS)
    CLEANUP(7);
  completion.request_id = UINT64_C(11); completion.target_id = UINT64_C(21);
  completion.arguments[1] = UINT64_C(2);
  if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS)
    CLEANUP(7);

  /* Guest consumes completions; verify timeline advances. */
  (void)memset(&desc, 0, sizeof(desc));
  if (mf_vfio_user_guest_ring_try_consume_v0(&guest, &desc) != MF_SHARED_SUCCESS ||
      desc.opcode != MF_RING_OPCODE_COMPLETION || desc.request_id != UINT64_C(10) ||
      desc.arguments[1] != UINT64_C(1))
    CLEANUP(8);
  if (mf_vfio_user_guest_ring_last_completion_timeline_v0(&guest) != UINT64_C(1))
    CLEANUP(9);
  if (mf_vfio_user_guest_ring_try_consume_v0(&guest, &desc) != MF_SHARED_SUCCESS ||
      desc.request_id != UINT64_C(11) || desc.arguments[1] != UINT64_C(2))
    CLEANUP(10);
  if (mf_vfio_user_guest_ring_last_completion_timeline_v0(&guest) != UINT64_C(2))
    CLEANUP(11);
  if (mf_vfio_user_guest_ring_try_consume_v0(&guest, &desc) != MF_SHARED_WOULD_BLOCK)
    CLEANUP(12);

  CLEANUP(0);
}

#undef CLEANUP

int main(void) {
  return test_ring_copy_and_completion();
}
