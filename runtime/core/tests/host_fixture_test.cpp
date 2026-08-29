#include "metaflux/runtime/host_fixture.h"

#include "metaflux/client/fastpath.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace {

struct ObjectRef final {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
};

constexpr std::size_t copy_argument_size =
    sizeof(mf_argument_block_header_v1) +
    MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 * sizeof(mf_argument_entry_v1);

struct alignas(64) CopyArgumentPayload final {
  std::array<std::uint8_t, copy_argument_size> bytes{};
};

void initialize_copy_arguments(CopyArgumentPayload& payload, ObjectRef destination,
                               std::uint64_t destination_offset, ObjectRef source,
                               std::uint64_t source_offset, std::uint64_t byte_count) {
  payload.bytes.fill(0U);
  auto* header = reinterpret_cast<mf_argument_block_header_v1*>(payload.bytes.data());
  auto* entries = reinterpret_cast<mf_argument_entry_v1*>(payload.bytes.data() +
                                                          sizeof(mf_argument_block_header_v1));
  header->magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
  header->abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  header->header_size = sizeof(*header);
  header->entry_size = sizeof(entries[0]);
  header->entry_count = MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1;
  header->flags = MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1;
  header->total_size = payload.bytes.size();
  entries[MF_COPY_REGION_DESTINATION_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags = MF_ARGUMENT_BUFFER_WRITE;
  entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_id = destination.id;
  entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_generation = destination.generation;
  entries[MF_COPY_REGION_DESTINATION_INDEX_V1].value = destination_offset;
  entries[MF_COPY_REGION_SOURCE_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  entries[MF_COPY_REGION_SOURCE_INDEX_V1].flags = MF_ARGUMENT_BUFFER_READ;
  entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_id = source.id;
  entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_generation = source.generation;
  entries[MF_COPY_REGION_SOURCE_INDEX_V1].value = source_offset;
  entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].kind = MF_ARGUMENT_KIND_U64;
  entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = byte_count;
}

struct FixtureOwner final {
  mf_host_fixture_v1* value = nullptr;
  ~FixtureOwner() { mf_host_fixture_destroy_v1(value); }
};

struct RegistryOwner final {
  mf_client_registry_v1 value{};
  ~RegistryOwner() { mf_client_registry_close_v1(&value); }
};

struct RingOwner final {
  mf_client_ring_v1 value{};
  RingOwner() { value.owned_fd = -1; }
  ~RingOwner() { mf_client_ring_close_v1(&value); }
};

[[nodiscard]] bool control(mf_host_fixture_v1* fixture, mf_registry_view_id_v1 view_id,
                           std::uint64_t& request_id, std::uint16_t opcode, std::uint16_t flags,
                           std::uint64_t object_id, std::uint64_t argument,
                           std::span<const std::uint8_t> payload, std::uint32_t expected_status,
                           ObjectRef& out_object, std::uint32_t* out_flags = nullptr) {
  mf_client_control_request_v1 request{};
  mf_client_control_response_v1 response{};
  mf_client_control_request_init_v1(&request, opcode, flags, request_id++,
                                    view_id.daemon_incarnation, view_id.view_serial, object_id,
                                    argument);
  if (mf_host_fixture_control_v1(fixture, &request, payload.empty() ? nullptr : payload.data(),
                                 payload.size(), &response) != MF_SHARED_SUCCESS ||
      mf_client_control_response_validate_v1(&response) != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le32_v1(response.bytes + 12) != expected_status) {
    return false;
  }
  out_object.id = mf_client_load_le64_v1(response.bytes + 48);
  out_object.generation = mf_client_load_le64_v1(response.bytes + 56);
  if (out_flags != nullptr) {
    *out_flags = mf_client_load_le32_v1(response.bytes + 16);
  }
  return expected_status != MF_CLIENT_CONTROL_OK ||
         (out_object.id != 0U && out_object.generation != 0U);
}

[[nodiscard]] bool complete_once(mf_host_fixture_v1* fixture, mf_client_ring_v1& completion_ring,
                                 std::uint64_t request_id, mf_shared_status_v1 expected_status,
                                 mf_client_completion_v1& out_completion) {
  return mf_host_fixture_pump_once_v1(fixture) == MF_SHARED_SUCCESS &&
         mf_client_try_consume_completion_v1(&completion_ring, &out_completion) ==
             MF_SHARED_SUCCESS &&
         out_completion.request_id == request_id && out_completion.status == expected_status;
}

template <typename T>
[[nodiscard]] std::span<const std::uint8_t> as_bytes(const T& value) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(&value), sizeof(value)};
}

