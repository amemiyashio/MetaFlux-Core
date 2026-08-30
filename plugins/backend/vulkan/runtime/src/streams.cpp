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
  const auto position = std::find_if(
      streams_.begin(), streams_.end(), [stream_id](const StreamState& stream) {
        return stream.id == stream_id;
      });
  return position == streams_.end() ? nullptr : &*position;
}

const StreamGraph::StreamState* StreamGraph::find_stream(std::uint64_t stream_id) const noexcept {
  const auto position = std::find_if(
      streams_.begin(), streams_.end(), [stream_id](const StreamState& stream) {
        return stream.id == stream_id;
      });
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

} // namespace metaflux::backend::vulkan
