#include "metaflux/backend/cpu/compiled_kernel.hpp"
#include "metaflux/backend/cpu/compiler.hpp"
#include "metaflux/backend/cpu/interpreter.hpp"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {

using metaflux::backend::cpu::Argument;
using metaflux::backend::cpu::BufferArgument;
using metaflux::backend::cpu::Float32Argument;
using metaflux::backend::cpu::LaunchDimensions;
namespace cpu_compiler = metaflux::backend::cpu::compiler;

constexpr std::uint32_t kRandomSeed = 0x4d465832U;
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

std::string executed_kernel_ir;

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "compiled PTX corpus failure: " << message << '\n';
  }
  return condition;
}

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    std::string pattern = "/tmp/metaflux-compiled-corpus-test-XXXXXX";
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

std::string read_fixture(std::string_view name) {
  std::ifstream input(std::string(METAFLUX_PTX_CORPUS_DIR) + "/" + std::string(name));
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

BufferArgument buffer(std::vector<std::uint32_t>& words, bool writable) {
  return BufferArgument{.words = std::span<std::uint32_t>(words), .writable = writable};
}

Float32Argument f32(std::uint32_t bits) { return Float32Argument{.bits = bits}; }

using BufferSnapshot = std::vector<std::optional<std::vector<std::uint32_t>>>;

BufferSnapshot snapshot(std::span<const Argument> arguments) {
  BufferSnapshot result;
  result.reserve(arguments.size());
  for (const auto& argument : arguments) {
    if (const auto* value = std::get_if<BufferArgument>(&argument); value != nullptr) {
      result.emplace_back(std::vector<std::uint32_t>(value->words.begin(), value->words.end()));
    } else {
      result.emplace_back(std::nullopt);
    }
  }
  return result;
}

void restore(std::span<const Argument> arguments, const BufferSnapshot& values) {
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    if (values[index].has_value()) {
      const auto& destination = std::get<BufferArgument>(arguments[index]).words;
      std::copy(values[index]->begin(), values[index]->end(), destination.begin());
    }
  }
}

bool matches(std::span<const Argument> arguments, const BufferSnapshot& values) {
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    if (values[index].has_value()) {
      const auto& actual = std::get<BufferArgument>(arguments[index]).words;
      if (!std::equal(actual.begin(), actual.end(), values[index]->begin(), values[index]->end())) {
        return false;
      }
    }
  }
  return true;
}

metaflux::compiler::PersistentCacheConfig cache_config(const TemporaryDirectory& temporary,
                                                       std::string_view tier) {
  return {
      .mutable_root = temporary.path() / (std::string(tier) + "-mutable"),
      .aot_root = temporary.path() / (std::string(tier) + "-aot"),
      .compiler_epoch = 1,
      .limits =
          {
              .per_uid_bytes = 128U * 1024U * 1024U,
              .global_bytes = 256U * 1024U * 1024U,
              .maximum_entry_bytes = 16U * 1024U * 1024U,
              .reserved_free_bytes = 0U,
              .reserved_free_percent = 0U,
          },
      .clock = {},
      .inject_fault = {},
      .filesystem_space = {},
  };
}

