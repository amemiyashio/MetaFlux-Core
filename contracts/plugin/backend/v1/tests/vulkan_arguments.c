#include "metaflux/backend/vulkan_arguments.h"

#include <stdint.h>
#include <string.h>

typedef struct argument_fixture {
  mf_vulkan_argument_block_header_v1 header;
  mf_vulkan_argument_entry_v1 entries[2];
} argument_fixture;

int main(void) {
  argument_fixture fixture = {0};
  uint8_t digest[32] = {0};
  uint64_t byte_count = 0U;
  if (mf_vulkan_argument_block_size_v1(2U, &byte_count) != MF_VULKAN_ARGUMENT_VALID ||
      byte_count != sizeof(fixture)) {
    return 1;
  }
  fixture.header.magic = MF_VULKAN_ARGUMENT_BLOCK_MAGIC_V1;
  fixture.header.abi_version = MF_VULKAN_ARGUMENT_ABI_VERSION_1;
  fixture.header.header_size = sizeof(fixture.header);
  fixture.header.entry_size = sizeof(fixture.entries[0]);
  fixture.header.entry_count = 2U;
  fixture.header.flags = MF_VULKAN_ARGUMENT_BLOCK_FLAG_BDA_V1;
  fixture.header.total_size = byte_count;
  for (uint32_t index = 0U; index < sizeof(digest); ++index) {
    digest[index] = (uint8_t)(index + 1U);
  }
  memcpy(fixture.header.target_digest, digest, sizeof(digest));
  fixture.entries[0].kind = MF_VULKAN_ARGUMENT_KIND_SCALAR_V1;
  fixture.entries[0].flags = MF_VULKAN_ARGUMENT_FLAG_READONLY_V1;
  fixture.entries[0].value = UINT64_C(7);
  fixture.entries[1].kind = MF_VULKAN_ARGUMENT_KIND_DEVICE_ADDRESS_V1;
  fixture.entries[1].flags = MF_VULKAN_ARGUMENT_FLAG_WRITE_V1;
  fixture.entries[1].byte_size = UINT64_C(4096);
  fixture.entries[1].value = UINT64_C(0x100000);
  fixture.entries[1].generation = UINT64_C(3);
  if (mf_vulkan_argument_block_validate_v1((const uint8_t*)&fixture, sizeof(fixture), digest) !=
      MF_VULKAN_ARGUMENT_VALID) {
    return 2;
  }
  fixture.entries[1].generation = 0U;
  if (mf_vulkan_argument_block_validate_v1((const uint8_t*)&fixture, sizeof(fixture), digest) !=
      MF_VULKAN_ARGUMENT_INVALID_ENTRY) {
    return 3;
  }
  fixture.entries[1].generation = 3U;
  digest[0] ^= 0xffU;
  return mf_vulkan_argument_block_validate_v1((const uint8_t*)&fixture, sizeof(fixture), digest) ==
                 MF_VULKAN_ARGUMENT_TARGET_MISMATCH
             ? 0
             : 4;
}
