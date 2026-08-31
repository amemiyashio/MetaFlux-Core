#ifndef METAFLUX_TRANSPORT_CDEV_WORKER_HPP
#define METAFLUX_TRANSPORT_CDEV_WORKER_HPP

#include <cstddef>
#include <cstdint>

#include "metaflux/backend/api.h"
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

/*
 * A resolver owns cdev object-table semantics. It returns a backend-neutral
 * launch description whose argument bytes are already encoded in the worker's
 * payload arena and remain borrowed for the duration of submit. The cdev
 * descriptor reserves the kernel slot for the primary entry point.
 */
struct CdevLaunchResolution final {
  mf_backend_module_v1 module = 0U;
  std::uint64_t kernel_id = 0U;
  std::uint64_t argument_offset = 0U;
  std::uint64_t argument_size = 0U;
  std::uint32_t grid[3]{};
  std::uint32_t block[3]{};
  std::uint32_t dynamic_shared_bytes = 0U;
  std::uint32_t reserved_word = 0U;
};

using CdevLaunchResolver = mf_shared_status_v1 (*)(void* context,
                                                   const mf_ring_descriptor_v1* request,
                                                   CdevLaunchResolution* out) noexcept;

/* Object-table result for a region COPY argument block. */
struct CdevCopyResolution final {
  mf_backend_memory_v1 destination = 0U;
  std::uint64_t destination_offset = 0U;
  mf_backend_memory_v1 source = 0U;
  std::uint64_t source_offset = 0U;
  std::uint64_t byte_count = 0U;
  std::uint32_t reserved_word = 0U;
};

using CdevCopyResolver = mf_shared_status_v1 (*)(void* context,
                                                 const mf_ring_descriptor_v1* request,
                                                 CdevCopyResolution* out) noexcept;

/*
 * A backend operation lease keeps the backend instance, queue, and memory
 * handles alive until the ABI call returns, or until its nonzero completion
 * event is observed complete. The lease owner may reject a new operation
 * during generation replacement or teardown.
 */
using CdevBackendLeaseAcquire = mf_shared_status_v1 (*)(void* context) noexcept;
using CdevBackendLeaseRelease = void (*)(void* context) noexcept;

/* The worker borrows these backend-owned handles for COPY and launch calls. */
struct CdevBackendBinding final {
  const mf_backend_api_v1* api = nullptr;
  mf_backend_instance_v1 instance = 0U;
  mf_backend_queue_v1 queue = 0U;
  mf_backend_memory_v1 memory = 0U;
  mf_backend_event_v1 completion_event = 0U;
  CdevCopyResolver copy_resolver = nullptr;
  void* copy_context = nullptr;
  CdevLaunchResolver launch_resolver = nullptr;
  void* launch_context = nullptr;
  CdevBackendLeaseAcquire lease_acquire = nullptr;
  CdevBackendLeaseRelease lease_release = nullptr;
  void* lease_context = nullptr;
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
  CdevWorker(WorkerQueueView view, CdevBackendBinding backend) noexcept
      : view_(view), backend_(backend) {}

  void bind_backend(CdevBackendBinding backend) noexcept { backend_ = backend; }
  [[nodiscard]] bool backend_bound() const noexcept;
  [[nodiscard]] bool backend_operation_pending() const noexcept { return pending_.active; }

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
  static bool valid_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_backend_event(const CdevBackendBinding& backend) noexcept;
  static bool valid_copy_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_region_copy_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_launch_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_backend_lease(const CdevBackendBinding& backend) noexcept;
  static std::int32_t map_backend_status(mf_backend_status_v1 status) noexcept;
  [[nodiscard]] mf_backend_status_v1 dispatch_copy(std::uint64_t base, std::uint64_t destination,
                                                   std::uint64_t source,
                                                   std::uint64_t byte_count) const noexcept;
  [[nodiscard]] mf_backend_status_v1
  dispatch_region_copy(const CdevCopyResolution& resolution) const noexcept;
  [[nodiscard]] mf_shared_status_v1
  dispatch_launch(const mf_ring_descriptor_v1& request) const noexcept;
  [[nodiscard]] WorkerResult finish_backend_request(const mf_ring_descriptor_v1& request,
                                                    mf_shared_status_v1 status) noexcept;
  [[nodiscard]] WorkerResult progress_pending() noexcept;
  [[nodiscard]] mf_shared_status_v1 acquire_backend_lease() const noexcept;
  void release_backend_lease() const noexcept;
  bool drain_lifecycle() noexcept;
  WorkerResult complete(const mf_ring_descriptor_v1& request, std::int32_t status) noexcept;

  WorkerQueueView view_{};
  std::uint64_t timeline_ = 0U;
  bool lifecycle_online_ = true;
  bool lifecycle_accepting_ = true;
  CdevBackendBinding backend_{};
  struct PendingOperation final {
    bool active = false;
    mf_ring_descriptor_v1 request{};
    mf_backend_event_v1 event = 0U;
  } pending_{};
};

} // namespace metaflux::transport::cdev

#endif
