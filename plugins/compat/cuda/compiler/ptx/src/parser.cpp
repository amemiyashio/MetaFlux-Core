#include "metaflux/compiler/ptx_frontend.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metaflux::compiler::ptx {
namespace {

constexpr std::array<SupportedForm, 36> kSupportedForms{{
    {"ld-param-u64", "ld.param.u64", "b64,param.u64", "param,register", "", 70,
     "load_parameter_address", "bounded global buffer handle"},
    {"ld-param-u32", "ld.param.u32", "b32,param.u32", "param,register", "", 70,
     "load_parameter_u32", "exact u32 parameter bits"},
    {"ld-param-f32", "ld.param.f32", "f32,param.f32", "param,register", "", 70,
     "load_parameter_f32", "exact binary32 parameter bits"},
    {"mov-special-u32", "mov.u32", "b32,special.u32", "special,register", "x|y", 70,
     "move_special_u32", "exact 2D coordinate or extent"},
    {"mov-immediate-u32", "mov.u32", "b32,u32-immediate", "register", "", 70,
     "move_immediate_u32", "exact zero-extended 32-bit immediate"},
    {"mov-shared-address", "mov.u32", "b32,shared-symbol", "shared,register", "", 70,
     "load_shared_address", "CTA-local allocation base"},
    {"add-u32", "add.u32", "u32,u32,u32", "register", "", 70, "add_u32", "modulo 2^32"},
    {"sub-u32", "sub.u32", "u32,u32,u32", "register", "", 70, "sub_u32", "modulo 2^32"},
    {"mul-lo-u32", "mul.lo.u32", "u32,u32,u32", "register", "lo", 70, "multiply_lo_u32",
     "low 32 product bits"},
    {"mad-lo-u32", "mad.lo.u32", "u32,u32,u32,u32", "register", "lo", 70, "mad_lo_u32",
     "low 32 bits of a*b+c"},
    {"mul-wide-u32", "mul.wide.u32", "u64,u32,u32-immediate", "register", "wide", 70,
     "multiply_wide_u32", "exact 64-bit product"},
    {"add-u64-global-address", "add.u64", "global-address,global-address,u64", "global,register",
     "", 70, "add_global_address", "checked global byte offset"},
    {"add-u32-shared-address", "add.u32", "shared-address,shared-address,u32", "shared,register",
     "", 70, "add_shared_address", "checked shared byte offset"},
    {"add-rn-f32", "add.rn.f32", "f32,f32,f32", "register", "rn", 70, "add_rn_f32",
     "binary32 round-nearest-even"},
    {"sub-rn-f32", "sub.rn.f32", "f32,f32,f32", "register", "rn", 70, "sub_rn_f32",
     "binary32 round-nearest-even"},
    {"div-rn-f32", "div.rn.f32", "f32,f32,f32", "register", "rn", 70, "div_rn_f32",
     "binary32 round-nearest-even"},
    {"abs-s32", "abs.s32", "s32,s32", "register", "", 70, "abs_s32",
     "exact two's-complement absolute value"},
    {"abs-f32", "abs.f32", "f32,f32", "register", "", 70, "abs_f32",
     "exact sign-bit clear"},
    {"sqrt-rn-f32", "sqrt.rn.f32", "f32,f32", "register", "rn", 70, "sqrt_rn_f32",
     "binary32 correctly-rounded square root"},
    {"mul-rn-f32", "mul.rn.f32", "f32,f32,f32", "register", "rn", 70, "multiply_rn_f32",
     "binary32 round-nearest-even"},
    {"mad-rn-f32", "mad.rn.f32", "f32,f32,f32,f32", "register", "rn,fused", 70, "mad_rn_f32",
     "single binary32 rounding"},
    {"fma-rn-f32", "fma.rn.f32", "f32,f32,f32,f32", "register", "rn,fused", 70, "fma_rn_f32",
     "single binary32 rounding"},
    {"cvt-rn-f32-u32", "cvt.rn.f32.u32", "f32,u32", "register", "rn", 70, "convert_rn_f32_u32",
     "u32 to binary32 round-nearest-even"},
    {"cvt-rzi-u32-f32", "cvt.rzi.u32.f32", "u32,f32", "register", "rzi", 70, "convert_rzi_u32_f32",
     "binary32 to u32 round-zero"},
    {"setp-ge-u32", "setp.ge.u32", "pred,u32,u32", "register", "ge", 70, "set_predicate_ge_u32",
     "unsigned greater-or-equal"},
    {"setp-eq-u32", "setp.eq.u32", "pred,u32,u32", "register", "eq", 70, "set_predicate_eq_u32",
     "exact equality"},
    {"setp-lt-f32", "setp.lt.f32", "pred,f32,f32", "register", "lt,ordered", 70,
     "set_predicate_lt_f32", "ordered less-than"},
    {"bra-predicate", "@{!}p bra", "pred,forward-return-label", "control,register",
     "predicate,negate", 70, "branch_if", "per-thread final-return guard"},
    {"ld-global-u32", "ld.global.u32", "u32,global-address", "global,register", "", 70,
     "load_global_u32", "checked exact u32 load"},
    {"st-global-u32", "st.global.u32", "global-address,u32", "global,register", "guard:none|@p|@!p",
     70, "store_global_u32", "guarded checked exact u32 store"},
    {"ld-global-f32", "ld.global.f32", "f32,global-address", "global,register", "", 70,
     "load_global_f32", "checked exact binary32 bit load"},
    {"st-global-f32", "st.global.f32", "global-address,f32", "global,register", "", 70,
     "store_global_f32", "exact binary32 bit store"},
    {"ld-shared-u32", "ld.shared.u32", "u32,shared-address", "shared,register", "", 70,
     "load_shared_u32", "checked CTA-local u32 load"},
    {"st-shared-u32", "st.shared.u32", "shared-address,u32", "shared,register", "guard:none|@p|@!p",
     70, "store_shared_u32", "guarded CTA-local u32 store"},
    {"bar-sync-0", "bar.sync 0", "", "shared,control", "barrier-id:0,unconditional", 70,
     "barrier_sync", "full-CTA rendezvous and shared visibility"},
    {"ret", "ret", "", "control", "", 70, "return", "terminates one logical thread"},
}};

enum class TokenKind : std::uint32_t {
  Word,
  Register,
  Number,
  LeftParenthesis,
  RightParenthesis,
  LeftBrace,
  RightBrace,
  LeftBracket,
  RightBracket,
  Comma,
  Semicolon,
  Colon,
  Less,
  Greater,
  At,
  Bang,
  Invalid,
  End,
};

struct Token {
  TokenKind kind = TokenKind::Invalid;
  std::string_view text;
  SourceLocation location{};
};

bool word_start(char character) {
  const auto value = static_cast<unsigned char>(character);
  return std::isalpha(value) != 0 || character == '_' || character == '$' || character == '.';
}

bool word_character(char character) {
  const auto value = static_cast<unsigned char>(character);
  return std::isalnum(value) != 0 || character == '_' || character == '$' || character == '.';
}

class Lexer {
public:
  explicit Lexer(std::string_view source) : source_(source) {}

