#ifndef METAFLUX_BACKEND_VULKAN_QUEUE_HPP
#define METAFLUX_BACKEND_VULKAN_QUEUE_HPP

#include "vulkan_device.hpp"

#include "metaflux/backend/vulkan_streams.hpp"
#include "metaflux/backend/vulkan_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>

namespace metaflux::backend::vulkan {

class VulkanComputePipeline;

enum class QueueExecutionStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  stale_generation = 2,
  not_ready = 3,
  device_lost = 4,
  unknown_stream = 5,
  invalid_visibility = 6,
  dependency_not_ready = 7,
  duplicate_dependency = 8,
  too_many_dependencies = 9,
  resource_exhausted = 10,
  busy = 11,
  invalid_timeline = 12,
  submission_failed = 13,
};

// Binds the host-independent graph/resource ledger to one generation-bound
// Vulkan context. The context is owned by the caller and must outlive this
// adapter; no Vulkan handle crosses the backend C ABI.
class VulkanQueueExecutor final {
  struct CompletionRecord final {
    std::uint64_t sequence = 0;
    std::uint64_t completion_value = 0;
    bool active = false;
  };

public:
  VulkanQueueExecutor(VulkanDeviceContext& context, std::size_t resource_capacity) noexcept
      : context_(&context), ledger_(resource_capacity, context.generation()),
        completion_capacity_(resource_capacity),
        completion_records_(resource_capacity == 0U
                                ? nullptr
                                : new (std::nothrow) CompletionRecord[resource_capacity]{}) {}

  VulkanQueueExecutor(const VulkanQueueExecutor&) = delete;
  VulkanQueueExecutor& operator=(const VulkanQueueExecutor&) = delete;

  [[nodiscard]] QueueExecutionStatus create_stream(std::uint64_t stream_id);
  [[nodiscard]] QueueExecutionStatus submit(
      std::uint64_t generation, std::uint64_t stream_id, OperationKind kind,
      Visibility visibility, std::span<const Dependency> dependencies,
      VkCommandBuffer command_buffer, QueueSubmission* out_submission);
  // Record one pipeline dispatch into a caller-owned command buffer, close the
  // recording, and submit it through the same resource/timeline ledger.
  [[nodiscard]] QueueExecutionStatus submit_compute(
      VulkanComputePipeline& pipeline, std::uint64_t generation, std::uint64_t stream_id,
      std::span<const Dependency> dependencies, VkCommandBuffer command_buffer,
      std::uint32_t groups_x, std::uint32_t groups_y, std::uint32_t groups_z,
      QueueSubmission* out_submission);
  // Composes the cache-hit warm path with queue admission. The session keeps
  // its pipeline pin until the caller finishes the corresponding submission.
  [[nodiscard]] QueueExecutionStatus submit_warm_launch(
      WarmLaunchSession& session, std::uint64_t generation, std::uint64_t stream_id,
      OperationKind kind, Visibility visibility, std::span<const Dependency> dependencies,
      VkCommandBuffer command_buffer, std::uint64_t argument_block_size,
      QueueSubmission* out_submission);
  // Record and submit a cache-resident compute dispatch. The warm session owns
  // the residency pin; this path never creates Vulkan objects or allocates.
  [[nodiscard]] QueueExecutionStatus submit_warm_compute(
      WarmLaunchSession& session, VulkanComputePipeline& pipeline, std::uint64_t generation,
      std::uint64_t stream_id, std::span<const Dependency> dependencies,
      VkCommandBuffer command_buffer, std::uint32_t groups_x, std::uint32_t groups_y,
      std::uint32_t groups_z, std::uint64_t argument_block_size,
      QueueSubmission* out_submission);
  [[nodiscard]] QueueExecutionStatus complete(std::uint64_t generation,
                                              std::uint64_t completed_value) noexcept;
  // Observe the Vulkan timeline and recycle every command resource at or below
  // the value reported by the device.
  [[nodiscard]] QueueExecutionStatus poll(std::uint64_t generation,
                                           std::uint64_t* out_completed_value) noexcept;
  // Wait for one submitted timeline value, then publish the observed value to
  // the host-independent resource ledger.
  [[nodiscard]] QueueExecutionStatus wait(std::uint64_t generation,
                                           std::uint64_t value,
                                           std::uint64_t timeout_ns) noexcept;
  [[nodiscard]] std::size_t in_flight_count() const noexcept {
    return ledger_.in_flight_count();
  }

private:
  [[nodiscard]] static QueueExecutionStatus map(QueueSubmissionStatus status) noexcept;
  [[nodiscard]] static QueueExecutionStatus map(DeviceStatus status) noexcept;
  [[nodiscard]] bool dependencies_known(std::span<const Dependency> dependencies) const noexcept;
  [[nodiscard]] const CompletionRecord* find_completion(std::uint64_t sequence) const noexcept;
  [[nodiscard]] CompletionRecord* reserve_completion(std::uint64_t sequence,
                                                     std::uint64_t completion_value) noexcept;
  void release_completion(std::uint64_t sequence) noexcept;
  void retire_completions(std::uint64_t completed_value) noexcept;

  VulkanDeviceContext* context_ = nullptr;
  QueueSubmissionLedger ledger_;
  const std::size_t completion_capacity_ = 0U;
  std::unique_ptr<CompletionRecord[]> completion_records_;
  std::uint64_t last_completed_value_ = 0U;
  bool physical_submission_failed_ = false;
};

[[nodiscard]] const char* queue_execution_status_string(QueueExecutionStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
