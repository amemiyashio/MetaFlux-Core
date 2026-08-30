#include <metaflux/transport/vfio_user_guest.h>

#include <stdint.h>
#include <string.h>

int main(void) {
  uint8_t packet[MF_VFIO_USER_MAX_PACKET_SIZE_V0];
  uint32_t packet_size = 0;
  mf_vfio_user_dma_map_v0 map;
  mf_transport_message_header_v0 header;

  if (mf_vfio_user_guest_encode_get_info_v0(UINT64_C(7), packet, sizeof(packet), &packet_size) !=
          MF_SHARED_SUCCESS ||
      packet_size != sizeof(mf_transport_message_header_v0)) {
    return 1;
  }
  (void)memcpy(&header, packet, sizeof(header));
  if (header.message_id != UINT64_C(7) || header.message_type != MF_VFIO_USER_MESSAGE_GET_INFO_V0 ||
      header.payload_size != 0U) {
    return 1;
  }

  (void)memset(&map, 0, sizeof(map));
  map.struct_size = sizeof(map);
  map.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  map.iova = UINT64_C(0x1000);
  map.size = UINT64_C(0x1000);
  map.mapping_epoch = UINT64_C(1);
  map.device_generation = UINT64_C(1);
  map.fd_index = 0;
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_SUCCESS ||
      packet_size != sizeof(mf_transport_message_header_v0) + sizeof(map)) {
    return 1;
  }
  if (mf_vfio_user_guest_encode_dma_unmap_v0(UINT64_C(7), NULL, UINT16_C(0), packet,
                                             sizeof(packet), &packet_size) !=
      MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  return 0;
}