std::optional<metaflux::backend::cpu::CompiledKernelSignature>
signature_for(const cpu_compiler::PreparedArtifact& artifact) {
  metaflux::backend::cpu::CompiledKernelSignature signature;
  signature.uses_floating_point = artifact.uses_floating_point;
  signature.parameters.reserve(artifact.parameters.size());
  for (const auto parameter : artifact.parameters) {
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

class Harness {
public:
  Harness()
      : jit_cache_(cache_config(temporary_, "jit")),
        aot_cache_(cache_config(temporary_, "administrator")) {}

  [[nodiscard]] bool ready() {
    return expect(temporary_.valid(), "temporary cache root must exist") &&
           expect(jit_cache_.reconcile() == metaflux::compiler::PersistentCacheError::None,
                  "JIT cache reconciliation must succeed") &&
           expect(aot_cache_.reconcile() == metaflux::compiler::PersistentCacheError::None,
                  "AOT cache reconciliation must succeed");
  }

  bool execute_fixture(std::string_view fixture, std::span<const Argument> arguments,
                       LaunchDimensions launch = {}) {
    return execute_source(fixture, read_fixture(fixture), arguments, launch);
  }

  bool execute_source(std::string_view label, std::string_view source,
                      std::span<const Argument> arguments, LaunchDimensions launch) {
    const auto parsed = metaflux::compiler::ptx::parse(source);
    if (!expect(parsed.ok(), std::string(label) + " must parse")) {
      return false;
    }
    const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
    if (!expect(serialized.ok(), std::string(label) + " must serialize")) {
      return false;
    }
    const auto cache_key = metaflux::compiler::make_cache_key(
        cpu_compiler::make_cpu_cache_identity(), serialized.text);
    const bool first_identity = seen_identities_.insert(cache_key).second;

    const auto original = snapshot(arguments);
    const auto interpreted =
        metaflux::backend::cpu::execute_kernel_ir(serialized.text, arguments, launch);
    if (!expect(interpreted.ok(), std::string(label) + " interpreter execution must succeed")) {
      return false;
    }
    const auto oracle = snapshot(arguments);
    restore(arguments, original);

    std::uint32_t cold_compiles = 0;
    const auto cold = cpu_compiler::acquire_artifact(jit_cache_, 1000U, *parsed.kernel, {}, [&] {
      ++cold_compiles;
      return cpu_compiler::compile_kernel(*parsed.kernel);
    });
    const auto warm = cpu_compiler::lookup_cached_artifact(jit_cache_, 1000U, *parsed.kernel);
    std::uint32_t aot_compiles = 0;
    const auto aot = cpu_compiler::prewarm_aot(aot_cache_, *parsed.kernel, {}, [&] {
      ++aot_compiles;
      return cpu_compiler::compile_kernel(*parsed.kernel);
    });
    for (const auto* result : {&cold, &warm, &aot}) {
      if (result->compile_diagnostic.has_value()) {
        std::cerr << cpu_compiler::compile_error_name(result->compile_diagnostic->error) << ": "
                  << result->compile_diagnostic->message << '\n';
      } else if (!result->ok()) {
        std::cerr << metaflux::compiler::persistent_cache_error_name(result->cache_error) << '\n';
      }
    }
    if (!expect(cold.ok(), std::string(label) + " cold compilation must succeed") ||
        !expect(warm.ok(), std::string(label) + " warm lookup must succeed") ||
        !expect(aot.ok(), std::string(label) + " AOT prewarm must succeed") ||
        !expect(cold_compiles == (first_identity ? 1U : 0U) &&
                    aot_compiles == (first_identity ? 1U : 0U),
                std::string(label) + " callbacks must run only for a new kernel identity") ||
        !expect(cold.artifact->mode == (first_identity ? cpu_compiler::ArtifactMode::ColdJit
                                                       : cpu_compiler::ArtifactMode::WarmJit) &&
                    warm.artifact->mode == cpu_compiler::ArtifactMode::WarmJit &&
                    aot.artifact->mode == cpu_compiler::ArtifactMode::AdministratorAot,
                std::string(label) + " must report distinct executable artifact modes") ||
        !expect(cold.artifact->cache_key == warm.artifact->cache_key &&
                    cold.artifact->cache_key == aot.artifact->cache_key &&
                    cold.artifact->elf_sha256 == warm.artifact->elf_sha256 &&
                    cold.artifact->elf_sha256 == aot.artifact->elf_sha256,
                std::string(label) + " modes must share identity and deterministic ELF")) {
      return false;
    }

    const std::array<const cpu_compiler::PreparedArtifact*, 3> artifacts{
        &*cold.artifact, &*warm.artifact, &*aot.artifact};
    for (const auto* artifact : artifacts) {
      const auto signature = signature_for(*artifact);
      auto loaded = metaflux::backend::cpu::load_compiled_kernel(artifact->path,
                                                                 artifact->elf_sha256, *signature);
      if (!expect(loaded.ok(), std::string(label) + " cached ELF must load")) {
        if (!loaded.diagnostic.empty()) {
          std::cerr << loaded.diagnostic << '\n';
        }
        return false;
      }
      const auto result = loaded.kernel.launch(arguments, launch);
      if (!expect(result.ok(), std::string(label) + " cached ELF must execute") ||
          !expect(matches(arguments, oracle),
                  std::string(label) + " cached ELF must equal interpreter buffers")) {
        return false;
      }
      restore(arguments, original);
    }
    restore(arguments, oracle);
    executed_kernel_ir += serialized.text;
    return true;
  }

  bool stress_fused_rounding() {
    constexpr std::string_view kFixture = "positive-fp-forms.ptx";
    const auto parsed = metaflux::compiler::ptx::parse(read_fixture(kFixture));
    if (!expect(parsed.ok(), "fused stress fixture must parse")) {
      return false;
    }
    const auto prepared = cpu_compiler::lookup_cached_artifact(jit_cache_, 1000U, *parsed.kernel);
    if (!expect(prepared.ok(), "fused stress fixture must have a warm executable")) {
      return false;
    }
    const auto signature = signature_for(*prepared.artifact);
    auto loaded = metaflux::backend::cpu::load_compiled_kernel(
        prepared.artifact->path, prepared.artifact->elf_sha256, *signature);
    if (!expect(loaded.ok(), "fused stress ELF must load")) {
      return false;
    }

    std::array<std::vector<std::uint32_t>, 5> outputs{
        std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U),
        std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U),
        std::vector<std::uint32_t>(1U)};
    std::array<Argument, 8> arguments{buffer(outputs[0], true),
                                      buffer(outputs[1], true),
                                      buffer(outputs[2], true),
                                      buffer(outputs[3], true),
                                      buffer(outputs[4], true),
                                      f32(0U),
                                      f32(0U),
                                      f32(0U)};
    const auto run = [&](std::uint32_t left_bits, std::uint32_t right_bits,
                         std::uint32_t addend_bits) {
      arguments[5] = f32(left_bits);
      arguments[6] = f32(right_bits);
      arguments[7] = f32(addend_bits);
      const auto result = loaded.kernel.launch(arguments, {});
      const auto expected = std::bit_cast<std::uint32_t>(
          std::fma(std::bit_cast<float>(left_bits), std::bit_cast<float>(right_bits),
                   std::bit_cast<float>(addend_bits)));
      return result.ok() && outputs[3][0] == expected && outputs[4][0] == expected;
    };

    constexpr std::uint32_t kMidpointLeft = 0x3f800001U;
    constexpr std::uint32_t kMidpointRight = 0x3fc00000U;
    constexpr std::uint32_t kNegativeMinimumSubnormal = 0x80000001U;
    const auto naive = std::bit_cast<std::uint32_t>(
        static_cast<float>(static_cast<double>(std::bit_cast<float>(kMidpointLeft)) *
                               static_cast<double>(std::bit_cast<float>(kMidpointRight)) +
                           static_cast<double>(std::bit_cast<float>(kNegativeMinimumSubnormal))));
    const auto fused = std::bit_cast<std::uint32_t>(
        std::fma(std::bit_cast<float>(kMidpointLeft), std::bit_cast<float>(kMidpointRight),
                 std::bit_cast<float>(kNegativeMinimumSubnormal)));
    if (!expect(naive != fused, "fused stress vector must exercise a double-rounding midpoint") ||
        !expect(run(kMidpointLeft, kMidpointRight, kNegativeMinimumSubnormal),
                "compiled midpoint correction must equal the fused reference")) {
      return false;
    }

    std::mt19937 random(kRandomSeed ^ 0xf32f32U);
    const auto finite_bits = [&] {
      std::uint32_t bits = 0U;
      do {
        bits = static_cast<std::uint32_t>(random());
      } while ((bits & 0x7f800000U) == 0x7f800000U);
      return bits;
    };
    constexpr std::uint32_t kTrials = 4096U;
    for (std::uint32_t trial = 0; trial < kTrials; ++trial) {
      if (!expect(run(finite_bits(), finite_bits(), finite_bits()),
                  "compiled random fused operation must equal std::fma")) {
        std::cerr << "seed=" << kRandomSeed << " trial=" << trial << '\n';
        return false;
      }
    }
    return true;
  }

private:
  TemporaryDirectory temporary_;
  metaflux::compiler::PersistentArtifactCache jit_cache_;
  metaflux::compiler::PersistentArtifactCache aot_cache_;
  std::unordered_set<std::string> seen_identities_;
};

