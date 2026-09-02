#ifndef METAFLUX_TRANSPORT_CDEV_WORKER_HPP
#define METAFLUX_TRANSPORT_CDEV_WORKER_HPP

#include <cstddef>
#include <cstdint>

#include "metaflux/backend/api.h"
#include "metaflux/runtime/lifecycle_dispatch.hpp"
#include "metaflux/shared/device.h"

namespace metaflux::transport::cdev {

struct WorkerQueueView final {
  mf_ring_header_v1* submission = nullptr;
  mf_ring_header_v1* completion = nullptr;
  std::uint8_t* payload = nullptr;
  std::uint64_t payload_size = 0U;
  std::uint64_t generation = 0U;
};

struct CdevWorkerLeaseView final {
  mf_registry_view_id_v1 registry_view_id{};
  std::uint64_t identity_record_id = 0U;
  std::uint64_t generation = 0U;
  std::uint64_t lease_id = 0U;
  std::uint64_t mapping_size = 0U;
};

struct CdevBackendBinding;

/* A cdev registration handle remains transport-owned and never crosses the backend ABI. */
struct CdevRegisteredMemory final {
  void* address = nullptr;
  std::uint64_t byte_count = 0U;
  std::uint64_t handle = 0U;
  std::uint64_t generation = 0U;
};

/*
 * Owns one generation-bound worker lease, paired shared queue mapping, and the
 * optional payload mapping granted by that lease. The data-plane payload
 * remains owned by its data fd; close() releases the worker-side mappings and
 * then revokes the kernel lease through the control descriptor.
 */
class CdevWorkerSession final {
public:
  CdevWorkerSession() noexcept = default;
  ~CdevWorkerSession();

  CdevWorkerSession(const CdevWorkerSession&) = delete;
  CdevWorkerSession& operator=(const CdevWorkerSession&) = delete;
  CdevWorkerSession(CdevWorkerSession&& other) noexcept;
  CdevWorkerSession& operator=(CdevWorkerSession&& other) noexcept;

  static mf_shared_status_v1 open(const char* control_path,
                                  mf_registry_view_id_v1 expected_view_id,
                                  std::uint64_t expected_generation,
                                  CdevWorkerSession& out) noexcept;
  static mf_shared_status_v1 open_current(const char* control_path,
                                          CdevWorkerSession& out) noexcept;

  void close() noexcept;
  [[nodiscard]] mf_shared_status_v1 map_payload(std::uint64_t mapping_size) noexcept;
  [[nodiscard]] mf_shared_status_v1 map_current_payload() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return control_fd_ >= 0; }
  [[nodiscard]] int control_fd() const noexcept { return control_fd_; }
  [[nodiscard]] int data_fd() const noexcept { return data_fd_; }
  [[nodiscard]] const CdevWorkerLeaseView& lease() const noexcept { return lease_; }
  [[nodiscard]] std::uint8_t* payload_mapping() const noexcept {
    return static_cast<std::uint8_t*>(payload_mapping_);
  }
  [[nodiscard]] std::uint64_t payload_mapping_size() const noexcept {
    return payload_mapping_size_;
  }
  [[nodiscard]] mf_shared_status_v1 register_memory(void* address, std::uint64_t byte_count,
                                                     std::uint32_t flags,
                                                     CdevRegisteredMemory& out) noexcept;
  void close_registered_memory(CdevRegisteredMemory& memory) noexcept;
  [[nodiscard]] WorkerQueueView queue_view(std::uint8_t* payload = nullptr,
                                            std::uint64_t payload_size = 0U) const noexcept;

private:
  static mf_shared_status_v1 open_internal(const char* control_path,
                                           mf_registry_view_id_v1 expected_view_id,
                                           std::uint64_t expected_generation,
                                           bool discover_current,
                                           CdevWorkerSession& out) noexcept;
  [[nodiscard]] mf_shared_status_v1 query_payload_size(std::uint64_t& out_size) const noexcept;

