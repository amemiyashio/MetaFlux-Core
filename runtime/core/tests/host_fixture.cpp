#include "metaflux/runtime/host_fixture.h"

#include "metaflux/client/fastpath.h"
#include "metaflux/runtime/core.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <linux/memfd.h>
#include <new>
#include <optional>
#include <span>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kRingCapacity = 256U;
constexpr std::uint64_t kIdentityRecordId = 1U;
constexpr std::uint64_t kDeviceGeneration = 1U;
constexpr std::uint64_t kObjectGeneration = 1U;
constexpr std::uint64_t kFirstObjectId = 1024U;
constexpr std::uint64_t kFixtureMemoryCapacity = 256U * 1024U * 1024U;
constexpr std::uint64_t kMaximumObjectSize = 64U * 1024U * 1024U;

enum class ObjectKind : std::uint32_t {
  kDeviceMemory = MF_OBJECT_TYPE_DEVICE_MEMORY,
  kHostMemory = MF_OBJECT_TYPE_HOST_MEMORY,
  kArtifact = MF_OBJECT_TYPE_ARTIFACT,
  kArgumentBlock = MF_OBJECT_TYPE_ARGUMENT_BLOCK,
  kModule = MF_OBJECT_TYPE_MODULE,
};

struct FixtureObject final {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
  ObjectKind kind = ObjectKind::kDeviceMemory;
  bool alive = false;
  std::uint64_t linked_id = 0;
  std::uint64_t linked_generation = 0;
  std::vector<std::uint8_t> bytes;
};

[[nodiscard]] bool view_equal(mf_registry_view_id_v1 left, mf_registry_view_id_v1 right) noexcept {
  return mf_registry_view_id_equal_v1(left, right) != 0;
}

[[nodiscard]] bool memory_kind(ObjectKind kind) noexcept {
  return kind == ObjectKind::kDeviceMemory || kind == ObjectKind::kHostMemory;
}

} // namespace

struct mf_host_fixture_v1 final {
  int registry_fd = -1;
  void* registry_mapping = nullptr;
  std::uint64_t registry_mapping_size = 0;
  bool registry_initialized = false;
  metaflux::runtime::RegistryView registry;
  mf_registry_view_id_v1 view_id{UINT64_C(0x4d46584d30303031), UINT64_C(1)};
  mf_client_ring_v1 submission{};
  mf_client_ring_v1 completion{};
  std::vector<FixtureObject> objects;
  std::optional<mf_ring_descriptor_v1> pending_completion;
  std::uint64_t next_object_id = kFirstObjectId;
  std::uint64_t committed_work_items = 0;
  std::uint64_t completed_work_items = 0;
  std::uint64_t sample_time = 1;
  std::uint64_t event_timeline = 0;

  mf_host_fixture_v1() noexcept {
    submission.owned_fd = -1;
    completion.owned_fd = -1;
  }

  [[nodiscard]] FixtureObject* find(std::uint64_t id) noexcept {
    for (auto& object : objects) {
      if (object.id == id) {
        return &object;
      }
    }
    return nullptr;
  }

  [[nodiscard]] const FixtureObject* find(std::uint64_t id) const noexcept {
    for (const auto& object : objects) {
      if (object.id == id) {
        return &object;
      }
    }
    return nullptr;
  }

  [[nodiscard]] mf_shared_status_v1 resolve(std::uint64_t id, std::uint64_t generation,
                                            ObjectKind expected,
                                            FixtureObject*& out_object) noexcept {
    FixtureObject* object = find(id);
    if (object == nullptr || !object->alive || object->generation != generation) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (object->kind != expected) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    out_object = object;
    return MF_SHARED_SUCCESS;
  }

  [[nodiscard]] mf_shared_status_v1 resolve_memory(std::uint64_t id, std::uint64_t generation,
                                                   FixtureObject*& out_object) noexcept {
    FixtureObject* object = find(id);
    if (object == nullptr || !object->alive || object->generation != generation) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (!memory_kind(object->kind)) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    out_object = object;
    return MF_SHARED_SUCCESS;
  }

