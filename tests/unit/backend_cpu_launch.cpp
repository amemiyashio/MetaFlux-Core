#include "metaflux/backend/cpu.h"

#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "CPU backend launch failure: " << message << '\n';
  }
  return condition;
}

std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return {};
  }
  const auto end = input.tellg();
  if (end <= 0) {
    return {};
  }
  std::string bytes(static_cast<std::size_t>(end), '\0');
  input.seekg(0, std::ios::beg);
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return input ? bytes : std::string{};
}

std::vector<std::uint8_t> make_argument_block(std::span<const mf_cpu_backend_argument_v1> entries) {
  const auto total_size = sizeof(mf_cpu_backend_argument_block_header_v1) +
                          entries.size() * sizeof(mf_cpu_backend_argument_v1);
  std::vector<std::uint8_t> bytes(total_size, 0U);
  mf_cpu_backend_argument_block_header_v1 header{};
  header.magic = MF_CPU_BACKEND_ARGUMENT_BLOCK_MAGIC_V1;
  header.version = MF_CPU_BACKEND_ARGUMENT_BLOCK_VERSION_V1;
  header.header_size = sizeof(header);
  header.entry_size = sizeof(mf_cpu_backend_argument_v1);
  header.entry_count = static_cast<std::uint32_t>(entries.size());
  header.total_size = total_size;
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + sizeof(header), entries.data(),
              entries.size() * sizeof(mf_cpu_backend_argument_v1));
  return bytes;
}

} // namespace

int main() {
#ifndef METAFLUX_CPU_ADD_PTX
  return 1;
#else
  const auto* api = mf_cpu_backend_get_api_v1();
  if (!expect(api != nullptr && api->header.abi_version == MF_BACKEND_ABI_VERSION_1 &&
                  (api->header.capabilities & MF_BACKEND_CAP_LAUNCH) != 0U &&
                  api->load_module != nullptr && api->unload_module != nullptr &&
                  api->submit != nullptr,
              "CPU backend must expose the launch table")) {
    return 1;
  }

  mf_backend_instance_v1 instance = 0U;
  mf_backend_context_v1 context = 0U;
  mf_backend_queue_v1 queue = 0U;
  mf_backend_module_v1 module = 0U;
  mf_backend_memory_v1 destination_handle = 0U;
  mf_backend_memory_v1 left_handle = 0U;
  mf_backend_memory_v1 right_handle = 0U;
  const auto cleanup = [&] {
    if (api->unload_module != nullptr) {
      api->unload_module(instance, module);
    }
    if (api->free_memory != nullptr) {
      api->free_memory(instance, destination_handle);
      api->free_memory(instance, left_handle);
      api->free_memory(instance, right_handle);
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

  if (api->create_instance(nullptr, &instance) != MF_BACKEND_SUCCESS ||
      api->create_context(instance, 0U, &context) != MF_BACKEND_SUCCESS ||
      api->create_queue(instance, context, &queue) != MF_BACKEND_SUCCESS) {
    cleanup();
    return 1;
  }

  alignas(std::uint32_t) std::array<std::uint32_t, 8> destination{};
  alignas(std::uint32_t) std::array<std::uint32_t, 8> left{};
  alignas(std::uint32_t) std::array<std::uint32_t, 8> right{};
  for (std::size_t index = 0; index < left.size(); ++index) {
    left[index] = static_cast<std::uint32_t>(index + 1U);
    right[index] = static_cast<std::uint32_t>(100U + index);
  }
  if (mf_cpu_backend_import_host_memory_v1(instance, context, destination.data(),
                                           sizeof(destination),
                                           &destination_handle) != MF_BACKEND_SUCCESS ||
      mf_cpu_backend_import_host_memory_v1(instance, context, left.data(), sizeof(left),
                                           &left_handle) != MF_BACKEND_SUCCESS ||
      mf_cpu_backend_import_host_memory_v1(instance, context, right.data(), sizeof(right),
                                           &right_handle) != MF_BACKEND_SUCCESS) {
    cleanup();
    return 1;
  }

  const auto ptx = read_file(METAFLUX_CPU_ADD_PTX);
  const auto parsed = metaflux::compiler::ptx::parse(ptx);
  if (!expect(parsed.ok(), "Add PTX fixture must parse")) {
    cleanup();
    return 1;
  }
  const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
  if (!expect(serialized.ok(), "Add PTX fixture must serialize to canonical KIR")) {
    cleanup();
    return 1;
  }
  if (!expect(api->load_module(instance, 0U,
                               reinterpret_cast<const std::uint8_t*>(serialized.text.data()),
                               serialized.text.size(), &module) == MF_BACKEND_SUCCESS &&
                  module != 0U,
              "canonical KIR module must load")) {
    cleanup();
    return 1;
  }

  std::array<mf_cpu_backend_argument_v1, 4> entries{
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1,
                                 .flags = MF_CPU_BACKEND_ARGUMENT_BUFFER_WRITE_V1,
                                 .memory = destination_handle,
                                 .offset = 0U,
                                 .byte_count = sizeof(destination),
                                 .value = 0U},
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1,
                                 .flags = MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1,
                                 .memory = left_handle,
                                 .offset = 0U,
                                 .byte_count = sizeof(left),
                                 .value = 0U},
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1,
                                 .flags = MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1,
                                 .memory = right_handle,
                                 .offset = 0U,
                                 .byte_count = sizeof(right),
                                 .value = 0U},
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_U32_V1,
                                 .flags = 0U,
                                 .memory = 0U,
                                 .offset = 0U,
                                 .byte_count = 0U,
                                 .value = left.size()},
  };
  const auto argument_block = make_argument_block(entries);
  mf_backend_launch_v1 launch{};
  launch.struct_size = sizeof(launch);
  launch.module = module;
  launch.kernel_id = 1U;
  launch.argument_bytes = argument_block.data();
  launch.argument_size = argument_block.size();
  launch.grid[0] = 1U;
  launch.grid[1] = 1U;
  launch.grid[2] = 1U;
  launch.block[0] = static_cast<std::uint32_t>(left.size());
  launch.block[1] = 1U;
  launch.block[2] = 1U;
  if (!expect(api->submit(instance, queue, &launch, 0U) == MF_BACKEND_SUCCESS,
              "backend submit must execute Add synchronously") ||
      !expect(destination[0] == 101U && destination[7] == 115U,
              "backend submit must produce bit-exact Add results")) {
    cleanup();
    return 1;
  }

  launch.block[2] = 2U;
  if (!expect(api->submit(instance, queue, &launch, 0U) == MF_BACKEND_INVALID_ARGUMENT,
              "z dimensions must be rejected by the 2D CPU subset")) {
    cleanup();
    return 1;
  }
  launch.block[2] = 1U;
  launch.argument_size -= 1U;
  if (!expect(api->submit(instance, queue, &launch, 0U) == MF_BACKEND_INVALID_ARGUMENT,
              "truncated argument blocks must be rejected")) {
    cleanup();
    return 1;
  }
  launch.argument_size = argument_block.size();
  api->unload_module(instance, module);
  module = 0U;
  if (!expect(api->submit(instance, queue, &launch, 0U) == MF_BACKEND_INVALID_ARGUMENT,
              "unloaded modules must reject stale submits")) {
    cleanup();
    return 1;
  }
  cleanup();
  return 0;
#endif
}
