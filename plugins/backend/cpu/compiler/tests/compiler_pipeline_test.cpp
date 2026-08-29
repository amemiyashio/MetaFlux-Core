#include "metaflux/backend/cpu/compiled_kernel.hpp"
#include "metaflux/backend/cpu/compiler.hpp"
#include "metaflux/backend/cpu/executor.hpp"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

constexpr std::string_view kAddPtx = R"ptx(.version 9.0
.target sm_70
.address_size 64
.visible .entry add_u32(
  .param .u64 destination,
  .param .u64 left,
  .param .u64 right,
  .param .u32 count
)
{
  .reg .pred %p;
  .reg .b32 %r<10>;
  .reg .b64 %rd<10>;
  ld.param.u64 %rd0, [destination];
  ld.param.u64 %rd1, [left];
  ld.param.u64 %rd2, [right];
  ld.param.u32 %r0, [count];
  mov.u32 %r1, %tid.x;
  mov.u32 %r2, %ctaid.x;
  mov.u32 %r3, %ntid.x;
  mad.lo.u32 %r4, %r2, %r3, %r1;
  setp.ge.u32 %p, %r4, %r0;
  @%p bra done;
  mul.wide.u32 %rd3, %r4, 4;
  add.u64 %rd4, %rd0, %rd3;
  add.u64 %rd5, %rd1, %rd3;
  add.u64 %rd6, %rd2, %rd3;
  ld.global.u32 %r5, [%rd5];
  ld.global.u32 %r6, [%rd6];
  add.u32 %r7, %r5, %r6;
  st.global.u32 [%rd4], %r7;
done:
  ret;
}
)ptx";

constexpr std::string_view kCopyPtx = R"ptx(.version 9.0
.target sm_70
.address_size 64
.visible .entry copy_u32(
  .param .u64 destination,
  .param .u64 source,
  .param .u32 count
)
{
  .reg .pred %p;
  .reg .b32 %r<8>;
  .reg .b64 %rd<8>;
  ld.param.u64 %rd0, [destination];
  ld.param.u64 %rd1, [source];
  ld.param.u32 %r0, [count];
  mov.u32 %r1, %tid.x;
  mov.u32 %r2, %ctaid.x;
  mov.u32 %r3, %ntid.x;
  mad.lo.u32 %r4, %r2, %r3, %r1;
  setp.ge.u32 %p, %r4, %r0;
  @%p bra done;
  mul.wide.u32 %rd2, %r4, 4;
  add.u64 %rd3, %rd0, %rd2;
  add.u64 %rd4, %rd1, %rd2;
  ld.global.u32 %r5, [%rd4];
  st.global.u32 [%rd3], %r5;
done:
  ret;
}
)ptx";

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "CPU compiler pipeline failure: " << message << '\n';
  }
  return condition;
}

bool write_elf(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  return output.good();
}

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    std::string pattern = "/tmp/metaflux-cpu-compiler-test-XXXXXX";
    if (const char* created = mkdtemp(pattern.data()); created != nullptr) {
      path_ = created;
    }
  }

  ~TemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code ignored;
      std::filesystem::remove_all(path_, ignored);
    }
  }

  [[nodiscard]] bool valid() const noexcept { return !path_.empty(); }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
};

bool write_hanging_linker(const std::filesystem::path& path,
                          const std::filesystem::path& entered_marker) {
  std::ofstream output(path, std::ios::trunc);
  output << "#!/bin/sh\n"
         << ": > '" << entered_marker.string() << "'\n"
         << "while :; do :; done\n";
  output.close();
  return output.good() && chmod(path.c_str(), 0700) == 0;
}