  [[nodiscard]] mf_shared_status_v1 add_object(ObjectKind kind, const std::uint8_t* payload,
                                               std::uint64_t payload_size, std::uint64_t linked_id,
                                               std::uint64_t linked_generation,
                                               std::uint64_t& out_id,
                                               std::uint64_t& out_generation) noexcept {
    if (payload_size > kMaximumObjectSize ||
        payload_size > std::numeric_limits<std::size_t>::max() ||
        next_object_id == std::numeric_limits<std::uint64_t>::max()) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    try {
      FixtureObject object;
      object.id = next_object_id++;
      object.generation = kObjectGeneration;
      object.kind = kind;
      object.alive = true;
      object.linked_id = linked_id;
      object.linked_generation = linked_generation;
      object.bytes.resize(static_cast<std::size_t>(payload_size));
      if (payload_size != 0U && payload != nullptr) {
        std::memcpy(object.bytes.data(), payload, static_cast<std::size_t>(payload_size));
      }
      out_id = object.id;
      out_generation = object.generation;
      objects.push_back(std::move(object));
      return MF_SHARED_SUCCESS;
    } catch (const std::bad_alloc&) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
  }

  [[nodiscard]] mf_shared_status_v1 publish_telemetry() noexcept {
    std::uint64_t memory_used = 0;
    for (const auto& object : objects) {
      if (object.alive && object.kind == ObjectKind::kDeviceMemory) {
        const std::uint64_t size = static_cast<std::uint64_t>(object.bytes.size());
        memory_used = size > kFixtureMemoryCapacity - memory_used ? kFixtureMemoryCapacity
                                                                  : memory_used + size;
      }
    }
    mf_virtual_device_telemetry_v1 row{};
    row.identity_record_id = kIdentityRecordId;
    row.observed_lifecycle_sequence = 1U;
    row.committed_work_items = committed_work_items;
    row.completed_work_items = completed_work_items;
    row.active_time_ns = completed_work_items;
    row.memory_active_time_ns = completed_work_items;
    row.memory_used_bytes = memory_used;
    row.memory_capacity_bytes = kFixtureMemoryCapacity;
    row.sample_time_ns = sample_time++;
    return registry.publish_telemetry(std::span<const mf_virtual_device_telemetry_v1>(&row, 1U));
  }
};