bool test_add_and_control(Harness& harness) {
  std::vector<std::uint32_t> destination(5U, 0xfeedfaceU);
  std::vector<std::uint32_t> left{0U, 1U, 0xffffffffU, 17U, 9U};
  std::vector<std::uint32_t> right{4U, 8U, 1U, 25U, 3U};
  const std::array<Argument, 4> arguments{buffer(destination, true), buffer(left, false),
                                          buffer(right, false), 5U};
  return harness.execute_fixture("positive-add-copy.ptx", arguments, {.grid_x = 2, .block_x = 4}) &&
         expect(destination == std::vector<std::uint32_t>({4U, 9U, 0U, 42U, 12U}),
                "compiled Add/control fixture must equal the modulo-u32 golden");
}

bool test_integer_forms(Harness& harness) {
  std::vector<std::uint32_t> add(1U), subtract(1U), multiply(1U), mad(1U), compare(1U),
      absolute(1U);
  const std::array<Argument, 9> arguments{buffer(add, true),
                                          buffer(subtract, true),
                                          buffer(multiply, true),
                                          buffer(mad, true),
                                          buffer(compare, true),
                                          buffer(absolute, true),
                                          0xffffffffU,
                                          2U,
                                          5U};
  return harness.execute_fixture("positive-integer-forms.ptx", arguments) &&
         expect(add[0] == 1U && subtract[0] == 0xfffffffdU && multiply[0] == 0xfffffffeU &&
                    mad[0] == 45U && compare[0] == 45U && absolute[0] == 3U,
                "compiled integer forms must match modulo-2^32 goldens, the "
                "mov.u32 immediate must zero-extend, abs.s32 must preserve the "
                "two's-complement magnitude, and the signed compare must select the false arm");
}

