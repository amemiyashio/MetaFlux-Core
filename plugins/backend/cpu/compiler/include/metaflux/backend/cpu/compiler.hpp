#ifndef METAFLUX_BACKEND_CPU_COMPILER_HPP
#define METAFLUX_BACKEND_CPU_COMPILER_HPP

#include "metaflux/backend/cpu/compiler_build_identity.hpp"
#include "metaflux/compiler/artifact_cache.hpp"
#include "metaflux/compiler/cache.hpp"
#include "metaflux/compiler/kernel_ir.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace metaflux::backend::cpu::compiler {

inline constexpr std::string_view kCpuPipelineIdentity =
    "kir-v2-to-llvm-dialect,cpu-single-cta-v2,cpu-loop-simd-v1,"
    "f32-fma-ro-v2,llvm-o2,pic-et-dyn-v1,ssa-reg-promote-v1,simd-region-unroll-v3,"
    "region-1d-guard-v1";
inline constexpr std::string_view kCpuToolchainFingerprint = METAFLUX_CPU_TOOLCHAIN_FINGERPRINT;
inline constexpr std::string_view kCpuPgoIdentity = METAFLUX_CPU_PGO_ID;
inline constexpr std::uint32_t kCpuBackendAbiVersion = 1;
inline constexpr std::uint32_t kCpuHelperAbiVersion = 2;
inline constexpr std::string_view kCpuCompiledEntrySymbol = "metaflux_cpu_cta_v2";

enum class OptimizationLevel : std::uint32_t {
  O0,
  O2,
};

enum class CompileError : std::uint32_t {
  None,
  InvalidKernel,
  UnsupportedTarget,
  ResourceLimit,
  Cancelled,
  MlirGeneration,
  MlirVerification,
  LlvmTranslation,
  LlvmVerification,
  ObjectEmission,
  LinkerFailed,
  InvalidElf,
  Io,
};

[[nodiscard]] std::string_view compile_error_name(CompileError error) noexcept;

struct CompileDiagnostic {
  CompileError error = CompileError::None;
  metaflux::compiler::SourceLocation location{};
  std::string message;
};

struct CompileLimits {
  std::uint64_t maximum_register_storage_bytes = 16U * 1024U * 1024U;
  std::uint64_t maximum_artifact_bytes = 256U * 1024U * 1024U;
  std::uint64_t linker_address_space_bytes = 1024U * 1024U * 1024U;
  std::uint32_t linker_cpu_seconds = 30;
};

struct CompileOptions {
  std::string target_triple = "x86_64-unknown-linux-gnu";
  std::string cpu_name = "x86-64";
  std::vector<std::string> canonical_features;
  OptimizationLevel optimization = OptimizationLevel::O2;
  std::filesystem::path linker_path;
  std::filesystem::path temporary_root;
  CompileLimits limits{};
  std::stop_token cancellation{};
  std::optional<std::chrono::steady_clock::time_point> deadline;
};

// Compile options targeting the executing host: LLVM's own host CPU name and
// its usable (OS-enabled) feature set. The cache identity already covers
// cpu_name and canonical_features, so artifacts only ever load on hosts whose
// reported target environment matches the compiling host exactly.
[[nodiscard]] CompileOptions host_compile_options();

struct CompiledArtifact {
  std::vector<std::byte> elf;
  std::string elf_sha256;
  std::string mlir_text;
  std::string llvm_ir_text;
  std::vector<metaflux::compiler::ParameterKind> parameters;
  bool uses_floating_point = false;
};

struct CompileResult {
  std::optional<CompiledArtifact> artifact;
  std::optional<CompileDiagnostic> diagnostic;

  [[nodiscard]] bool ok() const noexcept { return artifact.has_value(); }
};

[[nodiscard]] CompileResult compile_kernel(const metaflux::compiler::Kernel& kernel,
                                           const CompileOptions& options = {});

[[nodiscard]] bool validate_compiled_elf(std::span<const std::byte> elf) noexcept;

[[nodiscard]] metaflux::compiler::CacheIdentity
make_cpu_cache_identity(const CompileOptions& options = {});

enum class ArtifactMode : std::uint32_t {
  ColdJit,
  WarmJit,
  AdministratorAot,
};

struct PreparedArtifact {
  ArtifactMode mode = ArtifactMode::ColdJit;
  std::string cache_key;
  std::filesystem::path path;
  std::string elf_sha256;
  std::vector<metaflux::compiler::ParameterKind> parameters;
  bool uses_floating_point = false;
  std::shared_ptr<const void> cache_pin;
};

struct ArtifactResult {
  std::optional<PreparedArtifact> artifact;
  metaflux::compiler::PersistentCacheError cache_error =
      metaflux::compiler::PersistentCacheError::None;
  std::optional<CompileDiagnostic> compile_diagnostic;

  [[nodiscard]] bool ok() const noexcept { return artifact.has_value(); }
};

using CompileCallback = std::function<CompileResult()>;

[[nodiscard]] ArtifactResult
lookup_cached_artifact(metaflux::compiler::PersistentArtifactCache& cache, std::uint32_t uid,
                       const metaflux::compiler::Kernel& kernel,
                       const CompileOptions& options = {});

[[nodiscard]] ArtifactResult acquire_artifact(metaflux::compiler::PersistentArtifactCache& cache,
                                              std::uint32_t uid,
                                              const metaflux::compiler::Kernel& kernel,
                                              const CompileOptions& options = {},
                                              CompileCallback compile = {});

[[nodiscard]] ArtifactResult prewarm_aot(metaflux::compiler::PersistentArtifactCache& cache,
                                         const metaflux::compiler::Kernel& kernel,
                                         const CompileOptions& options = {},
                                         CompileCallback compile = {});

} // namespace metaflux::backend::cpu::compiler

#endif
