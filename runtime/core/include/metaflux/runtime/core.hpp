#ifndef METAFLUX_RUNTIME_CORE_HPP
#define METAFLUX_RUNTIME_CORE_HPP

#include <cstdint>
#include <span>

#include "metaflux/shared/device.h"

namespace metaflux::runtime {

struct FenceSnapshot final {
  std::uint64_t identity_record_id;
  std::uint64_t lifecycle_sequence;
  std::uint64_t epoch;
  std::uint64_t effective_quota_bytes;
  std::uint64_t policy_bits;
  std::uint32_t device_state;
};

struct TelemetrySnapshot final {
  std::uint64_t identity_record_id;
  std::uint64_t observed_lifecycle_sequence;
  std::uint64_t committed_work_items;
  std::uint64_t completed_work_items;
  std::uint64_t active_time_ns;
  std::uint64_t memory_active_time_ns;
  std::uint64_t memory_used_bytes;
  std::uint64_t memory_capacity_bytes;
  std::uint64_t sample_time_ns;
  std::uint64_t snapshot_sequence;
};

struct AdmissionLeaseToken final {
  mf_registry_view_id_v1 registry_view_id;
  std::uint64_t identity_record_id;
  std::uint64_t view_validation_generation;
  std::uint32_t device_validation_generation;
  std::uint32_t attempt_slot;
  std::uint32_t attempt_tag;
  std::uint32_t lease_slot;
  std::uint32_t lease_tag;
};

struct LifecycleRangeToken final {
  mf_registry_view_id_v1 registry_view_id;
  std::uint64_t range_begin;
  std::uint64_t range_end;
  std::uint64_t token_generation;
  std::uint32_t range_slot;
  std::uint32_t range_tag;
};

enum class RecoveryFaultPoint : std::uint32_t {
  None = 0,
  AttemptInitializing = 1,
  LeaseCommitting = 2,
  FenceActive = 3,
  TelemetryActiveOdd = 4,
  FenceOdd = 5,
  FenceCommitted = 6,
  AttemptClaimed = 7,
  LeaseClaimed = 8,
  RecoveryFenceBeforeExactCas = 9,
  UpdateReopened = 10,
  UpdatePublicationPublished = 11,
  UpdatePublisherReleased = 12,
  UpdateRangeRetired = 13,
  RecoveryPayloadHazardHeld = 14,
  UpdateTerminal = 15,
};

class RegistryView final {
public:
  [[nodiscard]] static std::uint32_t bootstrap_client_protocol_abi_version() noexcept;

  [[nodiscard]] static mf_shared_status_v1 required_mapping_size(std::uint32_t device_count,
                                                                 std::uint64_t& out_size) noexcept;

  [[nodiscard]] static mf_shared_status_v1
  required_recovery_mapping_size(std::uint32_t device_count, std::uint64_t& out_size) noexcept;

  [[nodiscard]] static mf_shared_status_v1
  required_legacy_mapping_size(std::uint32_t device_count, std::uint64_t& out_size) noexcept;

  [[nodiscard]] static mf_shared_status_v1
  current_owner_identity(mf_owner_identity_v1& out_owner) noexcept;

  [[nodiscard]] static mf_shared_status_v1
  initialize(void* mapping, std::uint64_t mapping_size, mf_registry_view_id_v1 view_id,
             std::uint64_t process_view_revision,
             std::span<const mf_virtual_device_identity_v1> identities,
             std::span<const FenceSnapshot> initial_fences, RegistryView& out_view) noexcept;

  [[nodiscard]] static mf_shared_status_v1 attach(void* mapping, std::uint64_t mapping_size,
                                                  RegistryView& out_view) noexcept;

  [[nodiscard]] mf_shared_status_v1 make_handle(std::uint32_t device_index, std::uint64_t object_id,
                                                std::uint64_t object_generation,
                                                std::uint32_t object_type,
                                                mf_generation_handle_v1& out_handle) const noexcept;

  [[nodiscard]] mf_shared_status_v1 validate_device(const mf_generation_handle_v1& handle,
                                                    FenceSnapshot& out_fence) const noexcept;

  [[nodiscard]] mf_shared_status_v1
  publish_fence(std::uint32_t device_index, std::uint32_t expected_validation_generation,
                const FenceSnapshot& intended_fence,
                std::uint32_t& out_validation_generation) noexcept;

  [[nodiscard]] mf_shared_status_v1 mark_device_lost(std::uint32_t device_index,
                                                     std::uint64_t lifecycle_sequence,
                                                     std::uint64_t epoch) noexcept;

  [[nodiscard]] mf_shared_status_v1
  publish_telemetry(std::span<const mf_virtual_device_telemetry_v1> rows) noexcept;

  [[nodiscard]] mf_shared_status_v1 begin_admission(std::uint32_t device_index,
                                                    std::uint64_t deadline_ns,
                                                    std::uint64_t operation_cookie,
                                                    std::uint64_t target_cookie,
                                                    AdmissionLeaseToken& out_token) noexcept;

