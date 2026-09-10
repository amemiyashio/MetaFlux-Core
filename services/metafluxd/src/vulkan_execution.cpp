#include "vulkan_execution.hpp"

#include <mutex>
#include <new>
#include <vector>

#if METAFLUX_DAEMON_VULKAN_EXECUTION

#include "metaflux/backend/vulkan_lowering.hpp"
#include "metaflux/backend/vulkan_capability.hpp"
#include "vulkan_device.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_staging.hpp"

#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>

namespace metaflux::service {
namespace {

constexpr std::uint32_t kWorkgroupSizeX = 128U;
constexpr std::uint64_t kLaunchTimeoutNs = UINT64_C(10000000000);

} // namespace

struct VulkanKernelModule::State final {
  backend::vulkan::VulkanDeviceContext* context = nullptr;
  backend::vulkan::SpirvLoweredModule lowered{};
  std::unique_ptr<backend::vulkan::VulkanComputePipeline> pipeline;
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  std::vector<std::unique_ptr<backend::vulkan::VulkanStagingBuffer>> staging{};
  std::uint32_t argument_count = 0U;
};

struct VulkanExecutionRoute::State final {
  mf_vulkan_capability_profile_v1 profile{};
  backend::vulkan::VulkanDeviceContext context{1U};
  std::mutex launch_mutex;
  std::atomic<std::uint64_t> timeline_value{1U};
};

VulkanExecutionRoute::VulkanExecutionRoute() : state_(std::make_unique<State>()) {}
VulkanExecutionRoute::~VulkanExecutionRoute() {
  if (state_ != nullptr) {
    state_->context.reset();
  }
}

bool VulkanExecutionRoute::ready() const noexcept { return state_->context.ready(); }

std::unique_ptr<VulkanKernelModule> VulkanExecutionRoute::prepare(const compiler::Kernel& kernel,
                                                                  std::string& diagnostic) {
  auto module = std::make_unique<VulkanKernelModule>();
  VulkanKernelModule::State& state = *module->state();
  const auto lowered = backend::vulkan::lower_kernel(kernel, state_->profile,
                                                     {kWorkgroupSizeX, 1U, 1U},
                                                     &state.lowered);
  if (lowered.status != backend::vulkan::LoweringStatus::success) {
    diagnostic = std::string("vulkan lowering: ") +
                 backend::vulkan::lowering_status_string(lowered.status);
    if (!lowered.diagnostic.empty()) {
      diagnostic += ": " + lowered.diagnostic;
    }
    return nullptr;
  }
  state.argument_count = state.lowered.reflection.argument_count;
  if (state.argument_count == 0U || state.argument_count > 8U) {
    diagnostic = "vulkan lowering: unsupported argument count";
    return nullptr;
  }

  backend::vulkan::VulkanDeviceContext& context = state_->context;
  state.context = &context;
  VkDevice device = context.device_handle();
  std::array<VkDescriptorSetLayoutBinding, 8> bindings{};
  for (std::uint32_t index = 0U; index < state.argument_count; ++index) {
    bindings[index].binding = index;
    bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[index].descriptorCount = 1U;
    bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  }
  {
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = state.argument_count;
    info.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device, &info, nullptr, &state.set_layout) != VK_SUCCESS) {
      diagnostic = "vulkan pipeline: descriptor set layout creation failed";
      return nullptr;
    }
  }
  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 1U;
  layout_info.pSetLayouts = &state.set_layout;
  if (vkCreatePipelineLayout(device, &layout_info, nullptr, &state.layout) != VK_SUCCESS) {
    diagnostic = "vulkan pipeline: layout creation failed";
    return nullptr;
  }

  state.pipeline = std::make_unique<backend::vulkan::VulkanComputePipeline>(context);
  const auto created = state.pipeline->create_validated(
      state_->profile, state.lowered.requirements, state.lowered.reflection,
      state.lowered.spirv_binary, state.layout);
  if (created != backend::vulkan::PipelineStatus::success) {
    diagnostic = std::string("vulkan pipeline: ") +
                 backend::vulkan::pipeline_status_string(created);
    return nullptr;
  }

  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_size.descriptorCount = state.argument_count;
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1U;
  pool_info.poolSizeCount = 1U;
  pool_info.pPoolSizes = &pool_size;
  if (vkCreateDescriptorPool(device, &pool_info, nullptr, &state.descriptor_pool) != VK_SUCCESS) {
    diagnostic = "vulkan pipeline: descriptor pool creation failed";
    return nullptr;
  }

  VkCommandPoolCreateInfo command_pool_info{};
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_info.queueFamilyIndex = context.queue_family_index();
  if (vkCreateCommandPool(device, &command_pool_info, nullptr, &state.command_pool) != VK_SUCCESS) {
    diagnostic = "vulkan pipeline: command pool creation failed";
    return nullptr;
  }
  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = state.command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1U;
  if (vkAllocateCommandBuffers(device, &allocate_info, &state.command_buffer) != VK_SUCCESS) {
    diagnostic = "vulkan pipeline: command buffer allocation failed";
    return nullptr;
  }
  state.staging.resize(state.argument_count);
  return module;
}

