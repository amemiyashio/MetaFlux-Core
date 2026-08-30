#include <cstdint>
#include <cstring>

#include <metaflux/shared/device.h>
#include <metaflux/transport/generated.h>

int main() {
  static_assert(sizeof(mf_transport_message_header_v0) == 16);
  static_assert(alignof(mf_transport_completion_v0) == 8);
  mf_transport_message_header_v0 header{};
  header.message_id = UINT64_C(17);
  header.message_type = 3;
  header.payload_size = 64;
  if (header.message_id != UINT64_C(17) || header.message_type != 3 ||
      header.payload_size != 64 || MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0 == 0) {
    return 1;
  }
  mf_transport_bar_layout_v0 bars{};
  bars.bar0_size = UINT64_C(65536);
  bars.bar2_size = UINT64_C(4096);
  bars.bar4_size = UINT64_C(4096);
  bars.msix_vectors = 2;
  mf_vfio_user_get_info_reply_v0 info{};
  info.status = MF_SHARED_SUCCESS;
  info.bar0_size = bars.bar0_size;
  info.bar2_size = bars.bar2_size;
  info.bar4_size = bars.bar4_size;
  info.msix_vectors = bars.msix_vectors;
  return bars.reserved[0] == 0 && info.reserved[0] == 0 ? 0 : 1;
}