  std::vector<Token> run() {
    std::vector<Token> tokens;
    while (true) {
      skip_trivia();
      if (unterminated_comment_.has_value()) {
        tokens.push_back(Token{.kind = TokenKind::Invalid,
                               .text = source_.substr(unterminated_comment_offset_),
                               .location = *unterminated_comment_});
        tokens.push_back(Token{.kind = TokenKind::End, .text = {}, .location = location()});
        return tokens;
      }
      if (offset_ == source_.size()) {
        tokens.push_back(Token{.kind = TokenKind::End, .text = {}, .location = location()});
        return tokens;
      }
      tokens.push_back(next_token());
    }
  }

private:
  [[nodiscard]] SourceLocation location() const noexcept {
    return SourceLocation{.line = line_, .column = column_};
  }

  void advance() {
    if (source_[offset_] == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
    ++offset_;
  }

  void skip_trivia() {
    while (offset_ < source_.size()) {
      const auto character = source_[offset_];
      if (std::isspace(static_cast<unsigned char>(character)) != 0) {
        advance();
        continue;
      }
      if (character == '/' && offset_ + 1U < source_.size() && source_[offset_ + 1U] == '/') {
        advance();
        advance();
        while (offset_ < source_.size() && source_[offset_] != '\n') {
          advance();
        }
        continue;
      }
      if (character == '/' && offset_ + 1U < source_.size() && source_[offset_ + 1U] == '*') {
        const auto start = location();
        const auto start_offset = offset_;
        advance();
        advance();
        bool closed = false;
        while (offset_ < source_.size()) {
          if (source_[offset_] == '*' && offset_ + 1U < source_.size() &&
              source_[offset_ + 1U] == '/') {
            advance();
            advance();
            closed = true;
            break;
          }
          advance();
        }
        if (!closed) {
          unterminated_comment_ = start;
          unterminated_comment_offset_ = start_offset;
          return;
        }
        continue;
      }
      return;
    }
  }

  Token punctuation(TokenKind kind) {
    const auto start = offset_;
    const auto start_location = location();
    advance();
    return Token{.kind = kind, .text = source_.substr(start, 1U), .location = start_location};
  }

  Token next_token() {
    const auto start = offset_;
    const auto start_location = location();
    const auto character = source_[offset_];
    switch (character) {
    case '(':
      return punctuation(TokenKind::LeftParenthesis);
    case ')':
      return punctuation(TokenKind::RightParenthesis);
    case '{':
      return punctuation(TokenKind::LeftBrace);
    case '}':
      return punctuation(TokenKind::RightBrace);
    case '[':
      return punctuation(TokenKind::LeftBracket);
    case ']':
      return punctuation(TokenKind::RightBracket);
    case ',':
      return punctuation(TokenKind::Comma);
    case ';':
      return punctuation(TokenKind::Semicolon);
    case ':':
      return punctuation(TokenKind::Colon);
    case '<':
      return punctuation(TokenKind::Less);
    case '>':
      return punctuation(TokenKind::Greater);
    case '@':
      return punctuation(TokenKind::At);
    case '!':
      return punctuation(TokenKind::Bang);
    case '%':
      advance();
      while (offset_ < source_.size() && word_character(source_[offset_])) {
        advance();
      }
      return Token{.kind = offset_ == start + 1U ? TokenKind::Invalid : TokenKind::Register,
                   .text = source_.substr(start, offset_ - start),
                   .location = start_location};
    default:
      break;
    }
    if (word_start(character)) {
      advance();
      while (offset_ < source_.size() && word_character(source_[offset_])) {
        advance();
      }
      return Token{.kind = TokenKind::Word,
                   .text = source_.substr(start, offset_ - start),
                   .location = start_location};
    }
    if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
      bool saw_period = false;
      advance();
      while (offset_ < source_.size()) {
        if (std::isdigit(static_cast<unsigned char>(source_[offset_])) != 0) {
          advance();
        } else if (source_[offset_] == '.' && !saw_period) {
          saw_period = true;
          advance();
        } else {
          break;
        }
      }
      return Token{.kind = TokenKind::Number,
                   .text = source_.substr(start, offset_ - start),
                   .location = start_location};
    }
    advance();
    return Token{
        .kind = TokenKind::Invalid, .text = source_.substr(start, 1U), .location = start_location};
  }

  std::string_view source_;
  std::size_t offset_ = 0;
  std::uint32_t line_ = 1;
  std::uint32_t column_ = 1;
  std::optional<SourceLocation> unterminated_comment_;
  std::size_t unterminated_comment_offset_ = 0;
};

enum class DeclaredRegisterKind : std::uint32_t { Predicate, B32, B64, F32 };

struct RegisterSymbol {
  std::uint32_t index;
  DeclaredRegisterKind declared_kind;
};

struct ParameterSymbol {
  std::uint32_t index;
  ParameterKind kind;
};

struct RegisterReference {
  std::uint32_t index;
  SourceLocation location;
};

struct AnyRegisterReference {
  std::uint32_t index;
  ValueKind kind;
  SourceLocation location;
};

struct Guard {
  std::uint32_t predicate;
  bool negated;
  SourceLocation location;
};

struct PendingBranch {
  std::size_t operation_index;
  std::string label;
  SourceLocation location;
};

class Parser {
public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  ParseResult run() {
    parse_header();
    if (!failed_) {
      parse_entry();
    }
    if (!failed_) {
      resolve_branches();
    }
    if (!failed_) {
      auto verification = verify_kernel(kernel_);
      diagnostics_.insert(diagnostics_.end(), std::make_move_iterator(verification.begin()),
                          std::make_move_iterator(verification.end()));
    }
    if (!diagnostics_.empty()) {
      return ParseResult{.kernel = std::nullopt, .diagnostics = std::move(diagnostics_)};
    }
    return ParseResult{.kernel = std::move(kernel_), .diagnostics = {}};
  }

private:
  const Token& current() const { return tokens_[position_]; }

