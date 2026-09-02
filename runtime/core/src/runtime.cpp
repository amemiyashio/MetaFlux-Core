#include "metaflux/runtime/core.hpp"

#include "metaflux/client/protocol.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <unistd.h>

namespace metaflux::runtime {
namespace {

constexpr std::uint32_t kSnapshotAttempts = 4;
constexpr std::uint32_t kLostFenceAttempts = 4096;

struct RegistryLayout final {
  std::uint64_t view_admission;
  std::uint64_t view_control;
  std::uint64_t identities;
  std::uint64_t device_admission;
  std::uint64_t lifecycle_fences;
  std::uint64_t telemetry_control;
  std::uint64_t telemetry_bank0;
  std::uint64_t telemetry_bank1;
  std::uint64_t legacy_end;
  std::uint64_t extension;
  std::uint64_t view_publisher;
  std::uint64_t telemetry_publisher;
  std::uint64_t admission_attempts;
  std::uint64_t admission_leases;
  std::uint64_t device_updates;
  std::uint64_t lifecycle_ranges;
  std::uint64_t view_publish_records;
  std::uint64_t telemetry_publish_records;
  std::uint64_t total;
};

[[nodiscard]] bool is_aligned(const void* pointer, std::uint64_t alignment) noexcept {
  return (reinterpret_cast<std::uintptr_t>(pointer) % alignment) == 0U;
}

[[nodiscard]] bool range_fits(std::uint64_t offset, std::uint64_t count, std::uint64_t stride,
                              std::uint64_t total) noexcept {
  if ((offset % MF_SHARED_CACHE_LINE_SIZE) != 0U || offset > total ||
      count > (std::numeric_limits<std::uint64_t>::max() - offset) / stride) {
    return false;
  }
  return offset + (count * stride) <= total;
}

[[nodiscard]] bool advance_layout(std::uint64_t& offset, std::uint64_t count,
                                  std::uint64_t stride) noexcept {
  if (count > (std::numeric_limits<std::uint64_t>::max() - offset) / stride) {
    return false;
  }
  offset += count * stride;
  return true;
}

[[nodiscard]] bool compute_layout(std::uint32_t device_count, bool recovery,
                                  RegistryLayout& layout) noexcept {
  std::uint64_t offset = sizeof(mf_shared_registry_header_v1);
  layout.view_admission = offset;
  if (!advance_layout(offset, 1U, sizeof(mf_view_admission_control_v1))) {
    return false;
  }
  layout.view_control = offset;
  if (!advance_layout(offset, 1U, sizeof(mf_registry_view_control_v1))) {
    return false;
  }
  layout.identities = offset;
  if (!advance_layout(offset, device_count, sizeof(mf_virtual_device_identity_v1))) {
    return false;
  }
  layout.device_admission = offset;
  if (!advance_layout(offset, device_count, sizeof(mf_device_admission_control_v1))) {
    return false;
  }
  layout.lifecycle_fences = offset;
  if (!advance_layout(offset, device_count, sizeof(mf_virtual_device_lifecycle_fence_v1))) {
    return false;
  }
  layout.telemetry_control = offset;
  if (!advance_layout(offset, 1U, sizeof(mf_telemetry_control_v1))) {
    return false;
  }
  layout.telemetry_bank0 = offset;
  if (!advance_layout(offset, device_count, sizeof(mf_virtual_device_telemetry_v1))) {
    return false;
  }
  layout.telemetry_bank1 = offset;
  if (!advance_layout(offset, device_count, sizeof(mf_virtual_device_telemetry_v1))) {
    return false;
  }
  layout.legacy_end = offset;
  layout.extension = offset;
  if (!recovery) {
    layout.total = offset;
    return true;
  }
  if (!advance_layout(offset, 1U, sizeof(mf_shared_registry_extension_header_v1))) {
    return false;
  }
  layout.view_publisher = offset;
  if (!advance_layout(offset, 1U, sizeof(mf_view_publisher_control_v1))) {
    return false;
  }
  layout.telemetry_publisher = offset;
  if (!advance_layout(offset, 1U, sizeof(mf_telemetry_publisher_control_v1))) {
    return false;
  }
  layout.admission_attempts = offset;
  if (!advance_layout(offset, MF_SHARED_ADMISSION_ATTEMPT_CAPACITY_V1,
                      sizeof(mf_admission_attempt_record_v1))) {
    return false;
  }
  layout.admission_leases = offset;
  if (!advance_layout(offset, MF_SHARED_ADMISSION_LEASE_CAPACITY_V1,
                      sizeof(mf_admission_lease_record_v1))) {
    return false;
  }
  layout.device_updates = offset;
  if (!advance_layout(offset, MF_SHARED_DEVICE_UPDATE_CAPACITY_V1,
                      sizeof(mf_device_validation_update_record_v1))) {
    return false;
  }
  layout.lifecycle_ranges = offset;
  if (!advance_layout(offset, MF_SHARED_LIFECYCLE_RANGE_CAPACITY_V1,
                      sizeof(mf_lifecycle_range_record_v1))) {
    return false;
  }
  layout.view_publish_records = offset;
  if (!advance_layout(offset, MF_SHARED_VIEW_PUBLISH_CAPACITY_V1,
                      sizeof(mf_view_publish_record_v1))) {
    return false;
  }
  layout.telemetry_publish_records = offset;
  if (!advance_layout(offset, MF_SHARED_TELEMETRY_PUBLISH_CAPACITY_V1,
                      sizeof(mf_telemetry_publish_record_v1))) {
    return false;
  }
  layout.total = offset;
  return true;
}

[[nodiscard]] bool
reserved_identity_words_are_zero(const mf_virtual_device_identity_v1& identity) noexcept {
  for (const std::uint64_t word : identity.reserved) {
    if (word != 0U) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool offsets_are_canonical(const mf_shared_registry_header_v1& header,
                                         const RegistryLayout& layout) noexcept {
  return header.view_admission_offset == layout.view_admission &&
         header.view_control_offset == layout.view_control &&
         header.identities_offset == layout.identities &&
         header.device_admission_offset == layout.device_admission &&
         header.lifecycle_fences_offset == layout.lifecycle_fences &&
         header.telemetry_control_offset == layout.telemetry_control &&
         header.telemetry_bank0_offset == layout.telemetry_bank0 &&
         header.telemetry_bank1_offset == layout.telemetry_bank1 &&
         header.total_size == layout.total;
}

[[nodiscard]] bool read_process_start_time(std::uint32_t pid,
                                           std::uint64_t& out_start_time) noexcept {
  char path[64]{};
  char buffer[4096]{};
  if (std::snprintf(path, sizeof(path), "/proc/%u/stat", pid) <= 0) {
    return false;
  }
  std::FILE* file = std::fopen(path, "r");
  if (file == nullptr) {
    return false;
  }
  const bool read = std::fgets(buffer, static_cast<int>(sizeof(buffer)), file) != nullptr;
  (void)std::fclose(file);
  if (!read) {
    return false;
  }
  char* cursor = std::strrchr(buffer, ')');
  if (cursor == nullptr) {
    return false;
  }
  ++cursor;
  for (std::uint32_t field = 3U; field < 22U; ++field) {
    while (*cursor == ' ') {
      ++cursor;
    }
    if (*cursor == '\0') {
      return false;
    }
    while (*cursor != '\0' && *cursor != ' ') {
      ++cursor;
    }
  }
  while (*cursor == ' ') {
    ++cursor;
  }
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(cursor, &end, 10);
  if (end == cursor || parsed == 0U) {
    return false;
  }
  out_start_time = static_cast<std::uint64_t>(parsed);
  return true;
}

} // namespace

struct RegistryView::ViewControlSnapshot final {
  std::uint64_t latch;
  mf_registry_view_id_v1 view_id;
  std::uint64_t process_view_revision;
  std::uint64_t allocation_high_water;
  std::uint64_t publication_cursor;
  std::uint64_t reservation_head_slot;
  std::uint64_t reservation_tail_slot;
  std::uint64_t next_committable_slot;
  std::uint64_t gate_generation;
  std::uint64_t wake_sequence;
  std::uint32_t selection_policy;
  std::uint32_t default_order;
  std::uint32_t gate_state;
  std::uint32_t mapping_terminal;
};

std::uint32_t RegistryView::bootstrap_client_protocol_abi_version() noexcept {
  return MF_CLIENT_PROTOCOL_ABI_VERSION_1;
}

std::uint32_t bootstrap_client_protocol_abi_version() noexcept {
  return RegistryView::bootstrap_client_protocol_abi_version();
}

mf_shared_status_v1 RegistryView::required_mapping_size(std::uint32_t device_count,
                                                        std::uint64_t& out_size) noexcept {
  return required_recovery_mapping_size(device_count, out_size);
}

mf_shared_status_v1 RegistryView::required_recovery_mapping_size(std::uint32_t device_count,
                                                                 std::uint64_t& out_size) noexcept {
  RegistryLayout layout{};
  if (!compute_layout(device_count, true, layout)) {
    return MF_SHARED_OVERFLOW;
  }
  out_size = layout.total;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::required_legacy_mapping_size(std::uint32_t device_count,
                                                               std::uint64_t& out_size) noexcept {
  RegistryLayout layout{};
  if (!compute_layout(device_count, false, layout)) {
    return MF_SHARED_OVERFLOW;
  }
  out_size = layout.total;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::current_owner_identity(mf_owner_identity_v1& out_owner) noexcept {
  const auto pid = static_cast<std::uint32_t>(::getpid());
  std::uint64_t start_time = 0;
  if (pid == 0U || !read_process_start_time(pid, start_time)) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  out_owner = {.start_time_ticks = start_time, .pid = pid, .reserved = 0U};
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::initialize(
    void* mapping, std::uint64_t mapping_size, mf_registry_view_id_v1 view_id,
    std::uint64_t process_view_revision, std::span<const mf_virtual_device_identity_v1> identities,
    std::span<const FenceSnapshot> initial_fences, RegistryView& out_view) noexcept {
  std::uint64_t legacy_size = 0;
  std::uint64_t recovery_size = 0;
  auto* bytes = static_cast<std::uint8_t*>(mapping);
  RegistryLayout layout{};
  const std::uint32_t device_count = static_cast<std::uint32_t>(identities.size());

  if (mapping == nullptr || !is_aligned(mapping, MF_SHARED_CACHE_LINE_SIZE) ||
      identities.size() != initial_fences.size() ||
      identities.size() > std::numeric_limits<std::uint32_t>::max() ||
      view_id.daemon_incarnation == 0U || view_id.view_serial == 0U ||
      process_view_revision == 0U ||
      required_legacy_mapping_size(device_count, legacy_size) != MF_SHARED_SUCCESS ||
      required_recovery_mapping_size(device_count, recovery_size) != MF_SHARED_SUCCESS ||
      (mapping_size != legacy_size && mapping_size != recovery_size) ||
      mapping_size > std::numeric_limits<std::size_t>::max()) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const bool has_recovery = mapping_size == recovery_size;
  if (!compute_layout(device_count, has_recovery, layout)) {
    return MF_SHARED_OVERFLOW;
  }

  for (std::size_t outer = 0; outer < identities.size(); ++outer) {
    if (identities[outer].identity_record_id == 0U ||
        identities[outer].committed_generation == 0U ||
        initial_fences[outer].identity_record_id != identities[outer].identity_record_id ||
        initial_fences[outer].lifecycle_sequence == 0U ||
        initial_fences[outer].device_state != MF_DEVICE_STATE_ONLINE ||
        !reserved_identity_words_are_zero(identities[outer])) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    for (std::size_t inner = 0; inner < outer; ++inner) {
      if (identities[inner].identity_record_id == identities[outer].identity_record_id) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
    }
  }

  std::memset(mapping, 0, static_cast<std::size_t>(mapping_size));
  auto* header = reinterpret_cast<mf_shared_registry_header_v1*>(bytes);
  header->magic = MF_SHARED_REGISTRY_MAGIC;
  header->abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  header->header_size = static_cast<std::uint32_t>(sizeof(*header));
  header->flags = has_recovery ? MF_SHARED_REGISTRY_FLAG_RECOVERY_TABLES_V1 : 0U;
  header->total_size = mapping_size;
  header->registry_view_id = view_id;
  header->process_view_revision = process_view_revision;
  header->device_count = static_cast<std::uint32_t>(identities.size());
  header->telemetry_row_count = header->device_count;
  header->view_admission_offset = layout.view_admission;
  header->view_control_offset = layout.view_control;
  header->identities_offset = layout.identities;
  header->device_admission_offset = layout.device_admission;
  header->lifecycle_fences_offset = layout.lifecycle_fences;
  header->telemetry_control_offset = layout.telemetry_control;
  header->telemetry_bank0_offset = layout.telemetry_bank0;
  header->telemetry_bank1_offset = layout.telemetry_bank1;

  auto* view_admission =
      reinterpret_cast<mf_view_admission_control_v1*>(bytes + header->view_admission_offset);
  auto* view_control =
      reinterpret_cast<mf_registry_view_control_v1*>(bytes + header->view_control_offset);
  auto* mapped_identities =
      reinterpret_cast<mf_virtual_device_identity_v1*>(bytes + header->identities_offset);
  auto* device_admission =
      reinterpret_cast<mf_device_admission_control_v1*>(bytes + header->device_admission_offset);
  auto* fences = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
      bytes + header->lifecycle_fences_offset);
  auto* telemetry_control =
      reinterpret_cast<mf_telemetry_control_v1*>(bytes + header->telemetry_control_offset);
  auto* bank0 =
      reinterpret_cast<mf_virtual_device_telemetry_v1*>(bytes + header->telemetry_bank0_offset);
  auto* bank1 =
      reinterpret_cast<mf_virtual_device_telemetry_v1*>(bytes + header->telemetry_bank1_offset);

  view_admission->registry_view_id = view_id;
  mf_atomic_store_u64_relaxed(&view_admission->state_generation,
                              mf_view_admission_pack_v1(1U, MF_VIEW_ADMISSION_CLOSING));
  view_control->registry_view_id = view_id;
  view_control->process_view_revision = process_view_revision;
  view_control->reservation_head_slot = MF_SHARED_RECORD_SLOT_NONE;
  view_control->reservation_tail_slot = MF_SHARED_RECORD_SLOT_NONE;
  view_control->next_committable_slot = MF_SHARED_RECORD_SLOT_NONE;
  view_control->gate_generation = 1U;
  view_control->gate_state = MF_VIEW_GATE_OPEN;
  for (std::uint32_t index = 0; index < header->device_count; ++index) {
    mapped_identities[index] = identities[index];
    device_admission[index].identity_record_id = identities[index].identity_record_id;
    mf_atomic_store_u64_relaxed(&device_admission[index].state_generation_tag,
                                mf_device_admission_pack_v1(1U, MF_DEVICE_ADMISSION_OPEN, 0U));
    fences[index].identity_record_id = initial_fences[index].identity_record_id;
    fences[index].lifecycle_sequence = initial_fences[index].lifecycle_sequence;
    fences[index].epoch = initial_fences[index].epoch;
    fences[index].effective_quota_bytes = initial_fences[index].effective_quota_bytes;
    fences[index].policy_bits = initial_fences[index].policy_bits;
    fences[index].device_state = initial_fences[index].device_state;
    bank0[index].identity_record_id = identities[index].identity_record_id;
    bank0[index].observed_lifecycle_sequence = initial_fences[index].lifecycle_sequence;
    bank1[index].identity_record_id = identities[index].identity_record_id;
    bank1[index].observed_lifecycle_sequence = initial_fences[index].lifecycle_sequence;
    if (initial_fences[index].lifecycle_sequence > view_control->allocation_high_water) {
      view_control->allocation_high_water = initial_fences[index].lifecycle_sequence;
      view_control->publication_cursor = initial_fences[index].lifecycle_sequence;
    }
  }
  telemetry_control->row_count = header->device_count;
  telemetry_control->active_bank_state =
      mf_telemetry_bank_state_pack_v1(0U, MF_TELEMETRY_STATE_UNAVAILABLE);

  if (has_recovery) {
    auto* extension =
        reinterpret_cast<mf_shared_registry_extension_header_v1*>(bytes + layout.extension);
    auto* view_publisher =
        reinterpret_cast<mf_view_publisher_control_v1*>(bytes + layout.view_publisher);
    auto* telemetry_publisher =
        reinterpret_cast<mf_telemetry_publisher_control_v1*>(bytes + layout.telemetry_publisher);
    auto* attempts =
        reinterpret_cast<mf_admission_attempt_record_v1*>(bytes + layout.admission_attempts);
    auto* leases = reinterpret_cast<mf_admission_lease_record_v1*>(bytes + layout.admission_leases);
    auto* updates =
        reinterpret_cast<mf_device_validation_update_record_v1*>(bytes + layout.device_updates);
    auto* ranges = reinterpret_cast<mf_lifecycle_range_record_v1*>(bytes + layout.lifecycle_ranges);
    auto* view_records =
        reinterpret_cast<mf_view_publish_record_v1*>(bytes + layout.view_publish_records);
    auto* telemetry_records =
        reinterpret_cast<mf_telemetry_publish_record_v1*>(bytes + layout.telemetry_publish_records);
    extension->magic = MF_SHARED_REGISTRY_EXTENSION_MAGIC;
    extension->abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
    extension->header_size = static_cast<std::uint32_t>(sizeof(*extension));
    extension->total_size = mapping_size;
    extension->registry_view_id = view_id;
    extension->admission_attempt_capacity = MF_SHARED_ADMISSION_ATTEMPT_CAPACITY_V1;
    extension->admission_lease_capacity = MF_SHARED_ADMISSION_LEASE_CAPACITY_V1;
    extension->device_update_capacity = MF_SHARED_DEVICE_UPDATE_CAPACITY_V1;
    extension->lifecycle_range_capacity = MF_SHARED_LIFECYCLE_RANGE_CAPACITY_V1;
    extension->view_publish_capacity = MF_SHARED_VIEW_PUBLISH_CAPACITY_V1;
    extension->telemetry_publish_capacity = MF_SHARED_TELEMETRY_PUBLISH_CAPACITY_V1;
    extension->view_publisher_control_offset = layout.view_publisher;
    extension->telemetry_publisher_control_offset = layout.telemetry_publisher;
    extension->admission_attempts_offset = layout.admission_attempts;
    extension->admission_leases_offset = layout.admission_leases;
    extension->device_updates_offset = layout.device_updates;
    extension->lifecycle_ranges_offset = layout.lifecycle_ranges;
    extension->view_publish_records_offset = layout.view_publish_records;
    extension->telemetry_publish_records_offset = layout.telemetry_publish_records;
    extension->ordinary_view_publish_capacity = MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1;
    extension->ordinary_telemetry_publish_capacity =
        MF_SHARED_TELEMETRY_PUBLISH_ORDINARY_CAPACITY_V1;
    extension->close_closing_slot = MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1;
    extension->close_terminal_slot = MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1 + 1U;
    extension->telemetry_terminal_slot = MF_SHARED_TELEMETRY_PUBLISH_ORDINARY_CAPACITY_V1;

    view_publisher->tagged_owner =
        mf_publisher_control_pack_v1(0U, MF_PUBLISHER_IDLE, MF_SHARED_RECORD_SLOT_NONE);
    telemetry_publisher->tagged_owner =
        mf_publisher_control_pack_v1(0U, MF_PUBLISHER_IDLE, MF_SHARED_RECORD_SLOT_NONE);
    for (std::uint32_t index = 0; index < MF_SHARED_ADMISSION_ATTEMPT_CAPACITY_V1; ++index) {
      attempts[index].target_lease_slot = MF_SHARED_RECORD_SLOT_NONE;
      attempts[index].tagged_phase =
          mf_admission_attempt_phase_pack_v1(0U, MF_ADMISSION_ATTEMPT_IDLE);
    }
    for (std::uint32_t index = 0; index < MF_SHARED_ADMISSION_LEASE_CAPACITY_V1; ++index) {
      leases[index].attempt_slot = MF_SHARED_RECORD_SLOT_NONE;
      leases[index].tagged_state =
          mf_admission_lease_state_pack_v1(0U, MF_ADMISSION_LEASE_FREE, 0U);
    }
    for (std::uint32_t index = 0; index < MF_SHARED_DEVICE_UPDATE_CAPACITY_V1; ++index) {
      updates[index].view_publish_slot = MF_SHARED_RECORD_SLOT_NONE;
      updates[index].tagged_state =
          mf_device_update_state_pack_v1(0U, MF_DEVICE_UPDATE_FREE, MF_SHARED_RECORD_SLOT_NONE);
    }
    for (std::uint32_t index = 0; index < MF_SHARED_LIFECYCLE_RANGE_CAPACITY_V1; ++index) {
      ranges[index].predecessor_tail_slot = MF_SHARED_RECORD_SLOT_NONE;
      ranges[index].next_tail_slot = MF_SHARED_RECORD_SLOT_NONE;
      ranges[index].tagged_state = mf_lifecycle_range_state_pack_v1(
          0U, MF_LIFECYCLE_RANGE_FREE, MF_LIFECYCLE_RANGE_DISPOSITION_NONE);
    }
    for (std::uint32_t index = 0; index < MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1; ++index) {
      view_records[index].range_slot = MF_SHARED_RECORD_SLOT_NONE;
      view_records[index].tagged_state =
          mf_view_publish_state_pack_v1(0U, MF_VIEW_PUBLISH_FREE, MF_SHARED_RECORD_SLOT_NONE);
    }
    auto& closing_record = view_records[extension->close_closing_slot];
    closing_record.registry_view_id = view_id;
    closing_record.operation_kind = MF_VIEW_PUBLISH_KIND_CLOSE_CLOSING;
    closing_record.tagged_state = mf_view_publish_state_pack_v1(
        MF_SHARED_RECORD_TAG_TERMINAL, MF_VIEW_PUBLISH_PREPARED, MF_SHARED_RECORD_SLOT_NONE);
    auto& terminal_record = view_records[extension->close_terminal_slot];
    terminal_record.registry_view_id = view_id;
    terminal_record.operation_kind = MF_VIEW_PUBLISH_KIND_CLOSE_TERMINAL;
    terminal_record.tagged_state = mf_view_publish_state_pack_v1(
        MF_SHARED_RECORD_TAG_TERMINAL, MF_VIEW_PUBLISH_PREPARED, MF_SHARED_RECORD_SLOT_NONE);
    for (std::uint32_t index = 0; index < MF_SHARED_TELEMETRY_PUBLISH_ORDINARY_CAPACITY_V1;
         ++index) {
      telemetry_records[index].tagged_state =
          mf_telemetry_publish_state_pack_v1(0U, MF_TELEMETRY_PUBLISH_FREE, 0U);
    }
    auto& telemetry_terminal = telemetry_records[extension->telemetry_terminal_slot];
    telemetry_terminal.registry_view_id = view_id;
    telemetry_terminal.operation_kind = MF_TELEMETRY_PUBLISH_KIND_TERMINAL;
    telemetry_terminal.tagged_state = mf_telemetry_publish_state_pack_v1(
        MF_SHARED_RECORD_TAG_TERMINAL, MF_TELEMETRY_PUBLISH_PREPARED, 0U);
  }

  mf_atomic_thread_fence_release();
  mf_atomic_store_u64_release(&view_admission->state_generation,
                              mf_view_admission_pack_v1(1U, MF_VIEW_ADMISSION_OPEN));
  return attach(mapping, mapping_size, out_view);
}

mf_shared_status_v1 RegistryView::attach(void* mapping, std::uint64_t mapping_size,
                                         RegistryView& out_view) noexcept {
  if (mapping == nullptr || !is_aligned(mapping, MF_SHARED_CACHE_LINE_SIZE) ||
      mapping_size < sizeof(mf_shared_registry_header_v1)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_atomic_thread_fence_acquire();
  auto* bytes = static_cast<std::uint8_t*>(mapping);
  auto* header = reinterpret_cast<mf_shared_registry_header_v1*>(bytes);
  RegistryLayout layout{};
  const bool has_recovery = (header->flags & MF_SHARED_REGISTRY_FLAG_RECOVERY_TABLES_V1) != 0U;
  if (header->magic != MF_SHARED_REGISTRY_MAGIC ||
      header->abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
      header->header_size != sizeof(*header) || header->total_size != mapping_size ||
      (header->flags & ~MF_SHARED_REGISTRY_KNOWN_FLAGS_V1) != 0U ||
      header->telemetry_row_count != header->device_count ||
      header->registry_view_id.daemon_incarnation == 0U ||
      header->registry_view_id.view_serial == 0U ||
      !compute_layout(header->device_count, has_recovery, layout) || layout.total != mapping_size ||
      !offsets_are_canonical(*header, layout) ||
      !range_fits(header->view_admission_offset, 1U, sizeof(mf_view_admission_control_v1),
                  mapping_size) ||
      !range_fits(header->view_control_offset, 1U, sizeof(mf_registry_view_control_v1),
                  mapping_size) ||
      !range_fits(header->identities_offset, header->device_count,
                  sizeof(mf_virtual_device_identity_v1), mapping_size) ||
      !range_fits(header->device_admission_offset, header->device_count,
                  sizeof(mf_device_admission_control_v1), mapping_size) ||
      !range_fits(header->lifecycle_fences_offset, header->device_count,
                  sizeof(mf_virtual_device_lifecycle_fence_v1), mapping_size) ||
      !range_fits(header->telemetry_control_offset, 1U, sizeof(mf_telemetry_control_v1),
                  mapping_size) ||
      !range_fits(header->telemetry_bank0_offset, header->device_count,
                  sizeof(mf_virtual_device_telemetry_v1), mapping_size) ||
      !range_fits(header->telemetry_bank1_offset, header->device_count,
                  sizeof(mf_virtual_device_telemetry_v1), mapping_size)) {
    return MF_SHARED_MALFORMED;
  }

  mf_shared_registry_extension_header_v1* extension = nullptr;
  if (has_recovery) {
    if (!range_fits(layout.extension, 1U, sizeof(mf_shared_registry_extension_header_v1),
                    mapping_size)) {
      return MF_SHARED_MALFORMED;
    }
    extension = reinterpret_cast<mf_shared_registry_extension_header_v1*>(bytes + layout.extension);
    if (extension->magic != MF_SHARED_REGISTRY_EXTENSION_MAGIC ||
        extension->abi_version != MF_SHARED_DEVICE_ABI_VERSION_1 ||
        extension->header_size != sizeof(*extension) || extension->flags != 0U ||
        extension->total_size != mapping_size ||
        !mf_registry_view_id_equal_v1(extension->registry_view_id, header->registry_view_id) ||
        extension->admission_attempt_capacity != MF_SHARED_ADMISSION_ATTEMPT_CAPACITY_V1 ||
        extension->admission_lease_capacity != MF_SHARED_ADMISSION_LEASE_CAPACITY_V1 ||
        extension->device_update_capacity != MF_SHARED_DEVICE_UPDATE_CAPACITY_V1 ||
        extension->lifecycle_range_capacity != MF_SHARED_LIFECYCLE_RANGE_CAPACITY_V1 ||
        extension->view_publish_capacity != MF_SHARED_VIEW_PUBLISH_CAPACITY_V1 ||
        extension->telemetry_publish_capacity != MF_SHARED_TELEMETRY_PUBLISH_CAPACITY_V1 ||
        extension->ordinary_view_publish_capacity !=
            MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1 ||
        extension->ordinary_telemetry_publish_capacity !=
            MF_SHARED_TELEMETRY_PUBLISH_ORDINARY_CAPACITY_V1 ||
        extension->close_closing_slot != MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1 ||
        extension->close_terminal_slot != MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1 + 1U ||
        extension->telemetry_terminal_slot != MF_SHARED_TELEMETRY_PUBLISH_ORDINARY_CAPACITY_V1 ||
        extension->view_publisher_control_offset != layout.view_publisher ||
        extension->telemetry_publisher_control_offset != layout.telemetry_publisher ||
        extension->admission_attempts_offset != layout.admission_attempts ||
        extension->admission_leases_offset != layout.admission_leases ||
        extension->device_updates_offset != layout.device_updates ||
        extension->lifecycle_ranges_offset != layout.lifecycle_ranges ||
        extension->view_publish_records_offset != layout.view_publish_records ||
        extension->telemetry_publish_records_offset != layout.telemetry_publish_records ||
        !range_fits(extension->view_publisher_control_offset, 1U,
                    sizeof(mf_view_publisher_control_v1), mapping_size) ||
        !range_fits(extension->telemetry_publisher_control_offset, 1U,
                    sizeof(mf_telemetry_publisher_control_v1), mapping_size) ||
        !range_fits(extension->admission_attempts_offset, extension->admission_attempt_capacity,
                    sizeof(mf_admission_attempt_record_v1), mapping_size) ||
        !range_fits(extension->admission_leases_offset, extension->admission_lease_capacity,
                    sizeof(mf_admission_lease_record_v1), mapping_size) ||
        !range_fits(extension->device_updates_offset, extension->device_update_capacity,
                    sizeof(mf_device_validation_update_record_v1), mapping_size) ||
        !range_fits(extension->lifecycle_ranges_offset, extension->lifecycle_range_capacity,
                    sizeof(mf_lifecycle_range_record_v1), mapping_size) ||
        !range_fits(extension->view_publish_records_offset, extension->view_publish_capacity,
                    sizeof(mf_view_publish_record_v1), mapping_size) ||
        !range_fits(extension->telemetry_publish_records_offset,
                    extension->telemetry_publish_capacity, sizeof(mf_telemetry_publish_record_v1),
                    mapping_size)) {
      return MF_SHARED_MALFORMED;
    }
  }

  RegistryView candidate;
  candidate.mapping_ = mapping;
  candidate.mapping_size_ = mapping_size;
  candidate.header_ = header;
  candidate.view_admission_ =
      reinterpret_cast<mf_view_admission_control_v1*>(bytes + header->view_admission_offset);
  candidate.view_control_ =
      reinterpret_cast<mf_registry_view_control_v1*>(bytes + header->view_control_offset);
  candidate.identities_ =
      reinterpret_cast<mf_virtual_device_identity_v1*>(bytes + header->identities_offset);
  candidate.device_admission_ =
      reinterpret_cast<mf_device_admission_control_v1*>(bytes + header->device_admission_offset);
  candidate.fences_ = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
      bytes + header->lifecycle_fences_offset);
  candidate.telemetry_control_ =
      reinterpret_cast<mf_telemetry_control_v1*>(bytes + header->telemetry_control_offset);
  candidate.telemetry_banks_[0] =
      reinterpret_cast<mf_virtual_device_telemetry_v1*>(bytes + header->telemetry_bank0_offset);
  candidate.telemetry_banks_[1] =
      reinterpret_cast<mf_virtual_device_telemetry_v1*>(bytes + header->telemetry_bank1_offset);
  candidate.extension_ = extension;
  if (extension != nullptr) {
    candidate.view_publisher_ = reinterpret_cast<mf_view_publisher_control_v1*>(
        bytes + extension->view_publisher_control_offset);
    candidate.telemetry_publisher_ = reinterpret_cast<mf_telemetry_publisher_control_v1*>(
        bytes + extension->telemetry_publisher_control_offset);
    candidate.admission_attempts_ = reinterpret_cast<mf_admission_attempt_record_v1*>(
        bytes + extension->admission_attempts_offset);
    candidate.admission_leases_ =
        reinterpret_cast<mf_admission_lease_record_v1*>(bytes + extension->admission_leases_offset);
    candidate.device_updates_ = reinterpret_cast<mf_device_validation_update_record_v1*>(
        bytes + extension->device_updates_offset);
    candidate.lifecycle_ranges_ =
        reinterpret_cast<mf_lifecycle_range_record_v1*>(bytes + extension->lifecycle_ranges_offset);
    candidate.view_publish_records_ = reinterpret_cast<mf_view_publish_record_v1*>(
        bytes + extension->view_publish_records_offset);
    candidate.telemetry_publish_records_ = reinterpret_cast<mf_telemetry_publish_record_v1*>(
        bytes + extension->telemetry_publish_records_offset);
  }
  candidate.view_id_ = header->registry_view_id;
  candidate.device_count_ = header->device_count;

  if (!mf_registry_view_id_equal_v1(candidate.view_admission_->registry_view_id,
                                    candidate.view_id_) ||
      !mf_registry_view_id_equal_v1(candidate.view_control_->registry_view_id,
                                    candidate.view_id_)) {
    return MF_SHARED_MALFORMED;
  }
  for (std::uint32_t index = 0; index < candidate.device_count_; ++index) {
    if (candidate.identities_[index].identity_record_id == 0U ||
        candidate.identities_[index].committed_generation == 0U ||
        candidate.device_admission_[index].identity_record_id !=
            candidate.identities_[index].identity_record_id ||
        candidate.fences_[index].identity_record_id !=
            candidate.identities_[index].identity_record_id) {
      return MF_SHARED_MALFORMED;
    }
  }
  out_view = candidate;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1
RegistryView::stable_view_control(ViewControlSnapshot& out_snapshot) const noexcept {
  for (std::uint32_t attempt = 0; attempt < kSnapshotAttempts; ++attempt) {
    const std::uint64_t first = mf_atomic_load_u64_acquire(&view_control_->control_latch_sequence);
    if ((first & 1U) != 0U) {
      continue;
    }
    out_snapshot.view_id.daemon_incarnation =
        mf_atomic_load_u64_relaxed(&view_control_->registry_view_id.daemon_incarnation);
    out_snapshot.view_id.view_serial =
        mf_atomic_load_u64_relaxed(&view_control_->registry_view_id.view_serial);
    out_snapshot.selection_policy = mf_atomic_load_u32_relaxed(&view_control_->selection_policy);
    out_snapshot.default_order = mf_atomic_load_u32_relaxed(&view_control_->default_order);
    out_snapshot.process_view_revision =
        mf_atomic_load_u64_relaxed(&view_control_->process_view_revision);
    out_snapshot.allocation_high_water =
        mf_atomic_load_u64_relaxed(&view_control_->allocation_high_water);
    out_snapshot.publication_cursor =
        mf_atomic_load_u64_relaxed(&view_control_->publication_cursor);
    out_snapshot.reservation_head_slot =
        mf_atomic_load_u64_relaxed(&view_control_->reservation_head_slot);
    out_snapshot.reservation_tail_slot =
        mf_atomic_load_u64_relaxed(&view_control_->reservation_tail_slot);
    out_snapshot.next_committable_slot =
        mf_atomic_load_u64_relaxed(&view_control_->next_committable_slot);
    out_snapshot.gate_generation = mf_atomic_load_u64_relaxed(&view_control_->gate_generation);
    out_snapshot.gate_state = mf_atomic_load_u32_relaxed(&view_control_->gate_state);
    out_snapshot.mapping_terminal = mf_atomic_load_u32_relaxed(&view_control_->mapping_terminal);
    out_snapshot.wake_sequence = mf_atomic_load_u64_relaxed(&view_control_->wake_sequence);
    mf_atomic_signal_fence_seq_cst();
    const std::uint64_t second = mf_atomic_load_u64_acquire(&view_control_->control_latch_sequence);
    if (first == second) {
      out_snapshot.latch = first;
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1 RegistryView::stable_fence(std::uint32_t device_index, FenceSnapshot& out_fence,
                                               std::uint64_t& out_latch) const noexcept {
  auto& fence = fences_[device_index];
  for (std::uint32_t attempt = 0; attempt < kSnapshotAttempts; ++attempt) {
    const std::uint64_t first = mf_atomic_load_u64_acquire(&fence.fence_latch_sequence);
    if ((first & 1U) != 0U) {
      continue;
    }
    out_fence.identity_record_id = mf_atomic_load_u64_relaxed(&fence.identity_record_id);
    out_fence.lifecycle_sequence = mf_atomic_load_u64_relaxed(&fence.lifecycle_sequence);
    out_fence.epoch = mf_atomic_load_u64_relaxed(&fence.epoch);
    out_fence.effective_quota_bytes = mf_atomic_load_u64_relaxed(&fence.effective_quota_bytes);
    out_fence.policy_bits = mf_atomic_load_u64_relaxed(&fence.policy_bits);
    out_fence.device_state = mf_atomic_load_u32_relaxed(&fence.device_state);
    mf_atomic_signal_fence_seq_cst();
    const std::uint64_t second = mf_atomic_load_u64_acquire(&fence.fence_latch_sequence);
    if (first == second) {
      out_latch = first;
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1 RegistryView::validate_telemetry_rows(
    std::span<const mf_virtual_device_telemetry_v1> rows) const noexcept {
  if (rows.size() != device_count_) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    const std::uint64_t admission_before =
        mf_atomic_load_u64_seq_cst(&device_admission_[index].state_generation_tag);
    const std::uint32_t admission_state = mf_device_admission_state_v1(admission_before);
    if (admission_state != MF_DEVICE_ADMISSION_OPEN) {
      return admission_state == MF_DEVICE_ADMISSION_UPDATING ? MF_SHARED_RETRY
                                                             : MF_SHARED_DEVICE_LOST;
    }
    FenceSnapshot fence{};
    std::uint64_t fence_latch = 0;
    const mf_shared_status_v1 fence_status = stable_fence(index, fence, fence_latch);
    if (fence_status != MF_SHARED_SUCCESS) {
      return fence_status;
    }
    if (fence.identity_record_id != identities_[index].identity_record_id ||
        rows[index].identity_record_id != fence.identity_record_id ||
        rows[index].observed_lifecycle_sequence == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    if (fence.device_state != MF_DEVICE_STATE_ONLINE) {
      return MF_SHARED_DEVICE_LOST;
    }
    if (rows[index].observed_lifecycle_sequence != fence.lifecycle_sequence) {
      return MF_SHARED_RETRY;
    }
    const std::uint64_t admission_after =
        mf_atomic_load_u64_seq_cst(&device_admission_[index].state_generation_tag);
    if (admission_after != admission_before) {
      return mf_device_admission_state_v1(admission_after) == MF_DEVICE_ADMISSION_CLOSED
                 ? MF_SHARED_DEVICE_LOST
                 : MF_SHARED_RETRY;
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::validate_telemetry_bank(std::uint32_t bank) const noexcept {
  if (bank > 1U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    const std::uint64_t admission_before =
        mf_atomic_load_u64_seq_cst(&device_admission_[index].state_generation_tag);
    const std::uint32_t admission_state = mf_device_admission_state_v1(admission_before);
    if (admission_state != MF_DEVICE_ADMISSION_OPEN) {
      return admission_state == MF_DEVICE_ADMISSION_UPDATING ? MF_SHARED_RETRY
                                                             : MF_SHARED_DEVICE_LOST;
    }
    FenceSnapshot fence{};
    std::uint64_t fence_latch = 0;
    const mf_shared_status_v1 fence_status = stable_fence(index, fence, fence_latch);
    if (fence_status != MF_SHARED_SUCCESS) {
      return fence_status;
    }
    const auto& row = telemetry_banks_[bank][index];
    const std::uint64_t identity_record_id = mf_atomic_load_u64_relaxed(&row.identity_record_id);
    const std::uint64_t observed_lifecycle_sequence =
        mf_atomic_load_u64_relaxed(&row.observed_lifecycle_sequence);
    if (fence.identity_record_id != identities_[index].identity_record_id ||
        identity_record_id != fence.identity_record_id || observed_lifecycle_sequence == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    if (fence.device_state != MF_DEVICE_STATE_ONLINE) {
      return MF_SHARED_DEVICE_LOST;
    }
    if (observed_lifecycle_sequence != fence.lifecycle_sequence) {
      return MF_SHARED_RETRY;
    }
    const std::uint64_t admission_after =
        mf_atomic_load_u64_seq_cst(&device_admission_[index].state_generation_tag);
    if (admission_after != admission_before) {
      return mf_device_admission_state_v1(admission_after) == MF_DEVICE_ADMISSION_CLOSED
                 ? MF_SHARED_DEVICE_LOST
                 : MF_SHARED_RETRY;
    }
  }
  return MF_SHARED_SUCCESS;
}

std::uint32_t RegistryView::find_identity(std::uint64_t identity_record_id) const noexcept {
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    if (identities_[index].identity_record_id == identity_record_id) {
      return index;
    }
  }
  return device_count_;
}

mf_shared_status_v1 RegistryView::make_handle(std::uint32_t device_index, std::uint64_t object_id,
                                              std::uint64_t object_generation,
                                              std::uint32_t object_type,
                                              mf_generation_handle_v1& out_handle) const noexcept {
  if (device_index >= device_count_ || object_id == 0U || object_generation == 0U ||
      object_type == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::uint64_t admission = mf_atomic_load_u64_acquire(&view_admission_->state_generation);
  const std::uint64_t device =
      mf_atomic_load_u64_acquire(&device_admission_[device_index].state_generation_tag);
  if (mf_view_admission_state_v1(admission) != MF_VIEW_ADMISSION_OPEN) {
    return MF_SHARED_TERMINAL_VIEW;
  }
  if (mf_device_admission_state_v1(device) != MF_DEVICE_ADMISSION_OPEN) {
    return mf_device_admission_state_v1(device) == MF_DEVICE_ADMISSION_UPDATING
               ? MF_SHARED_RETRY
               : MF_SHARED_DEVICE_LOST;
  }
  std::memset(&out_handle, 0, sizeof(out_handle));
  out_handle.registry_view_id = view_id_;
  out_handle.identity_record_id = identities_[device_index].identity_record_id;
  out_handle.device_generation = identities_[device_index].committed_generation;
  out_handle.object_id = object_id;
  out_handle.object_generation = object_generation;
  out_handle.object_type = object_type;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::publish_device_identity(
    std::uint32_t device_index, std::uint64_t expected_identity_record_id,
    std::uint64_t expected_generation, std::uint64_t next_identity_record_id,
    std::uint64_t next_generation, const FenceSnapshot& intended_fence) noexcept {
  if (device_index >= device_count_ || expected_identity_record_id == 0U ||
      expected_generation == 0U || next_identity_record_id <= expected_identity_record_id ||
      next_generation <= expected_generation || intended_fence.identity_record_id !=
                                                   next_identity_record_id ||
      intended_fence.lifecycle_sequence == 0U || intended_fence.device_state !=
                                                   MF_DEVICE_STATE_ONLINE ||
      mf_view_admission_state_v1(mf_atomic_load_u64_acquire(&view_admission_->state_generation)) !=
          MF_VIEW_ADMISSION_OPEN) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (identities_[device_index].identity_record_id != expected_identity_record_id ||
      identities_[device_index].committed_generation != expected_generation) {
    return MF_SHARED_STALE_HANDLE;
  }

  FenceSnapshot current_fence{};
  std::uint64_t current_fence_latch = 0U;
  if (stable_fence(device_index, current_fence, current_fence_latch) != MF_SHARED_SUCCESS) {
    return MF_SHARED_RETRY;
  }
  if (intended_fence.lifecycle_sequence <= current_fence.lifecycle_sequence) {
    return MF_SHARED_STALE_HANDLE;
  }

  auto& device_control = device_admission_[device_index].state_generation_tag;
  const std::uint64_t expected_device = mf_atomic_load_u64_seq_cst(&device_control);
  if (mf_device_admission_state_v1(expected_device) != MF_DEVICE_ADMISSION_OPEN) {
    return mf_device_admission_state_v1(expected_device) == MF_DEVICE_ADMISSION_UPDATING
               ? MF_SHARED_RETRY
               : MF_SHARED_DEVICE_LOST;
  }
  const std::uint32_t validation_generation =
      mf_device_admission_generation_v1(expected_device);
  if (validation_generation == 0U || validation_generation >= MF_DEVICE_GENERATION_MAX_NORMAL) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  const std::uint32_t next_validation_generation = validation_generation + 1U;
  const std::uint32_t update_tag = static_cast<std::uint32_t>(intended_fence.lifecycle_sequence);
  const std::uint64_t updating = mf_device_admission_pack_v1(
      next_validation_generation, MF_DEVICE_ADMISSION_UPDATING, update_tag == 0U ? 1U : update_tag);
  std::uint64_t expected = expected_device;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&device_control, &expected, updating)) {
    return mf_device_admission_state_v1(expected) == MF_DEVICE_ADMISSION_CLOSED
               ? MF_SHARED_DEVICE_LOST
               : MF_SHARED_RETRY;
  }

  const auto restore_device = [&]() noexcept -> mf_shared_status_v1 {
    std::uint64_t restore_expected = updating;
    if (!mf_atomic_compare_exchange_u64_seq_cst(&device_control, &restore_expected,
                                                expected_device)) {
      return MF_SHARED_DEVICE_LOST;
    }
    return MF_SHARED_RETRY;
  };

  const std::uint64_t control_even =
      mf_atomic_load_u64_acquire(&view_control_->control_latch_sequence);
  if ((control_even & 1U) != 0U ||
      control_even > std::numeric_limits<std::uint64_t>::max() - 2U) {
    return restore_device();
  }
  expected = control_even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&view_control_->control_latch_sequence, &expected,
                                              control_even + 1U)) {
    return restore_device();
  }

  const std::uint64_t fence_even =
      mf_atomic_load_u64_acquire(&fences_[device_index].fence_latch_sequence);
  if ((fence_even & 1U) != 0U || fence_even > std::numeric_limits<std::uint64_t>::max() - 2U) {
    mf_atomic_store_u64_release(&view_control_->control_latch_sequence, control_even + 2U);
    const mf_shared_status_v1 restored = restore_device();
    return fence_even > std::numeric_limits<std::uint64_t>::max() - 2U ? MF_SHARED_OVERFLOW
                                                                       : restored;
  }
  expected = fence_even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&fences_[device_index].fence_latch_sequence,
                                              &expected, fence_even + 1U)) {
    mf_atomic_store_u64_release(&view_control_->control_latch_sequence, control_even + 2U);
    return restore_device();
  }

  const std::uint64_t telemetry_even =
      mf_atomic_load_u64_acquire(&telemetry_control_->telemetry_latch_sequence);
  if ((telemetry_even & 1U) != 0U ||
      telemetry_even > std::numeric_limits<std::uint64_t>::max() - 2U) {
    mf_atomic_store_u64_release(&fences_[device_index].fence_latch_sequence, fence_even + 2U);
    mf_atomic_store_u64_release(&view_control_->control_latch_sequence, control_even + 2U);
    const mf_shared_status_v1 restored = restore_device();
    return telemetry_even > std::numeric_limits<std::uint64_t>::max() - 2U
               ? MF_SHARED_OVERFLOW
               : restored;
  }
  expected = telemetry_even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence,
                                              &expected, telemetry_even + 1U)) {
    mf_atomic_store_u64_release(&fences_[device_index].fence_latch_sequence, fence_even + 2U);
    mf_atomic_store_u64_release(&view_control_->control_latch_sequence, control_even + 2U);
    return restore_device();
  }

  identities_[device_index].identity_record_id = next_identity_record_id;
  identities_[device_index].committed_generation = next_generation;
  device_admission_[device_index].identity_record_id = next_identity_record_id;
  auto& fence = fences_[device_index];
  mf_atomic_store_u64_relaxed(&fence.identity_record_id, intended_fence.identity_record_id);
  mf_atomic_store_u64_relaxed(&fence.lifecycle_sequence, intended_fence.lifecycle_sequence);
  mf_atomic_store_u64_relaxed(&fence.epoch, intended_fence.epoch);
  mf_atomic_store_u64_relaxed(&fence.effective_quota_bytes, intended_fence.effective_quota_bytes);
  mf_atomic_store_u64_relaxed(&fence.policy_bits, intended_fence.policy_bits);
  mf_atomic_store_u32_relaxed(&fence.device_state, intended_fence.device_state);
  for (std::uint32_t bank = 0U; bank < 2U; ++bank) {
    auto& row = telemetry_banks_[bank][device_index];
    mf_atomic_store_u64_relaxed(&row.identity_record_id, next_identity_record_id);
    mf_atomic_store_u64_relaxed(&row.observed_lifecycle_sequence,
                                intended_fence.lifecycle_sequence);
  }
  mf_atomic_thread_fence_release();
  mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence, telemetry_even + 2U);
  mf_atomic_store_u64_release(&fences_[device_index].fence_latch_sequence, fence_even + 2U);
  mf_atomic_store_u64_release(&view_control_->control_latch_sequence, control_even + 2U);

  std::uint64_t reopen_expected = updating;
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &device_control, &reopen_expected,
          mf_device_admission_pack_v1(next_validation_generation, MF_DEVICE_ADMISSION_OPEN, 0U))) {
    return MF_SHARED_DEVICE_LOST;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::validate_device(const mf_generation_handle_v1& handle,
                                                  FenceSnapshot& out_fence) const noexcept {
  if (!mf_registry_view_id_equal_v1(handle.registry_view_id, view_id_)) {
    return MF_SHARED_STALE_HANDLE;
  }
  const std::uint32_t device_index = find_identity(handle.identity_record_id);
  if (device_index == device_count_ ||
      identities_[device_index].committed_generation != handle.device_generation) {
    return MF_SHARED_STALE_HANDLE;
  }

  for (std::uint32_t attempt = 0; attempt < kSnapshotAttempts; ++attempt) {
    const std::uint64_t view_before =
        mf_atomic_load_u64_acquire(&view_admission_->state_generation);
    if (mf_view_admission_state_v1(view_before) != MF_VIEW_ADMISSION_OPEN) {
      return MF_SHARED_TERMINAL_VIEW;
    }
    ViewControlSnapshot control{};
    if (stable_view_control(control) != MF_SHARED_SUCCESS) {
      continue;
    }
    if (!mf_registry_view_id_equal_v1(control.view_id, view_id_) ||
        control.gate_state != MF_VIEW_GATE_OPEN || control.mapping_terminal != 0U) {
      return MF_SHARED_TERMINAL_VIEW;
    }
    const std::uint64_t device_before =
        mf_atomic_load_u64_acquire(&device_admission_[device_index].state_generation_tag);
    if (mf_device_admission_state_v1(device_before) != MF_DEVICE_ADMISSION_OPEN) {
      if (mf_device_admission_state_v1(device_before) == MF_DEVICE_ADMISSION_UPDATING) {
        continue;
      }
      return MF_SHARED_DEVICE_LOST;
    }
    std::uint64_t fence_latch = 0;
    if (stable_fence(device_index, out_fence, fence_latch) != MF_SHARED_SUCCESS) {
      continue;
    }
    if (out_fence.identity_record_id != handle.identity_record_id) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (out_fence.device_state != MF_DEVICE_STATE_ONLINE) {
      return MF_SHARED_DEVICE_LOST;
    }
    const std::uint64_t view_after = mf_atomic_load_u64_acquire(&view_admission_->state_generation);
    const std::uint64_t control_after =
        mf_atomic_load_u64_acquire(&view_control_->control_latch_sequence);
    const std::uint64_t device_after =
        mf_atomic_load_u64_acquire(&device_admission_[device_index].state_generation_tag);
    if (view_before == view_after && control.latch == control_after &&
        device_before == device_after && (fence_latch & 1U) == 0U) {
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1 RegistryView::write_fence(std::uint32_t device_index,
                                              const FenceSnapshot& intended_fence,
                                              std::uint64_t expected_device_control,
                                              std::uint64_t expected_fence_latch) noexcept {
  auto& fence = fences_[device_index];
  if (intended_fence.identity_record_id != identities_[device_index].identity_record_id ||
      intended_fence.lifecycle_sequence == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if ((expected_fence_latch & 1U) != 0U) {
    return MF_SHARED_RETRY;
  }
  if (expected_fence_latch > std::numeric_limits<std::uint64_t>::max() - 2U) {
    return MF_SHARED_OVERFLOW;
  }
  if (mf_atomic_load_u64_seq_cst(&device_admission_[device_index].state_generation_tag) !=
      expected_device_control) {
    return MF_SHARED_DEVICE_LOST;
  }
  std::uint64_t expected_latch = expected_fence_latch;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&fence.fence_latch_sequence, &expected_latch,
                                              expected_fence_latch + 1U)) {
    return MF_SHARED_RETRY;
  }
  if (fault_point_ == RecoveryFaultPoint::FenceOdd) {
    return MF_SHARED_INTERRUPTED;
  }
  mf_atomic_store_u64_relaxed(&fence.identity_record_id, intended_fence.identity_record_id);
  mf_atomic_store_u64_relaxed(&fence.lifecycle_sequence, intended_fence.lifecycle_sequence);
  mf_atomic_store_u64_relaxed(&fence.epoch, intended_fence.epoch);
  mf_atomic_store_u64_relaxed(&fence.effective_quota_bytes, intended_fence.effective_quota_bytes);
  mf_atomic_store_u64_relaxed(&fence.policy_bits, intended_fence.policy_bits);
  mf_atomic_store_u32_relaxed(&fence.device_state, intended_fence.device_state);
  mf_atomic_thread_fence_release();
  mf_atomic_store_u64_release(&fence.fence_latch_sequence, expected_fence_latch + 2U);
  if (fault_point_ == RecoveryFaultPoint::FenceCommitted) {
    return MF_SHARED_INTERRUPTED;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::publish_fence(std::uint32_t device_index,
                                                std::uint32_t expected_validation_generation,
                                                const FenceSnapshot& intended_fence,
                                                std::uint32_t& out_validation_generation) noexcept {
  if (extension_ != nullptr) {
    return publish_fence_recovery(device_index, expected_validation_generation, intended_fence,
                                  out_validation_generation);
  }
  if (device_index >= device_count_ || intended_fence.device_state != MF_DEVICE_STATE_ONLINE ||
      expected_validation_generation == 0U ||
      expected_validation_generation >= MF_DEVICE_GENERATION_MAX_NORMAL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  auto& control = device_admission_[device_index].state_generation_tag;
  std::uint64_t expected =
      mf_device_admission_pack_v1(expected_validation_generation, MF_DEVICE_ADMISSION_OPEN, 0U);
  std::uint32_t next_generation = expected_validation_generation + 1U;
  std::uint32_t update_tag = static_cast<std::uint32_t>(intended_fence.lifecycle_sequence);
  if (update_tag == 0U) {
    update_tag = 1U;
  }
  const std::uint64_t updating =
      mf_device_admission_pack_v1(next_generation, MF_DEVICE_ADMISSION_UPDATING, update_tag);
  if (!mf_atomic_compare_exchange_u64_seq_cst(&control, &expected, updating)) {
    return mf_device_admission_state_v1(expected) == MF_DEVICE_ADMISSION_CLOSED
               ? MF_SHARED_DEVICE_LOST
               : MF_SHARED_RETRY;
  }

  const std::uint64_t expected_fence_latch =
      mf_atomic_load_u64_acquire(&fences_[device_index].fence_latch_sequence);
  const mf_shared_status_v1 write_status =
      write_fence(device_index, intended_fence, updating, expected_fence_latch);
  if (write_status != MF_SHARED_SUCCESS) {
    std::uint64_t close_expected = updating;
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &control, &close_expected,
        mf_device_admission_pack_v1(next_generation, MF_DEVICE_ADMISSION_CLOSED, 0U));
    return write_status;
  }
  std::uint64_t reopen_expected = updating;
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &control, &reopen_expected,
          mf_device_admission_pack_v1(next_generation, MF_DEVICE_ADMISSION_OPEN, 0U))) {
    return MF_SHARED_DEVICE_LOST;
  }
  out_validation_generation = next_generation;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::mark_device_lost(std::uint32_t device_index,
                                                   std::uint64_t lifecycle_sequence,
                                                   std::uint64_t epoch) noexcept {
  if (device_index >= device_count_ || lifecycle_sequence == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  auto& control = device_admission_[device_index].state_generation_tag;
  std::uint64_t current = mf_atomic_load_u64_seq_cst(&control);
  std::uint64_t closed = 0;
  for (;;) {
    if (mf_device_admission_state_v1(current) == MF_DEVICE_ADMISSION_CLOSED) {
      closed = current;
      break;
    }
    const std::uint32_t current_generation = mf_device_admission_generation_v1(current);
    const std::uint32_t next_generation = current_generation >= MF_DEVICE_GENERATION_MAX_NORMAL
                                              ? MF_DEVICE_GENERATION_TERMINAL
                                              : current_generation + 1U;
    closed = mf_device_admission_pack_v1(next_generation, MF_DEVICE_ADMISSION_CLOSED, 0U);
    if (mf_atomic_compare_exchange_u64_seq_cst(&control, &current, closed)) {
      break;
    }
  }

  FenceSnapshot lost{
      .identity_record_id = identities_[device_index].identity_record_id,
      .lifecycle_sequence = lifecycle_sequence,
      .epoch = epoch,
      .effective_quota_bytes = 0U,
      .policy_bits = 0U,
      .device_state = MF_DEVICE_STATE_LOST,
  };
  for (std::uint32_t attempt = 0; attempt < kLostFenceAttempts; ++attempt) {
    const std::uint64_t expected_fence_latch =
        mf_atomic_load_u64_acquire(&fences_[device_index].fence_latch_sequence);
    const mf_shared_status_v1 status =
        write_fence(device_index, lost, closed, expected_fence_latch);
    if (status == MF_SHARED_SUCCESS) {
      return MF_SHARED_SUCCESS;
    }
    if (status != MF_SHARED_RETRY) {
      return status;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1
RegistryView::publish_telemetry(std::span<const mf_virtual_device_telemetry_v1> rows) noexcept {
  if (extension_ != nullptr) {
    return publish_telemetry_recovery(rows);
  }
  if (rows.size() != device_count_ ||
      mf_view_admission_state_v1(mf_atomic_load_u64_acquire(&view_admission_->state_generation)) !=
          MF_VIEW_ADMISSION_OPEN) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    if (rows[index].identity_record_id != identities_[index].identity_record_id ||
        rows[index].observed_lifecycle_sequence == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
  }
  mf_shared_status_v1 telemetry_status = validate_telemetry_rows(rows);
  if (telemetry_status != MF_SHARED_SUCCESS) {
    return telemetry_status;
  }

  std::uint64_t even = mf_atomic_load_u64_acquire(&telemetry_control_->telemetry_latch_sequence);
  if ((even & 1U) != 0U) {
    return MF_SHARED_RETRY;
  }
  const std::uint64_t previous_snapshot =
      mf_atomic_load_u64_relaxed(&telemetry_control_->snapshot_sequence);
  const std::uint64_t previous_publish_generation =
      mf_atomic_load_u64_relaxed(&telemetry_control_->publish_generation);
  if (even > std::numeric_limits<std::uint64_t>::max() - 2U ||
      previous_snapshot >= std::numeric_limits<std::uint64_t>::max() - 1U ||
      previous_publish_generation >= std::numeric_limits<std::uint64_t>::max() - 1U) {
    return MF_SHARED_OVERFLOW;
  }
  std::uint64_t expected = even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence,
                                              &expected, even + 1U)) {
    return MF_SHARED_RETRY;
  }
  telemetry_status = validate_telemetry_rows(rows);
  if (telemetry_status != MF_SHARED_SUCCESS) {
    mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence, even + 2U);
    return telemetry_status;
  }
  const std::uint64_t old_bank_state =
      mf_atomic_load_u64_relaxed(&telemetry_control_->active_bank_state);
  const std::uint32_t next_bank = mf_telemetry_active_bank_v1(old_bank_state) ^ 1U;
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    auto& destination = telemetry_banks_[next_bank][index];
    mf_atomic_store_u64_relaxed(&destination.identity_record_id, rows[index].identity_record_id);
    mf_atomic_store_u64_relaxed(&destination.observed_lifecycle_sequence,
                                rows[index].observed_lifecycle_sequence);
    mf_atomic_store_u64_relaxed(&destination.committed_work_items,
                                rows[index].committed_work_items);
    mf_atomic_store_u64_relaxed(&destination.completed_work_items,
                                rows[index].completed_work_items);
    mf_atomic_store_u64_relaxed(&destination.active_time_ns, rows[index].active_time_ns);
    mf_atomic_store_u64_relaxed(&destination.memory_active_time_ns,
                                rows[index].memory_active_time_ns);
    mf_atomic_store_u64_relaxed(&destination.memory_used_bytes, rows[index].memory_used_bytes);
    mf_atomic_store_u64_relaxed(&destination.memory_capacity_bytes,
                                rows[index].memory_capacity_bytes);
    mf_atomic_store_u64_relaxed(&destination.sample_time_ns, rows[index].sample_time_ns);
  }
  telemetry_status = validate_telemetry_rows(rows);
  if (telemetry_status != MF_SHARED_SUCCESS) {
    mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence, even + 2U);
    return telemetry_status;
  }
  mf_atomic_store_u64_relaxed(&telemetry_control_->snapshot_sequence, previous_snapshot + 1U);
  mf_atomic_store_u64_relaxed(&telemetry_control_->active_bank_state,
                              mf_telemetry_bank_state_pack_v1(next_bank, MF_TELEMETRY_STATE_READY));
  mf_atomic_store_u64_relaxed(&telemetry_control_->publish_generation,
                              previous_publish_generation + 1U);
  mf_atomic_thread_fence_release();
  mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence, even + 2U);
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::read_telemetry(const mf_generation_handle_v1& handle,
                                                 TelemetrySnapshot& out_snapshot) const noexcept {
  FenceSnapshot fence_before{};
  if (const mf_shared_status_v1 status = validate_device(handle, fence_before);
      status != MF_SHARED_SUCCESS) {
    return status;
  }
  const std::uint32_t device_index = find_identity(handle.identity_record_id);
  for (std::uint32_t attempt = 0; attempt < kSnapshotAttempts; ++attempt) {
    const std::uint64_t first =
        mf_atomic_load_u64_acquire(&telemetry_control_->telemetry_latch_sequence);
    if ((first & 1U) != 0U) {
      continue;
    }
    const std::uint64_t snapshot_sequence =
        mf_atomic_load_u64_relaxed(&telemetry_control_->snapshot_sequence);
    const std::uint64_t bank_state =
        mf_atomic_load_u64_relaxed(&telemetry_control_->active_bank_state);
    const std::uint32_t row_count = mf_atomic_load_u32_relaxed(&telemetry_control_->row_count);
    const std::uint32_t bank = mf_telemetry_active_bank_v1(bank_state);
    if (mf_telemetry_state_v1(bank_state) != MF_TELEMETRY_STATE_READY || bank > 1U ||
        device_index >= row_count) {
      return MF_SHARED_RETRY;
    }
    const auto& row = telemetry_banks_[bank][device_index];
    out_snapshot.identity_record_id = mf_atomic_load_u64_relaxed(&row.identity_record_id);
    out_snapshot.observed_lifecycle_sequence =
        mf_atomic_load_u64_relaxed(&row.observed_lifecycle_sequence);
    out_snapshot.committed_work_items = mf_atomic_load_u64_relaxed(&row.committed_work_items);
    out_snapshot.completed_work_items = mf_atomic_load_u64_relaxed(&row.completed_work_items);
    out_snapshot.active_time_ns = mf_atomic_load_u64_relaxed(&row.active_time_ns);
    out_snapshot.memory_active_time_ns = mf_atomic_load_u64_relaxed(&row.memory_active_time_ns);
    out_snapshot.memory_used_bytes = mf_atomic_load_u64_relaxed(&row.memory_used_bytes);
    out_snapshot.memory_capacity_bytes = mf_atomic_load_u64_relaxed(&row.memory_capacity_bytes);
    out_snapshot.sample_time_ns = mf_atomic_load_u64_relaxed(&row.sample_time_ns);
    out_snapshot.snapshot_sequence = snapshot_sequence;
    mf_atomic_signal_fence_seq_cst();
    const std::uint64_t second =
        mf_atomic_load_u64_acquire(&telemetry_control_->telemetry_latch_sequence);
    if (first != second) {
      continue;
    }
    if (out_snapshot.identity_record_id != handle.identity_record_id ||
        out_snapshot.observed_lifecycle_sequence < fence_before.lifecycle_sequence) {
      return MF_SHARED_RETRY;
    }
    FenceSnapshot fence_after{};
    const mf_shared_status_v1 final_status = validate_device(handle, fence_after);
    if (final_status != MF_SHARED_SUCCESS) {
      return final_status;
    }
    if (fence_after.lifecycle_sequence == fence_before.lifecycle_sequence) {
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

mf_shared_status_v1 RegistryView::close() noexcept {
  if (extension_ != nullptr) {
    return close_recovery();
  }
  std::uint64_t admission = mf_atomic_load_u64_seq_cst(&view_admission_->state_generation);
  for (;;) {
    const std::uint32_t state = mf_view_admission_state_v1(admission);
    if (state == MF_VIEW_ADMISSION_TERMINAL || state == MF_VIEW_ADMISSION_QUARANTINED) {
      return MF_SHARED_SUCCESS;
    }
    const std::uint64_t closing =
        mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_CLOSING);
    if (mf_atomic_compare_exchange_u64_seq_cst(&view_admission_->state_generation, &admission,
                                               closing)) {
      break;
    }
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    std::uint64_t current =
        mf_atomic_load_u64_seq_cst(&device_admission_[index].state_generation_tag);
    while (mf_device_admission_state_v1(current) != MF_DEVICE_ADMISSION_CLOSED ||
           mf_device_admission_generation_v1(current) != MF_DEVICE_GENERATION_TERMINAL) {
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &device_admission_[index].state_generation_tag, &current,
              mf_device_admission_pack_v1(MF_DEVICE_GENERATION_TERMINAL, MF_DEVICE_ADMISSION_CLOSED,
                                          0U))) {
        break;
      }
    }
  }

  std::uint64_t even = mf_atomic_load_u64_acquire(&view_control_->control_latch_sequence);
  if ((even & 1U) != 0U || even > std::numeric_limits<std::uint64_t>::max() - 2U) {
    mf_atomic_store_u64_seq_cst(
        &view_admission_->state_generation,
        mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_QUARANTINED));
    return MF_SHARED_RETRY;
  }
  std::uint64_t expected = even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&view_control_->control_latch_sequence, &expected,
                                              even + 1U)) {
    mf_atomic_store_u64_seq_cst(
        &view_admission_->state_generation,
        mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_QUARANTINED));
    return MF_SHARED_RETRY;
  }
  mf_atomic_store_u64_relaxed(&view_control_->publication_cursor,
                              std::numeric_limits<std::uint64_t>::max());
  mf_atomic_store_u64_relaxed(&view_control_->gate_generation,
                              std::numeric_limits<std::uint64_t>::max());
  mf_atomic_store_u32_relaxed(&view_control_->gate_state, MF_VIEW_GATE_TERMINAL);
  mf_atomic_store_u32_relaxed(&view_control_->mapping_terminal, 1U);
  mf_atomic_thread_fence_release();
  mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
  mf_atomic_store_u64_seq_cst(
      &view_admission_->state_generation,
      mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_TERMINAL));
  return MF_SHARED_SUCCESS;
}

} // namespace metaflux::runtime
