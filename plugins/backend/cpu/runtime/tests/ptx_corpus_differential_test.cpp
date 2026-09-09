#include "metaflux/backend/cpu/interpreter.hpp"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using metaflux::backend::cpu::Argument;
using metaflux::backend::cpu::BufferArgument;
using metaflux::backend::cpu::Float32Argument;
using metaflux::backend::cpu::LaunchDimensions;

std::string executed_kernel_ir;

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "PTX corpus differential failure: " << message << '\n';
  }
  return condition;
}

std::string read_fixture(std::string_view name) {
  std::ifstream input(std::string(METAFLUX_PTX_CORPUS_DIR) + "/" + std::string(name));
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::string translate(std::string_view fixture) {
  const auto parsed = metaflux::compiler::ptx::parse(read_fixture(fixture));
  if (!parsed.ok()) {
    return {};
  }
  const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
  return serialized.ok() ? serialized.text : std::string{};
}

BufferArgument buffer(std::vector<std::uint32_t>& words, bool writable) {
  return BufferArgument{.words = std::span<std::uint32_t>(words), .writable = writable};
}

Float32Argument f32(std::uint32_t bits) { return Float32Argument{.bits = bits}; }

bool execute(std::string_view fixture, std::span<const Argument> arguments,
             LaunchDimensions launch = {}) {
  const auto kernel_ir = translate(fixture);
  if (!expect(!kernel_ir.empty(),
              std::string("fixture must translate before execution: ") + std::string(fixture))) {
    return false;
  }
  const auto result = metaflux::backend::cpu::execute_kernel_ir(kernel_ir, arguments, launch);
  if (!expect(result.ok(), std::string("fixture must execute: ") + std::string(fixture))) {
    if (result.diagnostic.has_value()) {
      std::cerr << metaflux::backend::cpu::execution_error_name(result.diagnostic->error)
                << " op=" << result.diagnostic->operation << " block=("
                << result.diagnostic->block_x << ',' << result.diagnostic->block_y << ") thread=("
                << result.diagnostic->thread_x << ',' << result.diagnostic->thread_y << ")\n";
    }
    return false;
  }
  executed_kernel_ir += kernel_ir;
  return true;
}

bool test_add_and_control() {
  std::vector<std::uint32_t> destination(5U, 0xfeedfaceU);
  std::vector<std::uint32_t> left{0U, 1U, 0xffffffffU, 17U, 9U};
  std::vector<std::uint32_t> right{4U, 8U, 1U, 25U, 3U};
  const std::array<Argument, 4> arguments{buffer(destination, true), buffer(left, false),
                                          buffer(right, false), 5U};
  return execute("positive-add-copy.ptx", arguments, {.grid_x = 2, .block_x = 4}) &&
         expect(destination == std::vector<std::uint32_t>({4U, 9U, 0U, 42U, 12U}),
                "Add/control fixture must equal the independent modulo-u32 oracle");
}

bool test_integer_forms() {
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
  return execute("positive-integer-forms.ptx", arguments) &&
         expect(add[0] == 1U, "add.u32 must wrap modulo 2^32") &&
         expect(subtract[0] == 0xfffffffdU, "sub.u32 must wrap modulo 2^32") &&
         expect(multiply[0] == 0xfffffffeU, "mul.lo.u32 must retain low product bits") &&
         expect(mad[0] == 45U, "mad.lo.u32 must retain low a*b+c bits and mov.u32 "
                               "immediate 42 must zero-extend into the addend") &&
         expect(absolute[0] == 3U, "abs.s32 must produce the exact two's-complement magnitude") &&
         expect(compare[0] == 45U, "setp.gt.s32 must select the false arm for -3 > 2 and "
                                   "st.global.u8 must store the exact low byte");
}

bool test_abs_int_min() {
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
  return execute("positive-integer-forms.ptx", arguments) &&
         expect(outputs[5][0] == 0x80000000U, "abs.s32 must preserve the PTX INT_MIN result");
}

bool test_fp_forms() {
  std::vector<std::uint32_t> add(1U), subtract(1U), multiply(1U), mad(1U), fma(1U);
  const std::array<Argument, 8> arguments{
      buffer(add, true), buffer(subtract, true), buffer(multiply, true), buffer(mad, true),
      buffer(fma, true), f32(0x3fc00000U),       f32(0x40100000U),       f32(0x3f000000U)};
  return execute("positive-fp-forms.ptx", arguments) &&
         expect(add[0] == 0x40700000U, "add.rn.f32 must produce the golden 3.75 bits") &&
         expect(subtract[0] == 0xbf400000U, "sub.rn.f32 must produce the golden -0.75 bits") &&
         expect(multiply[0] == 0x40580000U, "mul.rn.f32 must produce the golden 3.375 bits") &&
         expect(mad[0] == 0x40780000U && fma[0] == 0x40780000U,
                "mad/fma.rn.f32 must produce the fused golden 3.875 bits");
}

bool test_fp_environment_rejection() {
  std::vector<std::uint32_t> outputs[5] = {
      std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U),
      std::vector<std::uint32_t>(1U), std::vector<std::uint32_t>(1U),
      std::vector<std::uint32_t>(1U)};
  const std::array<Argument, 8> arguments{buffer(outputs[0], true), buffer(outputs[1], true),
                                          buffer(outputs[2], true), buffer(outputs[3], true),
                                          buffer(outputs[4], true), f32(0x3f800000U),
                                          f32(0x40000000U),         f32(0x40400000U)};
  const auto kernel_ir = translate("positive-fp-forms.ptx");
  const auto previous_rounding = std::fegetround();
  if (!expect(!kernel_ir.empty() && previous_rounding != -1,
              "FP environment test requires a translated fixture and readable rounding mode") ||
      !expect(std::fesetround(FE_DOWNWARD) == 0,
              "FP environment test must select a non-advertised rounding mode")) {
    return false;
  }
  const auto result = metaflux::backend::cpu::execute_kernel_ir(kernel_ir, arguments, {});
  const bool restored = std::fesetround(previous_rounding) == 0;
  return expect(restored, "FP environment test must restore the process rounding mode") &&
         expect(!result.ok() && result.diagnostic.has_value() &&
                    result.diagnostic->error ==
                        metaflux::backend::cpu::ExecutionError::UnsupportedFpEnvironment,
                "non-nearest FP environments must receive the stable execution error");
}

