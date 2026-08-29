#include "execution.hpp"

#include "metaflux/compiler/ptx_frontend.hpp"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace metaflux::service {
namespace {

constexpr std::uint64_t kMaximumPtxBytes = 64U * 1024U * 1024U;

[[nodiscard]] bool contains_parent_reference(const std::filesystem::path& path) {
  for (const auto& component : path) {
    if (component == "..") {
      return true;
    }
  }
  return false;
}

[[nodiscard]] compiler::PersistentCacheConfig
effective_cache_configuration(const CpuExecutionConfiguration& configuration) {
  auto cache = configuration.cache;
  switch (configuration.mode) {
  case CpuExecutionMode::Interpreter:
  case CpuExecutionMode::ColdJit:
  case CpuExecutionMode::WarmJit:
    break;
  case CpuExecutionMode::Aot:
    // AOT execution performs no mutable-tier lookup or last-used write.
    cache.mutable_root = cache.aot_root / ".mutable-tier-disabled";
    break;
  }
  return cache;
}

[[nodiscard]] std::optional<backend::cpu::CompiledKernelSignature>
compiled_signature(const backend::cpu::compiler::PreparedArtifact& artifact) {
  backend::cpu::CompiledKernelSignature signature;
  signature.parameters.reserve(artifact.parameters.size());
  for (const auto parameter : artifact.parameters) {
    switch (parameter) {
    case compiler::ParameterKind::BufferU32:
      signature.parameters.push_back(backend::cpu::CompiledParameterKind::BufferU32);
      break;
    case compiler::ParameterKind::ScalarU32:
      signature.parameters.push_back(backend::cpu::CompiledParameterKind::ScalarU32);
      break;
    case compiler::ParameterKind::ScalarF32:
      signature.parameters.push_back(backend::cpu::CompiledParameterKind::ScalarF32);
      break;
    default:
      return std::nullopt;
    }
  }
  signature.uses_floating_point = artifact.uses_floating_point;
  return signature;
}

[[nodiscard]] ModulePreparationError cache_error(compiler::PersistentCacheError error) noexcept {
  switch (error) {
  case compiler::PersistentCacheError::None:
    return ModulePreparationError::System;
  case compiler::PersistentCacheError::EntryAvailable:
    return ModulePreparationError::System;
  case compiler::PersistentCacheError::Miss:
    return ModulePreparationError::CacheMiss;
  case compiler::PersistentCacheError::ArtifactTooLarge:
  case compiler::PersistentCacheError::QuotaExceeded:
    return ModulePreparationError::ResourceExhausted;
  case compiler::PersistentCacheError::InvalidKey:
  case compiler::PersistentCacheError::MetadataMismatch:
    return ModulePreparationError::Malformed;
  case compiler::PersistentCacheError::ReservationNotFound:
  case compiler::PersistentCacheError::Io:
    return ModulePreparationError::System;
  }
  return ModulePreparationError::System;
}

[[nodiscard]] ModulePreparationError
compile_error(backend::cpu::compiler::CompileError error) noexcept {
  using backend::cpu::compiler::CompileError;
  switch (error) {
  case CompileError::None:
    return ModulePreparationError::System;
  case CompileError::InvalidKernel:
    return ModulePreparationError::Malformed;
  case CompileError::UnsupportedTarget:
    return ModulePreparationError::NotSupported;
  case CompileError::ResourceLimit:
    return ModulePreparationError::ResourceExhausted;
  case CompileError::Cancelled:
  case CompileError::MlirGeneration:
  case CompileError::MlirVerification:
  case CompileError::LlvmTranslation:
  case CompileError::LlvmVerification:
  case CompileError::ObjectEmission:
  case CompileError::LinkerFailed:
  case CompileError::InvalidElf:
  case CompileError::Io:
    return ModulePreparationError::System;
  }
  return ModulePreparationError::System;
}

[[nodiscard]] std::optional<std::string> read_ptx_file(std::string_view path,
                                                       std::string& diagnostic) {
  if (path.empty()) {
    diagnostic = "PTX path is empty";
    return std::nullopt;
  }
  std::ifstream input(std::filesystem::path(path), std::ios::binary | std::ios::ate);
  if (!input) {
    diagnostic = "PTX file cannot be opened";
    return std::nullopt;
  }
  const auto end = input.tellg();
  if (end <= 0 || static_cast<std::uint64_t>(end) > kMaximumPtxBytes ||
      static_cast<std::uint64_t>(end) > std::numeric_limits<std::size_t>::max()) {
    diagnostic = "PTX file size is outside the supported bound";
    return std::nullopt;
  }
  std::string source(static_cast<std::size_t>(end), '\0');
  input.seekg(0, std::ios::beg);
  input.read(source.data(), static_cast<std::streamsize>(source.size()));
  if (!input) {
    diagnostic = "PTX file cannot be read completely";
    return std::nullopt;
  }
  return source;
}

[[nodiscard]] CpuExecutionConfigurationResult configuration_failure(std::string diagnostic) {
  return {.configuration = std::nullopt, .diagnostic = std::move(diagnostic)};
}

[[nodiscard]] PrepareModuleResult preparation_failure(ModulePreparationError error,
                                                      std::string diagnostic) {
  return {.module = nullptr, .error = error, .diagnostic = std::move(diagnostic)};
}

[[nodiscard]] bool kernel_accesses_global_memory(const compiler::Kernel& kernel) noexcept {
  for (const auto& operation : kernel.operations) {
    switch (operation.opcode) {
    case compiler::Opcode::LoadGlobalU32:
    case compiler::Opcode::StoreGlobalU32:
    case compiler::Opcode::LoadGlobalF32:
    case compiler::Opcode::StoreGlobalF32:
      return true;
    default:
      break;
    }
  }
  return false;
}

[[nodiscard]] AotPrewarmResult prewarm_failure(std::string diagnostic, bool compiled = false) {
  return {
      .success = false, .compiled = compiled, .cache_key = {}, .diagnostic = std::move(diagnostic)};
}

} // namespace