  [[nodiscard]] mf_shared_status_v1 commit_admission(const AdmissionLeaseToken& token) noexcept;

  [[nodiscard]] mf_shared_status_v1 release_admission(const AdmissionLeaseToken& token) noexcept;

  [[nodiscard]] mf_shared_status_v1 help_admission(const AdmissionLeaseToken& token,
                                                   bool owner_may_still_run) noexcept;

  [[nodiscard]] mf_shared_status_v1 recover_owner(mf_owner_identity_v1 owner,
                                                  bool owner_may_still_run) noexcept;

  [[nodiscard]] mf_shared_status_v1 recover() noexcept;

  [[nodiscard]] mf_shared_status_v1
  reserve_lifecycle_range(std::uint64_t length, std::uint64_t deadline_ns,
                          LifecycleRangeToken& out_token) noexcept;

  [[nodiscard]] mf_shared_status_v1 retire_lifecycle_range(const LifecycleRangeToken& token,
                                                           std::uint64_t used_through) noexcept;

  [[nodiscard]] mf_shared_status_v1 read_telemetry(const mf_generation_handle_v1& handle,
                                                   TelemetrySnapshot& out_snapshot) const noexcept;

  [[nodiscard]] mf_shared_status_v1 close() noexcept;

  void set_fault_point_for_testing(RecoveryFaultPoint point) noexcept { fault_point_ = point; }

  [[nodiscard]] bool has_recovery_extension() const noexcept { return extension_ != nullptr; }

  [[nodiscard]] std::uint32_t device_count() const noexcept { return device_count_; }
  [[nodiscard]] mf_registry_view_id_v1 view_id() const noexcept { return view_id_; }

private:
  struct ViewControlSnapshot;

  [[nodiscard]] mf_shared_status_v1
  stable_view_control(ViewControlSnapshot& out_snapshot) const noexcept;
  [[nodiscard]] mf_shared_status_v1 stable_fence(std::uint32_t device_index,
                                                 FenceSnapshot& out_fence,
                                                 std::uint64_t& out_latch) const noexcept;
  [[nodiscard]] mf_shared_status_v1 write_fence(std::uint32_t device_index,
                                                const FenceSnapshot& intended_fence,
                                                std::uint64_t expected_device_control,
                                                std::uint64_t expected_fence_latch) noexcept;
  [[nodiscard]] std::uint32_t find_identity(std::uint64_t identity_record_id) const noexcept;

  [[nodiscard]] mf_shared_status_v1 quarantine() noexcept;
  [[nodiscard]] mf_shared_status_v1 recover_device_updates(mf_owner_identity_v1 owner) noexcept;
  [[nodiscard]] mf_shared_status_v1 recover_telemetry_publish(mf_owner_identity_v1 owner) noexcept;
  [[nodiscard]] mf_shared_status_v1
  publish_fence_recovery(std::uint32_t device_index, std::uint32_t expected_validation_generation,
                         const FenceSnapshot& intended_fence,
                         std::uint32_t& out_validation_generation) noexcept;
  [[nodiscard]] mf_shared_status_v1
  publish_telemetry_recovery(std::span<const mf_virtual_device_telemetry_v1> rows) noexcept;
  [[nodiscard]] mf_shared_status_v1 close_recovery() noexcept;

  void* mapping_ = nullptr;
  std::uint64_t mapping_size_ = 0;
  mf_shared_registry_header_v1* header_ = nullptr;
  mf_view_admission_control_v1* view_admission_ = nullptr;
  mf_registry_view_control_v1* view_control_ = nullptr;
  mf_virtual_device_identity_v1* identities_ = nullptr;
  mf_device_admission_control_v1* device_admission_ = nullptr;
  mf_virtual_device_lifecycle_fence_v1* fences_ = nullptr;
  mf_telemetry_control_v1* telemetry_control_ = nullptr;
  mf_virtual_device_telemetry_v1* telemetry_banks_[2] = {nullptr, nullptr};
  mf_shared_registry_extension_header_v1* extension_ = nullptr;
  mf_view_publisher_control_v1* view_publisher_ = nullptr;
  mf_telemetry_publisher_control_v1* telemetry_publisher_ = nullptr;
  mf_admission_attempt_record_v1* admission_attempts_ = nullptr;
  mf_admission_lease_record_v1* admission_leases_ = nullptr;
  mf_device_validation_update_record_v1* device_updates_ = nullptr;
  mf_lifecycle_range_record_v1* lifecycle_ranges_ = nullptr;
  mf_view_publish_record_v1* view_publish_records_ = nullptr;
  mf_telemetry_publish_record_v1* telemetry_publish_records_ = nullptr;
  mf_registry_view_id_v1 view_id_ = {0, 0};
  std::uint32_t device_count_ = 0;
  RecoveryFaultPoint fault_point_ = RecoveryFaultPoint::None;
};

[[nodiscard]] std::uint32_t bootstrap_client_protocol_abi_version() noexcept;

} // namespace metaflux::runtime

#endif