template <typename T, std::size_t Size>
[[nodiscard]] std::span<const std::uint8_t> array_bytes(const std::array<T, Size>& value) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), sizeof(value)};
}

} // namespace

int main() {
  FixtureOwner fixture;
  if (mf_host_fixture_create_v1(&fixture.value) != MF_SHARED_SUCCESS) {
    return 1;
  }
  const mf_registry_view_id_v1 view_id = mf_host_fixture_view_id_v1(fixture.value);
  RegistryOwner registry;
  RingOwner submission;
  RingOwner completion;
  if (mf_client_registry_attach_v1(mf_host_fixture_registry_fd_v1(fixture.value), view_id,
                                   &registry.value) != MF_SHARED_SUCCESS ||
      mf_client_ring_attach_v1(mf_host_fixture_submission_fd_v1(fixture.value), view_id,
                               MF_HOST_FIXTURE_SUBMISSION_QUEUE_ID,
                               MF_HOST_FIXTURE_QUEUE_GENERATION,
                               &submission.value) != MF_SHARED_SUCCESS ||
      mf_client_ring_attach_v1(mf_host_fixture_completion_fd_v1(fixture.value), view_id,
                               MF_HOST_FIXTURE_COMPLETION_QUEUE_ID,
                               MF_HOST_FIXTURE_QUEUE_GENERATION,
                               &completion.value) != MF_SHARED_SUCCESS) {
    return 2;
  }

  mf_virtual_device_identity_v1 identity{};
  mf_generation_handle_v1 context_handle{};
  mf_client_fence_snapshot_v1 fence{};
  mf_client_telemetry_snapshot_v1 telemetry{};
  if (mf_client_registry_device_count_v1(&registry.value) != 1U ||
      mf_client_registry_identity_v1(&registry.value, 0U, &identity) != MF_SHARED_SUCCESS ||
      identity.identity_record_id != 1U ||
      mf_client_registry_make_handle_v1(&registry.value, 0U, MF_HOST_FIXTURE_CONTEXT_ID, 1U,
                                        MF_OBJECT_TYPE_CONTEXT,
                                        &context_handle) != MF_SHARED_SUCCESS ||
      mf_client_registry_validate_device_v1(&registry.value, &context_handle, &fence) !=
          MF_SHARED_SUCCESS ||
      mf_client_registry_read_telemetry_v1(&registry.value, &context_handle, &telemetry) !=
          MF_SHARED_SUCCESS ||
      telemetry.memory_capacity_bytes == 0U) {
    return 3;
  }

  std::uint64_t control_request_id = 1U;
  std::uint64_t ring_request_id = 100U;
  ObjectRef ignored{};
  ObjectRef scratch{};
  mf_client_completion_v1 completed{};
  if (mf_client_submit_memory_alloc_v1(&submission.value, ring_request_id,
                                       MF_HOST_FIXTURE_CONTEXT_ID, 4096U, 64U,
                                       0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 4;
  }
  scratch = {completed.result_id, completed.result_generation};
  if (scratch.id == 0U || scratch.generation == 0U ||
      mf_client_submit_memory_free_v1(&submission.value, ring_request_id, scratch.id,
                                      scratch.generation) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed) ||
      mf_client_submit_memory_free_v1(&submission.value, ring_request_id, scratch.id,
                                      scratch.generation) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_STALE_HANDLE,
                     completed)) {
    return 5;
  }

  constexpr std::array<std::uint32_t, 8> left_values{{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U}};
  constexpr std::array<std::uint32_t, 8> right_values{{8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U}};
  constexpr std::array<std::uint32_t, 8> zero_values{};
  ObjectRef host_left{};
  ObjectRef host_right{};
  ObjectRef host_output{};
  if (!control(fixture.value, view_id, control_request_id,
               MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
               MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
               MF_HOST_FIXTURE_CONTEXT_ID, sizeof(left_values), array_bytes(left_values),
               MF_CLIENT_CONTROL_OK, host_left) ||
      !control(fixture.value, view_id, control_request_id,
               MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
               MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
               MF_HOST_FIXTURE_CONTEXT_ID, sizeof(right_values), array_bytes(right_values),
               MF_CLIENT_CONTROL_OK, host_right) ||
      !control(fixture.value, view_id, control_request_id,
               MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
               MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_WRITE,
               MF_HOST_FIXTURE_CONTEXT_ID, sizeof(zero_values), array_bytes(zero_values),
               MF_CLIENT_CONTROL_OK, host_output)) {
    return 6;
  }

  ObjectRef device_left{};
  ObjectRef device_right{};
  ObjectRef device_output{};
  const std::span<const std::uint8_t> empty{};
  if (!control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1,
               0U, MF_HOST_FIXTURE_CONTEXT_ID, sizeof(left_values), empty, MF_CLIENT_CONTROL_OK,
               device_left) ||
      !control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1,
               0U, MF_HOST_FIXTURE_CONTEXT_ID, sizeof(right_values), empty, MF_CLIENT_CONTROL_OK,
               device_right) ||
      !control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1,
               0U, MF_HOST_FIXTURE_CONTEXT_ID, sizeof(zero_values), empty, MF_CLIENT_CONTROL_OK,
               device_output)) {
    return 7;
  }

  if (mf_client_submit_copy_v1(&submission.value, ring_request_id, device_left.id,
                               device_left.generation, host_left.id, host_left.generation,
                               sizeof(left_values), 0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed) ||
      mf_client_submit_copy_v1(&submission.value, ring_request_id, device_right.id,
                               device_right.generation, host_right.id, host_right.generation,
                               sizeof(right_values), 0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 8;
  }

  std::array<CopyArgumentPayload, 3> region_payloads{};
  std::array<ObjectRef, region_payloads.size()> region_argument_blocks{};
  initialize_copy_arguments(region_payloads[0], device_left, 1U, host_left, 0U, 7U);
  initialize_copy_arguments(region_payloads[1], device_output, 3U, device_left, 1U, 7U);
  initialize_copy_arguments(region_payloads[2], host_output, 2U, device_output, 3U, 7U);
  for (std::size_t index = 0; index < region_payloads.size(); ++index) {
    if (mf_client_copy_region_argument_block_validate_v1(region_payloads[index].bytes.data(),
                                                         copy_argument_size) != MF_SHARED_SUCCESS ||
        !control(fixture.value, view_id, control_request_id,
                 MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1, MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD,
                 MF_HOST_FIXTURE_CONTEXT_ID, copy_argument_size,
                 {region_payloads[index].bytes.data(), region_payloads[index].bytes.size()},
                 MF_CLIENT_CONTROL_OK, region_argument_blocks[index]) ||
        mf_client_submit_copy_region_v1(
            &submission.value, ring_request_id, region_argument_blocks[index].id,
            region_argument_blocks[index].generation) != MF_SHARED_SUCCESS ||
        !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                       completed)) {
      return 22;
    }
  }
  std::array<std::uint8_t, sizeof(zero_values)> region_output{};
  if (mf_host_fixture_read_object_v1(fixture.value, host_output.id, host_output.generation, 0U,
                                     region_output.data(),
                                     region_output.size()) != MF_SHARED_SUCCESS ||
      region_output[1] != UINT8_C(0) ||
      std::memcmp(region_output.data() + 2U,
                  reinterpret_cast<const std::uint8_t*>(left_values.data()), 7U) != 0 ||
      region_output[9] != UINT8_C(0)) {
    return 23;
  }
  if (mf_client_submit_copy_v1(&submission.value, ring_request_id, device_left.id,
                               device_left.generation, host_left.id, host_left.generation,
                               sizeof(left_values), 0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 25;
  }

  constexpr std::string_view ptx =
      ".version 8.0\n.target sm_80\n.visible .entry add_i32() { ret; }\n";
  ObjectRef artifact{};
  if (!control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1,
               MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX,
               MF_HOST_FIXTURE_CONTEXT_ID, ptx.size(),
               {reinterpret_cast<const std::uint8_t*>(ptx.data()), ptx.size()},
               MF_CLIENT_CONTROL_OK, artifact)) {
    return 9;
  }
  ObjectRef resolved_artifact{};
  std::uint32_t resolve_flags = 0;
  if (!control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1,
               0U, artifact.id, artifact.generation, empty, MF_CLIENT_CONTROL_OK, resolved_artifact,
               &resolve_flags) ||
      resolved_artifact.id != artifact.id ||
      (resolve_flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) == 0U) {
    return 10;
  }

  if (mf_client_submit_module_load_v1(&submission.value, ring_request_id, artifact.id,
                                      artifact.generation, 0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 11;
  }
  const ObjectRef module{completed.result_id, completed.result_generation};

  const mf_host_fixture_add_arguments_v1 add_arguments{
      device_output.id, device_output.generation, device_left.id,     device_left.generation,
      device_right.id,  device_right.generation,  left_values.size(), 0U,
  };
  ObjectRef argument_block{};
  if (!control(fixture.value, view_id, control_request_id,
               MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1, MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD,
               MF_HOST_FIXTURE_CONTEXT_ID, sizeof(add_arguments), as_bytes(add_arguments),
               MF_CLIENT_CONTROL_OK, argument_block) ||
      mf_client_submit_launch_v1(&submission.value, ring_request_id, module.id, module.generation,
                                 MF_HOST_FIXTURE_KERNEL_ADD_I32, argument_block.id,
                                 argument_block.generation, 0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 12;
  }

  if (mf_client_submit_copy_v1(&submission.value, ring_request_id, host_output.id,
                               host_output.generation, device_output.id, device_output.generation,
                               sizeof(zero_values), 0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 13;
  }
  std::array<std::uint32_t, left_values.size()> output{};
  if (mf_host_fixture_read_object_v1(fixture.value, host_output.id, host_output.generation, 0U,
                                     reinterpret_cast<std::uint8_t*>(output.data()),
                                     sizeof(output)) != MF_SHARED_SUCCESS) {
    return 14;
  }
  for (std::size_t index = 0; index < output.size(); ++index) {
    if (output[index] != left_values[index] + right_values[index]) {
      return 15;
    }
  }

  if (mf_client_submit_event_record_v1(&submission.value, ring_request_id, MF_HOST_FIXTURE_EVENT_ID,
                                       MF_HOST_FIXTURE_EVENT_GENERATION, 7U,
                                       0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed) ||
      completed.timeline_value != 7U ||
      mf_client_submit_event_wait_v1(&submission.value, ring_request_id, MF_HOST_FIXTURE_EVENT_ID,
                                     MF_HOST_FIXTURE_EVENT_GENERATION, 7U,
                                     0U) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 16;
  }

  if (mf_client_registry_read_telemetry_v1(&registry.value, &context_handle, &telemetry) !=
          MF_SHARED_SUCCESS ||
      telemetry.completed_work_items < 9U || telemetry.memory_used_bytes == 0U) {
    return 17;
  }

  if (mf_client_submit_module_unload_v1(&submission.value, ring_request_id, module.id,
                                        module.generation) != MF_SHARED_SUCCESS ||
      !complete_once(fixture.value, completion.value, ring_request_id++, MF_SHARED_SUCCESS,
                     completed)) {
    return 18;
  }
  for (const ObjectRef memory : {device_left, device_right, device_output}) {
    if (!control(fixture.value, view_id, control_request_id,
                 MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1, 0U, memory.id, memory.generation, empty,
                 MF_CLIENT_CONTROL_OK, ignored)) {
      return 19;
    }
  }
  for (const ObjectRef host : {host_left, host_right, host_output}) {
    if (!control(fixture.value, view_id, control_request_id,
                 MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1, 0U, host.id, host.generation, empty,
                 MF_CLIENT_CONTROL_OK, ignored)) {
      return 20;
    }
  }
  for (const ObjectRef region_argument_block : region_argument_blocks) {
    if (!control(fixture.value, view_id, control_request_id,
                 MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, 0U, region_argument_block.id,
                 region_argument_block.generation, empty, MF_CLIENT_CONTROL_OK, ignored)) {
      return 24;
    }
  }
  if (!control(fixture.value, view_id, control_request_id,
               MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, 0U, argument_block.id,
               argument_block.generation, empty, MF_CLIENT_CONTROL_OK, ignored) ||
      !control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1,
               0U, artifact.id, artifact.generation, empty, MF_CLIENT_CONTROL_OK, ignored) ||
      !control(fixture.value, view_id, control_request_id, MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1,
               0U, artifact.id, artifact.generation, empty, MF_CLIENT_CONTROL_STALE_GENERATION,
               ignored)) {
    return 21;
  }
  return 0;
}
