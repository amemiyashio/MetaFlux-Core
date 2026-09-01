#include "metaflux/backend/vulkan_streams.hpp"

#include <algorithm>
#include <limits>

namespace metaflux::backend::vulkan {

namespace {

constexpr std::uint32_t kCopyAccesses = kAccessTransferRead | kAccessTransferWrite;
constexpr std::uint32_t kLaunchAccesses = kAccessShaderRead | kAccessShaderWrite;
constexpr std::size_t kMaxStreams = 32U;

} // namespace

StreamStatus StreamGraph::create_stream(std::uint64_t stream_id) {
  if (stream_id == 0U || streams_.size() >= kMaxStreams || has_stream(stream_id)) {
    return StreamStatus::invalid_argument;
  }
  streams_.push_back(StreamState{.id = stream_id});
  return StreamStatus::success;
}

StreamGraph::StreamState* StreamGraph::find_stream(std::uint64_t stream_id) noexcept {
  const auto position =
      std::find_if(streams_.begin(), streams_.end(),
                   [stream_id](const StreamState& stream) { return stream.id == stream_id; });
  return position == streams_.end() ? nullptr : &*position;
}

const StreamGraph::StreamState* StreamGraph::find_stream(std::uint64_t stream_id) const noexcept {
  const auto position =
      std::find_if(streams_.begin(), streams_.end(),
                   [stream_id](const StreamState& stream) { return stream.id == stream_id; });
  return position == streams_.end() ? nullptr : &*position;
}

bool StreamGraph::has_stream(std::uint64_t stream_id) const noexcept {
  return find_stream(stream_id) != nullptr;
}

bool StreamGraph::valid_visibility(OperationKind kind, Visibility visibility) noexcept {
  if (kind == OperationKind::copy) {
    return visibility.stage_mask == kStageTransfer && visibility.access_mask != 0U &&
           (visibility.access_mask & ~kCopyAccesses) == 0U;
  }
  if (kind == OperationKind::launch) {
    return visibility.stage_mask == kStageCompute && visibility.access_mask != 0U &&
           (visibility.access_mask & ~kLaunchAccesses) == 0U;
  }
  return false;
}

StreamStatus StreamGraph::submit(std::uint64_t generation, std::uint64_t stream_id,
                                 OperationKind kind, Visibility visibility,
                                 std::span<const Dependency> dependencies,
                                 SubmissionPlan* out_plan) {
  if (out_plan == nullptr || generation == 0U || generation != generation_) {
    return generation != generation_ && generation != 0U ? StreamStatus::stale_generation
                                                         : StreamStatus::invalid_argument;
  }
  auto* stream = find_stream(stream_id);
  if (stream == nullptr) {
    return StreamStatus::unknown_stream;
  }
  if (!valid_visibility(kind, visibility)) {
    return StreamStatus::invalid_visibility;
  }
  if (dependencies.size() > kMaxDependencies) {
    return StreamStatus::too_many_dependencies;
  }
  for (std::size_t index = 0U; index < dependencies.size(); ++index) {
    const auto dependency = dependencies[index];
    const auto* dependency_stream = find_stream(dependency.stream_id);
    if (dependency_stream == nullptr || dependency.timeline_value == 0U ||
        dependency.timeline_value > dependency_stream->last_sequence) {
      return StreamStatus::dependency_not_ready;
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (dependencies[prior].stream_id == dependency.stream_id &&
          dependencies[prior].timeline_value == dependency.timeline_value) {
        return StreamStatus::duplicate_dependency;
      }
    }
  }
  bool local_dependency_present = false;
  if (stream->last_sequence != 0U) {
    for (const auto dependency : dependencies) {
      if (dependency.stream_id == stream_id && dependency.timeline_value == stream->last_sequence) {
        local_dependency_present = true;
        break;
      }
    }
  }
  if (dependencies.size() + (stream->last_sequence != 0U && !local_dependency_present ? 1U : 0U) >
      kMaxDependencies) {
    return StreamStatus::too_many_dependencies;
  }
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return StreamStatus::timeline_exhausted;
  }

  SubmissionPlan plan{
      .sequence = next_sequence_++,
      .generation = generation_,
      .stream_id = stream_id,
      .kind = kind,
      .visibility = visibility,
      .dependency_count = 0U,
  };
  if (stream->last_sequence != 0U) {
    if (!local_dependency_present) {
      plan.dependencies[plan.dependency_count++] =
          Dependency{.stream_id = stream_id, .timeline_value = stream->last_sequence};
    }
  }
  for (const auto dependency : dependencies) {
    if (plan.dependency_count >= kMaxDependencies) {
      return StreamStatus::too_many_dependencies;
    }
    plan.dependencies[plan.dependency_count++] = dependency;
  }
  stream->last_sequence = plan.sequence;
  *out_plan = plan;
  return StreamStatus::success;
}

