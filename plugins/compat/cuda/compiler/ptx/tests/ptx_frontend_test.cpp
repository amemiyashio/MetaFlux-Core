#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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
    std::cerr << "PTX frontend test failure: " << message << '\n';
  }
  return condition;
}

std::string read_text(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::string read_fixture(std::string_view name) {
  return read_text(std::string(METAFLUX_PTX_CORPUS_DIR) + "/" + std::string(name));
}

std::string replace_once(std::string source, std::string_view before, std::string_view after) {
  const auto position = source.find(before);
  if (position != std::string::npos) {
    source.replace(position, before.size(), after);
  }
  return source;
}

void replace_all(std::string& source, std::string_view before, std::string_view after) {
  std::size_t position = 0;
  while ((position = source.find(before, position)) != std::string::npos) {
    source.replace(position, before.size(), after);
    position += after.size();
  }
}

metaflux::compiler::SourceLocation location_of(std::string_view source, std::string_view needle) {
  const auto target = source.find(needle);
  std::uint32_t line = 1;
  std::uint32_t column = 1;
  for (std::size_t index = 0; index < target; ++index) {
    if (source[index] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  return {.line = line, .column = column};
}

bool expect_diagnostic(std::string_view source, metaflux::compiler::DiagnosticCode code,
                       std::optional<metaflux::compiler::SourceLocation> location = std::nullopt) {
  const auto result = metaflux::compiler::ptx::parse(source);
  if (!expect(!result.ok() && !result.diagnostics.empty(), "invalid PTX must be rejected")) {
    return false;
  }
  if (!expect(result.diagnostics.front().code == code, "diagnostic code must be stable")) {
    return false;
  }
  return !location.has_value() || expect(result.diagnostics.front().location == *location,
                                         "diagnostic must identify the exact source token");
}

bool test_manifest() {
  const auto forms = metaflux::compiler::ptx::supported_forms();
  const auto jsonl = read_text(METAFLUX_PTX_FORMS_PATH);
  std::set<std::string_view> ids;
  for (const auto& form : forms) {
    ids.insert(form.id);
    if (!expect(!form.oracle.empty() && !form.kernel_ir_op.empty() && form.minimum_sm == 70U,
                "every form needs a sm_70 oracle and Kernel IR mapping")) {
      return false;
    }
    if (!expect(jsonl.find("\"id\":\"" + std::string(form.id) + "\"") != std::string::npos &&
                    jsonl.find("\"kir_op\":\"" + std::string(form.kernel_ir_op) + "\"") !=
                        std::string::npos,
                "code-level form id and Kernel IR op must exist in forms.jsonl")) {
      return false;
    }
  }
  return expect(forms.size() == 33U && ids.size() == forms.size(),
                "the supported PTX form manifest must be complete and unique");
}

bool test_positive_and_canonical() {
  const auto add = metaflux::compiler::ptx::parse(kAddPtx);
  const auto copy = metaflux::compiler::ptx::parse(kCopyPtx);
  if (!expect(add.ok(), "Add PTX must parse and verify") ||
      !expect(copy.ok(), "Copy PTX must parse and verify")) {
    if (!add.diagnostics.empty()) {
      std::cerr << metaflux::compiler::diagnostic_code_name(add.diagnostics.front().code) << " at "
                << add.diagnostics.front().location.line << ':'
                << add.diagnostics.front().location.column << ' ' << add.diagnostics.front().message
                << '\n';
    }
    return false;
  }
  const auto add_text = metaflux::compiler::serialize_kernel(*add.kernel);
  const auto copy_text = metaflux::compiler::serialize_kernel(*copy.kernel);
  if (!expect(add_text.ok() && copy_text.ok(), "translated kernels must serialize") ||
      !expect(add_text.text.find("OP add_u32") != std::string::npos,
              "Add translation must retain integer addition") ||
      !expect(copy_text.text.find("OP add_u32") == std::string::npos,
              "Copy translation must not invent arithmetic") ||
      !expect(add.kernel->operations.back().location == location_of(kAddPtx, "ret;"),
              "operation locations must survive PTX translation")) {
    return false;
  }

  std::string renamed(kAddPtx);
  replace_all(renamed, "%rd", "%ad");
  replace_all(renamed, "%r", "%x");
  replace_all(renamed, "%p", "%q");
  renamed = "// formatting-only prefix\n\n" + renamed;
  const auto metamorphic = metaflux::compiler::ptx::parse(renamed);
  return expect(metamorphic.ok(), "renamed-register PTX must parse") &&
         expect(metaflux::compiler::serialize_kernel(*metamorphic.kernel).text == add_text.text,
                "register spelling and source locations must not change semantic identity");
}

bool test_corpus_form_coverage() {
  constexpr std::array<std::string_view, 9> positive_fixtures{
      "positive-add-copy.ptx", "positive-integer-forms.ptx",
      "positive-fp-forms.ptx", "positive-convert-predicate.ptx",
      "edge-2d-specials.ptx",  "positive-shared-barrier.ptx",
      "edge-u32-wrap.ptx",     "edge-fp-rn.ptx",
      "edge-ordered-nan.ptx",
  };
  std::string canonical_corpus;
  for (const auto fixture : positive_fixtures) {
    const auto source = read_fixture(fixture);
    const auto parsed = metaflux::compiler::ptx::parse(source);
    if (!expect(parsed.ok(),
                std::string("positive corpus fixture must parse: ") + std::string(fixture))) {
      if (!parsed.diagnostics.empty()) {
        std::cerr << metaflux::compiler::diagnostic_code_name(parsed.diagnostics.front().code)
                  << " at " << parsed.diagnostics.front().location.line << ':'
                  << parsed.diagnostics.front().location.column << ' '
                  << parsed.diagnostics.front().message << '\n';
      }
      return false;
    }
    const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
    if (!expect(serialized.ok(), "positive corpus fixture must verify and serialize")) {
      return false;
    }
    canonical_corpus += serialized.text;
  }
  for (const auto& form : metaflux::compiler::ptx::supported_forms()) {
    if (!expect(canonical_corpus.find("OP " + std::string(form.kernel_ir_op)) != std::string::npos,
                std::string("corpus must translate advertised form: ") + std::string(form.id))) {
      return false;
    }
  }

  using Code = metaflux::compiler::DiagnosticCode;
  const std::array<std::pair<std::string_view, Code>, 8> negative_fixtures{{
      {"malformed-missing-semicolon.ptx", Code::PtxSyntax},
      {"malformed-register-type.ptx", Code::PtxTypeMismatch},
      {"malformed-barrier-id.ptx", Code::PtxUnsupportedInstruction},
      {"unsupported-atomic.ptx", Code::PtxUnsupportedInstruction},
      {"unsupported-ftz.ptx", Code::PtxUnsupportedInstruction},
      {"unsupported-special-z.ptx", Code::PtxUnsupportedInstruction},
      {"unsupported-predicated-arithmetic.ptx", Code::PtxUnsupportedInstruction},
      {"unsupported-predicated-barrier.ptx", Code::PtxUnsupportedInstruction},
  }};
  for (const auto& [fixture, code] : negative_fixtures) {
    if (!expect_diagnostic(read_fixture(fixture), code)) {
      std::cerr << "fixture=" << fixture << '\n';
      return false;
    }
  }
  return true;
}

bool test_negative_diagnostics() {
  auto source = replace_once(std::string(kAddPtx), "add.u32 %r7", "xor.b32 %r7");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxUnsupportedInstruction,
                         location_of(source, "xor.b32"))) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), ".version 9.0", ".version 8.7");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxUnsupportedVersion,
                         location_of(source, "8.7"))) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), ".target sm_70", ".target sm_90");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxUnsupportedTarget,
                         location_of(source, "sm_90"))) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), ".reg .b64 %rd<10>;",
                        ".reg .b64 %rd<10>;\n  .reg .b64 %rd0;");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxDuplicateSymbol)) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), "bra done", "bra missing");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxUnknownSymbol,
                         location_of(source, "missing"))) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), "ld.param.u64 %rd0", "ld.param.u64 %r8");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxTypeMismatch,
                         location_of(source, "%r8"))) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), "add.u32 %r7", "add.u32 %r5");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxUnsupportedInstruction,
                         location_of(source, "%r5, %r5"))) {
    return false;
  }
  source = replace_once(std::string(kAddPtx), "ret;", "ret#;");
  if (!expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxSyntax,
                         location_of(source, "#"))) {
    return false;
  }
  source = std::string(kAddPtx) + "/* unterminated";
  return expect_diagnostic(source, metaflux::compiler::DiagnosticCode::PtxSyntax,
                           location_of(source, "/* unterminated"));
}

} // namespace

int main() {
  return test_manifest() && test_positive_and_canonical() && test_corpus_form_coverage() &&
                 test_negative_diagnostics()
             ? 0
             : 1;
}