  int control_fd_ = -1;
  int data_fd_ = -1;
  void* mapping_ = nullptr;
  std::uint64_t mapping_size_ = 0U;
  void* payload_mapping_ = nullptr;
  std::uint64_t payload_mapping_size_ = 0U;
  CdevWorkerLeaseView lease_{};
};

using CdevBackendMemoryRetain = mf_shared_status_v1 (*)(void* context,
                                                        mf_backend_memory_v1 memory) noexcept;
using CdevBackendMemoryRelease = void (*)(void* context,
                                          mf_backend_memory_v1 memory) noexcept;

/* A resolver-owned backend memory reference held across one worker operation. */
struct CdevBackendMemoryReference final {
  mf_backend_memory_v1 handle = 0U;
  CdevBackendMemoryRetain retain = nullptr;
  CdevBackendMemoryRelease release = nullptr;
  void* context = nullptr;
};

inline constexpr std::size_t kCdevLaunchMemoryReferenceCapacity = 64U;

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
  std::uint32_t memory_reference_count = 0U;
  CdevBackendMemoryReference memory_references[kCdevLaunchMemoryReferenceCapacity]{};
};

using CdevLaunchResolver = mf_shared_status_v1 (*)(void* context,
                                                   const mf_ring_descriptor_v1* request,
                                                   CdevLaunchResolution* out) noexcept;

/*
 * Resolver-side import seam for a daemon-owned registered range. The resolver
 * validates the object identity and range before invoking this callback. The
 * returned reference must keep the imported backend handle alive until its
 * release callback runs; the caller-owned range must outlive that reference.
 */
using CdevBackendMemoryImporter = mf_shared_status_v1 (*)(
    void* context, mf_backend_instance_v1 instance, mf_backend_context_v1 backend_context,
    void* address, std::uint64_t byte_count, CdevBackendMemoryReference* out) noexcept;

/* Borrowed object-table view returned after ID, generation, kind, and access checks. */
struct CdevObjectTableView final {
  std::uint64_t object_id = 0U;
  std::uint64_t object_generation = 0U;
  std::uint32_t object_kind = 0U;
  std::uint32_t access_flags = 0U;
  const std::uint8_t* data = nullptr;
  void* address = nullptr;
  std::uint64_t byte_size = 0U;
  CdevBackendMemoryReference backend_reference{};
};

/*
 * The daemon supplies this lookup from its authoritative object table. A
 * zero expected_kind accepts either device or host memory; every other value
 * requires an exact object kind in the returned view.
 */
using CdevObjectTableLookup = mf_shared_status_v1 (*)(
    void* context, std::uint64_t object_id, std::uint64_t object_generation,
    std::uint32_t expected_kind, bool for_write, CdevObjectTableView* out) noexcept;

struct CdevCopyResolution;

/*
 * Converts a daemon object-table region COPY into the worker's backend-neutral
 * resolution. It is stateless with respect to objects: returned pointers and
 * backend references are borrowed until the worker's retain/release contract
 * completes the operation.
 */
class CdevObjectTableResolver final {
public:
  CdevObjectTableResolver() noexcept = default;
  CdevObjectTableResolver(void* object_context, CdevObjectTableLookup lookup,
                          void* importer_context, CdevBackendMemoryImporter importer,
                          mf_backend_instance_v1 instance,
                          mf_backend_context_v1 backend_context) noexcept;

  void configure(void* object_context, CdevObjectTableLookup lookup, void* importer_context,
                 CdevBackendMemoryImporter importer, mf_backend_instance_v1 instance,
                 mf_backend_context_v1 backend_context) noexcept;

  [[nodiscard]] mf_shared_status_v1 resolve_copy(const mf_ring_descriptor_v1* request,
                                                 CdevCopyResolution* out) const noexcept;
  [[nodiscard]] static mf_shared_status_v1 callback(void* context,
                                                    const mf_ring_descriptor_v1* request,
                                                    CdevCopyResolution* out) noexcept;

private:
  [[nodiscard]] mf_shared_status_v1 resolve_memory(const mf_argument_entry_v1& entry,
                                                   bool for_write, std::uint64_t byte_count,
                                                   CdevBackendMemoryReference* out_reference,
                                                   std::uint64_t* out_offset) const noexcept;

