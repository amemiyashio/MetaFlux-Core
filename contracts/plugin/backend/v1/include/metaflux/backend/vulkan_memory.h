#ifndef METAFLUX_BACKEND_VULKAN_MEMORY_H
#define METAFLUX_BACKEND_VULKAN_MEMORY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_VULKAN_EXTERNAL_MEMORY_ABI_VERSION_0 UINT32_C(0)

#define MF_VULKAN_MEMORY_TIER_OPAQUE_FD_V0 UINT32_C(1)
#define MF_VULKAN_MEMORY_TIER_DMA_BUF_V0 UINT32_C(2)
#define MF_VULKAN_MEMORY_TIER_STAGING_V0 UINT32_C(3)

#define MF_VULKAN_MEMORY_HANDLE_NONE_V0 UINT32_C(0)
#define MF_VULKAN_MEMORY_HANDLE_OPAQUE_FD_V0 UINT32_C(1)
#define MF_VULKAN_MEMORY_HANDLE_DMA_BUF_V0 UINT32_C(2)

#define MF_VULKAN_MEMORY_SYNC_NONE_V0 UINT32_C(0)
#define MF_VULKAN_MEMORY_SYNC_OPAQUE_FD_V0 UINT32_C(1)
#define MF_VULKAN_MEMORY_SYNC_SEMAPHORE_FD_V0 UINT32_C(2)

#define MF_VULKAN_MEMORY_FLAG_HOST_VISIBLE_V0 UINT32_C(1)
#define MF_VULKAN_MEMORY_FLAG_DEVICE_LOCAL_V0 UINT32_C(2)
#define MF_VULKAN_MEMORY_FLAG_HOST_COHERENT_V0 UINT32_C(4)
#define MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0 UINT32_C(8)
#define MF_VULKAN_MEMORY_FLAG_DEDICATED_ONLY_V0 UINT32_C(16)

typedef struct mf_vulkan_external_memory_profile_v0 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint32_t tier;
  uint32_t handle_type;
  uint32_t sync_type;
  uint32_t flags;
  uint32_t memory_type_bits;
  uint32_t reserved_word;
  uint64_t size;
  uint64_t alignment;
  uint64_t generation;
  uint64_t permissions;
  uint64_t reserved[4];
} mf_vulkan_external_memory_profile_v0;

static inline int mf_vulkan_external_memory_profile_valid_v0(
    const mf_vulkan_external_memory_profile_v0* profile) {
  if (profile == (const mf_vulkan_external_memory_profile_v0*)0 ||
      profile->struct_size != sizeof(*profile) ||
      profile->abi_version != MF_VULKAN_EXTERNAL_MEMORY_ABI_VERSION_0 || profile->size == 0U ||
      profile->alignment == 0U || profile->generation == 0U || profile->memory_type_bits == 0U) {
    return 0;
  }
  if (profile->tier == MF_VULKAN_MEMORY_TIER_STAGING_V0) {
    return profile->handle_type == MF_VULKAN_MEMORY_HANDLE_NONE_V0 &&
           profile->sync_type == MF_VULKAN_MEMORY_SYNC_NONE_V0 &&
           (profile->flags & MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0) == 0U;
  }
  if (profile->tier == MF_VULKAN_MEMORY_TIER_OPAQUE_FD_V0) {
    return profile->handle_type == MF_VULKAN_MEMORY_HANDLE_OPAQUE_FD_V0 &&
           profile->sync_type != MF_VULKAN_MEMORY_SYNC_NONE_V0 &&
           (profile->flags & MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0) != 0U;
  }
  if (profile->tier == MF_VULKAN_MEMORY_TIER_DMA_BUF_V0) {
    return profile->handle_type == MF_VULKAN_MEMORY_HANDLE_DMA_BUF_V0 &&
           profile->sync_type != MF_VULKAN_MEMORY_SYNC_NONE_V0 &&
           (profile->flags & MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0) != 0U;
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(mf_vulkan_external_memory_profile_v0) == 96,
              "Vulkan external memory profile size");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(mf_vulkan_external_memory_profile_v0) == 96,
               "Vulkan external memory profile size");
#endif

#endif
