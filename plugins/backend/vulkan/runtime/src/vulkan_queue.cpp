#include "vulkan_queue.hpp"

#include "vulkan_pipeline.hpp"

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
    if (dependency.stream_id == 0U || dependency.timeline_value == 0U) {
      return false;
    }
    if (find_completion(dependency.stream_id, dependency.timeline_value) != nullptr) {
      continue;
    }
    // In the normal path graph sequence and the Vulkan timeline are identical
    // because both advance once per accepted submission. Once a physical
    // submission failed they can diverge, so completed dependencies must not
    // be guessed from their sequence number.
    if (!physical_submission_failed_ && dependency.timeline_value <= last_completed_value_) {
      continue;
    }
    return false;
  }
  return true;
}

const VulkanQueueExecutor::CompletionRecord*
VulkanQueueExecutor::find_completion(std::uint64_t stream_id, std::uint64_t sequence) const noexcept {
  if (stream_id == 0U || sequence == 0U || completion_records_ == nullptr) {
    return nullptr;
  }
  for (std::size_t index = 0U; index < completion_capacity_; ++index) {
    const CompletionRecord& record = completion_records_[index];
    if (record.active && record.stream_id == stream_id && record.sequence == sequence) {
      return &record;
    }
  }
  return nullptr;
}

VulkanQueueExecutor::CompletionRecord*
VulkanQueueExecutor::reserve_completion(std::uint64_t stream_id, std::uint64_t sequence,
                                        std::uint64_t completion_value) noexcept {
  if (stream_id == 0U || sequence == 0U || completion_value == 0U ||
      completion_records_ == nullptr) {
    return nullptr;
  }
  for (std::size_t index = 0U; index < completion_capacity_; ++index) {
    CompletionRecord& record = completion_records_[index];
    if (!record.active) {
      record.sequence = sequence;
      record.stream_id = stream_id;
      record.completion_value = completion_value;
      record.active = true;
      return &record;
    }
  }
  return nullptr;
}

void VulkanQueueExecutor::release_completion(std::uint64_t sequence) noexcept {
  if (completion_records_ == nullptr) {
    return;
  }
  for (std::size_t index = 0U; index < completion_capacity_; ++index) {
    CompletionRecord& record = completion_records_[index];
    if (record.active && record.sequence == sequence) {
      record = CompletionRecord{};
      return;
    }
  }
}

void VulkanQueueExecutor::retire_completions(std::uint64_t completed_value) noexcept {
  if (completed_value <= last_completed_value_) {
    return;
  }
  last_completed_value_ = completed_value;
  if (completion_records_ == nullptr) {
    return;
  }
  for (std::size_t index = 0U; index < completion_capacity_; ++index) {
    CompletionRecord& record = completion_records_[index];
    if (record.active && record.completion_value <= completed_value) {
      record = CompletionRecord{};
    }
  }
}

QueueExecutionStatus VulkanQueueExecutor::create_stream(std::uint64_t stream_id) {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  return map(ledger_.create_stream(stream_id));
}

QueueExecutionStatus VulkanQueueExecutor::submit(
    std::uint64_t generation, std::uint64_t stream_id, OperationKind kind, Visibility visibility,
    std::span<const Dependency> dependencies, VkCommandBuffer command_buffer,
    QueueSubmission* out_submission) {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    const CompletionRecord* found = find_completion(dependency.stream_id, dependency.timeline_value);
    if (found == nullptr) {
      if (physical_submission_failed_ || dependency.timeline_value > last_completed_value_) {
        static_cast<void>(ledger_.discard(submission));
        return QueueExecutionStatus::dependency_not_ready;
      }
      wait_value = std::max(wait_value, dependency.timeline_value);
      continue;
    }
    if (found->completion_value == 0U) {
      static_cast<void>(ledger_.discard(submission));
      return QueueExecutionStatus::dependency_not_ready;
    }
    wait_value = std::max(wait_value, found->completion_value);
  }
  if (reserve_completion(submission.plan.stream_id, submission.plan.sequence,
                         submission.completion_value) == nullptr) {
    static_cast<void>(ledger_.discard(submission));
    return QueueExecutionStatus::resource_exhausted;
  }

  const auto submitted = context_->submit_commands(generation, command_buffer, wait_value,
                                                   submission.completion_value);
  if (submitted != DeviceStatus::success) {
    release_completion(submission.plan.sequence);
    physical_submission_failed_ = true;
    static_cast<void>(ledger_.discard(submission));
    return map(submitted);
  }
  *out_submission = submission;
  return QueueExecutionStatus::success;
}

