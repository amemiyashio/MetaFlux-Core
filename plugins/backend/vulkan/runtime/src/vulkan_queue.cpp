#include "vulkan_queue.hpp"

#include <algorithm>

namespace metaflux::backend::vulkan {

QueueExecutionStatus VulkanQueueExecutor::map(QueueSubmissionStatus status) noexcept {
  switch (status) {
  case QueueSubmissionStatus::success:
    return QueueExecutionStatus::success;
  case QueueSubmissionStatus::invalid_argument:
    return QueueExecutionStatus::invalid_argument;
  case QueueSubmissionStatus::stale_generation:
    return QueueExecutionStatus::stale_generation;
  case QueueSubmissionStatus::unknown_stream:
    return QueueExecutionStatus::unknown_stream;
  case QueueSubmissionStatus::invalid_visibility:
    return QueueExecutionStatus::invalid_visibility;
  case QueueSubmissionStatus::dependency_not_ready:
    return QueueExecutionStatus::dependency_not_ready;
  case QueueSubmissionStatus::duplicate_dependency:
    return QueueExecutionStatus::duplicate_dependency;
  case QueueSubmissionStatus::too_many_dependencies:
    return QueueExecutionStatus::too_many_dependencies;
  case QueueSubmissionStatus::resource_exhausted:
    return QueueExecutionStatus::resource_exhausted;
  case QueueSubmissionStatus::busy:
    return QueueExecutionStatus::busy;
  case QueueSubmissionStatus::invalid_timeline:
    return QueueExecutionStatus::invalid_timeline;
  case QueueSubmissionStatus::timeline_exhausted:
    return QueueExecutionStatus::submission_failed;
  case QueueSubmissionStatus::not_found:
    return QueueExecutionStatus::dependency_not_ready;
  }
  return QueueExecutionStatus::submission_failed;
}

QueueExecutionStatus VulkanQueueExecutor::map(DeviceStatus status) noexcept {
  switch (status) {
  case DeviceStatus::success:
    return QueueExecutionStatus::success;
  case DeviceStatus::invalid_argument:
    return QueueExecutionStatus::invalid_argument;
  case DeviceStatus::invalid_timeline:
    return QueueExecutionStatus::invalid_timeline;
  case DeviceStatus::stale_generation:
    return QueueExecutionStatus::stale_generation;
  case DeviceStatus::not_ready:
    return QueueExecutionStatus::not_ready;
  case DeviceStatus::device_lost:
    return QueueExecutionStatus::device_lost;
  case DeviceStatus::busy:
  case DeviceStatus::no_device:
  case DeviceStatus::unsupported_features:
  case DeviceStatus::initialization_failed:
    return QueueExecutionStatus::submission_failed;
  }
  return QueueExecutionStatus::submission_failed;
}

bool VulkanQueueExecutor::dependencies_known(
    std::span<const Dependency> dependencies) const noexcept {
  for (const auto dependency : dependencies) {
    if (dependency.stream_id == 0U || dependency.timeline_value == 0U ||
        completion_by_sequence_.find(dependency.timeline_value) == completion_by_sequence_.end()) {
      return false;
    }
  }
  return true;
}

QueueExecutionStatus VulkanQueueExecutor::create_stream(std::uint64_t stream_id) {
  return map(ledger_.create_stream(stream_id));
}

QueueExecutionStatus VulkanQueueExecutor::submit(
    std::uint64_t generation, std::uint64_t stream_id, OperationKind kind, Visibility visibility,
    std::span<const Dependency> dependencies, VkCommandBuffer command_buffer,
    QueueSubmission* out_submission) {
  if (context_ == nullptr || out_submission == nullptr || command_buffer == VK_NULL_HANDLE) {
    return QueueExecutionStatus::invalid_argument;
  }
  if (generation == 0U || generation != context_->generation()) {
    return generation != 0U ? QueueExecutionStatus::stale_generation
                            : QueueExecutionStatus::invalid_argument;
  }
  if (context_->lost()) {
    return QueueExecutionStatus::device_lost;
  }
  if (!context_->ready()) {
    return QueueExecutionStatus::not_ready;
  }
  if (!dependencies_known(dependencies)) {
    return QueueExecutionStatus::dependency_not_ready;
  }

  QueueSubmission submission{};
  const auto planned = ledger_.submit(generation, stream_id, kind, visibility, dependencies,
                                     &submission);
  if (planned != QueueSubmissionStatus::success) {
    return map(planned);
  }

  std::uint64_t wait_value = 0U;
  for (std::uint32_t index = 0U; index < submission.plan.dependency_count; ++index) {
    const auto dependency = submission.plan.dependencies[index];
    const auto found = completion_by_sequence_.find(dependency.timeline_value);
    if (found == completion_by_sequence_.end()) {
      static_cast<void>(ledger_.discard(submission));
      return QueueExecutionStatus::dependency_not_ready;
    }
    wait_value = std::max(wait_value, found->second);
  }
  try {
    completion_by_sequence_.emplace(submission.plan.sequence, submission.completion_value);
  } catch (...) {
    static_cast<void>(ledger_.discard(submission));
    return QueueExecutionStatus::resource_exhausted;
  }

  const auto submitted = context_->submit_commands(generation, command_buffer, wait_value,
                                                   submission.completion_value);
  if (submitted != DeviceStatus::success) {
    completion_by_sequence_.erase(submission.plan.sequence);
    static_cast<void>(ledger_.discard(submission));
    return map(submitted);
  }
  *out_submission = submission;
  return QueueExecutionStatus::success;
}

QueueExecutionStatus VulkanQueueExecutor::submit_warm_launch(
    WarmLaunchSession& session, std::uint64_t generation, std::uint64_t stream_id,
    OperationKind kind, Visibility visibility, std::span<const Dependency> dependencies,
    VkCommandBuffer command_buffer, std::uint64_t argument_block_size,
    QueueSubmission* out_submission) {
  if (!session.active() || session.generation() != generation) {
    return session.active() && generation != 0U ? QueueExecutionStatus::stale_generation
                                                : QueueExecutionStatus::invalid_argument;
  }
  const auto abort_session = [&session]() noexcept {
    return session.cancel() == CacheStatus::success;
  };
  if (session.bind_arguments(argument_block_size) != WarmLaunchStatus::success) {
    (void)abort_session();
    return QueueExecutionStatus::invalid_argument;
  }
  if (session.submit() != WarmLaunchStatus::success) {
    (void)abort_session();
    return QueueExecutionStatus::invalid_argument;
  }
  const auto status = submit(generation, stream_id, kind, visibility, dependencies, command_buffer,
                             out_submission);
  if (status != QueueExecutionStatus::success) {
    (void)abort_session();
  }
  return status;
}

QueueExecutionStatus VulkanQueueExecutor::complete(std::uint64_t generation,
                                                   std::uint64_t completed_value) noexcept {
  return map(ledger_.complete(generation, completed_value));
}

QueueExecutionStatus VulkanQueueExecutor::poll(std::uint64_t generation,
                                                std::uint64_t* out_completed_value) noexcept {
  if (context_ == nullptr || out_completed_value == nullptr || generation == 0U) {
    return QueueExecutionStatus::invalid_argument;
  }
  if (generation != context_->generation()) {
    return QueueExecutionStatus::stale_generation;
  }
  if (context_->lost()) {
    return QueueExecutionStatus::device_lost;
  }
  if (!context_->ready()) {
    return QueueExecutionStatus::not_ready;
  }

  std::uint64_t completed_value = 0U;
  const auto observed = context_->poll(generation, &completed_value);
  const auto observed_status = map(observed);
  if (observed_status != QueueExecutionStatus::success) {
    return observed_status;
  }
  const auto recycled = ledger_.complete(generation, completed_value);
  const auto recycled_status = map(recycled);
  if (recycled_status == QueueExecutionStatus::success) {
    *out_completed_value = completed_value;
  }
  return recycled_status;
}

QueueExecutionStatus VulkanQueueExecutor::wait(std::uint64_t generation, std::uint64_t value,
                                                std::uint64_t timeout_ns) noexcept {
  if (context_ == nullptr || generation == 0U || value == 0U) {
    return QueueExecutionStatus::invalid_argument;
  }
  if (generation != context_->generation()) {
    return QueueExecutionStatus::stale_generation;
  }
  if (context_->lost()) {
    return QueueExecutionStatus::device_lost;
  }
  if (!context_->ready()) {
    return QueueExecutionStatus::not_ready;
  }

  const auto waited = context_->wait(generation, value, timeout_ns);
  const auto waited_status = map(waited);
  if (waited_status != QueueExecutionStatus::success) {
    return waited_status;
  }
  return map(ledger_.complete(generation, value));
}

const char* queue_execution_status_string(QueueExecutionStatus status) noexcept {
  switch (status) {
  case QueueExecutionStatus::success:
    return "success";
  case QueueExecutionStatus::invalid_argument:
    return "invalid-argument";
  case QueueExecutionStatus::stale_generation:
    return "stale-generation";
  case QueueExecutionStatus::not_ready:
    return "not-ready";
  case QueueExecutionStatus::device_lost:
    return "device-lost";
  case QueueExecutionStatus::unknown_stream:
    return "unknown-stream";
  case QueueExecutionStatus::invalid_visibility:
    return "invalid-visibility";
  case QueueExecutionStatus::dependency_not_ready:
    return "dependency-not-ready";
  case QueueExecutionStatus::duplicate_dependency:
    return "duplicate-dependency";
  case QueueExecutionStatus::too_many_dependencies:
    return "too-many-dependencies";
  case QueueExecutionStatus::resource_exhausted:
    return "resource-exhausted";
  case QueueExecutionStatus::busy:
    return "busy";
  case QueueExecutionStatus::invalid_timeline:
    return "invalid-timeline";
  case QueueExecutionStatus::submission_failed:
    return "submission-failed";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
