#include "metaflux/backend/cpu.h"

#include "metaflux/backend/cpu/executor.hpp"
#include "metaflux/backend/cpu/interpreter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace metaflux::backend::cpu {
namespace {

constexpr std::uint64_t kMaximumMemoryBytes = UINT64_C(256) * 1024U * 1024U;
constexpr std::uint64_t kMaximumAlignment = 4096U;
constexpr std::uint64_t kMaximumArtifactBytes = UINT64_C(4) * 1024U * 1024U;

struct Instance final {
  std::unique_ptr<CpuExecutor> executor;

  Instance() : executor(std::make_unique<CpuExecutor>()) {}
};

struct Context final {
  mf_backend_instance_v1 instance = 0U;
};

struct Queue final {
  mf_backend_context_v1 context = 0U;
};

struct Memory final {
  mf_backend_instance_v1 instance = 0U;
  mf_backend_context_v1 context = 0U;
  std::uint8_t* address = nullptr;
  std::uint64_t byte_count = 0U;
  bool imported = false;
};

struct Module final {
  mf_backend_instance_v1 instance = 0U;
  std::string canonical_kernel_ir;
};

struct State final {
  std::mutex mutex;
  std::uint64_t next_handle = 1U;
  std::unordered_map<std::uint64_t, std::unique_ptr<Instance>> instances;
  std::unordered_map<std::uint64_t, std::unique_ptr<Context>> contexts;
  std::unordered_map<std::uint64_t, std::unique_ptr<Queue>> queues;
  std::unordered_map<std::uint64_t, std::unique_ptr<Memory>> memories;
  std::unordered_map<std::uint64_t, std::unique_ptr<Module>> modules;
};

State& state() noexcept {
  static State value;
  return value;
}

std::uint64_t allocate_handle(State& value) noexcept {
  if (value.next_handle == 0U || value.next_handle == UINT64_MAX) {
    return 0U;
  }
  return value.next_handle++;
}

template <typename Map>
auto find_handle(Map& map, std::uint64_t handle) noexcept ->
    typename Map::mapped_type::element_type* {
  const auto found = map.find(handle);
  return found == map.end() ? nullptr : found->second.get();
}

bool valid_size(std::uint32_t actual, std::size_t required) noexcept { return actual >= required; }

bool valid_alignment(std::uint64_t alignment) noexcept {
  if (alignment == 0U) {
    return true;
  }
  return alignment >= alignof(std::max_align_t) && alignment <= kMaximumAlignment &&
         (alignment & (alignment - 1U)) == 0U;
}

bool valid_range(const Memory& memory, std::uint64_t offset, std::uint64_t byte_count) noexcept {
  return offset <= memory.byte_count && byte_count <= memory.byte_count - offset;
}

bool reserved_zero(const std::uint64_t* values, std::size_t count) noexcept {
  for (std::size_t index = 0; index < count; ++index) {
    if (values[index] != 0U) {
      return false;
    }
  }
  return true;
}

mf_backend_status_v1 map_execution_result(const ExecutionResult& result) noexcept {
  if (result.ok()) {
    return MF_BACKEND_SUCCESS;
  }
  if (!result.diagnostic.has_value()) {
    return MF_BACKEND_INTERNAL_ERROR;
  }
  switch (result.diagnostic->error) {
  case ExecutionError::InvalidArtifact:
  case ExecutionError::SchemaMismatch:
    return MF_BACKEND_COMPILATION_FAILED;
  case ExecutionError::UnsupportedOperation:
  case ExecutionError::UnsupportedFpEnvironment:
    return MF_BACKEND_UNSUPPORTED;
  case ExecutionError::PlacementUnavailable:
  case ExecutionError::PlacementPinLost:
    return MF_BACKEND_DEVICE_LOST;
  case ExecutionError::Cancelled:
    return MF_BACKEND_TIMEOUT;
  case ExecutionError::InvalidLaunch:
  case ExecutionError::ArgumentCount:
  case ExecutionError::ArgumentType:
  case ExecutionError::AddressOverflow:
  case ExecutionError::MisalignedAddress:
  case ExecutionError::OutOfBounds:
  case ExecutionError::WriteToReadOnly:
  case ExecutionError::BarrierDivergence:
  case ExecutionError::StepLimit:
    return MF_BACKEND_INVALID_ARGUMENT;
  case ExecutionError::System:
    return MF_BACKEND_INTERNAL_ERROR;
  }
  return MF_BACKEND_INTERNAL_ERROR;
}

void release_memory(Memory& memory) noexcept {
  if (!memory.imported) {
    std::free(memory.address);
  }
  memory.address = nullptr;
}

mf_backend_status_v1 create_instance(const mf_backend_host_api_v1* host_api,
                                     mf_backend_instance_v1* out_instance) noexcept {
  (void)host_api;
  if (out_instance == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  *out_instance = 0U;
  try {
    auto& shared = state();
    std::lock_guard lock(shared.mutex);
    const auto handle = allocate_handle(shared);
    if (handle == 0U) {
      return MF_BACKEND_OUT_OF_MEMORY;
    }
    shared.instances.emplace(handle, std::make_unique<Instance>());
    *out_instance = handle;
    return MF_BACKEND_SUCCESS;
  } catch (...) {
    return MF_BACKEND_OUT_OF_MEMORY;
  }
}

void destroy_instance(mf_backend_instance_v1 instance) noexcept {
  if (instance == 0U) {
    return;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  if (find_handle(shared.instances, instance) == nullptr) {
    return;
  }
  for (auto it = shared.memories.begin(); it != shared.memories.end();) {
    if (it->second->instance == instance) {
      release_memory(*it->second);
      it = shared.memories.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = shared.modules.begin(); it != shared.modules.end();) {
    if (it->second->instance == instance) {
      it = shared.modules.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = shared.queues.begin(); it != shared.queues.end();) {
    const auto context = find_handle(shared.contexts, it->second->context);
    if (context == nullptr || context->instance == instance) {
      it = shared.queues.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = shared.contexts.begin(); it != shared.contexts.end();) {
    if (it->second->instance == instance) {
      it = shared.contexts.erase(it);
    } else {
      ++it;
    }
  }
  shared.instances.erase(instance);
}

mf_backend_status_v1 enumerate_devices(mf_backend_instance_v1 instance, std::uint32_t* inout_count,
                                       mf_backend_device_info_v1* devices) noexcept {
  if (inout_count == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  if (find_handle(shared.instances, instance) == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  constexpr std::uint32_t kDeviceCount = 1U;
  if (devices == nullptr) {
    *inout_count = kDeviceCount;
    return MF_BACKEND_SUCCESS;
  }
  if (*inout_count < kDeviceCount || devices[0].struct_size < sizeof(mf_backend_device_info_v1)) {
    *inout_count = kDeviceCount;
    return MF_BACKEND_OUT_OF_MEMORY;
  }
  auto& device = devices[0];
  std::memset(&device, 0, sizeof(device));
  device.struct_size = sizeof(device);
  device.backend_device_index = 0U;
  device.capability_bits = MF_BACKEND_CAP_COPY | MF_BACKEND_CAP_LAUNCH;
  device.memory_capacity_bytes = kMaximumMemoryBytes;
  device.virtual_compute_capability = 70U;
  constexpr char name[] = "MetaFlux CPU";
  std::memcpy(device.display_name, name, sizeof(name) - 1U);
  *inout_count = kDeviceCount;
  return MF_BACKEND_SUCCESS;
}

mf_backend_status_v1 create_context(mf_backend_instance_v1 instance, std::uint32_t device_index,
                                    mf_backend_context_v1* out_context) noexcept {
  if (out_context == nullptr || device_index != 0U) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  *out_context = 0U;
  try {
    auto& shared = state();
    std::lock_guard lock(shared.mutex);
    if (find_handle(shared.instances, instance) == nullptr) {
      return MF_BACKEND_INVALID_ARGUMENT;
    }
    const auto handle = allocate_handle(shared);
    if (handle == 0U) {
      return MF_BACKEND_OUT_OF_MEMORY;
    }
    shared.contexts.emplace(handle, std::make_unique<Context>(Context{.instance = instance}));
    *out_context = handle;
    return MF_BACKEND_SUCCESS;
  } catch (...) {
    return MF_BACKEND_OUT_OF_MEMORY;
  }
}

void destroy_context(mf_backend_instance_v1 instance, mf_backend_context_v1 context) noexcept {
  if (instance == 0U || context == 0U) {
    return;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  const auto* found = find_handle(shared.contexts, context);
  if (found == nullptr || found->instance != instance) {
    return;
  }
  for (auto it = shared.memories.begin(); it != shared.memories.end();) {
    if (it->second->context == context) {
      release_memory(*it->second);
      it = shared.memories.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = shared.queues.begin(); it != shared.queues.end();) {
    if (it->second->context == context) {
      it = shared.queues.erase(it);
    } else {
      ++it;
    }
  }
  shared.contexts.erase(context);
}

mf_backend_status_v1 create_queue(mf_backend_instance_v1 instance, mf_backend_context_v1 context,
                                  mf_backend_queue_v1* out_queue) noexcept {
  if (out_queue == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  *out_queue = 0U;
  try {
    auto& shared = state();
    std::lock_guard lock(shared.mutex);
    const auto* found = find_handle(shared.contexts, context);
    if (found == nullptr || found->instance != instance) {
      return MF_BACKEND_INVALID_ARGUMENT;
    }
    const auto handle = allocate_handle(shared);
    if (handle == 0U) {
      return MF_BACKEND_OUT_OF_MEMORY;
    }
    shared.queues.emplace(handle, std::make_unique<Queue>(Queue{.context = context}));
    *out_queue = handle;
    return MF_BACKEND_SUCCESS;
  } catch (...) {
    return MF_BACKEND_OUT_OF_MEMORY;
  }
}

void destroy_queue(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue) noexcept {
  if (instance == 0U || queue == 0U) {
    return;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  const auto* found = find_handle(shared.queues, queue);
  if (found == nullptr) {
    return;
  }
  const auto* context = find_handle(shared.contexts, found->context);
  if (context != nullptr && context->instance == instance) {
    shared.queues.erase(queue);
  }
}

mf_backend_status_v1 allocate_memory(mf_backend_instance_v1 instance, mf_backend_context_v1 context,
                                     std::uint64_t byte_count, std::uint64_t alignment,
                                     mf_backend_memory_v1* out_memory) noexcept {
  if (out_memory == nullptr || byte_count == 0U || byte_count > kMaximumMemoryBytes ||
      !valid_alignment(alignment)) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  *out_memory = 0U;
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  const auto* found = find_handle(shared.contexts, context);
  if (found == nullptr || found->instance != instance) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  void* allocation = nullptr;
  const auto effective_alignment = alignment == 0U ? alignof(std::max_align_t) : alignment;
  if (posix_memalign(&allocation, static_cast<std::size_t>(effective_alignment),
                     static_cast<std::size_t>(byte_count)) != 0 ||
      allocation == nullptr) {
    return MF_BACKEND_OUT_OF_MEMORY;
  }
  try {
    const auto handle = allocate_handle(shared);
    if (handle == 0U) {
      std::free(allocation);
      return MF_BACKEND_OUT_OF_MEMORY;
    }
    shared.memories.emplace(
        handle, std::make_unique<Memory>(Memory{.instance = instance,
                                                .context = context,
                                                .address = static_cast<std::uint8_t*>(allocation),
                                                .byte_count = byte_count,
                                                .imported = false}));
    *out_memory = handle;
    return MF_BACKEND_SUCCESS;
  } catch (...) {
    std::free(allocation);
    return MF_BACKEND_OUT_OF_MEMORY;
  }
}

void free_memory(mf_backend_instance_v1 instance, mf_backend_memory_v1 memory) noexcept {
  if (instance == 0U || memory == 0U) {
    return;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  auto* found = find_handle(shared.memories, memory);
  if (found == nullptr || found->instance != instance) {
    return;
  }
  release_memory(*found);
  shared.memories.erase(memory);
}

mf_backend_status_v1 import_host_memory(mf_backend_instance_v1 instance,
                                        mf_backend_context_v1 context, void* address,
                                        std::uint64_t byte_count,
                                        mf_backend_memory_v1* out_memory) noexcept {
  if (out_memory == nullptr || address == nullptr || byte_count == 0U ||
      byte_count > kMaximumMemoryBytes) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  *out_memory = 0U;
  try {
    auto& shared = state();
    std::lock_guard lock(shared.mutex);
    const auto* found = find_handle(shared.contexts, context);
    if (found == nullptr || found->instance != instance) {
      return MF_BACKEND_INVALID_ARGUMENT;
    }
    const auto handle = allocate_handle(shared);
    if (handle == 0U) {
      return MF_BACKEND_OUT_OF_MEMORY;
    }
    shared.memories.emplace(
        handle, std::make_unique<Memory>(Memory{.instance = instance,
                                                .context = context,
                                                .address = static_cast<std::uint8_t*>(address),
                                                .byte_count = byte_count,
                                                .imported = true}));
    *out_memory = handle;
    return MF_BACKEND_SUCCESS;
  } catch (...) {
    return MF_BACKEND_OUT_OF_MEMORY;
  }
}

mf_backend_status_v1 load_module(mf_backend_instance_v1 instance, std::uint32_t device_index,
                                 const std::uint8_t* artifact_bytes, std::uint64_t artifact_size,
                                 mf_backend_module_v1* out_module) noexcept {
  if (out_module == nullptr || artifact_bytes == nullptr || artifact_size == 0U ||
      artifact_size > kMaximumArtifactBytes || device_index != 0U || artifact_size > SIZE_MAX) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  *out_module = 0U;
  if (artifact_size < 5U || std::memcmp(artifact_bytes, "MFKIR", 5U) != 0) {
    return MF_BACKEND_COMPILATION_FAILED;
  }
  try {
    auto& shared = state();
    std::lock_guard lock(shared.mutex);
    if (find_handle(shared.instances, instance) == nullptr) {
      return MF_BACKEND_INVALID_ARGUMENT;
    }
    const auto handle = allocate_handle(shared);
    if (handle == 0U) {
      return MF_BACKEND_OUT_OF_MEMORY;
    }
    auto module = std::make_unique<Module>();
    module->instance = instance;
    module->canonical_kernel_ir.assign(reinterpret_cast<const char*>(artifact_bytes),
                                       static_cast<std::size_t>(artifact_size));
    shared.modules.emplace(handle, std::move(module));
    *out_module = handle;
    return MF_BACKEND_SUCCESS;
  } catch (...) {
    return MF_BACKEND_OUT_OF_MEMORY;
  }
}

void unload_module(mf_backend_instance_v1 instance, mf_backend_module_v1 module) noexcept {
  if (instance == 0U || module == 0U) {
    return;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  auto* found = find_handle(shared.modules, module);
  if (found != nullptr && found->instance == instance) {
    shared.modules.erase(module);
  }
}

mf_backend_status_v1 submit(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                            const mf_backend_launch_v1* launch,
                            mf_backend_event_v1 completion_event) noexcept {
  constexpr std::size_t kRequiredLaunchSize =
      offsetof(mf_backend_launch_v1, reserved_word) +
      sizeof(((mf_backend_launch_v1*)nullptr)->reserved_word);
  constexpr std::uint64_t kMaximumArgumentBytes =
      sizeof(mf_cpu_backend_argument_block_header_v1) +
      static_cast<std::uint64_t>(MF_CPU_BACKEND_MAX_ARGUMENTS_V1) *
          sizeof(mf_cpu_backend_argument_v1);
  if (launch == nullptr || launch->struct_size < kRequiredLaunchSize || launch->flags != 0U ||
      launch->reserved_word != 0U || launch->module == 0U || launch->kernel_id != 1U ||
      completion_event != 0U || launch->argument_bytes == nullptr || launch->argument_size == 0U ||
      launch->argument_size > kMaximumArgumentBytes || launch->argument_size > SIZE_MAX ||
      launch->grid[2] != 1U || launch->block[2] != 1U || launch->dynamic_shared_bytes != 0U) {
    return completion_event == 0U ? MF_BACKEND_INVALID_ARGUMENT : MF_BACKEND_UNSUPPORTED;
  }

  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  auto* instance_object = find_handle(shared.instances, instance);
  const auto* queue_object = find_handle(shared.queues, queue);
  auto* module_object = find_handle(shared.modules, launch->module);
  if (instance_object == nullptr || queue_object == nullptr || module_object == nullptr ||
      module_object->instance != instance) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  const auto* context = find_handle(shared.contexts, queue_object->context);
  if (context == nullptr || context->instance != instance) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }

  mf_cpu_backend_argument_block_header_v1 header{};
  std::memcpy(&header, launch->argument_bytes, sizeof(header));
  if (header.magic != MF_CPU_BACKEND_ARGUMENT_BLOCK_MAGIC_V1 ||
      header.version != MF_CPU_BACKEND_ARGUMENT_BLOCK_VERSION_V1 ||
      header.header_size != sizeof(header) ||
      header.entry_size != sizeof(mf_cpu_backend_argument_v1) || header.entry_count == 0U ||
      header.entry_count > MF_CPU_BACKEND_MAX_ARGUMENTS_V1 || header.reserved_word != 0U ||
      !reserved_zero(header.reserved, 2U) || header.total_size != launch->argument_size ||
      header.total_size < header.header_size ||
      static_cast<std::uint64_t>(header.entry_count) >
          (header.total_size - header.header_size) / header.entry_size ||
      header.header_size + static_cast<std::uint64_t>(header.entry_count) * header.entry_size !=
          header.total_size) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }

  std::array<Argument, MF_CPU_BACKEND_MAX_ARGUMENTS_V1> arguments{};
  for (std::uint32_t index = 0U; index < header.entry_count; ++index) {
    mf_cpu_backend_argument_v1 entry{};
    const auto entry_offset = static_cast<std::size_t>(header.header_size) +
                              static_cast<std::size_t>(index) * sizeof(entry);
    std::memcpy(&entry, launch->argument_bytes + entry_offset, sizeof(entry));
    if (entry.kind == MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1) {
      if ((entry.flags & ~MF_CPU_BACKEND_ARGUMENT_BUFFER_KNOWN_FLAGS_V1) != 0U ||
          (entry.flags & MF_CPU_BACKEND_ARGUMENT_BUFFER_KNOWN_FLAGS_V1) == 0U ||
          entry.memory == 0U || entry.byte_count == 0U ||
          (entry.byte_count % sizeof(std::uint32_t)) != 0U || entry.offset > SIZE_MAX ||
          entry.byte_count > SIZE_MAX) {
        return MF_BACKEND_INVALID_ARGUMENT;
      }
      auto* memory = find_handle(shared.memories, entry.memory);
      if (memory == nullptr || memory->instance != instance ||
          memory->context != queue_object->context ||
          !valid_range(*memory, entry.offset, entry.byte_count) || memory->address == nullptr ||
          reinterpret_cast<std::uintptr_t>(memory->address) >
              std::numeric_limits<std::uintptr_t>::max() - entry.offset ||
          (reinterpret_cast<std::uintptr_t>(memory->address) + entry.offset) %
                  alignof(std::uint32_t) !=
              0U ||
          entry.value != 0U) {
        return MF_BACKEND_INVALID_ARGUMENT;
      }
      arguments[index] = BufferArgument{
          .words = std::span<std::uint32_t>(
              reinterpret_cast<std::uint32_t*>(memory->address + entry.offset),
              static_cast<std::size_t>(entry.byte_count / sizeof(std::uint32_t))),
          .writable = (entry.flags & MF_CPU_BACKEND_ARGUMENT_BUFFER_WRITE_V1) != 0U,
      };
      continue;
    }
    if ((entry.kind != MF_CPU_BACKEND_ARGUMENT_KIND_U32_V1 &&
         entry.kind != MF_CPU_BACKEND_ARGUMENT_KIND_F32_V1) ||
        entry.flags != 0U || entry.memory != 0U || entry.offset != 0U || entry.byte_count != 0U ||
        entry.value > UINT32_MAX) {
      return MF_BACKEND_INVALID_ARGUMENT;
    }
    if (entry.kind == MF_CPU_BACKEND_ARGUMENT_KIND_U32_V1) {
      arguments[index] = static_cast<std::uint32_t>(entry.value);
    } else {
      arguments[index] = Float32Argument{.bits = static_cast<std::uint32_t>(entry.value)};
    }
  }

  const LaunchDimensions dimensions{
      .grid_x = launch->grid[0],
      .block_x = launch->block[0],
      .grid_y = launch->grid[1],
      .block_y = launch->block[1],
  };
  const auto result = execute_kernel_ir(
      *instance_object->executor, std::string_view(module_object->canonical_kernel_ir),
      std::span<const Argument>(arguments.data(), header.entry_count), dimensions);
  return map_execution_result(result);
}

mf_backend_status_v1 copy(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                          const mf_backend_copy_v1* request,
                          mf_backend_event_v1 completion_event) noexcept {
  constexpr std::size_t kRequiredSize =
      offsetof(mf_backend_copy_v1, reserved) + sizeof(((mf_backend_copy_v1*)nullptr)->reserved);
  if (request == nullptr || !valid_size(request->struct_size, kRequiredSize) ||
      request->flags != 0U || request->reserved[0] != 0U || request->reserved[1] != 0U ||
      completion_event != 0U) {
    return completion_event == 0U ? MF_BACKEND_INVALID_ARGUMENT : MF_BACKEND_UNSUPPORTED;
  }
  auto& shared = state();
  std::lock_guard lock(shared.mutex);
  const auto* queue_object = find_handle(shared.queues, queue);
  if (queue_object == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  const auto* context = find_handle(shared.contexts, queue_object->context);
  if (context == nullptr || context->instance != instance) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  const auto* source = find_handle(shared.memories, request->source);
  const auto* destination = find_handle(shared.memories, request->destination);
  if (source == nullptr || destination == nullptr || source->instance != instance ||
      destination->instance != instance ||
      !valid_range(*source, request->source_offset, request->byte_count) ||
      !valid_range(*destination, request->destination_offset, request->byte_count)) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  std::memmove(destination->address + request->destination_offset,
               source->address + request->source_offset,
               static_cast<std::size_t>(request->byte_count));
  return MF_BACKEND_SUCCESS;
}

const mf_backend_api_v1 kApi = {
    .header =
        {
            .abi_version = MF_BACKEND_ABI_VERSION_1,
            .struct_size = static_cast<std::uint32_t>(sizeof(mf_backend_api_v1)),
            .capabilities = MF_BACKEND_CAP_COPY | MF_BACKEND_CAP_LAUNCH,
            .extensions = nullptr,
        },
    .create_instance = create_instance,
    .destroy_instance = destroy_instance,
    .enumerate_devices = enumerate_devices,
    .compile = nullptr,
    .load_module = load_module,
    .unload_module = unload_module,
    .create_context = create_context,
    .destroy_context = destroy_context,
    .create_queue = create_queue,
    .destroy_queue = destroy_queue,
    .allocate_memory = allocate_memory,
    .free_memory = free_memory,
    .submit = submit,
    .copy = copy,
    .create_event = nullptr,
    .destroy_event = nullptr,
    .query_event = nullptr,
    .wait_event = nullptr,
    .synchronize_queue = nullptr,
    .cancel_queue = nullptr,
    .read_metrics = nullptr,
    .set_policy = nullptr,
};

} // namespace

extern "C" const mf_backend_api_v1* mf_cpu_backend_get_api_v1(void) { return &kApi; }

extern "C" mf_backend_status_v1
mf_cpu_backend_import_host_memory_v1(mf_backend_instance_v1 instance, mf_backend_context_v1 context,
                                     void* address, uint64_t byte_count,
                                     mf_backend_memory_v1* out_memory) {
  return import_host_memory(instance, context, address, byte_count, out_memory);
}

} // namespace metaflux::backend::cpu