std::optional<metaflux::backend::cpu::CompiledKernelSignature>
signature_for(std::span<const metaflux::compiler::ParameterKind> parameters, bool uses_fp) {
  metaflux::backend::cpu::CompiledKernelSignature signature;
  signature.uses_floating_point = uses_fp;
  signature.parameters.reserve(parameters.size());
  for (const auto parameter : parameters) {
    switch (parameter) {
    case metaflux::compiler::ParameterKind::BufferU32:
      signature.parameters.push_back(metaflux::backend::cpu::CompiledParameterKind::BufferU32);
      break;
    case metaflux::compiler::ParameterKind::ScalarU32:
      signature.parameters.push_back(metaflux::backend::cpu::CompiledParameterKind::ScalarU32);
      break;
    case metaflux::compiler::ParameterKind::ScalarF32:
      signature.parameters.push_back(metaflux::backend::cpu::CompiledParameterKind::ScalarF32);
      break;
    }
  }
  return signature;
}

bool launch_add(const metaflux::backend::cpu::compiler::PreparedArtifact& artifact) {
  const auto signature = signature_for(artifact.parameters, artifact.uses_floating_point);
  auto loaded =
      metaflux::backend::cpu::load_compiled_kernel(artifact.path, artifact.elf_sha256, *signature);
  if (!expect(loaded.ok(), loaded.diagnostic)) {
    return false;
  }
  std::vector<std::uint32_t> left{0U, 1U, 0xffffffffU, 8U, 13U};
  std::vector<std::uint32_t> right{4U, 7U, 1U, 9U, 29U};
  std::vector<std::uint32_t> destination(left.size(), 0xfeedfaceU);
  const std::array<metaflux::backend::cpu::Argument, 4> arguments{
      metaflux::backend::cpu::BufferArgument{.words = destination, .writable = true},
      metaflux::backend::cpu::BufferArgument{.words = left},
      metaflux::backend::cpu::BufferArgument{.words = right}, 5U};
  const auto execution = loaded.kernel.launch(arguments, {.grid_x = 1, .block_x = 64});
  return expect(execution.ok(), "cached ELF must launch") &&
         expect(destination == std::vector<std::uint32_t>({4U, 8U, 0U, 17U, 42U}),
                "cached ELF Add must be bit-exact");
}

metaflux::compiler::PersistentCacheConfig cache_config(const TemporaryDirectory& temporary) {
  return {
      .mutable_root = temporary.path() / "mutable",
      .aot_root = temporary.path() / "aot",
      .compiler_epoch = 1,
      .limits =
          {
              .per_uid_bytes = 64U * 1024U * 1024U,
              .global_bytes = 128U * 1024U * 1024U,
              .maximum_entry_bytes = 16U * 1024U * 1024U,
              .reserved_free_bytes = 0U,
              .reserved_free_percent = 0U,
          },
      .clock = {},
      .inject_fault = {},
      .filesystem_space = {},
  };
}

} // namespace