bool VulkanExecutionRoute::launch(VulkanKernelModule& module,
                                  std::span<const VulkanLaunchBuffer> buffers,
                                  std::uint32_t element_count, std::string& diagnostic) {
  VulkanKernelModule::State& state = *module.state();
  if (state.pipeline == nullptr || !state.pipeline->ready() ||
      buffers.size() < state.argument_count) {
    diagnostic = "vulkan launch: module is not dispatchable pipeline=" +
                 std::string(state.pipeline != nullptr ? "set" : "null") +
                 " ready=" + std::string(state.pipeline != nullptr && state.pipeline->ready() ? "1" : "0") +
                 " buffers=" + std::to_string(buffers.size()) +
                 " args=" + std::to_string(state.argument_count);
    return false;
  }
  std::lock_guard<std::mutex> lock(state_->launch_mutex);
  backend::vulkan::VulkanDeviceContext& context = state_->context;
  VkDevice device = context.device_handle();

  // The lowered shader binds kernel parameters in the template's order; the
  // reflection roles map each binding to the feeding Kernel IR parameter
  // index and mark the single copy-back output.
  const auto& sources = state.lowered.reflection.argument_sources;
  const auto& roles = state.lowered.reflection.argument_roles;
  for (std::uint32_t binding = 0; binding < state.argument_count; ++binding) {
    const std::uint8_t source = sources[binding];
    if (source >= buffers.size()) {
      diagnostic = "vulkan launch: binding source exceeds the launch arguments";
      return false;
    }
    const std::size_t bytes = buffers[source].size_bytes;
    if (state.staging[binding] == nullptr || !state.staging[binding]->ready() ||
        state.staging[binding]->allocation().requested_size < static_cast<VkDeviceSize>(bytes)) {
      auto buffer = std::make_unique<backend::vulkan::VulkanStagingBuffer>(context);
      if (buffer->allocate(static_cast<VkDeviceSize>(bytes), 256U) !=
              backend::vulkan::AllocationStatus::success ||
          buffer->map() != backend::vulkan::AllocationStatus::success) {
        diagnostic = "vulkan launch: staging allocation failed";
        return false;
      }
      state.staging[binding] = std::move(buffer);
    }
    void* mapped = state.staging[binding]->allocation().mapped;
    if (mapped == nullptr) {
      diagnostic = "vulkan launch: staging is not mapped";
      return false;
    }
    std::memcpy(mapped, buffers[source].bytes, bytes);
    (void)state.staging[binding]->flush(0U, static_cast<VkDeviceSize>(bytes));
  }

  (void)vkResetDescriptorPool(device, state.descriptor_pool, 0);
  VkDescriptorSet set_handle = VK_NULL_HANDLE;
  VkDescriptorSetAllocateInfo set_allocate{};
  set_allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  set_allocate.descriptorPool = state.descriptor_pool;
  set_allocate.descriptorSetCount = 1U;
  set_allocate.pSetLayouts = &state.set_layout;
  if (vkAllocateDescriptorSets(device, &set_allocate, &set_handle) != VK_SUCCESS) {
    diagnostic = "vulkan launch: descriptor set allocation failed";
    return false;
  }
  std::array<VkDescriptorBufferInfo, 8> buffer_infos{};
  std::array<VkWriteDescriptorSet, 8> writes{};
  for (std::uint32_t index = 0; index < state.argument_count; ++index) {
    buffer_infos[index] = VkDescriptorBufferInfo{state.staging[index]->allocation().buffer, 0U,
                                                 VK_WHOLE_SIZE};
    writes[index] = VkWriteDescriptorSet{};
    writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[index].dstSet = set_handle;
    writes[index].dstBinding = index;
    writes[index].descriptorCount = 1U;
    writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[index].pBufferInfo = &buffer_infos[index];
  }
  vkUpdateDescriptorSets(device, state.argument_count, writes.data(), 0U, nullptr);

  const std::uint64_t timeline = state_->timeline_value.fetch_add(1U, std::memory_order_relaxed);
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(state.command_buffer, &begin_info) != VK_SUCCESS) {
    diagnostic = "vulkan launch: command buffer begin failed";
    return false;
  }
  vkCmdBindDescriptorSets(state.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.layout,
                          0U, 1U, &set_handle, 0U, nullptr);
  if (state.pipeline->bind(state.command_buffer) != backend::vulkan::PipelineStatus::success) {
    (void)vkEndCommandBuffer(state.command_buffer);
    diagnostic = "vulkan launch: bind failed";
    return false;
  }
  const std::uint32_t groups = (element_count + kWorkgroupSizeX - 1U) / kWorkgroupSizeX;
  vkCmdDispatch(state.command_buffer, groups == 0U ? 1U : groups, 1U, 1U);
  if (vkEndCommandBuffer(state.command_buffer) != VK_SUCCESS) {
    diagnostic = "vulkan launch: command buffer end failed";
    return false;
  }
  const auto submit_status = context.submit_commands(1U, state.command_buffer, 0U, timeline);
  if (submit_status != backend::vulkan::DeviceStatus::success) {
    diagnostic = std::string("vulkan launch: submit failed status=") +
                 backend::vulkan::device_status_string(submit_status);
    return false;
  }
  // Some driver/loader combinations return spurious busy from binary-blocked
  // timeline waits; poll the counter instead.
  backend::vulkan::DeviceStatus wait_status = backend::vulkan::DeviceStatus::not_ready;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::nanoseconds(kLaunchTimeoutNs);
  for (;;) {
    std::uint64_t completed = 0U;
    wait_status = context.poll(1U, &completed);
    if (wait_status != backend::vulkan::DeviceStatus::success) {
      break;
    }
    if (completed >= timeline) {
      wait_status = backend::vulkan::DeviceStatus::success;
      break;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      wait_status = backend::vulkan::DeviceStatus::busy;
      break;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50U));
  }
  if (wait_status != backend::vulkan::DeviceStatus::success) {
    diagnostic = std::string("vulkan launch: wait failed status=") +
                 backend::vulkan::device_status_string(wait_status);
    return false;
  }

  for (std::uint32_t binding = 0; binding < state.argument_count; ++binding) {
    if (roles[binding] != backend::vulkan::kBindingRoleWrite) {
      continue;
    }
    const std::uint8_t source = sources[binding];
    const std::size_t copy_bytes = buffers[source].size_bytes;
    (void)state.staging[binding]->invalidate(0U, static_cast<VkDeviceSize>(copy_bytes));
    std::memcpy(buffers[source].bytes, state.staging[binding]->allocation().mapped, copy_bytes);
  }
  return true;
}