CommandResourcePool::Slot*
CommandResourcePool::find_slot(const CommandResource& resource) noexcept {
  const auto position = std::find_if(slots_.begin(), slots_.end(), [&resource](const Slot& slot) {
    return slot.resource.id == resource.id && slot.resource.generation == resource.generation &&
           slot.resource.stream_id == resource.stream_id &&
           slot.resource.sequence == resource.sequence;
  });
  return position == slots_.end() ? nullptr : &*position;
}

const CommandResourcePool::Slot*
CommandResourcePool::find_slot(const CommandResource& resource) const noexcept {
  const auto position = std::find_if(slots_.begin(), slots_.end(), [&resource](const Slot& slot) {
    return slot.resource.id == resource.id && slot.resource.generation == resource.generation &&
           slot.resource.stream_id == resource.stream_id &&
           slot.resource.sequence == resource.sequence;
  });
  return position == slots_.end() ? nullptr : &*position;
}

CommandResourceStatus CommandResourcePool::acquire(std::uint64_t generation,
                                                   std::uint64_t stream_id,
                                                   CommandResource* out_resource) noexcept {
  if (out_resource == nullptr || generation == 0U || generation != generation_ || stream_id == 0U) {
    return generation != generation_ && generation != 0U ? CommandResourceStatus::stale_generation
                                                         : CommandResourceStatus::invalid_argument;
  }
  if (next_id_ == std::numeric_limits<std::uint64_t>::max()) {
    return CommandResourceStatus::resource_id_exhausted;
  }
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    return CommandResourceStatus::timeline_exhausted;
  }
  for (Slot& slot : slots_) {
    if (slot.state != SlotState::available) {
      continue;
    }
    slot.state = SlotState::acquired;
    slot.resource = CommandResource{
        .id = next_id_++,
        .generation = generation_,
        .stream_id = stream_id,
        .sequence = next_sequence_++,
        .completion_value = 0U,
    };
    *out_resource = slot.resource;
    return CommandResourceStatus::success;
  }
  return CommandResourceStatus::exhausted;
}

CommandResourceStatus CommandResourcePool::submit(const CommandResource& resource,
                                                  std::uint64_t completion_value) noexcept {
  if (resource.generation == 0U || resource.generation != generation_) {
    return CommandResourceStatus::stale_generation;
  }
  if (completion_value == 0U || completion_value <= last_submission_timeline_ ||
      completion_value < last_completed_timeline_) {
    return CommandResourceStatus::invalid_timeline;
  }
  Slot* slot = find_slot(resource);
  if (slot == nullptr || slot->state != SlotState::acquired) {
    return CommandResourceStatus::not_found;
  }
  slot->resource.completion_value = completion_value;
  slot->state = SlotState::submitted;
  last_submission_timeline_ = completion_value;
  return CommandResourceStatus::success;
}

CommandResourceStatus CommandResourcePool::cancel(const CommandResource& resource) noexcept {
  if (resource.generation == 0U || resource.generation != generation_) {
    return CommandResourceStatus::stale_generation;
  }
  Slot* slot = find_slot(resource);
  if (slot == nullptr) {
    return CommandResourceStatus::not_found;
  }
  if (slot->state != SlotState::acquired) {
    return slot->state == SlotState::submitted ? CommandResourceStatus::busy
                                               : CommandResourceStatus::not_found;
  }
  slot->state = SlotState::available;
  slot->resource = CommandResource{};
  return CommandResourceStatus::success;
}

CommandResourceStatus CommandResourcePool::discard(const CommandResource& resource) noexcept {
  if (resource.generation == 0U || resource.generation != generation_) {
    return CommandResourceStatus::stale_generation;
  }
  Slot* slot = find_slot(resource);
  if (slot == nullptr) {
    return CommandResourceStatus::not_found;
  }
  if (slot->state != SlotState::submitted) {
    return slot->state == SlotState::acquired ? CommandResourceStatus::success
                                               : CommandResourceStatus::not_found;
  }
  slot->state = SlotState::available;
  slot->resource = CommandResource{};
  return CommandResourceStatus::success;
}

CommandResourceStatus CommandResourcePool::recycle(std::uint64_t generation,
                                                   std::uint64_t completed_value) noexcept {
  if (generation == 0U || generation != generation_) {
    return CommandResourceStatus::stale_generation;
  }
  if (completed_value == 0U || completed_value < last_completed_timeline_ ||
      completed_value > last_submission_timeline_) {
    return CommandResourceStatus::invalid_timeline;
  }
  last_completed_timeline_ = completed_value;
  for (Slot& slot : slots_) {
    if (slot.state == SlotState::submitted && slot.resource.completion_value <= completed_value) {
      slot.state = SlotState::available;
      slot.resource = CommandResource{};
    }
  }
  return CommandResourceStatus::success;
}

