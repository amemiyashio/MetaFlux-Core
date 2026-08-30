#include "metaflux/transport/cdev.h"

#include <stdint.h>
#include <string.h>

int main(void) {
  mf_cdev_copy_v0 copy = {
      .destination_offset = 128U,
      .source_offset = 256U,
      .byte_count = 64U,
  };
  mf_ring_descriptor_v1 descriptor;
  if (mf_cdev_copy_descriptor_v0(UINT64_C(9), UINT64_C(3), &copy, &descriptor) !=
          MF_SHARED_SUCCESS ||
      descriptor.opcode != MF_RING_OPCODE_COPY || descriptor.request_id != UINT64_C(9) ||
      descriptor.target_id != UINT64_C(3) || descriptor.arguments[0] != UINT64_C(128) ||
      descriptor.arguments[1] != UINT64_C(256) || descriptor.arguments[2] != UINT64_C(64) ||
      mf_cdev_copy_descriptor_v0(UINT64_C(0), UINT64_C(3), &copy, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_copy_descriptor_v0(UINT64_C(1), UINT64_C(1), NULL, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  return 0;
}