  const Token& peek(std::size_t distance) const {
    return tokens_[std::min(position_ + distance, tokens_.size() - 1U)];
  }

  void advance() {
    if (position_ + 1U < tokens_.size()) {
      ++position_;
    }
  }

  bool accept(TokenKind kind) {
    if (current().kind != kind) {
      return false;
    }
    advance();
    return true;
  }

  void fail(DiagnosticCode code, const Token& token, std::string message, std::string form = {}) {
    if (failed_) {
      return;
    }
    failed_ = true;
    diagnostics_.push_back(
        Diagnostic{.code = code,
                   .location = token.location,
                   .message = std::move(message),
                   .form = form.empty() ? std::string(token.text) : std::move(form)});
  }

  Token take(TokenKind kind, std::string_view description) {
    const auto token = current();
    if (token.kind != kind) {
      fail(DiagnosticCode::PtxSyntax, token, "expected " + std::string(description));
      return token;
    }
    advance();
    return token;
  }

  Token take_word(std::string_view spelling) {
    const auto token = take(TokenKind::Word, spelling);
    if (!failed_ && token.text != spelling) {
      fail(DiagnosticCode::PtxSyntax, token, "expected '" + std::string(spelling) + "'",
           std::string(token.text));
    }
    return token;
  }

  static std::optional<std::uint32_t> parse_u32(std::string_view text) {
    std::uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
      return std::nullopt;
    }
    return value;
  }

  void parse_header() {
    if (current().kind == TokenKind::Invalid) {
      fail(DiagnosticCode::PtxSyntax, current(), "invalid or unterminated PTX token");
      return;
    }
    take_word(".version");
    const auto version = take(TokenKind::Number, "PTX version");
    if (!failed_ && version.text != "9.0") {
      fail(DiagnosticCode::PtxUnsupportedVersion, version,
           "the milestone-0.1.0.0 manifest is pinned to PTX ISA 9.0", std::string(version.text));
      return;
    }
    take_word(".target");
    const auto target = take(TokenKind::Word, "PTX target");
    if (!failed_ && target.text != "sm_70") {
      fail(DiagnosticCode::PtxUnsupportedTarget, target, "the milestone-0.1.0.0 manifest requires target sm_70",
           std::string(target.text));
      return;
    }
    take_word(".address_size");
    const auto address_size = take(TokenKind::Number, "address size");
    if (!failed_ && address_size.text != "64") {
      fail(DiagnosticCode::PtxTypeMismatch, address_size,
           "the milestone-0.1.0.0 manifest requires 64-bit global addresses", std::string(address_size.text));
    }
  }

  void parse_entry() {
    if (current().kind == TokenKind::Word && current().text == ".visible") {
      advance();
    }
    take_word(".entry");
    const auto name = take(TokenKind::Word, "kernel name");
    kernel_.name = std::string(name.text);
    take(TokenKind::LeftParenthesis, "'('");
    parse_parameters();
    take(TokenKind::RightParenthesis, "')'");
    take(TokenKind::LeftBrace, "'{'");
    parse_body();
    take(TokenKind::RightBrace, "'}'");
    if (!failed_ && current().kind != TokenKind::End) {
      fail(DiagnosticCode::PtxSyntax, current(),
           "only one PTX entry is supported by this frontend slice");
    }
  }

  void parse_parameters() {
    if (current().kind == TokenKind::RightParenthesis) {
      return;
    }
    while (!failed_) {
      const auto directive = take_word(".param");
      const auto type = take(TokenKind::Word, "parameter type");
      ParameterKind kind{};
      if (type.text == ".u64") {
        kind = ParameterKind::BufferU32;
      } else if (type.text == ".u32") {
        kind = ParameterKind::ScalarU32;
      } else if (type.text == ".f32") {
        kind = ParameterKind::ScalarF32;
      } else {
        fail(DiagnosticCode::PtxTypeMismatch, type,
             "only .u64 buffer, .u32 scalar, and .f32 scalar parameters are advertised");
        return;
      }
      const auto name = take(TokenKind::Word, "parameter name");
      if (kernel_.parameters.size() >= 64U) {
        fail(DiagnosticCode::PtxSyntax, name, "parameter count exceeds the Kernel IR v2 bound");
        return;
      }
      const auto index = static_cast<std::uint32_t>(kernel_.parameters.size());
      const auto [unused, inserted] = parameters_.emplace(
          std::string(name.text), ParameterSymbol{.index = index, .kind = kind});
      static_cast<void>(unused);
      if (!inserted) {
        fail(DiagnosticCode::PtxDuplicateSymbol, name, "parameter is declared more than once");
        return;
      }
      kernel_.parameters.push_back(Parameter{.kind = kind, .location = directive.location});
      if (!accept(TokenKind::Comma)) {
        return;
      }
    }
  }