  void* object_context_ = nullptr;
  CdevObjectTableLookup lookup_ = nullptr;
  void* importer_context_ = nullptr;
  CdevBackendMemoryImporter importer_ = nullptr;
  mf_backend_instance_v1 instance_ = 0U;
  mf_backend_context_v1 backend_context_ = 0U;
};

/* Object-table result for a region COPY argument block. */
struct CdevCopyResolution final {
  mf_backend_memory_v1 destination = 0U;
  std::uint64_t destination_offset = 0U;
  CdevBackendMemoryReference destination_reference{};
  mf_backend_memory_v1 source = 0U;
  std::uint64_t source_offset = 0U;
  CdevBackendMemoryReference source_reference{};
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
using CdevBackendBindingRetire = void (*)(void* context) noexcept;
using CdevWorkerRebind = bool (*)(void* context, std::uint64_t generation,
                                  WorkerQueueView* out_view,
                                  CdevBackendBinding* out_backend) noexcept;

/* The worker borrows these backend-owned handles for COPY and launch calls. */
struct CdevBackendBinding final {
  const mf_backend_api_v1* api = nullptr;
  mf_backend_instance_v1 instance = 0U;
  mf_backend_queue_v1 queue = 0U;
  mf_backend_memory_v1 memory = 0U;
  CdevBackendMemoryReference memory_reference{};
  mf_backend_event_v1 completion_event = 0U;
  CdevCopyResolver copy_resolver = nullptr;
  void* copy_context = nullptr;
  CdevLaunchResolver launch_resolver = nullptr;
  void* launch_context = nullptr;
  CdevBackendLeaseAcquire lease_acquire = nullptr;
  CdevBackendLeaseRelease lease_release = nullptr;
  void* lease_context = nullptr;
  /*
   * The owner is notified only after this binding has no current or pending
   * operation references. It may then destroy the generation-bound backend.
   */
  CdevBackendBindingRetire retire = nullptr;
  void* retire_context = nullptr;
  /* A binding is usable only while it names the worker's current generation. */
  std::uint64_t generation = 0U;
  /* Stage a complete queue/backend pair before a lifecycle commit. */
  CdevWorkerRebind rebind = nullptr;
  void* rebind_context = nullptr;
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

  [[nodiscard]] bool bind_backend(CdevBackendBinding backend) noexcept;
  [[nodiscard]] bool backend_bound() const noexcept;
  [[nodiscard]] bool backend_operation_pending() const noexcept { return pending_.active; }

  WorkerResult consume_once() noexcept;
  std::uint32_t drain(std::uint32_t maximum) noexcept;
  std::uint64_t timeline() const noexcept { return timeline_; }

