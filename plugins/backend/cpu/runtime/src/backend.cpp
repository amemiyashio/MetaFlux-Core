#include "metaflux/backend/cpu.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace metaflux::backend::cpu {
namespace {

constexpr std::uint64_t kMaximumMemoryBytes = UINT64_C(256) * 1024U * 1024U;
constexpr std::uint64_t kMaximumAlignment = 4096U;

struct Instance final {};

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

struct State final {
  std::mutex mutex;
  std::uint64_t next_handle = 1U;
  std::unordered_map<std::uint64_t, std::unique_ptr<Instance>> instances;
  std::unordered_map<std::uint64_t, std::unique_ptr<Context>> contexts;
  std::unordered_map<std::uint64_t, std::unique_ptr<Queue>> queues;
  std::unordered_map<std::uint64_t, std::unique_ptr<Memory>> memories;
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
  device.capability_bits = MF_BACKEND_CAP_COPY;
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
            .capabilities = MF_BACKEND_CAP_COPY,
            .extensions = nullptr,
        },
    .create_instance = create_instance,
    .destroy_instance = destroy_instance,
    .enumerate_devices = enumerate_devices,
    .compile = nullptr,
    .load_module = nullptr,
    .unload_module = nullptr,
    .create_context = create_context,
    .destroy_context = destroy_context,
    .create_queue = create_queue,
    .destroy_queue = destroy_queue,
    .allocate_memory = allocate_memory,
    .free_memory = free_memory,
    .submit = nullptr,
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