CommandResourceStatus CommandResourcePool::reconfigure(std::uint64_t generation) noexcept {
  if (generation == 0U) {
    return CommandResourceStatus::invalid_argument;
  }
  if (generation <= generation_) {
    return CommandResourceStatus::stale_generation;
  }
  if (in_flight_count() != 0U) {
    return CommandResourceStatus::busy;
  }
  generation_ = generation;
  last_submission_timeline_ = 0U;
  last_completed_timeline_ = 0U;
  for (Slot& slot : slots_) {
    slot.state = SlotState::available;
    slot.resource = CommandResource{};
  }
  return CommandResourceStatus::success;
}

std::size_t CommandResourcePool::available_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(slots_.begin(), slots_.end(), [](const Slot& slot) {
    return slot.state == SlotState::available;
  }));
}

std::size_t CommandResourcePool::in_flight_count() const noexcept {
  return slots_.size() - available_count();
}

const char* stream_status_string(StreamStatus status) noexcept {
  switch (status) {
  case StreamStatus::success:
    return "success";
  case StreamStatus::invalid_argument:
    return "invalid-argument";
  case StreamStatus::stale_generation:
    return "stale-generation";
  case StreamStatus::unknown_stream:
    return "unknown-stream";
  case StreamStatus::invalid_visibility:
    return "invalid-visibility";
  case StreamStatus::dependency_not_ready:
    return "dependency-not-ready";
  case StreamStatus::duplicate_dependency:
    return "duplicate-dependency";
  case StreamStatus::too_many_dependencies:
    return "too-many-dependencies";
  case StreamStatus::timeline_exhausted:
    return "timeline-exhausted";
  }
  return "unknown";
}

const char* command_resource_status_string(CommandResourceStatus status) noexcept {
  switch (status) {
  case CommandResourceStatus::success:
    return "success";
  case CommandResourceStatus::invalid_argument:
    return "invalid-argument";
  case CommandResourceStatus::stale_generation:
    return "stale-generation";
  case CommandResourceStatus::exhausted:
    return "exhausted";
  case CommandResourceStatus::busy:
    return "busy";
  case CommandResourceStatus::not_found:
    return "not-found";
  case CommandResourceStatus::invalid_timeline:
    return "invalid-timeline";
  case CommandResourceStatus::resource_id_exhausted:
    return "resource-id-exhausted";
  case CommandResourceStatus::timeline_exhausted:
    return "timeline-exhausted";
  }
  return "unknown";
}

QueueSubmissionStatus QueueSubmissionLedger::map(StreamStatus status) noexcept {
  switch (status) {
  case StreamStatus::success:
    return QueueSubmissionStatus::success;
  case StreamStatus::invalid_argument:
    return QueueSubmissionStatus::invalid_argument;
  case StreamStatus::stale_generation:
    return QueueSubmissionStatus::stale_generation;
  case StreamStatus::unknown_stream:
    return QueueSubmissionStatus::unknown_stream;
  case StreamStatus::invalid_visibility:
    return QueueSubmissionStatus::invalid_visibility;
  case StreamStatus::dependency_not_ready:
    return QueueSubmissionStatus::dependency_not_ready;
  case StreamStatus::duplicate_dependency:
    return QueueSubmissionStatus::duplicate_dependency;
  case StreamStatus::too_many_dependencies:
    return QueueSubmissionStatus::too_many_dependencies;
  case StreamStatus::timeline_exhausted:
    return QueueSubmissionStatus::timeline_exhausted;
  }
  return QueueSubmissionStatus::invalid_argument;
}

QueueSubmissionStatus QueueSubmissionLedger::map(CommandResourceStatus status) noexcept {
  switch (status) {
  case CommandResourceStatus::success:
    return QueueSubmissionStatus::success;
  case CommandResourceStatus::invalid_argument:
    return QueueSubmissionStatus::invalid_argument;
  case CommandResourceStatus::stale_generation:
    return QueueSubmissionStatus::stale_generation;
  case CommandResourceStatus::exhausted:
    return QueueSubmissionStatus::resource_exhausted;
  case CommandResourceStatus::busy:
    return QueueSubmissionStatus::busy;
  case CommandResourceStatus::not_found:
    return QueueSubmissionStatus::not_found;
  case CommandResourceStatus::invalid_timeline:
    return QueueSubmissionStatus::invalid_timeline;
  case CommandResourceStatus::resource_id_exhausted:
    return QueueSubmissionStatus::resource_exhausted;
  case CommandResourceStatus::timeline_exhausted:
    return QueueSubmissionStatus::timeline_exhausted;
  }
  return QueueSubmissionStatus::invalid_argument;
}