  [[nodiscard]] bool
  attach_lifecycle(metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept;
  [[nodiscard]] metaflux::runtime::lifecycle::NormalizationResult report_disconnect(
      metaflux::runtime::lifecycle::Coordinator& coordinator, std::uint64_t request_id,
      std::uint64_t deadline_tick,
      metaflux::runtime::lifecycle::ResultDetails& out) noexcept;
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
  [[nodiscard]] bool stage_rebind(std::uint64_t generation) noexcept;
  void discard_staged_rebind() noexcept;
  static bool valid_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_backend_event(const CdevBackendBinding& backend) noexcept;
  static bool valid_copy_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_region_copy_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_memory_reference(mf_backend_memory_v1 memory,
                                     const CdevBackendMemoryReference& reference) noexcept;
  static bool valid_optional_memory_reference(
      mf_backend_memory_v1 memory, const CdevBackendMemoryReference& reference) noexcept;
  static bool valid_copy_resolution(const CdevCopyResolution& resolution) noexcept;
  static bool valid_launch_resolution(const CdevLaunchResolution& resolution) noexcept;
  static bool valid_backend_cancellation(const CdevBackendBinding& backend) noexcept;
  static bool valid_launch_backend(const CdevBackendBinding& backend) noexcept;
  static bool valid_backend_lease(const CdevBackendBinding& backend) noexcept;
  static bool same_backend_binding(const CdevBackendBinding& left,
                                   const CdevBackendBinding& right) noexcept;
  static void retire_backend_binding(const CdevBackendBinding& backend) noexcept;
  [[nodiscard]] bool backend_matches_generation() const noexcept;
  static mf_shared_status_v1 retain_copy_references(CdevCopyResolution& resolution) noexcept;
  static void release_copy_references(const CdevCopyResolution& resolution) noexcept;
  static mf_shared_status_v1 retain_launch_references(CdevLaunchResolution& resolution) noexcept;
  static void release_launch_references(const CdevLaunchResolution& resolution) noexcept;
  static mf_shared_status_v1 retain_memory_reference(
      const CdevBackendMemoryReference& reference) noexcept;
  static void release_memory_reference(const CdevBackendMemoryReference& reference) noexcept;
  static std::int32_t map_backend_status(mf_backend_status_v1 status) noexcept;
  [[nodiscard]] mf_backend_status_v1 dispatch_copy(std::uint64_t base, std::uint64_t destination,
                                                   std::uint64_t source,
                                                   std::uint64_t byte_count) const noexcept;
  [[nodiscard]] mf_backend_status_v1
  dispatch_region_copy(const CdevCopyResolution& resolution) const noexcept;
  [[nodiscard]] mf_shared_status_v1
  dispatch_launch(const mf_ring_descriptor_v1& request, CdevLaunchResolution* out) const noexcept;
  [[nodiscard]] WorkerResult finish_backend_request(const mf_ring_descriptor_v1& request,
                                                    mf_shared_status_v1 status,
                                                    const CdevCopyResolution* resolution = nullptr,
                                                    const CdevBackendMemoryReference* memory_reference =
                                                        nullptr,
                                                    const CdevLaunchResolution* launch_resolution =
                                                        nullptr) noexcept;
  void report_backend_loss(
      const metaflux::runtime::lifecycle::ExternalEvent& event) noexcept;
  [[nodiscard]] WorkerResult progress_pending() noexcept;
  [[nodiscard]] mf_shared_status_v1 acquire_backend_lease() const noexcept;
  bool cancel_pending() noexcept;
  void release_backend_lease(const CdevBackendBinding& backend) const noexcept;
  void release_backend_lease() const noexcept;
  bool drain_lifecycle() noexcept;
  WorkerResult complete(const mf_ring_descriptor_v1& request, std::int32_t status) noexcept;

  WorkerQueueView view_{};
  std::uint64_t timeline_ = 0U;
  bool lifecycle_online_ = true;
  bool lifecycle_accepting_ = true;
  CdevBackendBinding backend_{};
  WorkerQueueView staged_view_{};
  CdevBackendBinding staged_backend_{};
  bool staged_rebind_ = false;
  metaflux::runtime::lifecycle::Coordinator* lifecycle_coordinator_ = nullptr;
  struct PendingOperation final {
    bool active = false;
    mf_ring_descriptor_v1 request{};
    CdevBackendBinding backend{};
    CdevCopyResolution resolution{};
    bool has_memory_references = false;
    CdevBackendMemoryReference memory_reference{};
    bool has_backend_memory_reference = false;
    CdevLaunchResolution launch_resolution{};
    bool has_launch_memory_references = false;
    bool cancellation_requested = false;
    bool force_device_lost = false;
    bool retire_backend = false;
    mf_backend_event_v1 event = 0U;
    metaflux::runtime::lifecycle::ExternalEvent disconnect_event{};
    bool has_disconnect_event = false;
  } pending_{};
};

} // namespace metaflux::transport::cdev

#endif