std::string_view cpu_execution_mode_name(CpuExecutionMode mode) noexcept {
  switch (mode) {
  case CpuExecutionMode::Interpreter:
    return "interpreter";
  case CpuExecutionMode::ColdJit:
    return "cold-jit";
  case CpuExecutionMode::WarmJit:
    return "warm-jit";
  case CpuExecutionMode::Aot:
    return "aot";
  }
  return "invalid";
}

CpuExecutionConfigurationResult parse_cpu_execution_configuration(
    std::optional<std::string_view> mode, std::optional<std::string_view> cache_root,
    std::optional<std::string_view> explicit_cpu, std::optional<std::string_view> topology_root) {
  CpuExecutionConfiguration configuration;
  configuration.compiler_worker.executable = current_executable_path();
  if (mode.has_value()) {
    if (*mode == "interpreter") {
      configuration.mode = CpuExecutionMode::Interpreter;
    } else if (*mode == "cold-jit") {
      configuration.mode = CpuExecutionMode::ColdJit;
    } else if (*mode == "warm-jit") {
      configuration.mode = CpuExecutionMode::WarmJit;
    } else if (*mode == "aot") {
      configuration.mode = CpuExecutionMode::Aot;
    } else {
      return configuration_failure(
          "METAFLUX_CPU_EXECUTION_MODE must be interpreter, cold-jit, warm-jit, or aot");
    }
  }

  if (cache_root.has_value()) {
    if (cache_root->empty()) {
      return configuration_failure("METAFLUX_COMPILER_CACHE must not be empty");
    }
    const std::filesystem::path root(*cache_root);
    if (!root.is_absolute() || root == root.root_path() || contains_parent_reference(root)) {
      return configuration_failure(
          "METAFLUX_COMPILER_CACHE must be a bounded absolute path without '..'");
    }
    const auto normalized = root.lexically_normal();
    configuration.cache.mutable_root = normalized / "mutable";
    configuration.cache.aot_root = normalized / "aot";
  }
  if (explicit_cpu.has_value()) {
    std::uint32_t parsed_cpu = 0;
    const auto parsed = std::from_chars(explicit_cpu->data(),
                                        explicit_cpu->data() + explicit_cpu->size(), parsed_cpu);
    if (explicit_cpu->empty() || parsed.ec != std::errc{} ||
        parsed.ptr != explicit_cpu->data() + explicit_cpu->size()) {
      return configuration_failure("METAFLUX_CPU_PIN must be one non-negative CPU ID");
    }
    configuration.placement.explicit_cpu = parsed_cpu;
  }
  if (topology_root.has_value()) {
    auto placement_paths = backend::cpu::placement_paths_for_topology_root(topology_root);
    if (!placement_paths.ok()) {
      return configuration_failure(std::move(placement_paths.diagnostic));
    }
    configuration.placement_paths = std::move(*placement_paths.paths);
  }
  return {.configuration = std::move(configuration), .diagnostic = {}};
}