  void parse_body() {
    bool saw_instruction = false;
    while (!failed_ && current().kind != TokenKind::RightBrace &&
           current().kind != TokenKind::End) {
      if (current().kind == TokenKind::Invalid) {
        fail(DiagnosticCode::PtxSyntax, current(), "invalid PTX token");
        return;
      }
      if (current().kind == TokenKind::Word && current().text == ".reg") {
        if (saw_instruction) {
          fail(DiagnosticCode::PtxSyntax, current(),
               "register declarations must precede labels and instructions");
          return;
        }
        parse_register_declaration();
        continue;
      }
      if (current().kind == TokenKind::Word && current().text == ".shared") {
        if (saw_instruction) {
          fail(DiagnosticCode::PtxSyntax, current(),
               "shared declarations must precede labels and instructions");
          return;
        }
        parse_shared_declaration();
        continue;
      }
      if (current().kind == TokenKind::Word && peek(1).kind == TokenKind::Colon) {
        saw_instruction = true;
        parse_label();
        continue;
      }
      saw_instruction = true;
      parse_instruction();
    }
    if (!failed_ && current().kind == TokenKind::End) {
      fail(DiagnosticCode::PtxSyntax, current(), "unterminated kernel body");
    }
  }

  void parse_register_declaration() {
    const auto directive = take_word(".reg");
    const auto type = take(TokenKind::Word, "register type");
    DeclaredRegisterKind declared_kind{};
    ValueKind initial_kind{};
    if (type.text == ".pred") {
      declared_kind = DeclaredRegisterKind::Predicate;
      initial_kind = ValueKind::Predicate;
    } else if (type.text == ".b32" || type.text == ".u32") {
      declared_kind = DeclaredRegisterKind::B32;
      initial_kind = ValueKind::U32;
    } else if (type.text == ".b64" || type.text == ".u64") {
      declared_kind = DeclaredRegisterKind::B64;
      initial_kind = ValueKind::U64;
    } else if (type.text == ".f32") {
      declared_kind = DeclaredRegisterKind::F32;
      initial_kind = ValueKind::F32;
    } else {
      fail(DiagnosticCode::PtxTypeMismatch, type,
           "only .pred, .b32/.u32, .b64/.u64, and .f32 registers are advertised");
      return;
    }
    const auto base = take(TokenKind::Register, "register name");
    std::uint32_t count = 1;
    bool expanded = false;
    if (accept(TokenKind::Less)) {
      expanded = true;
      const auto count_token = take(TokenKind::Number, "register vector count");
      const auto parsed_count = parse_u32(count_token.text);
      if (!parsed_count.has_value() || *parsed_count == 0U || *parsed_count > 4096U) {
        fail(DiagnosticCode::PtxSyntax, count_token, "register vector count must be in [1, 4096]");
        return;
      }
      count = *parsed_count;
      take(TokenKind::Greater, "'>'");
    }
    take(TokenKind::Semicolon, "';'");
    if (failed_) {
      return;
    }
    if (kernel_.registers.size() + count > 4096U) {
      fail(DiagnosticCode::PtxSyntax, base,
           "expanded register count exceeds the Kernel IR v2 bound");
      return;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
      auto name = std::string(base.text);
      if (expanded) {
        name += std::to_string(index);
      }
      const auto register_index = static_cast<std::uint32_t>(kernel_.registers.size());
      const auto [unused, inserted] = registers_.emplace(
          name, RegisterSymbol{.index = register_index, .declared_kind = declared_kind});
      static_cast<void>(unused);
      if (!inserted) {
        fail(DiagnosticCode::PtxDuplicateSymbol, base, "register is declared more than once",
             std::move(name));
        return;
      }
      kernel_.registers.push_back(Register{.kind = initial_kind, .location = directive.location});
      register_defined_.push_back(false);
    }
  }

  void parse_shared_declaration() {
    const auto directive = take_word(".shared");
    take_word(".align");
    const auto alignment = take(TokenKind::Number, "shared alignment");
    take_word(".u32");
    const auto name = take(TokenKind::Word, "shared allocation name");
    take(TokenKind::LeftBracket, "'['");
    const auto count_token = take(TokenKind::Number, "shared u32 element count");
    take(TokenKind::RightBracket, "']'");
    take(TokenKind::Semicolon, "';'");
    const auto count = parse_u32(count_token.text);
    if (failed_) {
      return;
    }
    if (alignment.text != "4") {
      fail(DiagnosticCode::PtxTypeMismatch, alignment,
           "static shared u32 allocations require .align 4");
      return;
    }
    if (!count.has_value() || *count == 0U || *count > 12288U) {
      fail(DiagnosticCode::PtxSyntax, count_token,
           "static shared allocation must contain [1, 12288] u32 words");
      return;
    }
    const auto index = static_cast<std::uint32_t>(kernel_.shared_allocations.size());
    const auto [unused, inserted] = shared_.emplace(std::string(name.text), index);
    static_cast<void>(unused);
    if (!inserted) {
      fail(DiagnosticCode::PtxDuplicateSymbol, name,
           "shared allocation is declared more than once");
      return;
    }
    kernel_.shared_allocations.push_back(
        SharedAllocation{.words = *count, .location = directive.location});
  }

  void parse_label() {
    const auto label = take(TokenKind::Word, "label");
    take(TokenKind::Colon, "':'");
    const auto [unused, inserted] =
        labels_.emplace(std::string(label.text), kernel_.operations.size());
    static_cast<void>(unused);
    if (!inserted) {
      fail(DiagnosticCode::PtxDuplicateSymbol, label, "branch label is declared more than once");
    }
  }

  std::optional<RegisterReference> take_register(DeclaredRegisterKind expected) {
    const auto token = take(TokenKind::Register, "register operand");
    if (failed_) {
      return std::nullopt;
    }
    const auto found = registers_.find(std::string(token.text));
    if (found == registers_.end()) {
      fail(DiagnosticCode::PtxUnknownSymbol, token, "register is not declared");
      return std::nullopt;
    }
    if (found->second.declared_kind != expected) {
      fail(DiagnosticCode::PtxTypeMismatch, token,
           "register class does not match the advertised instruction form");
      return std::nullopt;
    }
    return RegisterReference{.index = found->second.index, .location = token.location};
  }

  std::optional<RegisterReference> take_source(DeclaredRegisterKind declared_kind,
                                               ValueKind value_kind) {
    const auto reference = take_register(declared_kind);
    if (!reference.has_value()) {
      return std::nullopt;
    }
    if (!register_defined_[reference->index]) {
      fail(DiagnosticCode::PtxUnknownSymbol,
           Token{.kind = TokenKind::Register, .text = {}, .location = reference->location},
           "register is read before it is defined");
      return std::nullopt;
    }
    if (kernel_.registers[reference->index].kind != value_kind) {
      fail(DiagnosticCode::PtxTypeMismatch,
           Token{.kind = TokenKind::Register, .text = {}, .location = reference->location},
           "register value provenance does not match the advertised instruction form");
      return std::nullopt;
    }
    return reference;
  }

