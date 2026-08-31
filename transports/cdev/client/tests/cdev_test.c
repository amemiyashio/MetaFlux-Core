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
  mf_cdev_memory_v0 memory = {.device_fd = -1};
  mf_cdev_session_v0 session;
  mf_cdev_copy_region_v0 copy_region = {
      .argument_block_id = 71U,
      .argument_block_generation = 73U,
  };
  session.device_fd = 0;
  mf_cdev_launch_v0 launch = {
      .module_id = 11U,
      .module_generation = 13U,
      .argument_block_id = 17U,
      .argument_block_generation = 19U,
  };
  if (mf_cdev_copy_descriptor_v0(UINT64_C(9), UINT64_C(3), &copy, &descriptor) !=
          MF_SHARED_SUCCESS ||
      descriptor.opcode != MF_RING_OPCODE_COPY || descriptor.request_id != UINT64_C(9) ||
      descriptor.target_id != UINT64_C(3) || descriptor.arguments[0] != UINT64_C(128) ||
      descriptor.arguments[1] != UINT64_C(256) || descriptor.arguments[2] != UINT64_C(64) ||
      mf_cdev_copy_region_descriptor_v0(UINT64_C(10), &copy_region, &descriptor) !=
          MF_SHARED_SUCCESS ||
      descriptor.opcode != MF_RING_OPCODE_COPY ||
      descriptor.flags != MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1 ||
      descriptor.request_id != UINT64_C(10) || descriptor.target_id != UINT64_C(71) ||
      descriptor.arguments[0] != UINT64_C(73) || descriptor.arguments[1] != UINT64_C(0) ||
      descriptor.arguments[2] != UINT64_C(0) || descriptor.arguments[3] != UINT64_C(0) ||
      mf_cdev_launch_descriptor_v0(UINT64_C(21), UINT64_C(23), &launch, &descriptor) !=
          MF_SHARED_SUCCESS ||
      descriptor.opcode != MF_RING_OPCODE_LAUNCH || descriptor.flags != 0U ||
      descriptor.request_id != UINT64_C(21) || descriptor.target_id != UINT64_C(23) ||
      descriptor.arguments[0] != UINT64_C(11) || descriptor.arguments[1] != UINT64_C(13) ||
      descriptor.arguments[2] != UINT64_C(17) || descriptor.arguments[3] != UINT64_C(19) ||
      mf_cdev_launch_descriptor_v0(UINT64_C(0), UINT64_C(23), &launch, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_launch_descriptor_v0(UINT64_C(1), UINT64_C(1), NULL, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_copy_descriptor_v0(UINT64_C(0), UINT64_C(3), &copy, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_copy_descriptor_v0(UINT64_C(1), UINT64_C(1), NULL, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_copy_region_descriptor_v0(UINT64_C(0), &copy_region, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_copy_region_descriptor_v0(UINT64_C(1), NULL, &descriptor) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_alloc_v0(NULL, UINT64_C(4096), UINT64_C(4096), &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_alloc_v0((mf_cdev_session_v0*)0, UINT64_C(0), UINT64_C(4096), &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(NULL, &session, UINT64_C(4096),
                                 MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0, &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(&session, NULL, UINT64_C(4096),
                                 MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0, &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(&session, &session, UINT64_C(4096), 0U, &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(&session, &session, UINT64_C(4096), UINT32_C(4), &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(&session, &session, UINT64_C(0),
                                 MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0, &memory) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(&session, &session, UINT64_C(4096),
                                 MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0, NULL) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_cdev_memory_register_v0(&session, &session, UINT64_C(4096),
                                 MF_CDEV_MEMORY_REGISTER_FLAG_READ_V0, &memory) !=
          MF_SHARED_NOT_SUPPORTED ||
      mf_cdev_session_open_with_eventfds_v0("/dev/null", -2, -1, &session) !=
          MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  mf_cdev_memory_close_v0(&memory);
  return 0;
}