CpuExecutionConfigurationResult cpu_execution_configuration_from_environment() {
  const char* mode = std::getenv("METAFLUX_CPU_EXECUTION_MODE");
  const char* cache_root = std::getenv("METAFLUX_COMPILER_CACHE");
  const char* explicit_cpu = std::getenv("METAFLUX_CPU_PIN");
  const char* topology_root = std::getenv("METAFLUX_CPU_TOPOLOGY_ROOT");
  return parse_cpu_execution_configuration(
      mode == nullptr ? std::nullopt : std::optional<std::string_view>(mode),
      cache_root == nullptr ? std::nullopt : std::optional<std::string_view>(cache_root),
      explicit_cpu == nullptr ? std::nullopt : std::optional<std::string_view>(explicit_cpu),
      topology_root == nullptr ? std::nullopt : std::optional<std::string_view>(topology_root));
}

PreparedModule::PreparedModule(std::string canonical_kernel_ir,
                               std::shared_ptr<backend::cpu::CpuExecutor> executor,
                               bool accesses_global_memory)
    : canonical_kernel_ir_(std::move(canonical_kernel_ir)), executor_(std::move(executor)),
      accesses_global_memory_(accesses_global_memory) {}

PreparedModule::PreparedModule(backend::cpu::compiler::PreparedArtifact artifact,
                               backend::cpu::LoadedCompiledKernel kernel,
                               std::shared_ptr<backend::cpu::CpuExecutor> executor,
                               bool accesses_global_memory)
    : artifact_(std::move(artifact)), compiled_kernel_(std::move(kernel)),
      executor_(std::move(executor)), accesses_global_memory_(accesses_global_memory) {}

PreparedModule::~PreparedModule() = default;
PreparedModule::PreparedModule(PreparedModule&&) noexcept = default;
PreparedModule& PreparedModule::operator=(PreparedModule&&) noexcept = default;

backend::cpu::ExecutionResult
PreparedModule::launch(std::span<const backend::cpu::Argument> arguments,
                       backend::cpu::LaunchDimensions dimensions) const {
  return launch(arguments, dimensions, std::stop_token{});
}

backend::cpu::ExecutionResult
PreparedModule::launch(std::span<const backend::cpu::Argument> arguments,
                       backend::cpu::LaunchDimensions dimensions,
                       std::stop_token cancellation) const {
  if (executor_ == nullptr) {
    return {.diagnostic = backend::cpu::ExecutionDiagnostic{
                .error = backend::cpu::ExecutionError::PlacementUnavailable}};
  }
  if (artifact_.has_value()) {
    return compiled_kernel_.launch(*executor_, arguments, dimensions, cancellation);
  }
  return backend::cpu::execute_kernel_ir(*executor_, canonical_kernel_ir_, arguments, dimensions,
                                         cancellation);
}

CpuExecutionEngine::CpuExecutionEngine(CpuExecutionConfiguration configuration)
    : configuration_(std::move(configuration)) {}

CpuExecutionEngine::~CpuExecutionEngine() = default;

bool CpuExecutionEngine::initialize(std::string& diagnostic) {
  executor_ = std::make_shared<backend::cpu::CpuExecutor>(backend::cpu::CpuExecutorOptions{
      .paths = configuration_.placement_paths,
      .policy = configuration_.placement,
  });
  const auto placement = executor_->refresh();
  if (!placement.ok()) {
    diagnostic = executor_->last_diagnostic();
    executor_.reset();
    return false;
  }
  if (configuration_.mode == CpuExecutionMode::Interpreter) {
    return true;
  }
  try {
    cache_ = std::make_unique<compiler::PersistentArtifactCache>(
        effective_cache_configuration(configuration_));
  } catch (const std::bad_alloc&) {
    diagnostic = "compiler cache allocation failed";
    return false;
  } catch (const std::filesystem::filesystem_error& error) {
    diagnostic = error.what();
    return false;
  }

  if (configuration_.mode == CpuExecutionMode::Aot) {
    return true;
  }
  const auto reconciled = cache_->reconcile();
  if (reconciled != compiler::PersistentCacheError::None) {
    diagnostic = std::string(compiler::persistent_cache_error_name(reconciled));
    cache_.reset();
    return false;
  }
  return true;
}