  std::optional<AnyRegisterReference> take_source_any(DeclaredRegisterKind declared_kind) {
    const auto reference = take_register(declared_kind);
    if (!reference.has_value()) {
      return std::nullopt;
    }
    if (!register_defined_[reference->index]) {
      fail(DiagnosticCode::PtxUnknownSymbol,
           Token{.kind = TokenKind::Register, .text = {}, .location = reference->location},
           "register is read before it is defined");
      return std::nullopt;
    }
    return AnyRegisterReference{.index = reference->index,
                                .kind = kernel_.registers[reference->index].kind,
                                .location = reference->location};
  }

  std::optional<RegisterReference> take_result(DeclaredRegisterKind declared_kind,
                                               ValueKind value_kind) {
    const auto reference = take_register(declared_kind);
    if (!reference.has_value()) {
      return std::nullopt;
    }
    if (!define_result(*reference, value_kind)) {
      return std::nullopt;
    }
    return reference;
  }

  bool define_result(const RegisterReference& reference, ValueKind value_kind) {
    if (register_defined_[reference.index]) {
      fail(DiagnosticCode::PtxUnsupportedInstruction,
           Token{.kind = TokenKind::Register, .text = {}, .location = reference.location},
           "the Kernel IR v2 slice requires single-assignment PTX registers");
      return false;
    }
    register_defined_[reference.index] = true;
    kernel_.registers[reference.index].kind = value_kind;
    return true;
  }

  void comma() { take(TokenKind::Comma, "','"); }
  void semicolon() { take(TokenKind::Semicolon, "';'"); }

  void add_operation(Opcode opcode, std::uint32_t result,
                     std::initializer_list<std::uint32_t> inputs, std::uint32_t attribute,
                     bool flag, SourceLocation location,
                     std::optional<Guard> guard = std::nullopt) {
    Operation operation{.opcode = opcode,
                        .result = result,
                        .inputs = {},
                        .input_count = static_cast<std::uint32_t>(inputs.size()),
                        .attribute = attribute,
                        .flag = flag,
                        .predicate = guard.has_value() ? guard->predicate : kNoValue,
                        .predicate_negated = guard.has_value() && guard->negated,
                        .location = location};
    std::size_t index = 0;
    for (const auto input : inputs) {
      operation.inputs[index++] = input;
    }
    kernel_.operations.push_back(operation);
  }

  std::optional<Guard> parse_guard() {
    if (!accept(TokenKind::At)) {
      return std::nullopt;
    }
    const auto location = tokens_[position_ - 1U].location;
    const bool negated = accept(TokenKind::Bang);
    const auto predicate = take_source(DeclaredRegisterKind::Predicate, ValueKind::Predicate);
    if (!predicate.has_value()) {
      return std::nullopt;
    }
    return Guard{.predicate = predicate->index, .negated = negated, .location = location};
  }