bool test_abs_int_min(Harness& harness) {
  std::array<std::vector<std::uint32_t>, 6> outputs{
      std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U),
      std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U),
      std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U)};
  const std::array<Argument, 9> arguments{buffer(outputs[0], true),
                                          buffer(outputs[1], true),
                                          buffer(outputs[2], true),
                                          buffer(outputs[3], true),
                                          buffer(outputs[4], true),
                                          buffer(outputs[5], true),
                                          0x80000000U,
                                          0U,
                                          0U};
  return harness.execute_fixture("positive-integer-forms.ptx", arguments) &&
         expect(outputs[5][0] == 0x80000000U,
                "compiled abs.s32 must preserve the PTX INT_MIN result");
}

bool test_fp_forms(Harness& harness) {
  std::vector<std::uint32_t> add(1U), subtract(1U), multiply(1U), mad(1U), fma(1U);
  const std::array<Argument, 8> arguments{
      buffer(add, true), buffer(subtract, true), buffer(multiply, true), buffer(mad, true),
      buffer(fma, true), f32(0x3fc00000U),       f32(0x40100000U),       f32(0x3f000000U)};
  return harness.execute_fixture("positive-fp-forms.ptx", arguments) &&
         expect(add[0] == 0x40700000U && subtract[0] == 0xbf400000U && multiply[0] == 0x40580000U &&
                    mad[0] == 0x40780000U && fma[0] == 0x40780000U,
                "compiled exact .rn forms must match IEEE-754 binary32 goldens");
}

bool test_conversion_and_predication(Harness& harness) {
  std::vector<std::uint32_t> converted_float(1U), converted_integer(1U);
  std::vector<std::uint32_t> guard(1U, 0xdeadbeefU);
  std::array<Argument, 6> arguments{buffer(converted_float, true),
                                    buffer(converted_integer, true),
                                    buffer(guard, true),
                                    3U,
                                    f32(0x40700000U),
                                    f32(0x40800000U)};
  if (!harness.execute_fixture("positive-convert-predicate.ptx", arguments) ||
      !expect(converted_float[0] == 0x40400000U && converted_integer[0] == 3U && guard[0] == 3U,
              "compiled conversions and true predicate stores must match goldens")) {
    return false;
  }
  guard[0] = 0xdeadbeefU;
  arguments[4] = f32(0x40a00000U);
  return harness.execute_fixture("positive-convert-predicate.ptx", arguments) &&
         expect(guard[0] == 3U, "compiled negated predicate store must execute");
}