PrepareModuleResult CpuExecutionEngine::prepare(std::uint32_t peer_uid,
                                                const compiler::Kernel& kernel,
                                                std::string canonical_kernel_ir) {
  return prepare(peer_uid, kernel, std::move(canonical_kernel_ir), std::stop_token{});
}

PrepareModuleResult CpuExecutionEngine::prepare(std::uint32_t peer_uid,
                                                const compiler::Kernel& kernel,
                                                std::string canonical_kernel_ir,
                                                std::stop_token cancellation) {
  try {
    const bool accesses_global_memory = kernel_accesses_global_memory(kernel);
    if (cancellation.stop_requested()) {
      return preparation_failure(ModulePreparationError::System, "module preparation cancelled");
    }
    if (configuration_.mode == CpuExecutionMode::Interpreter) {
      auto module = std::make_unique<PreparedModule>(std::move(canonical_kernel_ir), executor_,
                                                     accesses_global_memory);
      loaded_modules_.fetch_add(1U, std::memory_order_relaxed);
      return {.module = std::move(module), .error = ModulePreparationError::None, .diagnostic = {}};
    }
    if (cache_ == nullptr) {
      return preparation_failure(ModulePreparationError::System,
                                 "compiler cache is not initialized");
    }

    backend::cpu::compiler::ArtifactResult artifact;
    backend::cpu::compiler::CompileOptions compile_options;
    compile_options.cancellation = cancellation;
    if (configuration_.mode == CpuExecutionMode::ColdJit) {
      artifact = backend::cpu::compiler::acquire_artifact(
          *cache_, peer_uid, kernel, compile_options, [this, &kernel, cancellation] {
            compiler_requests_.fetch_add(1U, std::memory_order_relaxed);
            auto invocation =
                compile_kernel_in_worker(configuration_.compiler_worker, kernel, cancellation);
            if (invocation.launched) {
              compiler_worker_launches_.fetch_add(1U, std::memory_order_relaxed);
              last_compiler_worker_pid_.store(invocation.process_id, std::memory_order_relaxed);
            }
            if (!invocation.compilation.ok()) {
              compiler_worker_failures_.fetch_add(1U, std::memory_order_relaxed);
            }
            return std::move(invocation.compilation);
          });
    } else {
      // Warm JIT and AOT are lookup-only paths by construction.
      artifact = backend::cpu::compiler::lookup_cached_artifact(*cache_, peer_uid, kernel,
                                                                compile_options);
    }

    if (!artifact.ok()) {
      cache_misses_.fetch_add(1U, std::memory_order_relaxed);
      if (artifact.compile_diagnostic.has_value()) {
        return preparation_failure(compile_error(artifact.compile_diagnostic->error),
                                   std::string(backend::cpu::compiler::compile_error_name(
                                       artifact.compile_diagnostic->error)) +
                                       ": " + artifact.compile_diagnostic->message);
      }
      return preparation_failure(
          cache_error(artifact.cache_error),
          std::string(compiler::persistent_cache_error_name(artifact.cache_error)));
    }
    if (cancellation.stop_requested()) {
      return preparation_failure(ModulePreparationError::System, "module preparation cancelled");
    }

    const auto actual_mode = artifact.artifact->mode;
    const bool expected_mode =
        actual_mode == backend::cpu::compiler::ArtifactMode::AdministratorAot ||
        (configuration_.mode == CpuExecutionMode::ColdJit &&
         (actual_mode == backend::cpu::compiler::ArtifactMode::ColdJit ||
          actual_mode == backend::cpu::compiler::ArtifactMode::WarmJit)) ||
        (configuration_.mode == CpuExecutionMode::WarmJit &&
         actual_mode == backend::cpu::compiler::ArtifactMode::WarmJit) ||
        (configuration_.mode == CpuExecutionMode::Aot &&
         actual_mode == backend::cpu::compiler::ArtifactMode::AdministratorAot);
    if (!expected_mode) {
      cache_hits_.fetch_add(1U, std::memory_order_relaxed);
      return preparation_failure(ModulePreparationError::NotSupported,
                                 "cache entry does not match the selected execution mode");
    }
    if (actual_mode == backend::cpu::compiler::ArtifactMode::ColdJit) {
      cache_misses_.fetch_add(1U, std::memory_order_relaxed);
    } else {
      cache_hits_.fetch_add(1U, std::memory_order_relaxed);
    }

    auto signature = compiled_signature(*artifact.artifact);
    if (!signature.has_value()) {
      return preparation_failure(ModulePreparationError::Malformed,
                                 "compiled artifact has an unknown parameter kind");
    }
    auto loaded = backend::cpu::load_compiled_kernel(
        artifact.artifact->path, artifact.artifact->elf_sha256, std::move(*signature));
    if (!loaded.ok()) {
      return preparation_failure(ModulePreparationError::Malformed,
                                 "compiled ELF load failed: " + loaded.diagnostic);
    }
    if (cancellation.stop_requested()) {
      return preparation_failure(ModulePreparationError::System, "module preparation cancelled");
    }

    auto module = std::make_unique<PreparedModule>(
        std::move(*artifact.artifact), std::move(loaded.kernel), executor_, accesses_global_memory);
    loaded_modules_.fetch_add(1U, std::memory_order_relaxed);
    return {.module = std::move(module), .error = ModulePreparationError::None, .diagnostic = {}};
  } catch (const std::bad_alloc&) {
    return preparation_failure(ModulePreparationError::ResourceExhausted,
                               "module preparation allocation failed");
  } catch (const std::filesystem::filesystem_error& error) {
    return preparation_failure(ModulePreparationError::System, error.what());
  }
}