  void parse_instruction() {
    const bool had_guard = current().kind == TokenKind::At;
    const auto guard = parse_guard();
    if (failed_) {
      return;
    }
    const auto opcode = take(TokenKind::Word, "instruction opcode");
    if (failed_) {
      return;
    }
    if (opcode.text == "bra") {
      if (!had_guard || !guard.has_value()) {
        fail(DiagnosticCode::PtxUnsupportedInstruction, opcode,
             "only predicate-guarded forward branches are advertised", std::string(opcode.text));
        return;
      }
      parse_branch(opcode, *guard);
      return;
    }
    const bool guard_allowed = opcode.text == "st.global.u32" || opcode.text == "st.shared.u32";
    if (had_guard && !guard_allowed) {
      fail(DiagnosticCode::PtxUnsupportedInstruction, opcode,
           "this instruction form does not advertise predication", std::string(opcode.text));
      return;
    }

    if (opcode.text == "ld.param.u64") {
      parse_load_parameter(opcode, ParameterKind::BufferU32);
    } else if (opcode.text == "ld.param.u32") {
      parse_load_parameter(opcode, ParameterKind::ScalarU32);
    } else if (opcode.text == "ld.param.f32") {
      parse_load_parameter(opcode, ParameterKind::ScalarF32);
    } else if (opcode.text == "mov.u32") {
      parse_move(opcode);
    } else if (opcode.text == "add.u32") {
      parse_add_u32(opcode);
    } else if (opcode.text == "sub.u32") {
      parse_binary_u32(opcode, Opcode::SubU32);
    } else if (opcode.text == "mul.lo.u32") {
      parse_binary_u32(opcode, Opcode::MultiplyLoU32);
    } else if (opcode.text == "mad.lo.u32") {
      parse_ternary(opcode, Opcode::MadLoU32, DeclaredRegisterKind::B32, ValueKind::U32);
    } else if (opcode.text == "mul.wide.u32") {
      parse_multiply_wide(opcode);
    } else if (opcode.text == "add.u64") {
      parse_add_global_address(opcode);
    } else if (opcode.text == "add.rn.f32") {
      parse_binary_f32(opcode, Opcode::AddRnF32);
    } else if (opcode.text == "sub.rn.f32") {
      parse_binary_f32(opcode, Opcode::SubRnF32);
    } else if (opcode.text == "div.rn.f32") {
      parse_binary_f32(opcode, Opcode::DivRnF32);
    } else if (opcode.text == "abs.f32") {
      parse_unary_f32(opcode, Opcode::AbsF32);
    } else if (opcode.text == "sqrt.rn.f32") {
      parse_unary_f32(opcode, Opcode::SqrtRnF32);
    } else if (opcode.text == "abs.s32") {
      parse_unary_u32(opcode, Opcode::AbsS32);
    } else if (opcode.text == "mul.rn.f32") {
      parse_binary_f32(opcode, Opcode::MultiplyRnF32);
    } else if (opcode.text == "mad.rn.f32") {
      parse_ternary(opcode, Opcode::MadRnF32, DeclaredRegisterKind::F32, ValueKind::F32);
    } else if (opcode.text == "fma.rn.f32") {
      parse_ternary(opcode, Opcode::FmaRnF32, DeclaredRegisterKind::F32, ValueKind::F32);
    } else if (opcode.text == "cvt.rn.f32.u32") {
      parse_conversion(opcode, Opcode::ConvertRnF32U32, DeclaredRegisterKind::F32, ValueKind::F32,
                       DeclaredRegisterKind::B32, ValueKind::U32);
    } else if (opcode.text == "cvt.rzi.u32.f32") {
      parse_conversion(opcode, Opcode::ConvertRziU32F32, DeclaredRegisterKind::B32, ValueKind::U32,
                       DeclaredRegisterKind::F32, ValueKind::F32);
    } else if (opcode.text == "setp.ge.u32") {
      parse_set_predicate(opcode, Opcode::SetPredicateGeU32, DeclaredRegisterKind::B32,
                          ValueKind::U32);
    } else if (opcode.text == "setp.eq.u32") {
      parse_set_predicate(opcode, Opcode::SetPredicateEqU32, DeclaredRegisterKind::B32,
                          ValueKind::U32);
    } else if (opcode.text == "setp.lt.f32") {
      parse_set_predicate(opcode, Opcode::SetPredicateLtF32, DeclaredRegisterKind::F32,
                          ValueKind::F32);
    } else if (opcode.text == "ld.global.u32") {
      parse_load_memory(opcode, Opcode::LoadGlobalU32, DeclaredRegisterKind::B32, ValueKind::U32,
                        DeclaredRegisterKind::B64, ValueKind::GlobalAddress);
    } else if (opcode.text == "st.global.u32") {
      parse_store_memory(opcode, Opcode::StoreGlobalU32, DeclaredRegisterKind::B64,
                         ValueKind::GlobalAddress, DeclaredRegisterKind::B32, ValueKind::U32,
                         guard);
    } else if (opcode.text == "ld.global.f32") {
      parse_load_memory(opcode, Opcode::LoadGlobalF32, DeclaredRegisterKind::F32, ValueKind::F32,
                        DeclaredRegisterKind::B64, ValueKind::GlobalAddress);
    } else if (opcode.text == "st.global.f32") {
      parse_store_memory(opcode, Opcode::StoreGlobalF32, DeclaredRegisterKind::B64,
                         ValueKind::GlobalAddress, DeclaredRegisterKind::F32, ValueKind::F32,
                         guard);
    } else if (opcode.text == "ld.shared.u32") {
      parse_load_memory(opcode, Opcode::LoadSharedU32, DeclaredRegisterKind::B32, ValueKind::U32,
                        DeclaredRegisterKind::B32, ValueKind::SharedAddress);
    } else if (opcode.text == "st.shared.u32") {
      parse_store_memory(opcode, Opcode::StoreSharedU32, DeclaredRegisterKind::B32,
                         ValueKind::SharedAddress, DeclaredRegisterKind::B32, ValueKind::U32,
                         guard);
    } else if (opcode.text == "bar.sync") {
      parse_barrier(opcode);
    } else if (opcode.text == "ret") {
      semicolon();
      if (!failed_) {
        add_operation(Opcode::Return, kNoValue, {}, 0, false, opcode.location);
      }
    } else {
      fail(DiagnosticCode::PtxUnsupportedInstruction, opcode,
           "instruction form is outside the PTX 9.0/sm_70 manifest", std::string(opcode.text));
    }
  }

  void parse_load_parameter(const Token& opcode, ParameterKind kind) {
    DeclaredRegisterKind declared_kind = DeclaredRegisterKind::B32;
    ValueKind value_kind = ValueKind::U32;
    Opcode kernel_opcode = Opcode::LoadParameterU32;
    if (kind == ParameterKind::BufferU32) {
      declared_kind = DeclaredRegisterKind::B64;
      value_kind = ValueKind::GlobalAddress;
      kernel_opcode = Opcode::LoadParameterAddress;
    } else if (kind == ParameterKind::ScalarF32) {
      declared_kind = DeclaredRegisterKind::F32;
      value_kind = ValueKind::F32;
      kernel_opcode = Opcode::LoadParameterF32;
    }
    const auto result = take_result(declared_kind, value_kind);
    comma();
    take(TokenKind::LeftBracket, "'['");
    const auto parameter = take(TokenKind::Word, "parameter name");
    take(TokenKind::RightBracket, "']'");
    semicolon();
    if (failed_ || !result.has_value()) {
      return;
    }
    const auto found = parameters_.find(std::string(parameter.text));
    if (found == parameters_.end()) {
      fail(DiagnosticCode::PtxUnknownSymbol, parameter, "parameter is not declared");
      return;
    }
    if (found->second.kind != kind) {
      fail(DiagnosticCode::PtxTypeMismatch, parameter,
           "parameter type does not match the advertised load form");
      return;
    }
    add_operation(kernel_opcode, result->index, {}, found->second.index, false, opcode.location);
  }

