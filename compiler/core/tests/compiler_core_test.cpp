#include "metaflux/compiler/cache.hpp"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/reference.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using metaflux::compiler::DiagnosticCode;
using metaflux::compiler::Kernel;
using metaflux::compiler::Opcode;
using metaflux::compiler::Operation;
using metaflux::compiler::Parameter;
using metaflux::compiler::ParameterKind;
using metaflux::compiler::Register;
using metaflux::compiler::SpecialRegister;
using metaflux::compiler::ValueKind;

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "compiler core test failure: " << message << '\n';
  }
  return condition;
}

Operation make_operation(Opcode opcode, std::uint32_t result,
                         std::initializer_list<std::uint32_t> inputs, std::uint32_t attribute = 0,
                         bool flag = false) {
  Operation operation{
      .opcode = opcode,
      .result = result,
      .inputs = {},
      .input_count = static_cast<std::uint32_t>(inputs.size()),
      .attribute = attribute,
      .flag = flag,
      .location = {.line = 7, .column = 3},
  };
  std::size_t index = 0;
  for (const auto input : inputs) {
    operation.inputs[index] = input;
    ++index;
  }
  return operation;
}

Kernel make_add_kernel() {
  Kernel kernel{
      .schema_version = metaflux::compiler::kKernelIrSchemaVersion,
      .ptx_major = 9,
      .ptx_minor = 0,
      .name = "add_u32",
      .parameters =
          {
              Parameter{.kind = ParameterKind::BufferU32},
              Parameter{.kind = ParameterKind::BufferU32},
              Parameter{.kind = ParameterKind::BufferU32},
              Parameter{.kind = ParameterKind::ScalarU32},
          },
      .shared_allocations = {},
      .registers =
          {
              Register{.kind = ValueKind::GlobalAddress},
              Register{.kind = ValueKind::GlobalAddress},
              Register{.kind = ValueKind::GlobalAddress},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::Predicate},
              Register{.kind = ValueKind::U64},
              Register{.kind = ValueKind::GlobalAddress},
              Register{.kind = ValueKind::GlobalAddress},
              Register{.kind = ValueKind::GlobalAddress},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::U32},
              Register{.kind = ValueKind::U32},
          },
      .operations = {},
  };
  kernel.operations = {
      make_operation(Opcode::LoadParameterAddress, 0, {}, 0),
      make_operation(Opcode::LoadParameterAddress, 1, {}, 1),
      make_operation(Opcode::LoadParameterAddress, 2, {}, 2),
      make_operation(Opcode::LoadParameterU32, 3, {}, 3),
      make_operation(Opcode::MoveSpecialU32, 4, {},
                     static_cast<std::uint32_t>(SpecialRegister::ThreadIdX)),
      make_operation(Opcode::MoveSpecialU32, 5, {},
                     static_cast<std::uint32_t>(SpecialRegister::BlockIdX)),
      make_operation(Opcode::MoveSpecialU32, 6, {},
                     static_cast<std::uint32_t>(SpecialRegister::BlockDimX)),
      make_operation(Opcode::MadLoU32, 7, {5, 6, 4}),
      make_operation(Opcode::SetPredicateGeU32, 8, {7, 3}),
      make_operation(Opcode::BranchIf, metaflux::compiler::kNoValue, {8}, 18),
      make_operation(Opcode::MultiplyWideU32, 9, {7}, 4),
      make_operation(Opcode::AddGlobalAddress, 10, {0, 9}),
      make_operation(Opcode::AddGlobalAddress, 11, {1, 9}),
      make_operation(Opcode::AddGlobalAddress, 12, {2, 9}),
      make_operation(Opcode::LoadGlobalU32, 13, {11}),
      make_operation(Opcode::LoadGlobalU32, 14, {12}),
      make_operation(Opcode::AddU32, 15, {13, 14}),
      make_operation(Opcode::StoreGlobalU32, metaflux::compiler::kNoValue, {10, 15}),
      make_operation(Opcode::Return, metaflux::compiler::kNoValue, {}),
  };
  return kernel;
}