VulkanKernelModule::VulkanKernelModule() : state_(std::make_unique<State>()) {}
VulkanKernelModule::VulkanKernelModule(VulkanKernelModule&&) noexcept = default;
VulkanKernelModule& VulkanKernelModule::operator=(VulkanKernelModule&&) noexcept = default;
VulkanKernelModule::~VulkanKernelModule() {
  if (state_ == nullptr) {
    return;
  }
  VulkanKernelModule::State& state = *state_;
  if (state.context == nullptr || !state.context->ready()) {
    return;
  }
  VkDevice device = state.context->device_handle();
  state.staging.clear();
  state.pipeline.reset();
  if (state.command_buffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(device, state.command_pool, 1U, &state.command_buffer);
    state.command_buffer = VK_NULL_HANDLE;
  }
  if (state.command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, state.command_pool, nullptr);
    state.command_pool = VK_NULL_HANDLE;
  }
  if (state.descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, state.descriptor_pool, nullptr);
    state.descriptor_pool = VK_NULL_HANDLE;
  }
  if (state.layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, state.layout, nullptr);
    state.layout = VK_NULL_HANDLE;
  }
  if (state.set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, state.set_layout, nullptr);
    state.set_layout = VK_NULL_HANDLE;
  }
}

bool VulkanKernelModule::dispatchable() const noexcept {
#if METAFLUX_DAEMON_VULKAN_EXECUTION
  return state_ != nullptr && state_->pipeline != nullptr;
#else
  return false;
#endif
}