QueueExecutionStatus VulkanQueueExecutor::submit_compute(
    VulkanComputePipeline& pipeline, std::uint64_t generation, std::uint64_t stream_id,
    std::span<const Dependency> dependencies, VkCommandBuffer command_buffer,
    std::uint32_t groups_x, std::uint32_t groups_y, std::uint32_t groups_z,
    QueueSubmission* out_submission) {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (context_ == nullptr || out_submission == nullptr || command_buffer == VK_NULL_HANDLE ||
      !pipeline.uses_context(*context_)) {
    return QueueExecutionStatus::invalid_argument;
  }
  const auto bind_status = pipeline.bind(command_buffer);
  switch (bind_status) {
  case PipelineStatus::success:
    break;
  case PipelineStatus::not_ready:
    return QueueExecutionStatus::not_ready;
  case PipelineStatus::device_lost:
    return QueueExecutionStatus::device_lost;
  case PipelineStatus::target_mismatch:
  case PipelineStatus::invalid_argument:
  case PipelineStatus::invalid_module:
  case PipelineStatus::unsupported:
  case PipelineStatus::out_of_memory:
  case PipelineStatus::compile_required:
  case PipelineStatus::initialization_failed:
    return QueueExecutionStatus::invalid_argument;
  }
  const auto dispatch_status = pipeline.dispatch(command_buffer, groups_x, groups_y, groups_z);
  if (dispatch_status != PipelineStatus::success) {
    return dispatch_status == PipelineStatus::device_lost ? QueueExecutionStatus::device_lost
                                                           : QueueExecutionStatus::invalid_argument;
  }
  const auto end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    return end_result == VK_ERROR_DEVICE_LOST ? QueueExecutionStatus::device_lost
                                               : QueueExecutionStatus::submission_failed;
  }
  return submit(generation, stream_id, OperationKind::launch,
                Visibility{.stage_mask = kStageCompute,
                           .access_mask = kAccessShaderRead | kAccessShaderWrite},
                dependencies, command_buffer, out_submission);
}

QueueExecutionStatus VulkanQueueExecutor::submit_warm_launch(
    WarmLaunchSession& session, std::uint64_t generation, std::uint64_t stream_id,
    OperationKind kind, Visibility visibility, std::span<const Dependency> dependencies,
    VkCommandBuffer command_buffer, std::uint64_t argument_block_size,
    QueueSubmission* out_submission) {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
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

QueueExecutionStatus VulkanQueueExecutor::submit_warm_compute(
    WarmLaunchSession& session, VulkanComputePipeline& pipeline, std::uint64_t generation,
    std::uint64_t stream_id, std::span<const Dependency> dependencies,
    VkCommandBuffer command_buffer, std::uint32_t groups_x, std::uint32_t groups_y,
    std::uint32_t groups_z, std::uint64_t argument_block_size,
    QueueSubmission* out_submission) {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (context_ == nullptr || out_submission == nullptr || command_buffer == VK_NULL_HANDLE ||
      !session.active() || session.generation() != generation ||
      !pipeline.uses_context(*context_)) {
    return session.active() && generation != 0U ? QueueExecutionStatus::stale_generation
                                                : QueueExecutionStatus::invalid_argument;
  }
  const auto abort_session = [&session]() noexcept {
    return session.cancel() == CacheStatus::success;
  };
  const auto bind_status = pipeline.bind(command_buffer);
  switch (bind_status) {
  case PipelineStatus::success:
    break;
  case PipelineStatus::not_ready:
    (void)abort_session();
    return QueueExecutionStatus::not_ready;
  case PipelineStatus::device_lost:
    (void)abort_session();
    return QueueExecutionStatus::device_lost;
  case PipelineStatus::target_mismatch:
  case PipelineStatus::invalid_argument:
  case PipelineStatus::invalid_module:
  case PipelineStatus::unsupported:
  case PipelineStatus::out_of_memory:
  case PipelineStatus::compile_required:
  case PipelineStatus::initialization_failed:
    (void)abort_session();
    return QueueExecutionStatus::invalid_argument;
  }
  const auto dispatch_status = pipeline.dispatch(command_buffer, groups_x, groups_y, groups_z);
  if (dispatch_status != PipelineStatus::success) {
    (void)abort_session();
    return dispatch_status == PipelineStatus::device_lost ? QueueExecutionStatus::device_lost
                                                           : QueueExecutionStatus::invalid_argument;
  }
  const auto end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    (void)abort_session();
    return end_result == VK_ERROR_DEVICE_LOST ? QueueExecutionStatus::device_lost
                                               : QueueExecutionStatus::submission_failed;
  }
  return submit_warm_launch(
      session, generation, stream_id, OperationKind::launch,
      Visibility{.stage_mask = kStageCompute,
                 .access_mask = kAccessShaderRead | kAccessShaderWrite},
      dependencies, command_buffer, argument_block_size, out_submission);
}

QueueExecutionStatus VulkanQueueExecutor::complete(std::uint64_t generation,
                                                   std::uint64_t completed_value) noexcept {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  const auto status = map(ledger_.complete(generation, completed_value));
  if (status == QueueExecutionStatus::success) {
    retire_completions(completed_value);
  }
  return status;
}

QueueExecutionStatus VulkanQueueExecutor::poll(std::uint64_t generation,
                                                std::uint64_t* out_completed_value) noexcept {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
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
  if (completed_value == 0U) {
    *out_completed_value = 0U;
    return QueueExecutionStatus::success;
  }
  const auto recycled = ledger_.complete(generation, completed_value);
  const auto recycled_status = map(recycled);
  if (recycled_status == QueueExecutionStatus::success) {
    retire_completions(completed_value);
    *out_completed_value = completed_value;
  }
  return recycled_status;
}

QueueExecutionStatus VulkanQueueExecutor::wait(std::uint64_t generation, std::uint64_t value,
                                                std::uint64_t timeout_ns) noexcept {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
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
  const auto completed = map(ledger_.complete(generation, value));
  if (completed == QueueExecutionStatus::success) {
    retire_completions(value);
  }
  return completed;
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