CpuExecutionStatistics CpuExecutionEngine::statistics() const {
  return {
      .compiler_requests = compiler_requests_.load(std::memory_order_relaxed),
      .compiler_worker_launches = compiler_worker_launches_.load(std::memory_order_relaxed),
      .compiler_worker_failures = compiler_worker_failures_.load(std::memory_order_relaxed),
      .last_compiler_worker_pid = last_compiler_worker_pid_.load(std::memory_order_relaxed),
      .cache_hits = cache_hits_.load(std::memory_order_relaxed),
      .cache_misses = cache_misses_.load(std::memory_order_relaxed),
      .loaded_modules = loaded_modules_.load(std::memory_order_relaxed),
      .executor =
          executor_ == nullptr ? backend::cpu::CpuExecutorStatistics{} : executor_->statistics(),
  };
}

AotPrewarmResult prewarm_aot_file(const CpuExecutionConfiguration& configuration,
                                  std::string_view ptx_path) {
  try {
    std::string diagnostic;
    auto source = read_ptx_file(ptx_path, diagnostic);
    if (!source.has_value()) {
      return prewarm_failure(std::move(diagnostic));
    }
    const auto parsed = compiler::ptx::parse(*source);
    if (!parsed.ok()) {
      return prewarm_failure(parsed.diagnostics.empty()
                                 ? "PTX parsing failed"
                                 : "PTX parsing failed: " + parsed.diagnostics.front().message);
    }

    compiler::PersistentArtifactCache cache(configuration.cache);
    bool compiled = false;
    auto artifact = backend::cpu::compiler::prewarm_aot(cache, *parsed.kernel, {}, [&] {
      compiled = true;
      return compile_kernel_in_worker(configuration.compiler_worker, *parsed.kernel).compilation;
    });
    if (!artifact.ok()) {
      if (artifact.compile_diagnostic.has_value()) {
        return prewarm_failure(std::string(backend::cpu::compiler::compile_error_name(
                                   artifact.compile_diagnostic->error)) +
                                   ": " + artifact.compile_diagnostic->message,
                               compiled);
      }
      return prewarm_failure(
          std::string(compiler::persistent_cache_error_name(artifact.cache_error)), compiled);
    }
    if (artifact.artifact->mode != backend::cpu::compiler::ArtifactMode::AdministratorAot) {
      return prewarm_failure("prewarm did not publish an AOT artifact", compiled);
    }
    auto signature = compiled_signature(*artifact.artifact);
    if (!signature.has_value()) {
      return prewarm_failure("AOT signature is malformed", compiled);
    }
    auto loaded = backend::cpu::load_compiled_kernel(
        artifact.artifact->path, artifact.artifact->elf_sha256, std::move(*signature));
    if (!loaded.ok()) {
      return prewarm_failure("prewarmed ELF load failed: " + loaded.diagnostic, compiled);
    }
    return {.success = true,
            .compiled = compiled,
            .cache_key = std::move(artifact.artifact->cache_key),
            .diagnostic = {}};
  } catch (const std::bad_alloc&) {
    return prewarm_failure("AOT prewarm allocation failed");
  } catch (const std::filesystem::filesystem_error& error) {
    return prewarm_failure(error.what());
  } catch (const std::system_error& error) {
    return prewarm_failure(error.what());
  }
}

} // namespace metaflux::service
