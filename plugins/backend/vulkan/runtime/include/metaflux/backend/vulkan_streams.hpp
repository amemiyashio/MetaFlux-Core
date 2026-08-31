#ifndef METAFLUX_BACKEND_VULKAN_STREAMS_HPP
#define METAFLUX_BACKEND_VULKAN_STREAMS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace metaflux::backend::vulkan {

enum class StreamStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  stale_generation = 2,
  unknown_stream = 3,
  invalid_visibility = 4,
  dependency_not_ready = 5,
  duplicate_dependency = 6,
  too_many_dependencies = 7,
  timeline_exhausted = 8,
};

enum class OperationKind : std::uint32_t {
  copy = 1,
  launch = 2,
};

constexpr std::uint32_t kStageTransfer = 1U << 0U;
constexpr std::uint32_t kStageCompute = 1U << 1U;
constexpr std::uint32_t kAccessTransferRead = 1U << 0U;
constexpr std::uint32_t kAccessTransferWrite = 1U << 1U;
constexpr std::uint32_t kAccessShaderRead = 1U << 2U;
constexpr std::uint32_t kAccessShaderWrite = 1U << 3U;

struct Visibility {
  std::uint32_t stage_mask = 0;
  std::uint32_t access_mask = 0;
};

struct Dependency {
  std::uint64_t stream_id = 0;
  std::uint64_t timeline_value = 0;
};

constexpr std::size_t kMaxDependencies = 8U;

struct SubmissionPlan {
  std::uint64_t sequence = 0;
  std::uint64_t generation = 0;
  std::uint64_t stream_id = 0;
  OperationKind kind = OperationKind::copy;
  Visibility visibility{};
  std::uint32_t dependency_count = 0;
  std::array<Dependency, kMaxDependencies> dependencies{};
};

class StreamGraph final {
public:
  explicit StreamGraph(std::uint64_t generation = 0) noexcept : generation_(generation) {}

  [[nodiscard]] StreamStatus create_stream(std::uint64_t stream_id);
  [[nodiscard]] StreamStatus submit(std::uint64_t generation, std::uint64_t stream_id,
                                    OperationKind kind, Visibility visibility,
                                    std::span<const Dependency> dependencies,
                                    SubmissionPlan* out_plan);
  [[nodiscard]] bool has_stream(std::uint64_t stream_id) const noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint64_t next_sequence() const noexcept { return next_sequence_; }

private:
  struct StreamState {
    std::uint64_t id = 0;
    std::uint64_t last_sequence = 0;
  };

  [[nodiscard]] StreamState* find_stream(std::uint64_t stream_id) noexcept;
  [[nodiscard]] const StreamState* find_stream(std::uint64_t stream_id) const noexcept;
  [[nodiscard]] static bool valid_visibility(OperationKind kind, Visibility visibility) noexcept;

  std::uint64_t generation_ = 0;
  std::uint64_t next_sequence_ = 1;
  std::vector<StreamState> streams_;
};

enum class CommandResourceStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  stale_generation = 2,
  exhausted = 3,
  busy = 4,
  not_found = 5,
  invalid_timeline = 6,
  resource_id_exhausted = 7,
  timeline_exhausted = 8,
};

struct CommandResource {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
  std::uint64_t stream_id = 0;
  std::uint64_t sequence = 0;
  std::uint64_t completion_value = 0;
};

// Host-independent ownership model for command buffers and their completion
// timeline. Vulkan handles are attached by the future queue-submit adapter.
class CommandResourcePool final {
public:
  explicit CommandResourcePool(std::size_t capacity = 0U, std::uint64_t generation = 0U) noexcept
      : slots_(capacity), generation_(generation) {}

  [[nodiscard]] CommandResourceStatus acquire(std::uint64_t generation, std::uint64_t stream_id,
                                              CommandResource* out_resource) noexcept;
  [[nodiscard]] CommandResourceStatus submit(const CommandResource& resource,
                                             std::uint64_t completion_value) noexcept;
  [[nodiscard]] CommandResourceStatus cancel(const CommandResource& resource) noexcept;
  [[nodiscard]] CommandResourceStatus recycle(std::uint64_t generation,
                                              std::uint64_t completed_value) noexcept;
  [[nodiscard]] CommandResourceStatus reconfigure(std::uint64_t generation) noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
  [[nodiscard]] std::size_t available_count() const noexcept;
  [[nodiscard]] std::size_t in_flight_count() const noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint64_t next_sequence() const noexcept { return next_sequence_; }

private:
  enum class SlotState : std::uint8_t { available = 0, acquired = 1, submitted = 2 };

  struct Slot final {
    SlotState state = SlotState::available;
    CommandResource resource{};
  };

  [[nodiscard]] Slot* find_slot(const CommandResource& resource) noexcept;
  [[nodiscard]] const Slot* find_slot(const CommandResource& resource) const noexcept;

  std::vector<Slot> slots_;
  std::uint64_t generation_ = 0;
  std::uint64_t next_id_ = 1;
  std::uint64_t next_sequence_ = 1;
  std::uint64_t last_submission_timeline_ = 0;
  std::uint64_t last_completed_timeline_ = 0;
};

[[nodiscard]] const char* stream_status_string(StreamStatus status) noexcept;
[[nodiscard]] const char* command_resource_status_string(CommandResourceStatus status) noexcept;

enum class QueueSubmissionStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  stale_generation = 2,
  unknown_stream = 3,
  invalid_visibility = 4,
  dependency_not_ready = 5,
  duplicate_dependency = 6,
  too_many_dependencies = 7,
  resource_exhausted = 8,
  busy = 9,
  invalid_timeline = 10,
  timeline_exhausted = 11,
  not_found = 12,
};

struct QueueSubmission final {
  SubmissionPlan plan{};
  CommandResource resource{};
  std::uint64_t completion_value = 0;
};

// Host-independent transaction boundary for a planned graph operation and one
// finite command resource. A later adapter translates an accepted tuple to
// Vulkan command recording and vkQueueSubmit2.
class QueueSubmissionLedger final {
public:
  explicit QueueSubmissionLedger(std::size_t resource_capacity = 0U,
                                 std::uint64_t generation = 0U) noexcept
      : graph_(generation), resources_(resource_capacity, generation), generation_(generation) {}

  [[nodiscard]] QueueSubmissionStatus create_stream(std::uint64_t stream_id);
  [[nodiscard]] QueueSubmissionStatus submit(std::uint64_t generation, std::uint64_t stream_id,
                                             OperationKind kind, Visibility visibility,
                                             std::span<const Dependency> dependencies,
                                             QueueSubmission* out_submission);
  [[nodiscard]] QueueSubmissionStatus complete(std::uint64_t generation,
                                               std::uint64_t completed_value) noexcept;
  [[nodiscard]] QueueSubmissionStatus reconfigure(std::uint64_t generation) noexcept;
  [[nodiscard]] std::size_t available_count() const noexcept;
  [[nodiscard]] std::size_t in_flight_count() const noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept;
  [[nodiscard]] std::uint64_t next_completion_value() const noexcept;

private:
  [[nodiscard]] static QueueSubmissionStatus map(StreamStatus status) noexcept;
  [[nodiscard]] static QueueSubmissionStatus map(CommandResourceStatus status) noexcept;

  mutable std::mutex mutex_;
  StreamGraph graph_;
  CommandResourcePool resources_;
  std::uint64_t generation_ = 0;
  std::uint64_t next_completion_value_ = 1;
};

[[nodiscard]] const char* queue_submission_status_string(QueueSubmissionStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