namespace {

void initialize_response(const mf_host_fixture_v1& fixture,
                         const mf_client_control_request_v1& request, std::uint32_t status,
                         std::uint32_t flags, std::uint64_t object_id,
                         std::uint64_t object_generation,
                         mf_client_control_response_v1& response) noexcept {
  mf_client_control_response_init_v1(&response, status, flags,
                                     mf_client_load_le64_v1(request.bytes + 24),
                                     fixture.view_id.daemon_incarnation,
                                     fixture.view_id.view_serial, object_id, object_generation);
}

[[nodiscard]] std::uint32_t shared_to_control_status(mf_shared_status_v1 status) noexcept {
  switch (status) {
  case MF_SHARED_SUCCESS:
    return MF_CLIENT_CONTROL_OK;
  case MF_SHARED_STALE_HANDLE:
    return MF_CLIENT_CONTROL_STALE_GENERATION;
  case MF_SHARED_INVALID_ARGUMENT:
    return MF_CLIENT_CONTROL_INVALID_ARGUMENT;
  case MF_SHARED_RESOURCE_EXHAUSTED:
    return MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED;
  case MF_SHARED_NOT_SUPPORTED:
    return MF_CLIENT_CONTROL_UNSUPPORTED;
  default:
    return MF_CLIENT_CONTROL_INTERNAL_ERROR;
  }
}

[[nodiscard]] mf_shared_status_v1 release_object(mf_host_fixture_v1& fixture, std::uint64_t id,
                                                 std::uint64_t generation,
                                                 ObjectKind kind) noexcept {
  FixtureObject* object = nullptr;
  const mf_shared_status_v1 status = fixture.resolve(id, generation, kind, object);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  object->alive = false;
  object->bytes.clear();
  return MF_SHARED_SUCCESS;
}

[[nodiscard]] mf_shared_status_v1 process_launch(mf_host_fixture_v1& fixture,
                                                 const mf_ring_descriptor_v1& command) noexcept {
  FixtureObject* module = nullptr;
  FixtureObject* artifact = nullptr;
  FixtureObject* argument_block = nullptr;
  FixtureObject* destination = nullptr;
  FixtureObject* left = nullptr;
  FixtureObject* right = nullptr;
  if (fixture.resolve(command.target_id, command.arguments[0], ObjectKind::kModule, module) !=
          MF_SHARED_SUCCESS ||
      fixture.resolve(module->linked_id, module->linked_generation, ObjectKind::kArtifact,
                      artifact) != MF_SHARED_SUCCESS) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (command.arguments[1] != MF_HOST_FIXTURE_KERNEL_ADD_I32) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (fixture.resolve(command.arguments[2], command.arguments[3], ObjectKind::kArgumentBlock,
                      argument_block) != MF_SHARED_SUCCESS ||
      argument_block->bytes.size() != sizeof(mf_host_fixture_add_arguments_v1)) {
    return MF_SHARED_STALE_HANDLE;
  }

  mf_host_fixture_add_arguments_v1 arguments{};
  std::memcpy(&arguments, argument_block->bytes.data(), sizeof(arguments));
  if (arguments.reserved != 0U ||
      arguments.element_count > std::numeric_limits<std::uint64_t>::max() / sizeof(std::uint32_t)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::uint64_t byte_count = arguments.element_count * sizeof(std::uint32_t);
  if (fixture.resolve(arguments.destination_id, arguments.destination_generation,
                      ObjectKind::kDeviceMemory, destination) != MF_SHARED_SUCCESS ||
      fixture.resolve(arguments.left_id, arguments.left_generation, ObjectKind::kDeviceMemory,
                      left) != MF_SHARED_SUCCESS ||
      fixture.resolve(arguments.right_id, arguments.right_generation, ObjectKind::kDeviceMemory,
                      right) != MF_SHARED_SUCCESS) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (byte_count > destination->bytes.size() || byte_count > left->bytes.size() ||
      byte_count > right->bytes.size()) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (std::uint64_t index = 0; index < arguments.element_count; ++index) {
    std::uint32_t left_value = 0;
    std::uint32_t right_value = 0;
    std::uint32_t result = 0;
    const std::size_t offset = static_cast<std::size_t>(index * sizeof(std::uint32_t));
    std::memcpy(&left_value, left->bytes.data() + offset, sizeof(left_value));
    std::memcpy(&right_value, right->bytes.data() + offset, sizeof(right_value));
    result = left_value + right_value;
    std::memcpy(destination->bytes.data() + offset, &result, sizeof(result));
  }
  return MF_SHARED_SUCCESS;
}

[[nodiscard]] mf_shared_status_v1
process_command(mf_host_fixture_v1& fixture, const mf_ring_descriptor_v1& command,
                std::uint64_t& result_id, std::uint64_t& result_generation, std::uint64_t& timeline,
                std::uint64_t& detail) noexcept {
  result_id = command.target_id;
  result_generation = 0;
  timeline = 0;
  detail = 0;

  switch (command.opcode) {
  case MF_RING_OPCODE_NOOP:
    result_generation = 1U;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_MEMORY_ALLOC: {
    if (command.target_id != MF_HOST_FIXTURE_CONTEXT_ID || command.arguments[0] == 0U ||
        command.arguments[1] == 0U || (command.arguments[1] & (command.arguments[1] - 1U)) != 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    return fixture.add_object(ObjectKind::kDeviceMemory, nullptr, command.arguments[0], 0U, 0U,
                              result_id, result_generation);
  }
  case MF_RING_OPCODE_MEMORY_FREE:
    result_generation = command.arguments[0];
    return release_object(fixture, command.target_id, command.arguments[0],
                          ObjectKind::kDeviceMemory);
  case MF_RING_OPCODE_MODULE_LOAD: {
    FixtureObject* artifact = nullptr;
    if (fixture.resolve(command.target_id, command.arguments[0], ObjectKind::kArtifact, artifact) !=
        MF_SHARED_SUCCESS) {
      return MF_SHARED_STALE_HANDLE;
    }
    return fixture.add_object(ObjectKind::kModule, nullptr, 0U, artifact->id, artifact->generation,
                              result_id, result_generation);
  }
  case MF_RING_OPCODE_MODULE_UNLOAD:
    result_generation = command.arguments[0];
    return release_object(fixture, command.target_id, command.arguments[0], ObjectKind::kModule);
  case MF_RING_OPCODE_COPY: {
    FixtureObject* destination = nullptr;
    FixtureObject* source = nullptr;
    std::uint64_t destination_offset = 0U;
    std::uint64_t source_offset = 0U;
    std::uint64_t byte_count = 0U;
    result_generation = command.arguments[0];
    mf_shared_status_v1 destination_status = MF_SHARED_SUCCESS;
    mf_shared_status_v1 source_status = MF_SHARED_SUCCESS;
    if (command.flags == MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1) {
      constexpr std::size_t copy_argument_size =
          sizeof(mf_argument_block_header_v1) +
          MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 * sizeof(mf_argument_entry_v1);
      alignas(64) std::array<std::uint8_t, copy_argument_size> copy_arguments{};
      FixtureObject* argument_block = nullptr;
      if (command.arguments[1] != 0U || command.arguments[2] != 0U || command.arguments[3] != 0U ||
          fixture.resolve(command.target_id, command.arguments[0], ObjectKind::kArgumentBlock,
                          argument_block) != MF_SHARED_SUCCESS ||
          argument_block->bytes.size() != copy_arguments.size()) {
        return MF_SHARED_MALFORMED;
      }
      std::memcpy(copy_arguments.data(), argument_block->bytes.data(), copy_arguments.size());
      if (mf_client_copy_region_argument_block_validate_v1(
              copy_arguments.data(), copy_arguments.size()) != MF_SHARED_SUCCESS) {
        return MF_SHARED_MALFORMED;
      }
      const auto* entries = reinterpret_cast<const mf_argument_entry_v1*>(
          copy_arguments.data() + sizeof(mf_argument_block_header_v1));
      const mf_argument_entry_v1& destination_entry = entries[MF_COPY_REGION_DESTINATION_INDEX_V1];
      const mf_argument_entry_v1& source_entry = entries[MF_COPY_REGION_SOURCE_INDEX_V1];
      destination_status = fixture.resolve_memory(destination_entry.object_id,
                                                  destination_entry.object_generation, destination);
      source_status =
          fixture.resolve_memory(source_entry.object_id, source_entry.object_generation, source);
      destination_offset = destination_entry.value;
      source_offset = source_entry.value;
      byte_count = entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value;
    } else if (command.flags == 0U) {
      destination_status =
          fixture.resolve_memory(command.target_id, command.arguments[0], destination);
      source_status = fixture.resolve_memory(command.arguments[1], command.arguments[2], source);
      byte_count = command.arguments[3];
    } else {
      return MF_SHARED_MALFORMED;
    }
    if (destination_status != MF_SHARED_SUCCESS || source_status != MF_SHARED_SUCCESS) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (byte_count == 0U || destination_offset > destination->bytes.size() ||
        source_offset > source->bytes.size() ||
        byte_count > destination->bytes.size() - destination_offset ||
        byte_count > source->bytes.size() - source_offset) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    std::memmove(destination->bytes.data() + static_cast<std::size_t>(destination_offset),
                 source->bytes.data() + static_cast<std::size_t>(source_offset),
                 static_cast<std::size_t>(byte_count));
    return MF_SHARED_SUCCESS;
  }
  case MF_RING_OPCODE_LAUNCH:
    result_generation = command.arguments[0];
    return process_launch(fixture, command);
  case MF_RING_OPCODE_EVENT_RECORD:
    result_generation = command.arguments[0];
    if (command.target_id != MF_HOST_FIXTURE_EVENT_ID ||
        command.arguments[0] != MF_HOST_FIXTURE_EVENT_GENERATION || command.arguments[1] == 0U) {
      return MF_SHARED_STALE_HANDLE;
    }
    fixture.event_timeline = command.arguments[1];
    timeline = fixture.event_timeline;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_EVENT_WAIT:
    result_generation = command.arguments[0];
    if (command.target_id != MF_HOST_FIXTURE_EVENT_ID ||
        command.arguments[0] != MF_HOST_FIXTURE_EVENT_GENERATION) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (fixture.event_timeline < command.arguments[1]) {
      return MF_SHARED_WOULD_BLOCK;
    }
    timeline = fixture.event_timeline;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_QUEUE_SYNCHRONIZE:
  case MF_RING_OPCODE_QUEUE_CANCEL:
    if (command.target_id != MF_HOST_FIXTURE_SUBMISSION_QUEUE_ID) {
      return MF_SHARED_STALE_HANDLE;
    }
    result_generation = MF_HOST_FIXTURE_QUEUE_GENERATION;
    return MF_SHARED_SUCCESS;
  default:
    return MF_SHARED_NOT_SUPPORTED;
  }
}

[[nodiscard]] mf_ring_descriptor_v1
make_completion(const mf_ring_descriptor_v1& command, mf_shared_status_v1 status,
                std::uint64_t result_id, std::uint64_t result_generation, std::uint64_t timeline,
                std::uint64_t detail) noexcept {
  mf_ring_descriptor_v1 completion{};
  completion.opcode = MF_RING_OPCODE_COMPLETION;
  completion.flags = command.flags;
  completion.request_id = command.request_id;
  completion.target_id = result_id;
  completion.arguments[0] = static_cast<std::uint32_t>(status);
  completion.arguments[1] = result_generation;
  completion.arguments[2] = timeline;
  completion.arguments[3] = detail;
  return completion;
}

} // namespace

extern "C" {

mf_shared_status_v1 mf_host_fixture_create_v1(mf_host_fixture_v1** out_fixture) {
  if (out_fixture == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_fixture = nullptr;
  auto* fixture = new (std::nothrow) mf_host_fixture_v1();
  if (fixture == nullptr) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }

  if (metaflux::runtime::RegistryView::required_mapping_size(1U, fixture->registry_mapping_size) !=
          MF_SHARED_SUCCESS ||
      fixture->registry_mapping_size >
          static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
    delete fixture;
    return MF_SHARED_SYSTEM_ERROR;
  }
  const long created_fd =
      syscall(SYS_memfd_create, "metaflux-registry-fixture-v1", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (created_fd < 0 || created_fd > std::numeric_limits<int>::max()) {
    delete fixture;
    return MF_SHARED_SYSTEM_ERROR;
  }
  fixture->registry_fd = static_cast<int>(created_fd);
  if (ftruncate(fixture->registry_fd, static_cast<off_t>(fixture->registry_mapping_size)) != 0) {
    mf_host_fixture_destroy_v1(fixture);
    return MF_SHARED_SYSTEM_ERROR;
  }
  fixture->registry_mapping =
      mmap(nullptr, static_cast<std::size_t>(fixture->registry_mapping_size),
           PROT_READ | PROT_WRITE, MAP_SHARED, fixture->registry_fd, 0);
  if (fixture->registry_mapping == MAP_FAILED) {
    fixture->registry_mapping = nullptr;
    mf_host_fixture_destroy_v1(fixture);
    return MF_SHARED_SYSTEM_ERROR;
  }

  std::array<mf_virtual_device_identity_v1, 1> identities{};
  identities[0].identity_record_id = kIdentityRecordId;
  identities[0].logical_device_id[15] = UINT8_C(1);
  identities[0].gpu_uuid[15] = UINT8_C(1);
  constexpr char name[] = "MetaFlux host fixture";
  std::memcpy(identities[0].display_name, name, sizeof(name));
  identities[0].committed_generation = kDeviceGeneration;
  identities[0].capability_bits = UINT64_C(1);
  identities[0].backend_id = UINT32_C(1);
  identities[0].virtual_compute_capability = UINT32_C(80);

  const std::array<metaflux::runtime::FenceSnapshot, 1> fences{{
      {kIdentityRecordId, 1U, 1U, kFixtureMemoryCapacity, 0U, MF_DEVICE_STATE_ONLINE},
  }};
  if (metaflux::runtime::RegistryView::initialize(
          fixture->registry_mapping, fixture->registry_mapping_size, fixture->view_id, 1U,
          identities, fences, fixture->registry) != MF_SHARED_SUCCESS) {
    mf_host_fixture_destroy_v1(fixture);
    return MF_SHARED_SYSTEM_ERROR;
  }
  fixture->registry_initialized = true;
  if (fcntl(fixture->registry_fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) != 0 ||
      mf_client_ring_create_v1(kRingCapacity, fixture->view_id, MF_HOST_FIXTURE_SUBMISSION_QUEUE_ID,
                               MF_HOST_FIXTURE_QUEUE_GENERATION,
                               &fixture->submission) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(kRingCapacity, fixture->view_id, MF_HOST_FIXTURE_COMPLETION_QUEUE_ID,
                               MF_HOST_FIXTURE_QUEUE_GENERATION,
                               &fixture->completion) != MF_SHARED_SUCCESS ||
      fixture->publish_telemetry() != MF_SHARED_SUCCESS) {
    mf_host_fixture_destroy_v1(fixture);
    return MF_SHARED_SYSTEM_ERROR;
  }

  *out_fixture = fixture;
  return MF_SHARED_SUCCESS;
}

void mf_host_fixture_destroy_v1(mf_host_fixture_v1* fixture) {
  if (fixture == nullptr) {
    return;
  }
  mf_client_ring_close_v1(&fixture->completion);
  mf_client_ring_close_v1(&fixture->submission);
  if (fixture->registry_initialized) {
    (void)fixture->registry.close();
  }
  if (fixture->registry_mapping != nullptr &&
      fixture->registry_mapping_size <= std::numeric_limits<std::size_t>::max()) {
    (void)munmap(fixture->registry_mapping,
                 static_cast<std::size_t>(fixture->registry_mapping_size));
  }
  if (fixture->registry_fd >= 0) {
    (void)close(fixture->registry_fd);
  }
  delete fixture;
}

mf_registry_view_id_v1 mf_host_fixture_view_id_v1(const mf_host_fixture_v1* fixture) {
  return fixture == nullptr ? mf_registry_view_id_v1{0U, 0U} : fixture->view_id;
}

int32_t mf_host_fixture_registry_fd_v1(const mf_host_fixture_v1* fixture) {
  return fixture == nullptr ? -1 : fixture->registry_fd;
}

int32_t mf_host_fixture_submission_fd_v1(const mf_host_fixture_v1* fixture) {
  return fixture == nullptr ? -1 : mf_client_ring_borrow_fd_v1(&fixture->submission);
}

int32_t mf_host_fixture_completion_fd_v1(const mf_host_fixture_v1* fixture) {
  return fixture == nullptr ? -1 : mf_client_ring_borrow_fd_v1(&fixture->completion);
}

mf_shared_status_v1 mf_host_fixture_control_v1(mf_host_fixture_v1* fixture,
                                               const mf_client_control_request_v1* request,
                                               const uint8_t* payload, uint64_t payload_size,
                                               mf_client_control_response_v1* response) {
  if (fixture == nullptr || request == nullptr || response == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (mf_client_control_request_validate_v1(request) != MF_CLIENT_CONTROL_OK) {
    return MF_SHARED_MALFORMED;
  }
  const mf_registry_view_id_v1 requested_view{
      mf_client_load_le64_v1(request->bytes + 32),
      mf_client_load_le64_v1(request->bytes + 40),
  };
  if (!view_equal(requested_view, fixture->view_id)) {
    initialize_response(*fixture, *request, MF_CLIENT_CONTROL_STALE_GENERATION, UINT32_C(0),
                        UINT64_C(0), UINT64_C(0), *response);
    return MF_SHARED_SUCCESS;
  }

  const std::uint16_t opcode = mf_client_load_le16_v1(request->bytes + 12);
  const std::uint16_t flags = mf_client_load_le16_v1(request->bytes + 14);
  const std::uint64_t object_id = mf_client_load_le64_v1(request->bytes + 48);
  const std::uint64_t argument = mf_client_load_le64_v1(request->bytes + 56);
  std::uint64_t response_id = object_id;
  std::uint64_t response_generation = argument;
  std::uint32_t response_flags = UINT32_C(0);
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;

  switch (opcode) {
  case MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1:
    if (flags != UINT16_C(0) || object_id != MF_HOST_FIXTURE_CONTEXT_ID || argument == 0U) {
      status = MF_SHARED_INVALID_ARGUMENT;
      break;
    }
    status = fixture->add_object(ObjectKind::kDeviceMemory, nullptr, argument, 0U, 0U, response_id,
                                 response_generation);
    break;
  case MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1:
    if (flags != UINT16_C(0) || payload_size != 0U) {
      status = MF_SHARED_INVALID_ARGUMENT;
      break;
    }
    status = release_object(*fixture, object_id, argument, ObjectKind::kDeviceMemory);
    break;
  case MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1:
    if ((flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) == 0U ||
        (flags & (MF_CLIENT_CONTROL_FLAG_READ | MF_CLIENT_CONTROL_FLAG_WRITE)) == 0U ||
        (flags & MF_CLIENT_CONTROL_FLAG_PTX) != 0U || object_id != MF_HOST_FIXTURE_CONTEXT_ID ||
        argument == 0U || payload == nullptr || payload_size != argument) {
      status = MF_SHARED_INVALID_ARGUMENT;
      break;
    }
    status = fixture->add_object(ObjectKind::kHostMemory, payload, payload_size, 0U, 0U,
                                 response_id, response_generation);
    break;
  case MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1:
    status = flags == UINT16_C(0) && payload_size == 0U
                 ? release_object(*fixture, object_id, argument, ObjectKind::kHostMemory)
                 : MF_SHARED_INVALID_ARGUMENT;
    break;
  case MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1:
    if (flags != (MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX) ||
        object_id != MF_HOST_FIXTURE_CONTEXT_ID || argument == 0U || payload == nullptr ||
        payload_size != argument) {
      status = MF_SHARED_INVALID_ARGUMENT;
      break;
    }
    status = fixture->add_object(ObjectKind::kArtifact, payload, payload_size, 0U, 0U, response_id,
                                 response_generation);
    break;
  case MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1: {
    FixtureObject* artifact = nullptr;
    status = flags == UINT16_C(0) && payload_size == 0U
                 ? fixture->resolve(object_id, argument, ObjectKind::kArtifact, artifact)
                 : MF_SHARED_INVALID_ARGUMENT;
    response_flags = status == MF_SHARED_SUCCESS ? MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD : UINT32_C(0);
    break;
  }
  case MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1:
    status = flags == UINT16_C(0) && payload_size == 0U
                 ? release_object(*fixture, object_id, argument, ObjectKind::kArtifact)
                 : MF_SHARED_INVALID_ARGUMENT;
    break;
  case MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1:
    if (flags != MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD || object_id != MF_HOST_FIXTURE_CONTEXT_ID ||
        argument == 0U || payload == nullptr || payload_size != argument) {
      status = MF_SHARED_INVALID_ARGUMENT;
      break;
    }
    status = fixture->add_object(ObjectKind::kArgumentBlock, payload, payload_size, 0U, 0U,
                                 response_id, response_generation);
    break;
  case MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1:
    status = flags == UINT16_C(0) && payload_size == 0U
                 ? release_object(*fixture, object_id, argument, ObjectKind::kArgumentBlock)
                 : MF_SHARED_INVALID_ARGUMENT;
    break;
  default:
    status = MF_SHARED_NOT_SUPPORTED;
    break;
  }

  initialize_response(*fixture, *request, shared_to_control_status(status), response_flags,
                      status == MF_SHARED_SUCCESS ? response_id : UINT64_C(0),
                      status == MF_SHARED_SUCCESS ? response_generation : UINT64_C(0), *response);
  if (status == MF_SHARED_SUCCESS && opcode != MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1) {
    const mf_shared_status_v1 telemetry_status = fixture->publish_telemetry();
    if (telemetry_status != MF_SHARED_SUCCESS) {
      return telemetry_status;
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_host_fixture_write_object_v1(mf_host_fixture_v1* fixture, uint64_t object_id,
                                                    uint64_t object_generation, uint64_t offset,
                                                    const uint8_t* bytes, uint64_t byte_count) {
  if (fixture == nullptr || (byte_count != 0U && bytes == nullptr)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  FixtureObject* object = fixture->find(object_id);
  if (object == nullptr || !object->alive || object->generation != object_generation) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (!memory_kind(object->kind) || offset > object->bytes.size() ||
      byte_count > object->bytes.size() - static_cast<std::size_t>(offset)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (byte_count != 0U) {
    std::memcpy(object->bytes.data() + static_cast<std::size_t>(offset), bytes,
                static_cast<std::size_t>(byte_count));
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_host_fixture_read_object_v1(const mf_host_fixture_v1* fixture,
                                                   uint64_t object_id, uint64_t object_generation,
                                                   uint64_t offset, uint8_t* bytes,
                                                   uint64_t byte_count) {
  if (fixture == nullptr || (byte_count != 0U && bytes == nullptr)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const FixtureObject* object = fixture->find(object_id);
  if (object == nullptr || !object->alive || object->generation != object_generation) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (offset > object->bytes.size() ||
      byte_count > object->bytes.size() - static_cast<std::size_t>(offset)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (byte_count != 0U) {
    std::memcpy(bytes, object->bytes.data() + static_cast<std::size_t>(offset),
                static_cast<std::size_t>(byte_count));
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_host_fixture_pump_once_v1(mf_host_fixture_v1* fixture) {
  if (fixture == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (fixture->pending_completion.has_value()) {
    const mf_shared_status_v1 pending_status =
        mf_client_ring_try_submit_v1(&fixture->completion, &*fixture->pending_completion);
    if (pending_status == MF_SHARED_SUCCESS) {
      fixture->pending_completion.reset();
    }
    return pending_status;
  }

  mf_ring_descriptor_v1 command{};
  const mf_shared_status_v1 consume_status =
      mf_client_ring_try_consume_v1(&fixture->submission, &command);
  if (consume_status != MF_SHARED_SUCCESS) {
    return consume_status;
  }
  ++fixture->committed_work_items;
  std::uint64_t result_id = command.target_id;
  std::uint64_t result_generation = 0;
  std::uint64_t timeline = 0;
  std::uint64_t detail = 0;
  const mf_shared_status_v1 command_status =
      process_command(*fixture, command, result_id, result_generation, timeline, detail);
  ++fixture->completed_work_items;
  const mf_shared_status_v1 telemetry_status = fixture->publish_telemetry();
  if (telemetry_status != MF_SHARED_SUCCESS) {
    return telemetry_status;
  }

  const mf_ring_descriptor_v1 completion =
      make_completion(command, command_status, result_id, result_generation, timeline, detail);
  const mf_shared_status_v1 submit_status =
      mf_client_ring_try_submit_v1(&fixture->completion, &completion);
  if (submit_status == MF_SHARED_WOULD_BLOCK) {
    fixture->pending_completion = completion;
  }
  return submit_status;
}

} // extern "C"
