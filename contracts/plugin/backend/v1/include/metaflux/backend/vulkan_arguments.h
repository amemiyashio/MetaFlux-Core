#ifndef METAFLUX_BACKEND_VULKAN_ARGUMENTS_H
#define METAFLUX_BACKEND_VULKAN_ARGUMENTS_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_VULKAN_ARGUMENT_ABI_VERSION_1 UINT32_C(1)
#define MF_VULKAN_ARGUMENT_BLOCK_MAGIC_V1 UINT32_C(0x4d564142)
#define MF_VULKAN_ARGUMENT_BLOCK_FLAG_BDA_V1 UINT32_C(1)
#define MF_VULKAN_ARGUMENT_BLOCK_KNOWN_FLAGS_V1 MF_VULKAN_ARGUMENT_BLOCK_FLAG_BDA_V1
#define MF_VULKAN_ARGUMENT_ENTRY_COUNT_MAX_V1 UINT32_C(1024)

#define MF_VULKAN_ARGUMENT_KIND_SCALAR_V1 UINT32_C(1)
#define MF_VULKAN_ARGUMENT_KIND_DEVICE_ADDRESS_V1 UINT32_C(2)

#define MF_VULKAN_ARGUMENT_FLAG_READONLY_V1 UINT32_C(1)
#define MF_VULKAN_ARGUMENT_FLAG_WRITE_V1 UINT32_C(2)
#define MF_VULKAN_ARGUMENT_KNOWN_FLAGS_V1 \
  (MF_VULKAN_ARGUMENT_FLAG_READONLY_V1 | MF_VULKAN_ARGUMENT_FLAG_WRITE_V1)

typedef enum mf_vulkan_argument_status_v1 {
  MF_VULKAN_ARGUMENT_VALID = 0,
  MF_VULKAN_ARGUMENT_INVALID_ARGUMENT = 1,
  MF_VULKAN_ARGUMENT_INVALID_SIZE = 2,
  MF_VULKAN_ARGUMENT_INVALID_HEADER = 3,
  MF_VULKAN_ARGUMENT_INVALID_ENTRY = 4,
  MF_VULKAN_ARGUMENT_TARGET_MISMATCH = 5,
} mf_vulkan_argument_status_v1;

typedef struct mf_vulkan_argument_block_header_v1 {
  uint32_t magic;
  uint32_t abi_version;
  uint32_t header_size;
  uint32_t entry_size;
  uint32_t entry_count;
  uint32_t flags;
  uint64_t total_size;
  uint8_t target_digest[32];
} mf_vulkan_argument_block_header_v1;

typedef struct mf_vulkan_argument_entry_v1 {
  uint32_t kind;
  uint32_t flags;
  uint64_t byte_offset;
  uint64_t byte_size;
  uint64_t value;
  uint64_t generation;
  uint64_t reserved;
} mf_vulkan_argument_entry_v1;

static inline mf_vulkan_argument_status_v1 mf_vulkan_argument_block_size_v1(
    uint32_t entry_count, uint64_t* out_size) {
  if (out_size == (uint64_t*)0 || entry_count > MF_VULKAN_ARGUMENT_ENTRY_COUNT_MAX_V1) {
    return MF_VULKAN_ARGUMENT_INVALID_SIZE;
  }
  *out_size = (uint64_t)sizeof(mf_vulkan_argument_block_header_v1) +
              ((uint64_t)entry_count * (uint64_t)sizeof(mf_vulkan_argument_entry_v1));
  return MF_VULKAN_ARGUMENT_VALID;
}

static inline mf_vulkan_argument_status_v1 mf_vulkan_argument_block_validate_v1(
    const uint8_t* bytes, uint64_t byte_count, const uint8_t* expected_target_digest) {
  if (bytes == (const uint8_t*)0 || byte_count < sizeof(mf_vulkan_argument_block_header_v1)) {
    return MF_VULKAN_ARGUMENT_INVALID_ARGUMENT;
  }

  mf_vulkan_argument_block_header_v1 header;
  memcpy(&header, bytes, sizeof(header));
  uint64_t expected_size = 0U;
  if (mf_vulkan_argument_block_size_v1(header.entry_count, &expected_size) !=
          MF_VULKAN_ARGUMENT_VALID ||
      header.magic != MF_VULKAN_ARGUMENT_BLOCK_MAGIC_V1 ||
      header.abi_version != MF_VULKAN_ARGUMENT_ABI_VERSION_1 ||
      header.header_size != sizeof(mf_vulkan_argument_block_header_v1) ||
      header.entry_size != sizeof(mf_vulkan_argument_entry_v1) ||
      header.flags == 0U ||
      (header.flags & ~MF_VULKAN_ARGUMENT_BLOCK_KNOWN_FLAGS_V1) != 0U ||
      header.total_size != expected_size || byte_count != expected_size) {
    return MF_VULKAN_ARGUMENT_INVALID_HEADER;
  }
  if (expected_target_digest != (const uint8_t*)0 &&
      memcmp(header.target_digest, expected_target_digest, sizeof(header.target_digest)) != 0) {
    return MF_VULKAN_ARGUMENT_TARGET_MISMATCH;
  }

  const uint8_t* entry_bytes = bytes + sizeof(header);
  for (uint32_t index = 0U; index < header.entry_count; ++index) {
    mf_vulkan_argument_entry_v1 entry;
    memcpy(&entry, entry_bytes + ((uint64_t)index * sizeof(entry)), sizeof(entry));
    if ((entry.flags & ~MF_VULKAN_ARGUMENT_KNOWN_FLAGS_V1) != 0U ||
        (entry.flags & MF_VULKAN_ARGUMENT_KNOWN_FLAGS_V1) == 0U || entry.reserved != 0U) {
      return MF_VULKAN_ARGUMENT_INVALID_ENTRY;
    }
    if (entry.kind == MF_VULKAN_ARGUMENT_KIND_SCALAR_V1) {
      if (entry.byte_offset != 0U || entry.byte_size != 0U || entry.generation != 0U) {
        return MF_VULKAN_ARGUMENT_INVALID_ENTRY;
      }
    } else if (entry.kind == MF_VULKAN_ARGUMENT_KIND_DEVICE_ADDRESS_V1) {
      if (entry.value == 0U || entry.byte_size == 0U || entry.generation == 0U) {
        return MF_VULKAN_ARGUMENT_INVALID_ENTRY;
      }
    } else {
      return MF_VULKAN_ARGUMENT_INVALID_ENTRY;
    }
  }
  return MF_VULKAN_ARGUMENT_VALID;
}

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(mf_vulkan_argument_block_header_v1) == 64,
              "Vulkan argument block header size");
static_assert(sizeof(mf_vulkan_argument_entry_v1) == 48, "Vulkan argument entry size");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(mf_vulkan_argument_block_header_v1) == 64,
               "Vulkan argument block header size");
_Static_assert(sizeof(mf_vulkan_argument_entry_v1) == 48, "Vulkan argument entry size");
#endif

#endif
