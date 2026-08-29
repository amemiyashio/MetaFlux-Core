#include "metaflux/backend/cpu/interpreter.hpp"
#include "metaflux/compiler/cache.hpp"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"
#include "metaflux/compiler/reference.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t kRandomSeed = 0x4d465831U;

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
.entry copy_u32(
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
    std::cerr << "CPU Add/Copy differential failure (seed " << kRandomSeed << "): " << message
              << '\n';
  }
  return condition;
}

std::string translate(std::string_view source) {
  const auto parsed = metaflux::compiler::ptx::parse(source);
  if (!parsed.ok()) {
    return {};
  }
  const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
  return serialized.ok() ? serialized.text : std::string{};
}

metaflux::compiler::CacheIdentity cache_identity() {
  return metaflux::compiler::CacheIdentity{
      .toolchain_fingerprint = "llvm-22.1.8-w03-fixture",
      .compiler_epoch = 1,
      .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
      .pass_pipeline = "kir-canonicalize,cpu-scalar-v1",
      .target_triple = "x86_64-unknown-linux-gnu",
      .cpu_name = "generic",
      .canonical_features = {},
      .optimization_level = "O0-interpreter-fixture",
      .fp_semantics = "integer-exact",
      .backend_abi = 1,
      .helper_abi = 1,
      .pgo_id = "none",
  };
}

std::string_view selected_text(const metaflux::compiler::ArtifactSelection& selection,
                               std::string_view canonical) {
  return selection.artifact.has_value() ? std::string_view(selection.artifact->canonical_kernel_ir)
                                        : canonical;
}

metaflux::backend::cpu::LaunchDimensions launch_for(std::uint32_t count) {
  constexpr std::uint32_t block_size = 64;
  return {
      .grid_x = std::max(1U, (count + block_size - 1U) / block_size),
      .block_x = block_size,
  };
}

std::vector<metaflux::backend::cpu::Argument> add_arguments(std::vector<std::uint32_t>& destination,
                                                            std::vector<std::uint32_t>& left,
                                                            std::vector<std::uint32_t>& right,
                                                            std::uint32_t count) {
  using metaflux::backend::cpu::Argument;
  using metaflux::backend::cpu::BufferArgument;
  std::vector<Argument> arguments;
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(destination),
      .writable = true,
  });
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(left),
      .writable = false,
  });
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(right),
      .writable = false,
  });
  arguments.emplace_back(count);
  return arguments;
}

std::vector<metaflux::backend::cpu::Argument>
copy_arguments(std::vector<std::uint32_t>& destination, std::vector<std::uint32_t>& source,
               std::uint32_t count) {
  using metaflux::backend::cpu::Argument;
  using metaflux::backend::cpu::BufferArgument;
  std::vector<Argument> arguments;
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(destination),
      .writable = true,
  });
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(source),
      .writable = false,
  });
  arguments.emplace_back(count);
  return arguments;
}

bool run_add(std::string_view kernel_ir, std::vector<std::uint32_t>& destination,
             std::vector<std::uint32_t>& left, std::vector<std::uint32_t>& right,
             std::uint32_t count) {
  const auto arguments = add_arguments(destination, left, right, count);
  return metaflux::backend::cpu::execute_kernel_ir(kernel_ir, arguments, launch_for(count)).ok();
}

bool run_copy(std::string_view kernel_ir, std::vector<std::uint32_t>& destination,
              std::vector<std::uint32_t>& source, std::uint32_t count) {
  const auto arguments = copy_arguments(destination, source, count);
  return metaflux::backend::cpu::execute_kernel_ir(kernel_ir, arguments, launch_for(count)).ok();
}

