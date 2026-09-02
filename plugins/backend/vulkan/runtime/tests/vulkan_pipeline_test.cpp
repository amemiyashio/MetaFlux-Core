#include "../src/vulkan_pipeline.hpp"
#include "../src/vulkan_pipeline_cache.hpp"
#include "../src/vulkan_queue.hpp"

#include <metaflux/backend/vulkan_capability.hpp>
#include <metaflux/backend/vulkan.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <new>
#include <unistd.h>
#include <vector>

namespace {

std::atomic<bool> g_track_warm_allocations{false};
std::atomic<std::uint64_t> g_warm_allocations{0U};

void* allocate_test_memory(std::size_t size, std::size_t alignment) {
  if (size == 0U) {
    size = 1U;
  }
  void* result = nullptr;
  if (alignment <= alignof(std::max_align_t)) {
    result = std::malloc(size);
  } else if (posix_memalign(&result, alignment, size) != 0) {
    result = nullptr;
  }
  if (result == nullptr) {
    throw std::bad_alloc();
  }
  if (g_track_warm_allocations.load(std::memory_order_relaxed)) {
    g_warm_allocations.fetch_add(1U, std::memory_order_relaxed);
  }
  return result;
}

} // namespace

void* operator new(std::size_t size) { return allocate_test_memory(size, alignof(std::max_align_t)); }

void* operator new[](std::size_t size) {
  return allocate_test_memory(size, alignof(std::max_align_t));
}

