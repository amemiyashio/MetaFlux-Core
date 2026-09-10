#ifndef METAFLUX_SERVICE_VULKAN_EXECUTION_HPP
#define METAFLUX_SERVICE_VULKAN_EXECUTION_HPP

#include "metaflux/compiler/kernel_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace metaflux::service {

// One routed compute kernel: the lowered SPIR-V module plus its device-side
// pipelines and per-launch scratch. The type stays Vulkan-opaque so session
// code compiles whether or not the daemon was built with the route.
class VulkanKernelModule final {
public:
  VulkanKernelModule();
  ~VulkanKernelModule();
  VulkanKernelModule(VulkanKernelModule&&) noexcept;
  VulkanKernelModule& operator=(VulkanKernelModule&&) noexcept;
  VulkanKernelModule(const VulkanKernelModule&) = delete;
  VulkanKernelModule& operator=(const VulkanKernelModule&) = delete;

  struct State;

  [[nodiscard]] State* state() noexcept { return state_.get(); }
  [[nodiscard]] const State* state() const noexcept { return state_.get(); }
  [[nodiscard]] std::string_view entry_point() const noexcept;
  [[nodiscard]] bool dispatchable() const noexcept;

private:
  std::unique_ptr<State> state_;
};

// One launch buffer view: the daemon-owned host bytes plus the writability
// mark that selects the copy-back set after the device dispatch.
struct VulkanLaunchBuffer final {
  std::byte* bytes = nullptr;
  std::size_t size_bytes = 0;
  bool writable = false;
};

// Process-wide single-adapter compute route. Empty shared_ptr when the daemon
// lacks the route, the environment gate is unset, or no adapter admits the
// lowered target; session code treats absence as the CPU-only mode.
[[nodiscard]] std::shared_ptr<class VulkanExecutionRoute> global_vulkan_execution_route();

class VulkanExecutionRoute final {
public:
  VulkanExecutionRoute();
  ~VulkanExecutionRoute();
  VulkanExecutionRoute(const VulkanExecutionRoute&) = delete;
  VulkanExecutionRoute& operator=(const VulkanExecutionRoute&) = delete;

  [[nodiscard]] bool ready() const noexcept;

  // Lowers one daemon-owned Kernel IR and builds the device pipelines.
  // Returns an empty module when the kernel shape has no verified SPIR-V
  // route; the diagnostic then states why and the caller keeps the CPU path.
  [[nodiscard]] std::unique_ptr<VulkanKernelModule> prepare(const compiler::Kernel& kernel,
                                                            std::string& diagnostic);

  // Uploads the launch buffers, dispatches the workgroups, and copies writable
  // buffers back. The buffers stay daemon-owned host memory throughout.
  [[nodiscard]] bool launch(VulkanKernelModule& module,
                            std::span<const VulkanLaunchBuffer> buffers,
                            std::uint32_t element_count, std::string& diagnostic);

private:
  struct State;
  std::unique_ptr<State> state_;

  friend std::shared_ptr<VulkanExecutionRoute> global_vulkan_execution_route();
};

} // namespace metaflux::service

#endif