metaflux::compiler::CacheIdentity cache_identity() {
  return metaflux::compiler::CacheIdentity{
      .toolchain_fingerprint = "llvm-22.1.8-fixture",
      .compiler_epoch = 1,
      .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
      .pass_pipeline = "kir-canonicalize,cpu-scalar-v1",
      .target_triple = "x86_64-unknown-linux-gnu",
      .cpu_name = "x86-64-v2",
      .canonical_features = {"+sse4.2", "+popcnt"},
      .optimization_level = "O2",
      .fp_semantics = "integer-exact",
      .backend_abi = 1,
      .helper_abi = 1,
      .pgo_id = "none",
  };
}

bool has_code(const std::vector<metaflux::compiler::Diagnostic>& diagnostics, DiagnosticCode code) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.code == code) {
      return true;
    }
  }
  return false;
}

bool test_kernel_ir() {
  auto kernel = make_add_kernel();
  auto diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!expect(diagnostics.empty(), "valid Add Kernel IR must verify")) {
    return false;
  }
  const auto serialized = metaflux::compiler::serialize_kernel(kernel);
  if (!expect(serialized.ok(), "valid Add Kernel IR must serialize") ||
      !expect(serialized.text.starts_with("MFKIR 2\nPTX 9 0\nKERNEL add_u32\n"),
              "serialization must carry the stable schema header") ||
      !expect(serialized.text.ends_with("OP return - 0 0 0 - 0\nEND\n"),
              "serialization must end canonically")) {
    return false;
  }
  const auto repeated = metaflux::compiler::serialize_kernel(kernel);
  if (!expect(repeated.text == serialized.text, "serialization must be deterministic")) {
    return false;
  }

  kernel.schema_version = 1;
  diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!expect(has_code(diagnostics, DiagnosticCode::KernelIrSchemaMismatch),
              "schema mismatch must have a stable diagnostic")) {
    return false;
  }
  kernel = make_add_kernel();
  kernel.registers[15].kind = ValueKind::U64;
  diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!expect(has_code(diagnostics, DiagnosticCode::KernelIrTypeMismatch),
              "result type mismatch must be rejected")) {
    return false;
  }
  kernel = make_add_kernel();
  std::swap(kernel.operations[4], kernel.operations[7]);
  diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!expect(has_code(diagnostics, DiagnosticCode::KernelIrUseBeforeDefinition),
              "use before definition must be rejected")) {
    return false;
  }
  kernel = make_add_kernel();
  kernel.operations[1].result = 0;
  diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!expect(has_code(diagnostics, DiagnosticCode::KernelIrDuplicateDefinition),
              "duplicate SSA definition must be rejected")) {
    return false;
  }
  kernel = make_add_kernel();
  kernel.operations[9].attribute = 4;
  diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!expect(has_code(diagnostics, DiagnosticCode::KernelIrInvalidControlFlow),
              "backward branch must be rejected")) {
    return false;
  }
  kernel = make_add_kernel();
  kernel.operations.pop_back();
  diagnostics = metaflux::compiler::verify_kernel(kernel);
  return expect(has_code(diagnostics, DiagnosticCode::KernelIrMissingReturn),
                "missing return must be rejected");
}

bool test_reference() {
  const std::array<std::uint32_t, 5> left{0U, 1U, 0xffffffffU, 17U, 9U};
  const std::array<std::uint32_t, 5> right{4U, 8U, 1U, 25U, 3U};
  std::array<std::uint32_t, 5> destination{};
  if (!expect(metaflux::compiler::reference::add_u32(destination, left, right, 5),
              "scalar Add reference must accept exact bounds") ||
      !expect(destination == std::array<std::uint32_t, 5>{4U, 9U, 0U, 42U, 12U},
              "scalar Add reference must use modulo-2^32 arithmetic") ||
      !expect(!metaflux::compiler::reference::add_u32(destination, left, right, 6),
              "scalar Add reference must reject out-of-bounds count")) {
    return false;
  }
  destination.fill(0U);
  return expect(metaflux::compiler::reference::copy_u32(destination, left, 5),
                "scalar Copy reference must accept exact bounds") &&
         expect(destination == left, "scalar Copy reference must preserve every bit") &&
         expect(!metaflux::compiler::reference::copy_u32(destination, left, 6),
                "scalar Copy reference must reject out-of-bounds count");
}

