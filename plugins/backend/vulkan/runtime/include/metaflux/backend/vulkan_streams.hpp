#ifndef METAFLUX_BACKEND_VULKAN_STREAMS_HPP
#define METAFLUX_BACKEND_VULKAN_STREAMS_HPP

#include <array>
#include <cstdint>
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

[[nodiscard]] const char* stream_status_string(StreamStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
