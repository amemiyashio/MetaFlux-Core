#ifndef METAFLUX_SERVICE_COMPILER_WORKER_HPP
#define METAFLUX_SERVICE_COMPILER_WORKER_HPP

#include "metaflux/backend/cpu/compiler.hpp"
#include "metaflux/compiler/kernel_ir.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stop_token>

namespace metaflux::service {

struct CompilerWorkerPolicy final {
  std::filesystem::path executable;
  std::chrono::milliseconds deadline{120000};
};

struct CompilerWorkerInvocation final {
  backend::cpu::compiler::CompileResult compilation;
  std::int64_t process_id = -1;
  bool launched = false;
};

[[nodiscard]] std::filesystem::path current_executable_path();

[[nodiscard]] CompilerWorkerInvocation compile_kernel_in_worker(const CompilerWorkerPolicy& policy,
                                                                const compiler::Kernel& kernel);
[[nodiscard]] CompilerWorkerInvocation compile_kernel_in_worker(const CompilerWorkerPolicy& policy,
                                                                const compiler::Kernel& kernel,
                                                                std::stop_token cancellation);

// Private exec entry used only by the parent daemon's bounded worker protocol.
[[nodiscard]] int run_compiler_worker_process(std::int64_t expected_parent_process_id) noexcept;

} // namespace metaflux::service

#endif