bool test_signed_conversion(Harness& harness) {
  std::vector<std::uint32_t> negative(1U), tie(1U), minimum(1U);
  const std::array<Argument, 6> arguments{
      buffer(negative, true), buffer(tie, true), buffer(minimum, true),
      UINT32_C(0xfffffff9),   UINT32_C(0xfeffffff), UINT32_C(0x80000000),
  };
  return harness.execute_fixture("edge-convert-s32.ptx", arguments) &&
         expect(negative[0] == UINT32_C(0xc0e00000),
                "compiled cvt.rn.f32.s32 must preserve exact negative values") &&
         expect(tie[0] == UINT32_C(0xcb800000),
                "compiled cvt.rn.f32.s32 must round a negative tie to even") &&
         expect(minimum[0] == UINT32_C(0xcf000000),
                "compiled cvt.rn.f32.s32 must preserve INT_MIN exactly");
}

bool test_2d_special_registers(Harness& harness) {
  std::vector<std::uint32_t> output(24U, 0xffffffffU);
  const std::array<Argument, 1> arguments{buffer(output, true)};
  if (!harness.execute_fixture("edge-2d-specials.ptx", arguments,
                               {.grid_x = 2, .block_x = 3, .grid_y = 2, .block_y = 2})) {
    return false;
  }
  for (std::uint32_t index = 0; index < output.size(); ++index) {
    if (!expect(output[index] == index, "compiled 2D specials must produce row-major indices")) {
      return false;
    }
  }
  return true;
}

bool test_shared_barrier(Harness& harness) {
  std::vector<std::uint32_t> output(4U, 0U);
  const std::array<Argument, 3> arguments{buffer(output, true), 3U, 1U};
  return harness.execute_fixture("positive-shared-barrier.ptx", arguments,
                                 {.grid_x = 1, .block_x = 4}) &&
         expect(output == std::vector<std::uint32_t>({4U, 3U, 2U, 1U}),
                "compiled barrier must expose prior CTA-shared stores");
}

bool test_edge_oracles(Harness& harness) {
  std::vector<std::uint32_t> wrap(1U, 0U);
  const std::array<Argument, 3> wrap_arguments{buffer(wrap, true), 0xffffffffU, 2U};
  if (!harness.execute_fixture("edge-u32-wrap.ptx", wrap_arguments) ||
      !expect(wrap[0] == 1U, "compiled u32 edge must wrap")) {
    return false;
  }
  std::vector<std::uint32_t> rounded(1U, 0U);
  std::vector<std::uint32_t> one{0x3f800000U};
  const std::array<Argument, 4> rounding_arguments{buffer(rounded, true), buffer(one, false),
                                                   16777217U, f32(0x00000000U)};
  if (!harness.execute_fixture("edge-fp-rn.ptx", rounding_arguments) ||
      !expect(rounded[0] == 0x4b800000U, "compiled binary32 tie must round to even")) {
    return false;
  }
  std::vector<std::uint32_t> ordered_nan(1U, 0x12345678U);
  const std::array<Argument, 4> nan_arguments{buffer(ordered_nan, true), f32(0x7fc00000U),
                                              f32(0x3f800000U), 42U};
  return harness.execute_fixture("edge-ordered-nan.ptx", nan_arguments) &&
         expect(ordered_nan[0] == 0x12345678U,
                "compiled ordered comparison with quiet NaN must suppress the store");
}

bool test_random_add_copy(Harness& harness) {
  constexpr std::uint32_t kCount = 1001U;
  constexpr std::size_t kStorage = 1024U;
  std::mt19937 random(kRandomSeed);
  std::vector<std::uint32_t> left(kStorage), right(kStorage), source(kStorage);
  for (std::size_t index = 0; index < kStorage; ++index) {
    left[index] = static_cast<std::uint32_t>(random());
    right[index] = static_cast<std::uint32_t>(random());
    source[index] = static_cast<std::uint32_t>(random());
  }
  std::vector<std::uint32_t> add(kStorage, 0xa5a5a5a5U);
  const std::array<Argument, 4> add_arguments{buffer(add, true), buffer(left, false),
                                              buffer(right, false), kCount};
  if (!harness.execute_fixture("positive-add-copy.ptx", add_arguments,
                               {.grid_x = 4, .block_x = 256})) {
    return false;
  }
  for (std::size_t index = 0; index < kStorage; ++index) {
    const auto expected = index < kCount ? left[index] + right[index] : 0xa5a5a5a5U;
    if (!expect(add[index] == expected, "random compiled Add must match scalar modulo oracle")) {
      return false;
    }
  }

  std::vector<std::uint32_t> copy(kStorage, 0x5a5a5a5aU);
  const std::array<Argument, 3> copy_arguments{buffer(copy, true), buffer(source, false), kCount};
  if (!harness.execute_source("copy-u32", kCopyPtx, copy_arguments,
                              {.grid_x = 4, .block_x = 256})) {
    return false;
  }
  for (std::size_t index = 0; index < kStorage; ++index) {
    const auto expected = index < kCount ? source[index] : 0x5a5a5a5aU;
    if (!expect(copy[index] == expected, "random compiled Copy must match scalar oracle")) {
      return false;
    }
  }
  return true;
}