void* operator new(std::size_t size, std::align_val_t alignment) {
  return allocate_test_memory(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
  return allocate_test_memory(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
  std::free(pointer);
}

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
         pipeline.dispatch(VK_NULL_HANDLE, 1U, 1U, 1U) == PipelineStatus::not_ready &&
         std::string(metaflux::backend::vulkan::pipeline_status_string(
             PipelineStatus::compile_required)) == "compile-required";
}

bool validates_spirv_header() {
  using metaflux::backend::vulkan::PipelineStatus;
  const std::array<std::uint32_t, 5> valid{
      UINT32_C(0x07230203), UINT32_C(0x00010000), 0U, 1U, 0U};
  const auto expect_invalid = [](std::array<std::uint32_t, 5> words) {
    return metaflux::backend::vulkan::validate_spirv_binary(words) ==
           PipelineStatus::invalid_module;
  };
  return metaflux::backend::vulkan::validate_spirv_binary(valid) == PipelineStatus::success &&
         metaflux::backend::vulkan::validate_spirv_binary({}) == PipelineStatus::invalid_module &&
         expect_invalid({0U, valid[1], valid[2], valid[3], valid[4]}) &&
         expect_invalid({valid[0], 0U, valid[2], valid[3], valid[4]}) &&
         expect_invalid({valid[0], valid[1], valid[2], 0U, valid[4]}) &&
         expect_invalid({valid[0], valid[1], valid[2], valid[3], 1U});
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
  const std::array<std::uint32_t, 5> malformed{
      UINT32_C(0U), UINT32_C(0x00010000), 0U, 1U, 0U};
  if (pipeline.create(malformed, "main", layout) != PipelineStatus::invalid_module) {
    vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
    return false;
  }
  VulkanPipelineCache cache(context);
  if (cache.create() != metaflux::backend::vulkan::PipelineCacheStatus::success) {
    vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
    return false;
  }
  const auto status = pipeline.create(words, "main", layout, cache.handle());
  const bool ready = status == PipelineStatus::success && pipeline.ready();
  if (ready) {
    metaflux::backend::vulkan::VulkanQueueExecutor executor(context, 2U);
    if (executor.create_stream(1U) !=
        metaflux::backend::vulkan::QueueExecutionStatus::success) {
      vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
      return false;
    }
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
      const bool recorded = vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS;
      metaflux::backend::vulkan::QueueSubmission submission{};
      const auto submitted = recorded
                                 ? executor.submit_compute(pipeline, 7U, 1U, {}, command_buffer,
                                                           1U, 1U, 1U, &submission)
                                 : metaflux::backend::vulkan::QueueExecutionStatus::submission_failed;
      const auto waited = submitted ==
                                  metaflux::backend::vulkan::QueueExecutionStatus::success
                              ? executor.wait(7U, submission.completion_value,
                                              UINT64_C(5000000000))
                              : submitted;
      if (waited != metaflux::backend::vulkan::QueueExecutionStatus::success) {
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      if (executor.create_stream(2U) !=
              metaflux::backend::vulkan::QueueExecutionStatus::success ||
          vkResetCommandBuffer(command_buffer, 0U) != VK_SUCCESS ||
          vkBeginCommandBuffer(command_buffer, &begin) != VK_SUCCESS) {
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      const std::array<metaflux::backend::vulkan::Dependency, 1> cross_stream_dependency{{
          metaflux::backend::vulkan::Dependency{
              .stream_id = 1U, .timeline_value = submission.completion_value}}};
      metaflux::backend::vulkan::QueueSubmission dependent_submission{};
      const auto dependent_submitted = executor.submit_compute(
          pipeline, 7U, 2U, cross_stream_dependency, command_buffer, 1U, 1U, 1U,
          &dependent_submission);
      const auto dependent_waited =
          dependent_submitted == metaflux::backend::vulkan::QueueExecutionStatus::success
              ? executor.wait(7U, dependent_submission.completion_value,
                              UINT64_C(5000000000))
              : dependent_submitted;
      if (dependent_waited != metaflux::backend::vulkan::QueueExecutionStatus::success) {
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      char directory_template[] = "/tmp/metaflux-vulkan-warm-XXXXXX";
      const char* directory = ::mkdtemp(directory_template);
      if (directory == nullptr) {
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      const std::filesystem::path cache_root(directory);
      metaflux::backend::vulkan::PersistentCacheRepository repository(cache_root / "cache", 4U,
                                                                       1024U * 1024U);
      std::vector<std::uint8_t> warm_cache_bytes;
      const auto warm_export = cache.export_data(&warm_cache_bytes);
      const std::string warm_payload(reinterpret_cast<const char*>(warm_cache_bytes.data()),
                                     warm_cache_bytes.size());
      metaflux::backend::vulkan::WarmLaunchSession session;
      std::string loaded_payload;
      const auto warm_key = std::string("physical-device-pipeline");
      const bool warm_started =
          warm_export == metaflux::backend::vulkan::PipelineCacheStatus::success &&
          !warm_payload.empty() &&
          repository.publish(warm_key, warm_payload, true) ==
              metaflux::backend::vulkan::CacheStatus::success &&
          metaflux::backend::vulkan::WarmLaunchSession::start(
              repository, warm_key, true, 7U, &loaded_payload, session) ==
              metaflux::backend::vulkan::CacheStatus::success &&
          loaded_payload == warm_payload;
      if (!warm_started || vkResetCommandBuffer(command_buffer, 0U) != VK_SUCCESS ||
          vkBeginCommandBuffer(command_buffer, &begin) != VK_SUCCESS) {
        (void)session.cancel();
        std::error_code error;
        std::filesystem::remove_all(cache_root, error);
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      metaflux::backend::vulkan::QueueSubmission warm_submission{};
      g_warm_allocations.store(0U, std::memory_order_relaxed);
      g_track_warm_allocations.store(true, std::memory_order_release);
      const auto warm_submitted = executor.submit_warm_compute(
          session, pipeline, 7U, 1U, {}, command_buffer, 1U, 1U, 1U, 64U, &warm_submission);
      const auto warm_waited =
          warm_submitted == metaflux::backend::vulkan::QueueExecutionStatus::success
              ? executor.wait(7U, warm_submission.completion_value, UINT64_C(5000000000))
              : warm_submitted;
      const auto warm_finished =
          warm_waited == metaflux::backend::vulkan::QueueExecutionStatus::success
              ? session.finish()
              : metaflux::backend::vulkan::CacheStatus::invalid_argument;
      g_track_warm_allocations.store(false, std::memory_order_release);
      const bool warm_valid =
          warm_waited == metaflux::backend::vulkan::QueueExecutionStatus::success &&
          session.submitted() &&
          metaflux::backend::vulkan::validate_warm_launch_trace(session.trace()) ==
              metaflux::backend::vulkan::WarmLaunchStatus::success &&
          warm_finished == metaflux::backend::vulkan::CacheStatus::success &&
          g_warm_allocations.load(std::memory_order_relaxed) == 0U &&
          !session.active();
      std::error_code error;
      std::filesystem::remove_all(cache_root, error);
      if (!warm_valid) {
        (void)session.cancel();
        vkDestroyCommandPool(context.device_handle(), pool, nullptr);
        vkDestroyPipelineLayout(context.device_handle(), layout, nullptr);
        return false;
      }
      vkDestroyCommandPool(context.device_handle(), pool, nullptr);
      if (waited != metaflux::backend::vulkan::QueueExecutionStatus::success) {
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
  if (!invalid_guards() || !validates_spirv_header() || !invalid_cache_guard()) {
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
