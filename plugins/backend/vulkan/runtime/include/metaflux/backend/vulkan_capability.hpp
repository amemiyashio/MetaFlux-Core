#ifndef METAFLUX_BACKEND_VULKAN_CAPABILITY_HPP
#define METAFLUX_BACKEND_VULKAN_CAPABILITY_HPP

#include "metaflux/backend/vulkan.h"

namespace metaflux::backend::vulkan {

[[nodiscard]] inline mf_vulkan_probe_status_v1 probe(
    mf_vulkan_capability_profile_v1* profile) noexcept {
  return mf_vulkan_probe_capabilities_v1(profile);
}

} // namespace metaflux::backend::vulkan

#endif
