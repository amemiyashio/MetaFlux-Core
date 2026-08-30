#ifndef METAFLUX_TRANSPORT_CDEV_WORKER_HPP
#define METAFLUX_TRANSPORT_CDEV_WORKER_HPP

#include <cstddef>
#include <cstdint>

#include "metaflux/runtime/lifecycle.hpp"
#include "metaflux/shared/device.h"

namespace metaflux::transport::cdev {

struct WorkerQueueView final {
  mf_ring_header_v1* submission = nullptr;
  mf_ring_header_v1* completion = nullptr;
  std::uint8_t* payload = nullptr;
  std::uint64_t payload_size = 0U;
  std::uint64_t generation = 0U;
};

enum class WorkerResult : std::uint32_t {
  Idle = 0U,
  Completed = 1U,
  Backpressure = 2U,
  Malformed = 3U,
};

class CdevWorker final {
public:
  explicit CdevWorker(WorkerQueueView view) noexcept : view_(view) {}

  WorkerResult consume_once() noexcept;
  std::uint32_t drain(std::uint32_t maximum) noexcept;
  std::uint64_t timeline() const noexcept { return timeline_; }

  [[nodiscard]] bool
  attach_lifecycle(metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept;
  [[nodiscard]] metaflux::runtime::lifecycle::Mirror lifecycle_mirror() noexcept;
  [[nodiscard]] bool lifecycle_online() const noexcept { return lifecycle_online_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return view_.generation; }

private:
  static bool queue_readable(const mf_ring_header_v1* header) noexcept;
  static bool queue_writable(const mf_ring_header_v1* header) noexcept;
  static bool consume(mf_ring_header_v1* header, mf_ring_descriptor_v1* out) noexcept;
  static bool produce(mf_ring_header_v1* header, const mf_ring_descriptor_v1& descriptor) noexcept;
  static bool lifecycle_prepare(void* context,
                                const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_quiesce(void* context,
                                const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_drain(void* context,
                              const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_commit(void* context,
                               const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_abort(void* context,
                              const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static void lifecycle_lost(void* context,
                             const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  bool drain_lifecycle() noexcept;
  WorkerResult complete(const mf_ring_descriptor_v1& request, std::int32_t status) noexcept;

  WorkerQueueView view_{};
  std::uint64_t timeline_ = 0U;
  bool lifecycle_online_ = true;
  bool lifecycle_accepting_ = true;
};

} // namespace metaflux::transport::cdev

#endif