int main(int argc, char** argv) {
  if (!expect(argc == 2, "test requires the qualified ELF output path")) {
    return 1;
  }
  if (!expect(
          metaflux::backend::cpu::compiler::make_cpu_cache_identity().pgo_id ==
              metaflux::backend::cpu::compiler::kCpuPgoIdentity,
          "CPU cache identity must use the configured build PGO identity")) {
    return 1;
  }
  const auto parsed = metaflux::compiler::ptx::parse(kAddPtx);
  if (!expect(parsed.ok(), "Add PTX must parse")) {
    return 1;
  }
  const auto compiled = metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
  if (!compiled.ok()) {
    if (compiled.diagnostic.has_value()) {
      std::cerr << metaflux::backend::cpu::compiler::compile_error_name(compiled.diagnostic->error)
                << ": " << compiled.diagnostic->message << '\n';
    }
    return 1;
  }
  const auto& artifact = *compiled.artifact;
  const auto parsed_copy = metaflux::compiler::ptx::parse(kCopyPtx);
  const auto compiled_copy =
      parsed_copy.ok() ? metaflux::backend::cpu::compiler::compile_kernel(*parsed_copy.kernel)
                       : metaflux::backend::cpu::compiler::CompileResult{};
  const auto repeated = metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
  if (!expect(metaflux::backend::cpu::compiler::kCpuHelperAbiVersion ==
                      metaflux::backend::cpu::kCompiledKernelHelperAbiVersion &&
                  metaflux::backend::cpu::compiler::kCpuCompiledEntrySymbol ==
                      metaflux::backend::cpu::kCompiledKernelEntrySymbol,
              "compiler and runtime must agree on the helper ABI and symbol") ||
      !expect(artifact.mlir_text.find("llvm.func @metaflux_cpu_cta_v2") != std::string_view::npos,
              "MLIR must contain the CPU entry") ||
      !expect(artifact.llvm_ir_text.find("@metaflux_cpu_cta_v2") != std::string_view::npos,
              "LLVM IR must contain the translated CPU entry") ||
      !expect(artifact.llvm_ir_text.find("vector.body") != std::string_view::npos &&
                  artifact.llvm_ir_text.find("<4 x i32>") != std::string_view::npos,
              "optimized LLVM IR must contain a real integer vector loop") ||
      !expect(compiled_copy.ok() &&
                  compiled_copy.artifact->llvm_ir_text.find("vector.body") !=
                      std::string_view::npos &&
                  compiled_copy.artifact->llvm_ir_text.find("<4 x i32>") != std::string_view::npos,
              "optimized Copy LLVM IR must contain a real vector loop") ||
      !expect(artifact.elf_sha256.size() == 64U, "ELF must have a SHA-256 digest") ||
      !expect(repeated.ok() && repeated.artifact->elf_sha256 == artifact.elf_sha256 &&
                  repeated.artifact->elf == artifact.elf,
              "identical input and compatibility identity must emit deterministic ELF") ||
      !expect(write_elf(argv[1], artifact.elf), "qualified ELF must be written")) {
    return 1;
  }

  metaflux::backend::cpu::compiler::CompileOptions cancelled_options;
  cancelled_options.deadline = std::chrono::steady_clock::now();
  const auto cancelled =
      metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel, cancelled_options);
  auto invalid_kernel = *parsed.kernel;
  invalid_kernel.operations.front().result =
      static_cast<std::uint32_t>(invalid_kernel.registers.size() + 1U);
  const auto invalid = metaflux::backend::cpu::compiler::compile_kernel(invalid_kernel);
  metaflux::backend::cpu::compiler::CompileOptions unsupported_options;
  unsupported_options.target_triple = "aarch64-unknown-linux-gnu";
  const auto unsupported =
      metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel, unsupported_options);
  metaflux::backend::cpu::compiler::CompileOptions limited_options;
  limited_options.limits.maximum_register_storage_bytes = 1U;
  const auto limited =
      metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel, limited_options);
  metaflux::backend::cpu::compiler::CompileOptions linker_limited_options;
  linker_limited_options.limits.maximum_artifact_bytes = 1U;
  const auto linker_limited =
      metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel, linker_limited_options);
  TemporaryDirectory cancellation_temporary;
  const auto hanging_linker = cancellation_temporary.path() / "hanging-linker";
  const auto linker_entered = cancellation_temporary.path() / "linker-entered";
  metaflux::backend::cpu::compiler::CompileOptions linking_cancelled_options;
  linking_cancelled_options.linker_path = hanging_linker;
  linking_cancelled_options.temporary_root = cancellation_temporary.path() / "work";
  linking_cancelled_options.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  const bool hanging_linker_ready =
      cancellation_temporary.valid() && write_hanging_linker(hanging_linker, linker_entered);
  const auto linking_cancelled = hanging_linker_ready
                                     ? metaflux::backend::cpu::compiler::compile_kernel(
                                           *parsed.kernel, linking_cancelled_options)
                                     : metaflux::backend::cpu::compiler::CompileResult{};
  if (!expect(!cancelled.ok() && cancelled.diagnostic.has_value() &&
                  cancelled.diagnostic->error ==
                      metaflux::backend::cpu::compiler::CompileError::Cancelled,
              "pre-requested cancellation must stop compilation") ||
      !expect(!invalid.ok() && invalid.diagnostic.has_value() &&
                  invalid.diagnostic->error ==
                      metaflux::backend::cpu::compiler::CompileError::InvalidKernel &&
                  invalid.diagnostic->location.line ==
                      invalid_kernel.operations.front().location.line,
              "invalid Kernel IR must be rejected with its source location") ||
      !expect(!unsupported.ok() && unsupported.diagnostic.has_value() &&
                  unsupported.diagnostic->error ==
                      metaflux::backend::cpu::compiler::CompileError::UnsupportedTarget,
              "unsupported target triples must fail before lowering") ||
      !expect(!limited.ok() && limited.diagnostic.has_value() &&
                  limited.diagnostic->error ==
                      metaflux::backend::cpu::compiler::CompileError::ResourceLimit,
              "register storage limit must reject compilation deterministically") ||
      !expect(!linker_limited.ok() && linker_limited.diagnostic.has_value() &&
                  linker_limited.diagnostic->error ==
                      metaflux::backend::cpu::compiler::CompileError::LinkerFailed,
              "linker file-size limit must reject artifact publication") ||
      !expect(hanging_linker_ready && std::filesystem::exists(linker_entered) &&
                  !linking_cancelled.ok() && linking_cancelled.diagnostic.has_value() &&
                  linking_cancelled.diagnostic->error ==
                      metaflux::backend::cpu::compiler::CompileError::Cancelled,
              "deadline cancellation must kill and reap an active linker")) {
    return 1;
  }

  metaflux::backend::cpu::CompiledKernelSignature signature{
      .parameters =
          {
              metaflux::backend::cpu::CompiledParameterKind::BufferU32,
              metaflux::backend::cpu::CompiledParameterKind::BufferU32,
              metaflux::backend::cpu::CompiledParameterKind::BufferU32,
              metaflux::backend::cpu::CompiledParameterKind::ScalarU32,
          },
  };
  auto loaded =
      metaflux::backend::cpu::load_compiled_kernel(argv[1], artifact.elf_sha256, signature);
  if (!expect(loaded.ok(), loaded.diagnostic)) {
    return 1;
  }

  std::vector<std::uint32_t> left{0U, 1U, 0xffffffffU, 8U, 13U};
  std::vector<std::uint32_t> right{4U, 7U, 1U, 9U, 29U};
  std::vector<std::uint32_t> destination(left.size(), 0xfeedfaceU);
  std::vector<metaflux::backend::cpu::Argument> arguments;
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{
      .words = destination,
      .writable = true,
  });
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{.words = left});
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{.words = right});
  arguments.emplace_back(5U);
  metaflux::backend::cpu::CpuExecutor executor;
  const auto execution = loaded.kernel.launch(executor, arguments, {.grid_x = 2, .block_x = 64});
  const auto execution_statistics = executor.statistics();
  if (!expect(execution.ok(), "loaded ELF must launch") ||
      !expect(destination == std::vector<std::uint32_t>({4U, 8U, 0U, 17U, 42U}),
              "loaded ELF Add must be bit-exact") ||
      !expect(execution_statistics.cta_jobs == 2U && execution_statistics.whole_kernel_jobs == 0U,
              "compiled launch must submit one executor job per CTA")) {
    return 1;
  }

  std::vector<std::uint32_t> in_place{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  std::vector<std::uint32_t> in_place_right{8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
  const std::array<metaflux::backend::cpu::Argument, 4> aliased_arguments{
      metaflux::backend::cpu::BufferArgument{.words = in_place, .writable = true},
      metaflux::backend::cpu::BufferArgument{.words = in_place},
      metaflux::backend::cpu::BufferArgument{.words = in_place_right}, 8U};
  const auto aliased_execution =
      loaded.kernel.launch(executor, aliased_arguments, {.grid_x = 1, .block_x = 8});
  if (!expect(aliased_execution.ok(), "aliased Add launch must succeed") ||
      !expect(in_place == std::vector<std::uint32_t>(8U, 9U),
              "runtime alias check and scalar fallback must preserve in-place Add")) {
    return 1;
  }

  std::vector<std::uint32_t> short_destination(4U);
  std::vector<std::uint32_t> full_left(5U, 1U);
  std::vector<std::uint32_t> full_right(5U, 2U);
  const std::array<metaflux::backend::cpu::Argument, 4> readonly_short_arguments{
      metaflux::backend::cpu::BufferArgument{.words = short_destination, .writable = false},
      metaflux::backend::cpu::BufferArgument{.words = full_left},
      metaflux::backend::cpu::BufferArgument{.words = full_right}, 5U};
  const auto readonly_short =
      loaded.kernel.launch(executor, readonly_short_arguments, {.grid_x = 1, .block_x = 8});
  if (!expect(!readonly_short.ok() && readonly_short.diagnostic->error ==
                                          metaflux::backend::cpu::ExecutionError::WriteToReadOnly,
              "vector preflight must preserve write-permission error precedence")) {
    return 1;
  }

  std::vector<std::uint32_t> two_dimensional_destination(4U);
  std::vector<std::uint32_t> two_dimensional_left(4U, 3U);
  std::vector<std::uint32_t> two_dimensional_right(4U, 4U);
  const std::array<metaflux::backend::cpu::Argument, 4> two_dimensional_arguments{
      metaflux::backend::cpu::BufferArgument{.words = two_dimensional_destination,
                                             .writable = true},
      metaflux::backend::cpu::BufferArgument{.words = two_dimensional_left},
      metaflux::backend::cpu::BufferArgument{.words = two_dimensional_right}, 4U};
  const auto two_dimensional = loaded.kernel.launch(executor, two_dimensional_arguments,
                                                    {.grid_x = 1, .block_x = 4, .block_y = 2});
  if (!expect(two_dimensional.ok() &&
                  two_dimensional_destination == std::vector<std::uint32_t>(4U, 7U),
              "2D blocks must retain the generic lane-phase fallback")) {
    return 1;
  }

  TemporaryDirectory temporary;
  if (!expect(temporary.valid(), "persistent-cache test root must exist")) {
    return 1;
  }
  const auto config = cache_config(temporary);
  constexpr std::uint32_t kPeerUid = 1000U;
  {
    auto legacy_config = config;
    legacy_config.mutable_root = temporary.path() / "legacy-mutable";
    legacy_config.aot_root = temporary.path() / "legacy-aot";
    metaflux::compiler::PersistentArtifactCache legacy_cache(legacy_config);
    const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
    auto legacy_identity = metaflux::backend::cpu::compiler::make_cpu_cache_identity();
    legacy_identity.pass_pipeline =
        "kir-v2-to-llvm-dialect,cpu-scalar-cta-phases,f32-fma-twosum-v1,"
        "llvm-o2,pic-et-dyn-v1";
    legacy_identity.helper_abi = 1U;
    const auto legacy_key = metaflux::compiler::make_cache_key(legacy_identity, serialized.text);
    const auto current_key = metaflux::compiler::make_cache_key(
        metaflux::backend::cpu::compiler::make_cpu_cache_identity(), serialized.text);
    const metaflux::compiler::ArtifactDescriptor legacy_descriptor{
        .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
        .helper_abi = 1U,
        .payload = "cpu-v1;fp=0;params=0,0,0,1",
    };
    if (!expect(legacy_key != current_key,
                "single-CTA helper and SIMD pipeline must change cache identity") ||
        !expect(legacy_cache.install_aot(legacy_key, artifact.elf, legacy_descriptor) ==
                    metaflux::compiler::PersistentCacheError::None,
                "legacy helper artifact fixture must install") ||
        !expect(metaflux::backend::cpu::compiler::lookup_cached_artifact(legacy_cache, kPeerUid,
                                                                         *parsed.kernel)
                        .cache_error == metaflux::compiler::PersistentCacheError::Miss,
                "legacy whole-grid artifact must deterministically miss the v2 lookup")) {
      return 1;
    }
  }
  auto quota_config = config;
  quota_config.mutable_root = temporary.path() / "quota-mutable";
  quota_config.aot_root = temporary.path() / "quota-aot";
  quota_config.limits.per_uid_bytes = 512U * 1024U;
  quota_config.limits.global_bytes = 512U * 1024U;
  metaflux::compiler::PersistentArtifactCache quota_cache(quota_config);
  std::uint32_t quota_callbacks = 0;
  const auto quota_rejected = metaflux::backend::cpu::compiler::acquire_artifact(
      quota_cache, kPeerUid, *parsed.kernel, {}, [&] {
        ++quota_callbacks;
        return metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
      });
  if (!expect(!quota_rejected.ok() && quota_rejected.cache_error ==
                                          metaflux::compiler::PersistentCacheError::QuotaExceeded,
              "insufficient UID quota must reject the compile reservation") ||
      !expect(quota_callbacks == 0U, "quota rejection must happen before compiler callback")) {
    return 1;
  }

  TemporaryDirectory lock_timeout_temporary;
  auto lock_timeout_config = cache_config(lock_timeout_temporary);
  metaflux::compiler::PersistentArtifactCache lock_holder(lock_timeout_config);
  metaflux::compiler::PersistentArtifactCache lock_follower(lock_timeout_config);
  const auto lock_timeout_kernel = metaflux::compiler::serialize_kernel(*parsed.kernel);
  const auto lock_timeout_key = metaflux::compiler::make_cache_key(
      metaflux::backend::cpu::compiler::make_cpu_cache_identity(), lock_timeout_kernel.text);
  const auto held_reservation = lock_holder.reserve(kPeerUid, lock_timeout_key, 1024U * 1024U);
  metaflux::backend::cpu::compiler::CompileOptions lock_timeout_options;
  const auto lock_timeout_started = std::chrono::steady_clock::now();
  lock_timeout_options.deadline = lock_timeout_started + std::chrono::milliseconds(50);
  std::uint32_t lock_timeout_callbacks = 0;
  const auto lock_timed_out = metaflux::backend::cpu::compiler::acquire_artifact(
      lock_follower, kPeerUid, *parsed.kernel, lock_timeout_options, [&] {
        ++lock_timeout_callbacks;
        return metaflux::backend::cpu::compiler::CompileResult{.artifact = artifact,
                                                               .diagnostic = std::nullopt};
      });
  const auto lock_timeout_elapsed = std::chrono::steady_clock::now() - lock_timeout_started;
  if (held_reservation.ok()) {
    lock_holder.cancel(*held_reservation.reservation);
  }
  if (!expect(held_reservation.ok(), "lock-timeout fixture must hold the cache key") ||
      !expect(!lock_timed_out.ok() &&
                  lock_timed_out.cache_error == metaflux::compiler::PersistentCacheError::Io,
              "CompileOptions deadline must bound same-key cache lock waiting") ||
      !expect(lock_timeout_callbacks == 0U,
              "cache lock timeout must occur before the compiler callback") ||
      !expect(lock_timeout_elapsed < std::chrono::seconds(1),
              "pipeline cache lock timeout must return within a broad bounded interval")) {
    return 1;
  }

  TemporaryDirectory concurrent_temporary;
  auto concurrent_config = cache_config(concurrent_temporary);
  metaflux::compiler::PersistentArtifactCache first_concurrent_cache(concurrent_config);
  metaflux::compiler::PersistentArtifactCache second_concurrent_cache(concurrent_config);
  metaflux::backend::cpu::compiler::ArtifactResult first_concurrent;
  metaflux::backend::cpu::compiler::ArtifactResult second_concurrent;
  std::mutex compile_gate;
  std::condition_variable compile_condition;
  std::uint32_t concurrent_compile_callbacks = 0;
  bool first_compile_entered = false;
  bool release_first_compile = false;
  std::thread first_compile([&] {
    first_concurrent = metaflux::backend::cpu::compiler::acquire_artifact(
        first_concurrent_cache, kPeerUid, *parsed.kernel, {}, [&] {
          std::unique_lock lock(compile_gate);
          ++concurrent_compile_callbacks;
          first_compile_entered = true;
          compile_condition.notify_all();
          compile_condition.wait(lock, [&] { return release_first_compile; });
          return metaflux::backend::cpu::compiler::CompileResult{.artifact = artifact,
                                                                 .diagnostic = std::nullopt};
        });
  });
  {
    std::unique_lock lock(compile_gate);
    compile_condition.wait(lock, [&] { return first_compile_entered; });
  }
  std::thread second_compile([&] {
    second_concurrent = metaflux::backend::cpu::compiler::acquire_artifact(
        second_concurrent_cache, kPeerUid, *parsed.kernel, {}, [&] {
          std::scoped_lock lock(compile_gate);
          ++concurrent_compile_callbacks;
          compile_condition.notify_all();
          return metaflux::backend::cpu::compiler::CompileResult{.artifact = artifact,
                                                                 .diagnostic = std::nullopt};
        });
  });
  {
    std::unique_lock lock(compile_gate);
    static_cast<void>(compile_condition.wait_for(
        lock, std::chrono::milliseconds(100), [&] { return concurrent_compile_callbacks == 2U; }));
    release_first_compile = true;
    compile_condition.notify_all();
  }
  first_compile.join();
  second_compile.join();
  const bool one_cold_one_warm = first_concurrent.ok() && second_concurrent.ok() &&
                                 ((first_concurrent.artifact->mode ==
                                       metaflux::backend::cpu::compiler::ArtifactMode::ColdJit &&
                                   second_concurrent.artifact->mode ==
                                       metaflux::backend::cpu::compiler::ArtifactMode::WarmJit) ||
                                  (first_concurrent.artifact->mode ==
                                       metaflux::backend::cpu::compiler::ArtifactMode::WarmJit &&
                                   second_concurrent.artifact->mode ==
                                       metaflux::backend::cpu::compiler::ArtifactMode::ColdJit));
  if (!expect(concurrent_compile_callbacks == 1U,
              "cross-instance same-key acquisition must invoke one compiler callback") ||
      !expect(one_cold_one_warm,
              "same-key publisher and follower must resolve as one cold and one warm artifact") ||
      !launch_add(*first_concurrent.artifact) || !launch_add(*second_concurrent.artifact)) {
    return 1;
  }
  std::string cache_key;
  std::string cold_path;
  {
    metaflux::compiler::PersistentArtifactCache cache(config);
    if (!expect(cache.reconcile() == metaflux::compiler::PersistentCacheError::None,
                "cold cache reconciliation must succeed")) {
      return 1;
    }
    std::uint32_t compile_callbacks = 0;
    const auto cold = metaflux::backend::cpu::compiler::acquire_artifact(
        cache, kPeerUid, *parsed.kernel, {}, [&] {
          ++compile_callbacks;
          return metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
        });
    if (!expect(cold.ok() &&
                    cold.artifact->mode == metaflux::backend::cpu::compiler::ArtifactMode::ColdJit,
                "first executable artifact acquisition must be a cold JIT publication") ||
        !expect(compile_callbacks == 1U, "cold JIT must invoke exactly one compiler callback") ||
        !expect(cold.artifact->elf_sha256 == artifact.elf_sha256,
                "cold cache content must preserve the compiled digest") ||
        !expect(cold.artifact->path.filename().string().size() == 9U,
                "published artifact must use a fixed kernel.so filename") ||
        !launch_add(*cold.artifact)) {
      return 1;
    }
    cache_key = cold.artifact->cache_key;
    cold_path = cold.artifact->path.string();
    const auto digest_directory = cold.artifact->path.parent_path();
    if (!expect(digest_directory.filename().string().size() == 64U &&
                    digest_directory.parent_path().filename() ==
                        digest_directory.filename().string().substr(0U, 2U),
                "D0014 cache path must be sha-prefix/full-sha/kernel.so")) {
      return 1;
    }
  }

  {
    metaflux::compiler::PersistentArtifactCache cache(config);
    if (!expect(cache.reconcile() == metaflux::compiler::PersistentCacheError::None,
                "warm cache reconciliation must succeed")) {
      return 1;
    }
    std::uint32_t unexpected_compiles = 0;
    auto warm = metaflux::backend::cpu::compiler::acquire_artifact(
        cache, kPeerUid, *parsed.kernel, {}, [&] {
          ++unexpected_compiles;
          return metaflux::backend::cpu::compiler::CompileResult{};
        });
    if (!expect(warm.ok() &&
                    warm.artifact->mode == metaflux::backend::cpu::compiler::ArtifactMode::WarmJit,
                "persisted executable artifact must be a warm hit after cache restart") ||
        !expect(unexpected_compiles == 0U,
                "warm hit must load without a compiler callback or worker") ||
        !expect(warm.artifact->cache_key == cache_key && warm.artifact->path.string() == cold_path,
                "cold and warm modes must share one compatibility identity") ||
        !launch_add(*warm.artifact)) {
      return 1;
    }
    warm.artifact->cache_pin.reset();
    std::ofstream corrupt(warm.artifact->path, std::ios::binary | std::ios::trunc);
    corrupt.put('X');
    corrupt.close();
    std::uint32_t recovery_compiles = 0;
    auto recovered = metaflux::backend::cpu::compiler::acquire_artifact(
        cache, kPeerUid, *parsed.kernel, {}, [&] {
          ++recovery_compiles;
          return metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
        });
    if (!expect(recovered.ok() && recovered.artifact->mode ==
                                      metaflux::backend::cpu::compiler::ArtifactMode::ColdJit,
                "corrupt warm artifact must recover through a new cold publication") ||
        !expect(recovery_compiles == 1U,
                "corruption recovery must invoke exactly one compiler callback") ||
        !expect(recovered.artifact->elf_sha256 == artifact.elf_sha256,
                "corruption recovery must restore the deterministic ELF") ||
        !launch_add(*recovered.artifact)) {
      return 1;
    }
    recovered.artifact->cache_pin.reset();
    const auto metadata_path = recovered.artifact->path.parent_path() / "metadata.v1";
    std::ifstream metadata_input(metadata_path);
    std::ostringstream metadata_contents;
    metadata_contents << metadata_input.rdbuf();
    auto metadata = metadata_contents.str();
    const auto payload_begin = metadata.find("payload=");
    const auto payload_end = metadata.find('\n', payload_begin);
    if (!expect(payload_begin != std::string::npos && payload_end != std::string::npos,
                "published metadata must contain its signature payload")) {
      return 1;
    }
    metadata.replace(payload_begin, payload_end - payload_begin,
                     "payload=cpu-single-cta-v2;fp=0;params=2");
    std::ofstream metadata_output(metadata_path, std::ios::trunc);
    metadata_output << metadata;
    metadata_output.close();
    std::uint32_t metadata_recovery_compiles = 0;
    const auto metadata_recovered = metaflux::backend::cpu::compiler::acquire_artifact(
        cache, kPeerUid, *parsed.kernel, {}, [&] {
          ++metadata_recovery_compiles;
          return metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
        });
    if (!expect(metadata_recovered.ok() &&
                    metadata_recovered.artifact->mode ==
                        metaflux::backend::cpu::compiler::ArtifactMode::ColdJit,
                "incompatible mutable metadata must recover through cold compilation") ||
        !expect(metadata_recovery_compiles == 1U,
                "metadata recovery must invoke exactly one compiler callback") ||
        !launch_add(*metadata_recovered.artifact)) {
      return 1;
    }

    std::uint32_t aot_compiles = 0;
    const auto aot = metaflux::backend::cpu::compiler::prewarm_aot(cache, *parsed.kernel, {}, [&] {
      ++aot_compiles;
      return metaflux::backend::cpu::compiler::compile_kernel(*parsed.kernel);
    });
    if (!expect(aot.ok() && aot.artifact->mode ==
                                metaflux::backend::cpu::compiler::ArtifactMode::AdministratorAot,
                "administrator prewarm must select the read-only AOT tier") ||
        !expect(aot_compiles == 1U, "AOT prewarm must invoke exactly one compiler callback") ||
        !expect(aot.artifact->cache_key == cache_key &&
                    aot.artifact->elf_sha256 == artifact.elf_sha256,
                "AOT must retain the same compatibility identity and executable digest") ||
        !expect(aot.artifact->path.string() != cold_path,
                "AOT and mutable tiers must use physically separate roots") ||
        !launch_add(*aot.artifact)) {
      return 1;
    }
  }
  return 0;
}