std::string_view VulkanKernelModule::entry_point() const noexcept {
  if (state_ == nullptr) {
    return {};
  }
#if METAFLUX_DAEMON_VULKAN_EXECUTION
  return state_->lowered.entry_point;
#else
  return {};
#endif
}

std::shared_ptr<VulkanExecutionRoute> global_vulkan_execution_route() {
  static std::mutex mutex;
  static std::weak_ptr<VulkanExecutionRoute> route;
  std::lock_guard<std::mutex> lock(mutex);
  if (auto existing = route.lock()) {
    return existing;
  }
  const char* enabled = std::getenv("METAFLUX_VULKAN_EXECUTION");
  if (enabled == nullptr || enabled[0] != '1') {
    return nullptr;
  }
  auto candidate = std::make_shared<VulkanExecutionRoute>();
  mf_vulkan_capability_profile_v1 profile{};
  profile.struct_size = static_cast<std::uint32_t>(sizeof(profile));
  if (mf_vulkan_probe_capabilities_v1(&profile) != MF_VULKAN_PROBE_SUCCESS) {
    return nullptr;
  }
  candidate->state_->profile = profile;
  if (candidate->state_->context.initialize(profile) !=
      backend::vulkan::DeviceStatus::success) {
    return nullptr;
  }
  route = candidate;
  return candidate;
}

} // namespace metaflux::service

#else // !METAFLUX_DAEMON_VULKAN_EXECUTION

namespace metaflux::service {

struct VulkanKernelModule::State final {};
VulkanKernelModule::VulkanKernelModule() : state_(std::make_unique<State>()) {}
VulkanKernelModule::~VulkanKernelModule() = default;

bool VulkanKernelModule::dispatchable() const noexcept {
#if METAFLUX_DAEMON_VULKAN_EXECUTION
  return state_ != nullptr && state_->pipeline != nullptr;
#else
  return false;
#endif
}

std::string_view VulkanKernelModule::entry_point() const noexcept {
  if (state_ == nullptr) {
    return {};
  }
#if METAFLUX_DAEMON_VULKAN_EXECUTION
  return state_->lowered.entry_point;
#else
  return {};
#endif
}

struct VulkanExecutionRoute::State final {};
VulkanExecutionRoute::VulkanExecutionRoute() = default;
VulkanExecutionRoute::~VulkanExecutionRoute() = default;
bool VulkanExecutionRoute::ready() const noexcept { return false; }
std::unique_ptr<VulkanKernelModule>
VulkanExecutionRoute::prepare(const compiler::Kernel& kernel, std::string& diagnostic) {
  static_cast<void>(kernel);
  diagnostic = "vulkan execution route is not built into this daemon";
  return nullptr;
}
bool VulkanExecutionRoute::launch(VulkanKernelModule& module,
                                  std::span<const VulkanLaunchBuffer> buffers,
                                  std::uint32_t element_count, std::string& diagnostic) {
  static_cast<void>(module);
  static_cast<void>(buffers);
  static_cast<void>(element_count);
  static_cast<void>(diagnostic);
  return false;
}

std::shared_ptr<VulkanExecutionRoute> global_vulkan_execution_route() { return nullptr; }

} // namespace metaflux::service

#endif