  void parse_move(const Token& opcode) {
    const auto result = take_register(DeclaredRegisterKind::B32);
    comma();
    if (failed_ || !result.has_value()) {
      return;
    }
    if (current().kind == TokenKind::Register) {
      const auto special = take(TokenKind::Register, "special register");
      semicolon();
      const std::array<std::pair<std::string_view, SpecialRegister>, 8> selectors{{
          {"%tid.x", SpecialRegister::ThreadIdX},
          {"%tid.y", SpecialRegister::ThreadIdY},
          {"%ctaid.x", SpecialRegister::BlockIdX},
          {"%ctaid.y", SpecialRegister::BlockIdY},
          {"%ntid.x", SpecialRegister::BlockDimX},
          {"%ntid.y", SpecialRegister::BlockDimY},
          {"%nctaid.x", SpecialRegister::GridDimX},
          {"%nctaid.y", SpecialRegister::GridDimY},
      }};
      const auto found = std::find_if(selectors.begin(), selectors.end(), [&](const auto& entry) {
        return entry.first == special.text;
      });
      if (found == selectors.end()) {
        fail(DiagnosticCode::PtxUnsupportedInstruction, special,
             "only x/y thread, block, block-dimension, and grid-dimension special registers are "
             "advertised");
        return;
      }
      if (!failed_ && define_result(*result, ValueKind::U32)) {
        add_operation(Opcode::MoveSpecialU32, result->index, {},
                      static_cast<std::uint32_t>(found->second), false, opcode.location);
      }
      return;
    }
    if (current().kind == TokenKind::Number) {
      const auto immediate = take(TokenKind::Number, "u32 immediate");
      semicolon();
      const auto value = parse_u32(immediate.text);
      if (!failed_ && !value.has_value()) {
        fail(DiagnosticCode::PtxSyntax, immediate, "immediate is not a u32 value");
        return;
      }
      if (!failed_ && result.has_value() && value.has_value()) {
        if (define_result(*result, ValueKind::U32)) {
          add_operation(Opcode::MoveImmediateU32, result->index, {}, *value, false,
                        opcode.location);
        }
      }
      return;
    }
    const auto symbol = take(TokenKind::Word, "shared allocation name");
    semicolon();
    if (failed_) {
      return;
    }
    const auto found = shared_.find(std::string(symbol.text));
    if (found == shared_.end()) {
      fail(DiagnosticCode::PtxUnknownSymbol, symbol, "shared allocation is not declared");
      return;
    }
    if (define_result(*result, ValueKind::SharedAddress)) {
      add_operation(Opcode::LoadSharedAddress, result->index, {}, found->second, false,
                    opcode.location);
    }
  }

  void parse_binary_u32(const Token& opcode, Opcode kernel_opcode) {
    const auto result = take_result(DeclaredRegisterKind::B32, ValueKind::U32);
    comma();
    const auto left = take_source(DeclaredRegisterKind::B32, ValueKind::U32);
    comma();
    const auto right = take_source(DeclaredRegisterKind::B32, ValueKind::U32);
    semicolon();
    if (!failed_ && result.has_value() && left.has_value() && right.has_value()) {
      add_operation(kernel_opcode, result->index, {left->index, right->index}, 0, false,
                    opcode.location);
    }
  }

  void parse_add_u32(const Token& opcode) {
    const auto result = take_register(DeclaredRegisterKind::B32);
    comma();
    const auto left = take_source_any(DeclaredRegisterKind::B32);
    comma();
    const auto right = take_source_any(DeclaredRegisterKind::B32);
    semicolon();
    if (failed_ || !result.has_value() || !left.has_value() || !right.has_value()) {
      return;
    }
    if (left->kind == ValueKind::U32 && right->kind == ValueKind::U32) {
      if (define_result(*result, ValueKind::U32)) {
        add_operation(Opcode::AddU32, result->index, {left->index, right->index}, 0, false,
                      opcode.location);
      }
      return;
    }
    if (left->kind == ValueKind::SharedAddress && right->kind == ValueKind::U32) {
      if (define_result(*result, ValueKind::SharedAddress)) {
        add_operation(Opcode::AddSharedAddress, result->index, {left->index, right->index}, 0,
                      false, opcode.location);
      }
      return;
    }
    fail(DiagnosticCode::PtxTypeMismatch,
         Token{.kind = TokenKind::Register, .text = {}, .location = left->location},
         "add.u32 accepts u32+u32 or shared-address+u32 in this manifest");
  }

  void parse_ternary(const Token& opcode, Opcode kernel_opcode, DeclaredRegisterKind declared_kind,
                     ValueKind value_kind) {
    const auto result = take_result(declared_kind, value_kind);
    comma();
    const auto left = take_source(declared_kind, value_kind);
    comma();
    const auto right = take_source(declared_kind, value_kind);
    comma();
    const auto addend = take_source(declared_kind, value_kind);
    semicolon();
    if (!failed_ && result.has_value() && left.has_value() && right.has_value() &&
        addend.has_value()) {
      add_operation(kernel_opcode, result->index, {left->index, right->index, addend->index}, 0,
                    false, opcode.location);
    }
  }

  void parse_multiply_wide(const Token& opcode) {
    const auto result = take_result(DeclaredRegisterKind::B64, ValueKind::U64);
    comma();
    const auto input = take_source(DeclaredRegisterKind::B32, ValueKind::U32);
    comma();
    const auto immediate = take(TokenKind::Number, "u32 immediate");
    semicolon();
    const auto value = parse_u32(immediate.text);
    if (!failed_ && !value.has_value()) {
      fail(DiagnosticCode::PtxSyntax, immediate, "immediate is not a u32 value");
      return;
    }
    if (!failed_ && result.has_value() && input.has_value() && value.has_value()) {
      add_operation(Opcode::MultiplyWideU32, result->index, {input->index}, *value, false,
                    opcode.location);
    }
  }

  void parse_add_global_address(const Token& opcode) {
    const auto result = take_result(DeclaredRegisterKind::B64, ValueKind::GlobalAddress);
    comma();
    const auto base = take_source(DeclaredRegisterKind::B64, ValueKind::GlobalAddress);
    comma();
    const auto offset = take_source(DeclaredRegisterKind::B64, ValueKind::U64);
    semicolon();
    if (!failed_ && result.has_value() && base.has_value() && offset.has_value()) {
      add_operation(Opcode::AddGlobalAddress, result->index, {base->index, offset->index}, 0, false,
                    opcode.location);
    }
  }

  void parse_unary_f32(const Token& opcode, Opcode kernel_opcode) {
    const auto result = take_result(DeclaredRegisterKind::F32, ValueKind::F32);
    comma();
    const auto input = take_source(DeclaredRegisterKind::F32, ValueKind::F32);
    semicolon();
    if (!failed_ && result.has_value() && input.has_value()) {
      add_operation(kernel_opcode, result->index, {input->index}, 0, false, opcode.location);
    }
  }

  void parse_unary_u32(const Token& opcode, Opcode kernel_opcode) {
    const auto result = take_result(DeclaredRegisterKind::B32, ValueKind::U32);
    comma();
    const auto input = take_source(DeclaredRegisterKind::B32, ValueKind::U32);
    semicolon();
    if (!failed_ && result.has_value() && input.has_value()) {
      add_operation(kernel_opcode, result->index, {input->index}, 0, false, opcode.location);
    }
  }

