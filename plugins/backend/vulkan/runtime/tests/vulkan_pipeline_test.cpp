#include "../src/vulkan_pipeline.hpp"
#include "../src/vulkan_pipeline_cache.hpp"

#include <metaflux/backend/vulkan_capability.hpp>
#include <metaflux/backend/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool invalid_guards() {
  using metaflux::backend::vulkan::PipelineStatus;
  using metaflux::backend::vulkan::VulkanComputePipeline;
  metaflux::backend::vulkan::VulkanDeviceContext context(7U);
  VulkanComputePipeline pipeline(context);
  const std::uint32_t invalid_word = 0U;
  return pipeline.create({}, "main", VK_NULL_HANDLE) == PipelineStatus::not_ready &&
         pipeline.create(std::span<const std::uint32_t>(&invalid_word, 1U), "main",
                         VK_NULL_HANDLE) == PipelineStatus::not_ready &&
         std::string(metaflux::backend::vulkan::pipeline_status_string(
             PipelineStatus::compile_required)) == "compile-required";
}

bool invalid_cache_guard() {
  using metaflux::backend::vulkan::PipelineCacheStatus;
  using metaflux::backend::vulkan::VulkanPipelineCache;
  metaflux::backend::vulkan::VulkanDeviceContext context(7U);
  VulkanPipelineCache cache(context);
  const std::array<std::uint8_t, 4> malformed{0U, 0U, 0U, 0U};
  return cache.create(malformed) == PipelineCacheStatus::not_ready &&
         std::string(metaflux::backend::vulkan::pipeline_cache_status_string(
             PipelineCacheStatus::corrupt)) == "corrupt";
}

#ifdef METAFLUX_VULKAN_PIPELINE_FIXTURE
bool load_fixture(std::vector<std::uint32_t>* out_words) {
  if (out_words == nullptr) {
    return false;
  }
  std::ifstream input(METAFLUX_VULKAN_PIPELINE_FIXTURE, std::ios::binary);
  if (!input) {
    return false;
  }
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0U) {
    return false;
  }
  out_words->resize(bytes.size() / sizeof(std::uint32_t));
  std::copy(bytes.begin(), bytes.end(), reinterpret_cast<char*>(out_words->data()));
  return !out_words->empty() && out_words->front() == UINT32_C(0x07230203);
}

bool physical_pipeline_round_trip() {
  using metaflux::backend::vulkan::PipelineStatus;
  using metaflux::backend::vulkan::VulkanComputePipeline;
  using metaflux::backend::vulkan::VulkanDeviceContext;
  using metaflux::backend::vulkan::VulkanPipelineCache;
  mf_vulkan_capability_profile_v1 profile{};
  const auto probe_status = metaflux::backend::vulkan::probe(&profile);
  if (probe_status == MF_VULKAN_PROBE_LOADER_UNAVAILABLE ||
      probe_status == MF_VULKAN_PROBE_NO_DEVICE ||
      probe_status == MF_VULKAN_PROBE_UNSUPPORTED_DEVICE) {
    std::printf("vulkan pipeline: host probe unavailable (%s)\n",
                mf_vulkan_probe_status_string_v1(probe_status));
    return true;
  }
  if (probe_status != MF_VULKAN_PROBE_SUCCESS) {
    return false;
  }
  VulkanDeviceContext context(7U);
  const auto initialized = context.initialize(profile);
  if (initialized == metaflux::backend::vulkan::DeviceStatus::no_device ||
      initialized == metaflux::backend::vulkan::DeviceStatus::unsupported_features) {
    std::printf("vulkan pipeline: context unavailable (%s)\n",
                metaflux::backend::vulkan::device_status_string(initialized));
    return true;
  }
  if (initialized != metaflux::backend::vulkan::DeviceStatus::success) {
    return false;
  }
  std::vector<std::uint32_t> words;
  if (!load_fixture(&words)) {
    return false;
  }
  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  if (vkCreatePipelineLayout(context.device_handle(), &layout_info, nullptr, &layout) != VK_SUCCESS) {
    return false;
  }
  VulkanComputePipeline pipeline(context);
  VulkanPipelineCache cache(context);
  if (cache.create() != metaflux::backend::vulkan::PipelineCacheStatus::success) {
    vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
    return false;
  }
  const auto status = pipeline.create(words, "main", layout, cache.handle());
  const bool ready = status == PipelineStatus::success && pipeline.ready();
  if (ready) {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = context.queue_family_index();
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    const bool pool_ready = vkCreateCommandPool(context.device_handle(), &pool_info, nullptr, &pool) ==
                            VK_SUCCESS;
    if (pool_ready) {
      VkCommandBufferAllocateInfo allocation{};
      allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocation.commandPool = pool;
      allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocation.commandBufferCount = 1U;
      if (vkAllocateCommandBuffers(context.device_handle(), &allocation, &command_buffer) != VK_SUCCESS) {
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      VkCommandBufferBeginInfo begin{};
      begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      const bool recorded = vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS &&
                            pipeline.bind(command_buffer) == PipelineStatus::success;
      const bool ended = recorded && vkEndCommandBuffer(command_buffer) == VK_SUCCESS;
      const auto submitted = ended ? context.submit_commands(7U, command_buffer, 0U, 1U)
                                   : metaflux::backend::vulkan::DeviceStatus::initialization_failed;
      const auto waited = submitted == metaflux::backend::vulkan::DeviceStatus::success
                              ? context.wait(7U, 1U, UINT64_C(5000000000))
                              : submitted;
      vkDestroyCommandPool(context.device_handle(), pool, nullptr);
      if (waited != metaflux::backend::vulkan::DeviceStatus::success) {
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
    } else {
      vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
      return false;
    }
  }
  std::vector<std::uint8_t> cache_bytes;
  const auto exported = cache.export_data(&cache_bytes);
  if (ready && exported != metaflux::backend::vulkan::PipelineCacheStatus::success) {
    vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
    return false;
  }
  if (ready && !cache_bytes.empty()) {
    pipeline.destroy();
    VulkanPipelineCache imported(context);
    if (imported.create(cache_bytes) !=
            metaflux::backend::vulkan::PipelineCacheStatus::success ||
        pipeline.create(words, "main", layout, imported.handle()) != PipelineStatus::success) {
      vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
      return false;
    }
  }
  vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
  if (!ready) {
    std::fprintf(stderr, "vulkan pipeline creation failed: %s\n",
                 metaflux::backend::vulkan::pipeline_status_string(status));
  }
  return ready;
}
#endif

} // namespace

int main() {
  if (!invalid_guards() || !invalid_cache_guard()) {
    return 1;
  }
#ifdef METAFLUX_VULKAN_PIPELINE_FIXTURE
  if (!physical_pipeline_round_trip()) {
    return 2;
  }
#else
  std::printf("vulkan pipeline: glslangValidator unavailable; physical fixture skipped\n");
#endif
  return 0;
}
