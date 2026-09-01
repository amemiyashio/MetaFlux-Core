#include "../src/vulkan_queue.hpp"

#include <cstdint>

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
  return executor.complete(7U, 1U) == QueueExecutionStatus::invalid_timeline ? 0 : 3;
}