  void parse_binary_f32(const Token& opcode, Opcode kernel_opcode) {
    const auto result = take_result(DeclaredRegisterKind::F32, ValueKind::F32);
    comma();
    const auto left = take_source(DeclaredRegisterKind::F32, ValueKind::F32);
    comma();
    const auto right = take_source(DeclaredRegisterKind::F32, ValueKind::F32);
    semicolon();
    if (!failed_ && result.has_value() && left.has_value() && right.has_value()) {
      add_operation(kernel_opcode, result->index, {left->index, right->index}, 0, false,
                    opcode.location);
    }
  }

  void parse_conversion(const Token& opcode, Opcode kernel_opcode,
                        DeclaredRegisterKind result_declared, ValueKind result_kind,
                        DeclaredRegisterKind input_declared, ValueKind input_kind) {
    const auto result = take_result(result_declared, result_kind);
    comma();
    const auto input = take_source(input_declared, input_kind);
    semicolon();
    if (!failed_ && result.has_value() && input.has_value()) {
      add_operation(kernel_opcode, result->index, {input->index}, 0, false, opcode.location);
    }
  }

  void parse_set_predicate(const Token& opcode, Opcode kernel_opcode,
                           DeclaredRegisterKind input_declared, ValueKind input_kind) {
    const auto result = take_result(DeclaredRegisterKind::Predicate, ValueKind::Predicate);
    comma();
    const auto left = take_source(input_declared, input_kind);
    comma();
    const auto right = take_source(input_declared, input_kind);
    semicolon();
    if (!failed_ && result.has_value() && left.has_value() && right.has_value()) {
      add_operation(kernel_opcode, result->index, {left->index, right->index}, 0, false,
                    opcode.location);
    }
  }

  void parse_branch(const Token& opcode, const Guard& guard) {
    const auto label = take(TokenKind::Word, "branch label");
    semicolon();
    if (!failed_) {
      const auto operation_index = kernel_.operations.size();
      add_operation(Opcode::BranchIf, kNoValue, {guard.predicate}, 0, guard.negated,
                    opcode.location);
      pending_branches_.push_back(PendingBranch{.operation_index = operation_index,
                                                .label = std::string(label.text),
                                                .location = label.location});
    }
  }

  void parse_load_memory(const Token& opcode, Opcode kernel_opcode,
                         DeclaredRegisterKind result_declared, ValueKind result_kind,
                         DeclaredRegisterKind address_declared, ValueKind address_kind) {
    const auto result = take_result(result_declared, result_kind);
    comma();
    take(TokenKind::LeftBracket, "'['");
    const auto address = take_source(address_declared, address_kind);
    take(TokenKind::RightBracket, "']'");
    semicolon();
    if (!failed_ && result.has_value() && address.has_value()) {
      add_operation(kernel_opcode, result->index, {address->index}, 0, false, opcode.location);
    }
  }

  void parse_store_memory(const Token& opcode, Opcode kernel_opcode,
                          DeclaredRegisterKind address_declared, ValueKind address_kind,
                          DeclaredRegisterKind value_declared, ValueKind value_kind,
                          std::optional<Guard> guard) {
    take(TokenKind::LeftBracket, "'['");
    const auto address = take_source(address_declared, address_kind);
    take(TokenKind::RightBracket, "']'");
    comma();
    const auto value = take_source(value_declared, value_kind);
    semicolon();
    if (!failed_ && address.has_value() && value.has_value()) {
      add_operation(kernel_opcode, kNoValue, {address->index, value->index}, 0, false,
                    opcode.location, guard);
    }
  }

  void parse_barrier(const Token& opcode) {
    const auto barrier = take(TokenKind::Number, "barrier id");
    semicolon();
    const auto value = parse_u32(barrier.text);
    if (failed_) {
      return;
    }
    if (!value.has_value() || *value != 0U) {
      fail(DiagnosticCode::PtxUnsupportedInstruction, barrier,
           "only the unconditional bar.sync 0 form is advertised", std::string(barrier.text));
      return;
    }
    add_operation(Opcode::BarrierSync, kNoValue, {}, 0, false, opcode.location);
  }

  void resolve_branches() {
    for (const auto& branch : pending_branches_) {
      const auto found = labels_.find(branch.label);
      if (found == labels_.end()) {
        fail(DiagnosticCode::PtxUnknownSymbol,
             Token{.kind = TokenKind::Word, .text = {}, .location = branch.location},
             "branch target label is not declared", branch.label);
        return;
      }
      if (found->second > std::numeric_limits<std::uint32_t>::max()) {
        fail(DiagnosticCode::PtxSyntax,
             Token{.kind = TokenKind::Word, .text = {}, .location = branch.location},
             "branch target exceeds the Kernel IR v2 operation bound", branch.label);
        return;
      }
      kernel_.operations[branch.operation_index].attribute =
          static_cast<std::uint32_t>(found->second);
    }
  }

  std::vector<Token> tokens_;
  std::size_t position_ = 0;
  bool failed_ = false;
  Kernel kernel_;
  std::vector<Diagnostic> diagnostics_;
  std::unordered_map<std::string, ParameterSymbol> parameters_;
  std::unordered_map<std::string, RegisterSymbol> registers_;
  std::unordered_map<std::string, std::uint32_t> shared_;
  std::vector<bool> register_defined_;
  std::unordered_map<std::string, std::size_t> labels_;
  std::vector<PendingBranch> pending_branches_;
};

} // namespace

std::span<const SupportedForm> supported_forms() noexcept { return kSupportedForms; }

ParseResult parse(std::string_view source) {
  if (source.size() > 4U * 1024U * 1024U) {
    return ParseResult{
        .kernel = std::nullopt,
        .diagnostics = {Diagnostic{.code = DiagnosticCode::PtxSyntax,
                                   .location = {},
                                   .message = "PTX source exceeds the 4 MiB parser bound",
                                   .form = {}}},
    };
  }
  return Parser(Lexer(source).run()).run();
}

} // namespace metaflux::compiler::ptx
