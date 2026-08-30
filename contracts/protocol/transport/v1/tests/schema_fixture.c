#include <stdint.h>
#include <string.h>

#include <metaflux/shared/device.h>
#include <metaflux/transport/generated.h>
#include <metaflux/uapi/transport.h>

int main(void) {
  mf_transport_negotiate_v0 negotiation;
  mf_uapi_queue_v0 queue;
  uint32_t magic;

  if (MF_SCHEMA_MF_RING_DESCRIPTOR_V1_SIZE != sizeof(mf_ring_descriptor_v1) ||
      MF_SCHEMA_MF_RING_DESCRIPTOR_V1_OFFSET_arguments != 32u ||
      MF_SCHEMA_MF_TRANSPORT_NEGOTIATE_V0_SIZE != sizeof(mf_transport_negotiate_v0) ||
      MF_SCHEMA_MF_UAPI_QUEUE_V0_SIZE != sizeof(mf_uapi_queue_v0)) {
    return 1;
  }
  memset(&negotiation, 0, sizeof(negotiation));
  memcpy(&magic, mf_transport_negotiate_golden_v0, sizeof(magic));
  if (magic != UINT32_C(0x3054464d) || negotiation.reserved[0] != 0u ||
      mf_transport_negotiate_golden_v0[6] != 1u ||
      mf_transport_negotiate_golden_v0[8] != 128u ||
      mf_transport_negotiate_golden_v0[16] != 1u) {
    return 1;
  }
  memset(&queue, 0, sizeof(queue));
  queue.struct_size = (uint32_t)sizeof(queue);
  queue.queue_id = UINT64_C(41);
  if (queue.struct_size != 128u || queue.queue_id != UINT64_C(41) ||
      queue.reserved[0] != 0u || MF_UAPI_IOCTL_QUEUE_CREATE == 0) {
    return 1;
  }
  return 0;
}