bool test_artifact_modes(std::string_view add_ir, std::string_view copy_ir) {
  const auto identity = cache_identity();
  metaflux::compiler::FixtureArtifactCache cache;
  const auto interpreted =
      cache.select(identity, add_ir, metaflux::compiler::ExecutionRequest::Interpreter);
  const auto cold = cache.select(identity, add_ir, metaflux::compiler::ExecutionRequest::Jit);
  const auto warm = cache.select(identity, add_ir, metaflux::compiler::ExecutionRequest::Jit);
  metaflux::compiler::FixtureArtifactCache aot_cache;
  aot_cache.prewarm_aot(identity, add_ir);
  const auto aot = aot_cache.select(identity, add_ir, metaflux::compiler::ExecutionRequest::Jit);
  if (!expect(interpreted.path == metaflux::compiler::ExecutionPath::Interpreter,
              "explicit interpreter mode must remain distinct") ||
      !expect(cold.path == metaflux::compiler::ExecutionPath::ColdJit,
              "first fixture artifact publication must be cold") ||
      !expect(warm.path == metaflux::compiler::ExecutionPath::WarmJit,
              "second fixture artifact lookup must be warm") ||
      !expect(aot.path == metaflux::compiler::ExecutionPath::Aot,
              "prewarmed fixture artifact must select AOT") ||
      !expect(cold.cache_key == warm.cache_key && cold.cache_key == aot.cache_key,
              "cold, warm, and AOT fixture paths must share compatibility identity")) {
    return false;
  }

  std::vector<std::uint32_t> left{0U, 1U, 0xffffffffU, 8U, 13U};
  std::vector<std::uint32_t> right{4U, 7U, 1U, 9U, 29U};
  const std::vector<std::uint32_t> expected{4U, 8U, 0U, 17U, 42U};
  for (const auto* selection : {&interpreted, &cold, &warm, &aot}) {
    std::vector<std::uint32_t> destination(expected.size(), 0xfeedfaceU);
    if (!expect(run_add(selected_text(*selection, add_ir), destination, left, right, 5U),
                "every Add fixture artifact mode must execute") ||
        !expect(destination == expected, "every Add fixture artifact mode must be bit-exact")) {
      return false;
    }
  }

  metaflux::compiler::FixtureArtifactCache copy_cache;
  const auto copy_interpreter =
      copy_cache.select(identity, copy_ir, metaflux::compiler::ExecutionRequest::Interpreter);
  const auto copy_cold =
      copy_cache.select(identity, copy_ir, metaflux::compiler::ExecutionRequest::Jit);
  const auto copy_warm =
      copy_cache.select(identity, copy_ir, metaflux::compiler::ExecutionRequest::Jit);
  metaflux::compiler::FixtureArtifactCache copy_aot_cache;
  copy_aot_cache.prewarm_aot(identity, copy_ir);
  const auto copy_aot =
      copy_aot_cache.select(identity, copy_ir, metaflux::compiler::ExecutionRequest::Jit);
  std::vector<std::uint32_t> source{0U, 0xffffffffU, 17U, 42U, 99U};
  for (const auto* selection : {&copy_interpreter, &copy_cold, &copy_warm, &copy_aot}) {
    std::vector<std::uint32_t> destination(source.size(), 0U);
    if (!expect(run_copy(selected_text(*selection, copy_ir), destination, source, 5U),
                "every Copy fixture artifact mode must execute") ||
        !expect(destination == source, "every Copy fixture artifact mode must be bit-exact")) {
      return false;
    }
  }
  return true;
}

bool test_randomized_differential(std::string_view add_ir, std::string_view copy_ir) {
  std::mt19937 random(kRandomSeed);
  constexpr std::uint32_t trials = 128;
  for (std::uint32_t trial = 0; trial < trials; ++trial) {
    const auto count = static_cast<std::uint32_t>(random() % 1025U);
    const auto storage_size = static_cast<std::size_t>(count) + 17U;
    std::vector<std::uint32_t> left(storage_size);
    std::vector<std::uint32_t> right(storage_size);
    std::vector<std::uint32_t> add_expected(storage_size, 0xa5a5a5a5U);
    std::vector<std::uint32_t> add_actual(storage_size, 0xa5a5a5a5U);
    std::vector<std::uint32_t> copy_expected(storage_size, 0x5a5a5a5aU);
    std::vector<std::uint32_t> copy_actual(storage_size, 0x5a5a5a5aU);
    for (std::size_t index = 0; index < storage_size; ++index) {
      left[index] = static_cast<std::uint32_t>(random());
      right[index] = static_cast<std::uint32_t>(random());
    }
    const bool add_reference_ok = metaflux::compiler::reference::add_u32(
        std::span<std::uint32_t>(add_expected), std::span<const std::uint32_t>(left),
        std::span<const std::uint32_t>(right), count);
    const bool copy_reference_ok = metaflux::compiler::reference::copy_u32(
        std::span<std::uint32_t>(copy_expected), std::span<const std::uint32_t>(left), count);
    if (!expect(add_reference_ok && copy_reference_ok, "scalar references must accept trial") ||
        !expect(run_add(add_ir, add_actual, left, right, count),
                "CPU Add interpreter must accept randomized trial") ||
        !expect(run_copy(copy_ir, copy_actual, left, count),
                "CPU Copy interpreter must accept randomized trial") ||
        !expect(add_actual == add_expected,
                "CPU Add interpreter must equal the independent scalar reference") ||
        !expect(copy_actual == copy_expected,
                "CPU Copy interpreter must equal the independent scalar reference")) {
      std::cerr << "trial=" << trial << " count=" << count << '\n';
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  const auto add_ir = translate(kAddPtx);
  const auto copy_ir = translate(kCopyPtx);
  if (!expect(!add_ir.empty() && !copy_ir.empty(), "PTX fixtures must translate")) {
    return 1;
  }
  return test_artifact_modes(add_ir, copy_ir) && test_randomized_differential(add_ir, copy_ir) ? 0
                                                                                               : 1;
}