bool test_cache_identity_and_modes() {
  if (!expect(metaflux::compiler::make_cache_key({}, {}) ==
                  "mf-cache-v1-c93c1d6a097fa3f6eced97484d7f424b20f1971955c574c1cffc0bf942f79bc9",
              "SHA-256 and empty identity encoding must match an independent known vector")) {
    return false;
  }
  const auto serialized = metaflux::compiler::serialize_kernel(make_add_kernel());
  if (!expect(serialized.ok(), "cache test requires serialized Kernel IR")) {
    return false;
  }
  const auto identity = cache_identity();
  const auto key = metaflux::compiler::make_cache_key(identity, serialized.text);
  constexpr std::string_view expected_key =
      "mf-cache-v1-2f1218d05c82826e016da9881aca5840f52c3c7200969463fac84a7563e04dfe";
  if (key != expected_key) {
    std::cerr << "compiler core cache snapshot actual: " << key << '\n';
  }
  if (!expect(key.size() == 76U && key.starts_with("mf-cache-v1-"),
              "cache key must be a namespaced SHA-256 digest") ||
      !expect(key == expected_key,
              "cache identity encoding must match its qualification snapshot") ||
      !expect(key == metaflux::compiler::make_cache_key(identity, serialized.text),
              "cache key must be repeatable")) {
    return false;
  }

  auto reordered = identity;
  reordered.canonical_features = {"+popcnt", "+sse4.2", "+popcnt"};
  if (!expect(metaflux::compiler::make_cache_key(reordered, serialized.text) == key,
              "feature identity must be sorted and deduplicated")) {
    return false;
  }

  std::set<std::string> changed_keys;
  auto changed = identity;
  changed.toolchain_fingerprint += "-patched";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  ++changed.compiler_epoch;
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  ++changed.kernel_ir_schema;
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.pass_pipeline += ",vectorize";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.target_triple = "x86_64-pc-linux-gnu";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.cpu_name = "znver4";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.canonical_features.push_back("+avx2");
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.optimization_level = "O3";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.fp_semantics = "rn-even-ftz";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  ++changed.backend_abi;
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  ++changed.helper_abi;
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed = identity;
  changed.pgo_id = "profile-1";
  changed_keys.insert(metaflux::compiler::make_cache_key(changed, serialized.text));
  changed_keys.insert(metaflux::compiler::make_cache_key(identity, serialized.text + "\n"));
  if (!expect(changed_keys.size() == 13U && !changed_keys.contains(key),
              "every compatibility field and kernel content must cause a cache miss")) {
    return false;
  }

  metaflux::compiler::FixtureArtifactCache cache;
  const auto interpreted =
      cache.select(identity, serialized.text, metaflux::compiler::ExecutionRequest::Interpreter);
  if (!expect(interpreted.path == metaflux::compiler::ExecutionPath::Interpreter &&
                  !interpreted.artifact.has_value() && cache.size() == 0U,
              "interpreter selection must not publish an artifact")) {
    return false;
  }
  const auto cold =
      cache.select(identity, serialized.text, metaflux::compiler::ExecutionRequest::Jit);
  const auto warm =
      cache.select(identity, serialized.text, metaflux::compiler::ExecutionRequest::Jit);
  if (!expect(cold.path == metaflux::compiler::ExecutionPath::ColdJit && cold.artifact.has_value(),
              "first JIT selection must be a cold publication") ||
      !expect(warm.path == metaflux::compiler::ExecutionPath::WarmJit &&
                  warm.artifact.has_value() && warm.cache_key == cold.cache_key,
              "second JIT selection must reuse the same warm artifact")) {
    return false;
  }

  metaflux::compiler::FixtureArtifactCache aot_cache;
  aot_cache.prewarm_aot(identity, serialized.text);
  const auto aot =
      aot_cache.select(identity, serialized.text, metaflux::compiler::ExecutionRequest::Jit);
  return expect(aot.path == metaflux::compiler::ExecutionPath::Aot && aot.artifact.has_value() &&
                    aot.artifact->kind == metaflux::compiler::ArtifactKind::Aot,
                "prewarmed artifact must select the AOT path without a cold publication");
}

} // namespace

int main() {
  return test_kernel_ir() && test_reference() && test_cache_identity_and_modes() ? 0 : 1;
}
