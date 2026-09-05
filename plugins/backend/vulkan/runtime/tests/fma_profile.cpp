// FP32 FMA throughput probe for the physical Vulkan device: runs one FMA-chain
// compute kernel over host-visible staging buffers, times the dispatch with
// device timestamps plus host walls, and verifies results bit-exactly against
// the scalar fmaf reference. Emitted as METAFLUX_SAMPLE rows via
// benchmark_common.h. Baseline measurement only: no performance-threshold
// assertion.

#include "vulkan_device.hpp"
#include "vulkan_staging.hpp"

#include "benchmark_common.h"

#include <vulkan/vulkan.h>

#include "metaflux/backend/vulkan.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

using metaflux::backend::vulkan::AllocationStatus;
using metaflux::backend::vulkan::DeviceStatus;
using metaflux::backend::vulkan::VulkanDeviceContext;
using metaflux::backend::vulkan::VulkanStagingBuffer;

constexpr std::uint32_t kWorkgroupSize = 256U;
constexpr std::uint32_t kDefaultElements = 1024U * 1024U;
constexpr std::uint32_t kDefaultRounds = 64U;
constexpr std::uint32_t kChains = 16U;
constexpr float kSourceValue = 1.0F;

std::uint64_t steady_ns() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

[[nodiscard]] float scalar_reference(std::uint32_t rounds) noexcept {
  float sum = 0.0F;
  for (std::uint32_t chain = 0U; chain < kChains; ++chain) {
    float x = kSourceValue + static_cast<float>(chain);
    for (std::uint32_t step = 0U; step < rounds; ++step) {
      x = std::fma(x, 0.5F, kSourceValue);
    }
    sum += x;
  }
  return sum;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    (void)std::fprintf(stderr,
                       "usage: %s SPIRV_PATH ELEMENTS FMA_ROUNDS SAMPLES\n", argv[0]);
    return 2;
  }
  const char* const spirv_path = argv[1];
  std::uint32_t elements = kDefaultElements;
  std::uint32_t iterations = kDefaultRounds;
  std::uint32_t sample_count = 3U;
  if (mf_benchmark_parse_u32(argv[2], &elements) != 0 ||
      mf_benchmark_parse_u32(argv[3], &iterations) != 0 ||
      mf_benchmark_parse_u32(argv[4], &sample_count) != 0 || elements == 0U ||
      iterations == 0U || sample_count == 0U || elements % kWorkgroupSize != 0U) {
    (void)std::fprintf(stderr, "invalid arguments\n");
    return 2;
  }

  mf_vulkan_capability_profile_v1 profile{};
  profile.struct_size = static_cast<std::uint32_t>(sizeof(profile));
  if (mf_vulkan_probe_capabilities_v1(&profile) != MF_VULKAN_PROBE_SUCCESS) {
    (void)std::fprintf(stderr, "no qualified physical Vulkan device\n");
    return 1;
  }

  std::vector<std::uint32_t> spirv_words;
  if (std::FILE* spirv_file = std::fopen(spirv_path, "rb")) {
    (void)std::fseek(spirv_file, 0L, SEEK_END);
    const long bytes = std::ftell(spirv_file);
    (void)std::fseek(spirv_file, 0L, SEEK_SET);
    if (bytes > 0 && bytes % 4 == 0) {
      spirv_words.resize(static_cast<std::size_t>(bytes) / sizeof(std::uint32_t));
      if (std::fread(spirv_words.data(), sizeof(std::uint32_t), spirv_words.size(),
                     spirv_file) != spirv_words.size()) {
        spirv_words.clear();
      }
    }
    (void)std::fclose(spirv_file);
  }
  if (spirv_words.empty()) {
    (void)std::fprintf(stderr, "cannot read SPIR-V: %s\n", spirv_path);
    return 1;
  }

  VulkanDeviceContext context(1U);
  if (context.initialize(profile) != DeviceStatus::success) {
    (void)std::fprintf(stderr, "device context initialization failed\n");
    return 1;
  }
  VkDevice device = context.device_handle();

  VulkanStagingBuffer source_buffer(context);
  VulkanStagingBuffer destination_buffer(context);
  const VkDeviceSize buffer_bytes =
      static_cast<VkDeviceSize>(elements) * static_cast<VkDeviceSize>(sizeof(float));
  if (source_buffer.allocate(buffer_bytes, 256U) != AllocationStatus::success ||
      source_buffer.map() != AllocationStatus::success ||
      destination_buffer.allocate(buffer_bytes, 256U) != AllocationStatus::success ||
      destination_buffer.map() != AllocationStatus::success) {
    (void)std::fprintf(stderr, "staging allocation failed\n");
    return 1;
  }
  const auto source_bits = std::bit_cast<std::uint32_t>(kSourceValue);
  for (std::uint32_t index = 0U; index < elements; ++index) {
    std::memcpy(static_cast<float*>(source_buffer.allocation().mapped) + index, &source_bits,
                sizeof(float));
  }
  (void)source_buffer.flush(0U, buffer_bytes);

  VkShaderModuleCreateInfo module_info{};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = static_cast<std::size_t>(spirv_words.size()) * sizeof(std::uint32_t);
  module_info.pCode = spirv_words.data();
  VkShaderModule shader = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  VkQueryPool query_pool = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  const auto destroy_all = [&]() noexcept {
    if (command_buffer != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(device, command_pool, 1U, &command_buffer);
    }
    if (command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device, command_pool, nullptr);
    }
    if (query_pool != VK_NULL_HANDLE) {
      vkDestroyQueryPool(device, query_pool, nullptr);
    }
    if (descriptor_set != VK_NULL_HANDLE && descriptor_pool != VK_NULL_HANDLE) {
      vkFreeDescriptorSets(device, descriptor_pool, 1U, &descriptor_set);
    }
    if (descriptor_pool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    }
    if (set_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
    }
    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(device, pipeline, nullptr);
    }
    if (layout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device, layout, nullptr);
    }
    if (shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, shader, nullptr);
    }
  };

  std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
  bindings[0] = {0U, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1U,
                 VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  bindings[1] = {1U, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1U,
                 VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo set_layout_info{};
  set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set_layout_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
  set_layout_info.pBindings = bindings.data();
  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_range.offset = 0U;
  push_range.size = 8U;
  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 1U;
  layout_info.pSetLayouts = &set_layout;
  layout_info.pushConstantRangeCount = 1U;
  layout_info.pPushConstantRanges = &push_range;
  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = VK_NULL_HANDLE;
  stage.pName = "main";

  VkQueryPoolCreateInfo query_info{};
  query_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  query_info.queryCount = 2U;
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = context.queue_family_index();

  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_size.descriptorCount = 2U;
  VkDescriptorPoolCreateInfo descriptor_pool_info{};
  descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_info.maxSets = 1U;
  descriptor_pool_info.poolSizeCount = 1U;
  descriptor_pool_info.pPoolSizes = &pool_size;

  VkResult result = vkCreateShaderModule(device, &module_info, nullptr, &shader);
  if (result == VK_SUCCESS) {
    stage.module = shader;
  }
  if (result == VK_SUCCESS) {
    result = vkCreateDescriptorSetLayout(device, &set_layout_info, nullptr, &set_layout);
  }
  if (result == VK_SUCCESS) {
    result = vkCreatePipelineLayout(device, &layout_info, nullptr, &layout);
  }
  if (result == VK_SUCCESS) {
    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = stage;
    pipeline_info.layout = layout;
    result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1U, &pipeline_info, nullptr,
                                      &pipeline);
  }
  if (result == VK_SUCCESS) {
    result = vkCreateDescriptorPool(device, &descriptor_pool_info, nullptr, &descriptor_pool);
  }
  if (result == VK_SUCCESS) {
    VkDescriptorSetAllocateInfo set_allocate{};
    set_allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_allocate.descriptorPool = descriptor_pool;
    set_allocate.descriptorSetCount = 1U;
    set_allocate.pSetLayouts = &set_layout;
    result = vkAllocateDescriptorSets(device, &set_allocate, &descriptor_set);
  }
  if (result == VK_SUCCESS) {
    VkDescriptorBufferInfo buffer_infos[2] = {};
    buffer_infos[0] = {destination_buffer.allocation().buffer, 0U, VK_WHOLE_SIZE};
    buffer_infos[1] = {source_buffer.allocation().buffer, 0U, VK_WHOLE_SIZE};
    VkWriteDescriptorSet writes[2] = {};
    writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0U, 0U, 1U,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer_infos[0], nullptr};
    writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 1U, 0U, 1U,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer_infos[1], nullptr};
    vkUpdateDescriptorSets(device, 2U, writes, 0U, nullptr);
  }
  if (result == VK_SUCCESS) {
    result = vkCreateQueryPool(device, &query_info, nullptr, &query_pool);
  }
  if (result == VK_SUCCESS) {
    result = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
  }
  if (result == VK_SUCCESS) {
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1U;
    result = vkAllocateCommandBuffers(device, &allocate_info, &command_buffer);
  }
  if (result != VK_SUCCESS || !source_buffer.ready() || !destination_buffer.ready()) {
    (void)std::fprintf(stderr, "resource creation failed: %d\n", static_cast<int>(result));
    destroy_all();
    return 1;
  }

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(context.physical_device_handle(), &properties);
  const double period_ns = static_cast<double>(properties.limits.timestampPeriod);
  const std::uint32_t groups = elements / kWorkgroupSize;

  const auto record_and_run = [&](std::uint64_t timeline_value, std::uint64_t* dispatch_window,
                                  std::uint64_t* wall) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
      return false;
    }
    vkCmdResetQueryPool(command_buffer, query_pool, 0U, 2U);
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0U);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0U, 1U,
                            &descriptor_set, 0U, nullptr);
    const std::uint32_t push[2] = {elements, iterations};
    vkCmdPushConstants(command_buffer, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0U, 8U, push);
    vkCmdDispatch(command_buffer, groups, 1U, 1U);
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1U);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
      return false;
    }
    const auto wall_started = steady_ns();
    if (context.submit_commands(1U, command_buffer, 0U, timeline_value) !=
        DeviceStatus::success) {
      return false;
    }
    if (context.wait(1U, timeline_value, UINT64_C(10000000000)) != DeviceStatus::success) {
      return false;
    }
    *wall = steady_ns() - wall_started;
    std::uint64_t stamps[2] = {0U, 0U};
    if (vkGetQueryPoolResults(device, query_pool, 0U, 2U, sizeof(stamps), stamps,
                              sizeof(std::uint64_t),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS) {
      return false;
    }
    *dispatch_window = static_cast<std::uint64_t>(static_cast<double>(stamps[1] - stamps[0]) *
                                                  period_ns);
    return true;
  };

  std::uint64_t window = 0U;
  std::uint64_t wall = 0U;
  bool all_ok = true;
  if (!record_and_run(1U, &window, &wall)) {
    (void)std::fprintf(stderr, "first launch failed\n");
    destroy_all();
    return 1;
  }
  (void)destination_buffer.invalidate(0U, buffer_bytes);
  const float expected = scalar_reference(iterations);
  const auto expected_bits = std::bit_cast<std::uint32_t>(expected);
  const auto* results = static_cast<const float*>(destination_buffer.allocation().mapped);
  for (std::uint32_t index = 0U; index < elements; ++index) {
    if (std::bit_cast<std::uint32_t>(results[index]) != expected_bits) {
      (void)std::fprintf(stderr, "element %u mismatch\n", index);
      all_ok = false;
      break;
    }
  }
  if (!all_ok) {
    destroy_all();
    return 1;
  }

  mf_benchmark_emit_metadata_u64("elements", elements);
  mf_benchmark_emit_metadata_u64("fma_rounds", iterations);
  mf_benchmark_emit_metadata_u64("chains", kChains);
  mf_benchmark_emit_metadata_u64(
      "flops_per_launch",
      static_cast<std::uint64_t>(elements) * kChains * iterations * 2U);
  mf_benchmark_emit_metadata_u64("workgroups", groups);
  mf_benchmark_emit_metadata_text("clock", "CLOCK_MONOTONIC_RAW");
  mf_benchmark_emit_metadata_text("device_name", profile.device_name);
  mf_benchmark_emit_metadata_u64(
      "timestamp_period_fs",
      static_cast<std::uint64_t>(properties.limits.timestampPeriod));

  for (std::uint32_t index = 0U; index < sample_count; ++index) {
    if (!record_and_run(2U + index, &window, &wall)) {
      (void)std::fprintf(stderr, "sample launch failed\n");
      all_ok = false;
      break;
    }
    const double flops = static_cast<double>(elements) * static_cast<double>(kChains) *
                         static_cast<double>(iterations) * 2.0;
    mf_benchmark_emit_sample("gpu_fma_dispatch_window_ns", index, window, "ns");
    mf_benchmark_emit_sample("gpu_fma_wall_ns", index, wall, "ns");
    mf_benchmark_emit_sample("gpu_fma_throughput", index,
                             static_cast<std::uint64_t>(flops * 1e9 / static_cast<double>(window)),
                             "flop/s");
    mf_benchmark_emit_sample("gpu_fma_wall_throughput", index,
                             static_cast<std::uint64_t>(flops * 1e9 / static_cast<double>(wall)),
                             "flop/s");
  }

  destroy_all();
  return all_ok ? 0 : 1;
}