QueueSubmissionStatus QueueSubmissionLedger::create_stream(std::uint64_t stream_id) {
  std::lock_guard lock(mutex_);
  return map(graph_.create_stream(stream_id));
}

QueueSubmissionStatus QueueSubmissionLedger::submit(std::uint64_t generation,
                                                    std::uint64_t stream_id, OperationKind kind,
                                                    Visibility visibility,
                                                    std::span<const Dependency> dependencies,
                                                    QueueSubmission* out_submission) {
  if (out_submission == nullptr) {
    return QueueSubmissionStatus::invalid_argument;
  }
  std::lock_guard lock(mutex_);
  if (generation == 0U) {
    return QueueSubmissionStatus::invalid_argument;
  }
  if (generation != generation_) {
    return QueueSubmissionStatus::stale_generation;
  }
  if (next_completion_value_ == std::numeric_limits<std::uint64_t>::max()) {
    return QueueSubmissionStatus::timeline_exhausted;
  }

  CommandResource resource{};
  const auto acquired = resources_.acquire(generation, stream_id, &resource);
  if (acquired != CommandResourceStatus::success) {
    return map(acquired);
  }
  SubmissionPlan plan{};
  const auto planned = graph_.submit(generation, stream_id, kind, visibility, dependencies, &plan);
  if (planned != StreamStatus::success) {
    static_cast<void>(resources_.cancel(resource));
    return map(planned);
  }
  const auto completion_value = next_completion_value_;
  const auto submitted = resources_.submit(resource, completion_value);
  if (submitted != CommandResourceStatus::success) {
    static_cast<void>(resources_.cancel(resource));
    return map(submitted);
  }
  out_submission->plan = plan;
  out_submission->resource = resource;
  out_submission->completion_value = completion_value;
  ++next_completion_value_;
  return QueueSubmissionStatus::success;
}

QueueSubmissionStatus QueueSubmissionLedger::complete(std::uint64_t generation,
                                                      std::uint64_t completed_value) noexcept {
  std::lock_guard lock(mutex_);
  if (generation == 0U) {
    return QueueSubmissionStatus::invalid_argument;
  }
  return map(resources_.recycle(generation, completed_value));
}

QueueSubmissionStatus QueueSubmissionLedger::discard(
    const QueueSubmission& submission) noexcept {
  std::lock_guard lock(mutex_);
  return map(resources_.discard(submission.resource));
}

QueueSubmissionStatus QueueSubmissionLedger::reconfigure(std::uint64_t generation) noexcept {
  std::lock_guard lock(mutex_);
  if (generation == 0U) {
    return QueueSubmissionStatus::invalid_argument;
  }
  if (generation <= generation_) {
    return QueueSubmissionStatus::stale_generation;
  }
  if (resources_.in_flight_count() != 0U) {
    return QueueSubmissionStatus::busy;
  }
  const auto status = resources_.reconfigure(generation);
  if (status != CommandResourceStatus::success) {
    return map(status);
  }
  graph_ = StreamGraph(generation);
  generation_ = generation;
  next_completion_value_ = 1U;
  return QueueSubmissionStatus::success;
}

std::size_t QueueSubmissionLedger::available_count() const noexcept {
  std::lock_guard lock(mutex_);
  return resources_.available_count();
}

std::size_t QueueSubmissionLedger::in_flight_count() const noexcept {
  std::lock_guard lock(mutex_);
  return resources_.in_flight_count();
}

std::uint64_t QueueSubmissionLedger::generation() const noexcept {
  std::lock_guard lock(mutex_);
  return generation_;
}

std::uint64_t QueueSubmissionLedger::next_completion_value() const noexcept {
  std::lock_guard lock(mutex_);
  return next_completion_value_;
}

const char* queue_submission_status_string(QueueSubmissionStatus status) noexcept {
  switch (status) {
  case QueueSubmissionStatus::success:
    return "success";
  case QueueSubmissionStatus::invalid_argument:
    return "invalid-argument";
  case QueueSubmissionStatus::stale_generation:
    return "stale-generation";
  case QueueSubmissionStatus::unknown_stream:
    return "unknown-stream";
  case QueueSubmissionStatus::invalid_visibility:
    return "invalid-visibility";
  case QueueSubmissionStatus::dependency_not_ready:
    return "dependency-not-ready";
  case QueueSubmissionStatus::duplicate_dependency:
    return "duplicate-dependency";
  case QueueSubmissionStatus::too_many_dependencies:
    return "too-many-dependencies";
  case QueueSubmissionStatus::resource_exhausted:
    return "resource-exhausted";
  case QueueSubmissionStatus::busy:
    return "busy";
  case QueueSubmissionStatus::invalid_timeline:
    return "invalid-timeline";
  case QueueSubmissionStatus::timeline_exhausted:
    return "timeline-exhausted";
  case QueueSubmissionStatus::not_found:
    return "not-found";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