bool test_exp_compiled(Harness& harness) {
  std::vector<std::uint32_t> exponential(1U, 0U), source(1U, 0x3f000000U), unused(1U);
  const std::array<Argument, 4> arguments{buffer(exponential, true), buffer(source, false),
                                          buffer(unused, true), 1U};
  if (!harness.execute_fixture("exp-f32.ptx", arguments) ||
      !expect(exponential[0] == 0x3fd3094cU,
              "compiled expf must match the independent golden bits for 0.5")) {
    return false;
  }
  const auto matches_oracle = [](std::uint32_t actual, std::uint32_t expected) {
    const auto distance = actual > expected ? actual - expected : expected - actual;
    return expected > 0x7f800000U
               ? (actual & 0x7fffffffU) > 0x7f800000U
               : expected == 0U || expected == 0x7f800000U
                     ? actual == expected
                     : actual != 0U && actual < 0x7f800000U && distance <= 2U;
  };
  if (!expect(!matches_oracle(0U, 1U) && !matches_oracle(1U, 0U),
              "exponential oracle must reject zero/nonzero classification drift")) {
    return false;
  }
  std::istringstream rows(read_fixture("edge-expf-rounding.ptx"));
  std::string line;
  std::size_t count = 0;
  while (std::getline(rows, line)) {
    if (!line.starts_with("// oracle ")) {
      continue;
    }
    std::uint32_t input = 0, expected = 0;
    std::istringstream values(line.substr(10));
    if (!expect(static_cast<bool>(values >> std::hex >> input >> expected),
                "exponential oracle row must contain input and result bits")) {
      return false;
    }
    const std::array<Argument, 2> edge{buffer(exponential, true), f32(input)};
    if (!harness.execute_fixture("edge-expf-rounding.ptx", edge)) {
      return false;
    }
    const auto actual = exponential[0];
    const bool correct = matches_oracle(actual, expected);
    if (!expect(correct, "compiled exponential must satisfy its independent accuracy/class oracle")) {
      std::cerr << std::hex << "input=" << input << " actual=" << actual
                << " expected=" << expected << std::dec << '\n';
      return false;
    }
    ++count;
  }
  return expect(count == 23U, "all exponential boundary rows must pass all four CPU modes");
}

bool test_float_select_compiled(Harness& harness) {
  std::vector<std::uint32_t> selected(1U, 0U), source{0x3f800000U, 0x40000000U};
  const std::array<Argument, 3> arguments{buffer(selected, true), buffer(source, false), 1U};
  return harness.execute_fixture("positive-float-select.ptx", arguments) &&
         expect(selected[0] == 0x40000000U,
                "compiled selp.f32 must select the greater adjacent binary32 value");
}

bool test_compiled_form_coverage() {
  for (const auto& form : metaflux::compiler::ptx::supported_forms()) {
    if (!expect(
            executed_kernel_ir.find("OP " + std::string(form.kernel_ir_op)) != std::string::npos,
            std::string("compiled corpus must execute advertised form: ") + std::string(form.id))) {
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  Harness harness;
  return harness.ready() && test_add_and_control(harness) && test_integer_forms(harness) &&
                 test_abs_int_min(harness) && test_fp_forms(harness) &&
                 harness.stress_fused_rounding() && test_conversion_and_predication(harness) &&
                 test_signed_conversion(harness) && test_2d_special_registers(harness) &&
                 test_shared_barrier(harness) &&
                 test_edge_oracles(harness) && test_random_add_copy(harness) &&
                 test_exp_compiled(harness) && test_float_select_compiled(harness) &&
                 test_compiled_form_coverage()
             ? 0
             : 1;
}
