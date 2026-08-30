#include "metaflux/backend/cpu.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

int main() {
  const auto* api = mf_cpu_backend_get_api_v1();
  if (api == nullptr || api->header.abi_version != MF_BACKEND_ABI_VERSION_1 ||
      api->header.struct_size < sizeof(mf_backend_api_v1) ||
      (api->header.capabilities & MF_BACKEND_CAP_COPY) == 0U || api->create_instance == nullptr ||
      api->enumerate_devices == nullptr || api->create_context == nullptr ||
      api->create_queue == nullptr || api->allocate_memory == nullptr ||
      api->free_memory == nullptr || api->copy == nullptr) {
    return 1;
  }

  mf_backend_instance_v1 instance = 0U;
  mf_backend_context_v1 context = 0U;
  mf_backend_queue_v1 queue = 0U;
  mf_backend_memory_v1 source_handle = 0U;
  mf_backend_memory_v1 destination_handle = 0U;
  mf_backend_memory_v1 allocated_handle = 0U;
  auto cleanup = [&] {
    if (api->free_memory != nullptr) {
      api->free_memory(instance, source_handle);
      api->free_memory(instance, destination_handle);
      api->free_memory(instance, allocated_handle);
    }
    if (api->destroy_queue != nullptr) {
      api->destroy_queue(instance, queue);
    }
    if (api->destroy_context != nullptr) {
      api->destroy_context(instance, context);
    }
    if (api->destroy_instance != nullptr) {
      api->destroy_instance(instance);
    }
  };

  if (api->create_instance(nullptr, &instance) != MF_BACKEND_SUCCESS) {
    return 1;
  }
  std::uint32_t device_count = 0U;
  if (api->enumerate_devices(instance, &device_count, nullptr) != MF_BACKEND_SUCCESS ||
      device_count != 1U) {
    cleanup();
    return 1;
  }
  mf_backend_device_info_v1 device{};
  device.struct_size = sizeof(mf_backend_device_info_v1);
  if (api->enumerate_devices(instance, &device_count, &device) != MF_BACKEND_SUCCESS ||
      device_count != 1U || device.backend_device_index != 0U ||
      (device.capability_bits & MF_BACKEND_CAP_COPY) == 0U) {
    cleanup();
    return 1;
  }
  if (api->create_context(instance, 0U, &context) != MF_BACKEND_SUCCESS ||
      api->create_queue(instance, context, &queue) != MF_BACKEND_SUCCESS ||
      api->allocate_memory(instance, context, 128U, 64U, &allocated_handle) != MF_BACKEND_SUCCESS) {
    cleanup();
    return 1;
  }

  std::array<std::uint8_t, 128> source{};
  std::array<std::uint8_t, 128> destination{};
  for (std::size_t index = 0; index < source.size(); ++index) {
    source[index] = static_cast<std::uint8_t>(index);
  }
  if (mf_cpu_backend_import_host_memory_v1(instance, context, source.data(), source.size(),
                                           &source_handle) != MF_BACKEND_SUCCESS ||
      mf_cpu_backend_import_host_memory_v1(instance, context, destination.data(),
                                           destination.size(),
                                           &destination_handle) != MF_BACKEND_SUCCESS) {
    cleanup();
    return 1;
  }

  mf_backend_copy_v1 request{};
  request.struct_size = sizeof(mf_backend_copy_v1);
  request.destination = destination_handle;
  request.source = source_handle;
  request.byte_count = 64U;
  if (api->copy(instance, queue, &request, 0U) != MF_BACKEND_SUCCESS ||
      std::memcmp(destination.data(), source.data(), 64U) != 0 ||
      api->copy(instance, queue, &request, 1U) != MF_BACKEND_UNSUPPORTED) {
    cleanup();
    return 1;
  }
  request.source_offset = 120U;
  request.destination_offset = 120U;
  request.byte_count = 16U;
  if (api->copy(instance, queue, &request, 0U) != MF_BACKEND_INVALID_ARGUMENT) {
    cleanup();
    return 1;
  }
  cleanup();
  return 0;
}