bool test_conversion_and_predication() {
  std::vector<std::uint32_t> converted_float(1U), converted_integer(1U), guard(1U, 0xdeadbeefU);
  std::array<Argument, 6> arguments{buffer(converted_float, true),
                                    buffer(converted_integer, true),
                                    buffer(guard, true),
                                    3U,
                                    f32(0x40700000U),
                                    f32(0x40800000U)};
  if (!execute("positive-convert-predicate.ptx", arguments) ||
      !expect(converted_float[0] == 0x40400000U, "cvt.rn.f32.u32 must produce exact 3.0 bits") ||
      !expect(converted_integer[0] == 3U, "cvt.rzi.u32.f32 must truncate 3.75") ||
      !expect(guard[0] == 3U, "true and false store guards must preserve the oracle value")) {
    return false;
  }

  guard[0] = 0xdeadbeefU;
  arguments[4] = f32(0x40a00000U);
  return execute("positive-convert-predicate.ptx", arguments) &&
         expect(guard[0] == 3U, "@!p store must execute when ordered comparison is false");
}

bool test_signed_conversion() {
  std::vector<std::uint32_t> negative(1U), tie(1U), minimum(1U);
  const std::array<Argument, 6> arguments{
      buffer(negative, true), buffer(tie, true), buffer(minimum, true),
      UINT32_C(0xfffffff9),   UINT32_C(0xfeffffff), UINT32_C(0x80000000),
  };
  return execute("edge-convert-s32.ptx", arguments) &&
         expect(negative[0] == UINT32_C(0xc0e00000),
                "cvt.rn.f32.s32 must preserve exact negative values") &&
         expect(tie[0] == UINT32_C(0xcb800000),
                "cvt.rn.f32.s32 must round a negative tie to even") &&
         expect(minimum[0] == UINT32_C(0xcf000000),
                "cvt.rn.f32.s32 must preserve INT_MIN exactly");
}

bool test_2d_special_registers() {
  std::vector<std::uint32_t> output(24U, 0xffffffffU);
  const std::array<Argument, 1> arguments{buffer(output, true)};
  if (!execute("edge-2d-specials.ptx", arguments,
               {.grid_x = 2, .block_x = 3, .grid_y = 2, .block_y = 2})) {
    return false;
  }
  for (std::uint32_t index = 0; index < output.size(); ++index) {
    if (!expect(output[index] == index, "2D special registers must map to the golden flat index")) {
      return false;
    }
  }
  return true;
}

bool test_shared_barrier() {
  std::vector<std::uint32_t> output(4U, 0U);
  const std::array<Argument, 3> arguments{buffer(output, true), 3U, 1U};
  return execute("positive-shared-barrier.ptx", arguments, {.grid_x = 1, .block_x = 4}) &&
         expect(output == std::vector<std::uint32_t>({4U, 3U, 2U, 1U}),
                "bar.sync 0 must expose all prior CTA-shared stores");
}

bool test_edge_oracles() {
  std::vector<std::uint32_t> wrap(1U, 0U);
  const std::array<Argument, 3> wrap_arguments{buffer(wrap, true), 0xffffffffU, 2U};
  if (!execute("edge-u32-wrap.ptx", wrap_arguments) ||
      !expect(wrap[0] == 1U, "u32 edge fixture must wrap exactly")) {
    return false;
  }

  std::vector<std::uint32_t> rounded(1U, 0U);
  std::vector<std::uint32_t> one{0x3f800000U};
  const std::array<Argument, 4> rounding_arguments{buffer(rounded, true), buffer(one, false),
                                                   16777217U, f32(0x00000000U)};
  if (!execute("edge-fp-rn.ptx", rounding_arguments) ||
      !expect(rounded[0] == 0x4b800000U,
              "binary32 tie must round to the even 16777216 representation")) {
    return false;
  }

  std::vector<std::uint32_t> ordered_nan(1U, 0x12345678U);
  const std::array<Argument, 4> nan_arguments{buffer(ordered_nan, true), f32(0x7fc00000U),
                                              f32(0x3f800000U), 42U};
  return execute("edge-ordered-nan.ptx", nan_arguments) &&
         expect(ordered_nan[0] == 0x12345678U,
                "ordered setp.lt.f32 must be false for a quiet NaN operand");
}

bool test_executed_form_coverage() {
  for (const auto& form : metaflux::compiler::ptx::supported_forms()) {
    if (!expect(executed_kernel_ir.find("OP " + std::string(form.kernel_ir_op)) !=
                    std::string::npos,
                std::string("interpreter corpus must execute advertised form: ") +
                    std::string(form.id))) {
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  return test_add_and_control() && test_integer_forms() && test_abs_int_min() && test_fp_forms() &&
                 test_fp_environment_rejection() && test_conversion_and_predication() &&
                 test_signed_conversion() && test_2d_special_registers() &&
                 test_shared_barrier() && test_edge_oracles() &&
                 test_executed_form_coverage()
             ? 0
             : 1;
}
