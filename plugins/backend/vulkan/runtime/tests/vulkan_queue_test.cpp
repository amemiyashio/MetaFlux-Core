#include "../src/vulkan_queue.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>

int main() {
  using metaflux::backend::vulkan::OperationKind;
  using metaflux::backend::vulkan::QueueExecutionStatus;
  using metaflux::backend::vulkan::QueueSubmission;
  using metaflux::backend::vulkan::VulkanDeviceContext;
  using metaflux::backend::vulkan::VulkanQueueExecutor;
  using metaflux::backend::vulkan::Visibility;

  VulkanDeviceContext context(7U);
  VulkanQueueExecutor executor(context, 2U);
  if (executor.create_stream(1U) != QueueExecutionStatus::success ||
      executor.create_stream(1U) != QueueExecutionStatus::invalid_argument) {
    return 1;
  }

  QueueSubmission submission{};
  const Visibility visibility{
      .stage_mask = metaflux::backend::vulkan::kStageTransfer,
      .access_mask = metaflux::backend::vulkan::kAccessTransferRead,
  };
  const auto fake_command_buffer = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1U));
  if (executor.submit(6U, 1U, OperationKind::copy, visibility, {}, fake_command_buffer, &submission) !=
          QueueExecutionStatus::stale_generation ||
      executor.submit(7U, 1U, OperationKind::copy, visibility, {}, VK_NULL_HANDLE, &submission) !=
          QueueExecutionStatus::invalid_argument ||
      executor.submit(7U, 1U, OperationKind::copy, visibility, {}, fake_command_buffer, &submission) !=
          QueueExecutionStatus::not_ready ||
      executor.in_flight_count() != 0U) {
    return 2;
  }

  char directory_template[] = "/tmp/metaflux-vulkan-warm-XXXXXX";
  const char* directory = ::mkdtemp(directory_template);
  if (directory == nullptr) {
    return 3;
  }
  const std::filesystem::path cache_path(directory);
  metaflux::backend::vulkan::PersistentCacheRepository repository(cache_path / "cache", 4U,
                                                                  1024U);
  const std::string key = "device-key";
  std::string payload;
  metaflux::backend::vulkan::WarmLaunchSession session;
  const bool started = repository.publish(key, "pipeline", true) ==
                           metaflux::backend::vulkan::CacheStatus::success &&
                       metaflux::backend::vulkan::WarmLaunchSession::start(
                           repository, key, true, 7U, &payload, session) ==
                           metaflux::backend::vulkan::CacheStatus::success;
  if (!started || executor.submit_warm_launch(session, 7U, 1U, OperationKind::copy, visibility,
                                               {}, fake_command_buffer, 64U, &submission) !=
                      QueueExecutionStatus::not_ready ||
      session.active() || repository.acquire_pipeline(key, 7U) !=
                              metaflux::backend::vulkan::CacheStatus::success ||
      repository.release_pipeline(key, 7U) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      executor.complete(7U, 1U) != QueueExecutionStatus::invalid_timeline) {
    std::error_code error;
    std::filesystem::remove_all(cache_path, error);
    return 4;
  }
  std::error_code error;
  std::filesystem::remove_all(cache_path, error);
  return 0;
}
