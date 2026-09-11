#include "compiler_worker_protocol.hpp"

#include "metaflux/compiler/cache.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace metaflux::service::compiler_worker_protocol {
namespace {

constexpr std::uint64_t kRequestMagic = 0x3151455257464dULL;
constexpr std::uint64_t kResponseMagic = 0x3150535257464dULL;
constexpr std::uint32_t kProtocolVersion = 1;
constexpr std::uint32_t kResponseSuccess = 0;
constexpr std::uint32_t kResponseFailure = 1;
constexpr std::uint32_t kMaximumCollectionElements = 1024U * 1024U;
constexpr std::size_t kMaximumDiagnosticBytes = 64U * 1024U;

class Encoder final {
public:
  explicit Encoder(std::size_t maximum) : maximum_(maximum) {}

  void append_u8(std::uint8_t value) { append_scalar(value, 1U); }
  void append_u32(std::uint32_t value) { append_scalar(value, 4U); }
  void append_u64(std::uint64_t value) { append_scalar(value, 8U); }

  void append_string(std::string_view value) {
    append_u64(static_cast<std::uint64_t>(value.size()));
    append_bytes(std::as_bytes(std::span(value.data(), value.size())));
  }

  void append_bytes(std::span<const std::byte> value) {
    if (!ok_ || value.size() > maximum_ - bytes_.size()) {
      ok_ = false;
      return;
    }
    bytes_.insert(bytes_.end(), value.begin(), value.end());
  }

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] std::vector<std::byte> take() && { return std::move(bytes_); }

private:
  void append_scalar(std::uint64_t value, std::size_t width) {
    if (!ok_ || width > maximum_ - bytes_.size()) {
      ok_ = false;
      return;
    }
    for (std::size_t index = 0; index < width; ++index) {
      bytes_.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
  }

  std::size_t maximum_;
  std::vector<std::byte> bytes_;
  bool ok_ = true;
};

class Decoder final {
public:
  explicit Decoder(std::span<const std::byte> bytes) : bytes_(bytes) {}

  [[nodiscard]] std::optional<std::uint8_t> read_u8() {
    const auto value = read_scalar(1U);
    return value.has_value() ? std::optional(static_cast<std::uint8_t>(*value)) : std::nullopt;
  }

  [[nodiscard]] std::optional<std::uint32_t> read_u32() {
    const auto value = read_scalar(4U);
    return value.has_value() ? std::optional(static_cast<std::uint32_t>(*value)) : std::nullopt;
  }

  [[nodiscard]] std::optional<std::uint64_t> read_u64() { return read_scalar(8U); }

  [[nodiscard]] std::optional<std::string> read_string(std::size_t maximum) {
    const auto length = read_u64();
    if (!length.has_value() || *length > maximum || *length > remaining()) {
      return std::nullopt;
    }
    const auto size = static_cast<std::size_t>(*length);
    const auto* begin = reinterpret_cast<const char*>(bytes_.data() + offset_);
    std::string result(begin, size);
    offset_ += size;
    return result;
  }

  [[nodiscard]] std::optional<std::vector<std::byte>> read_bytes(std::size_t maximum) {
    const auto length = read_u64();
    if (!length.has_value() || *length > maximum || *length > remaining()) {
      return std::nullopt;
    }
    const auto size = static_cast<std::size_t>(*length);
    std::vector<std::byte> result(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
                                  bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + size));
    offset_ += size;
    return result;
  }

  [[nodiscard]] bool finished() const noexcept { return offset_ == bytes_.size(); }

private:
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

  [[nodiscard]] std::optional<std::uint64_t> read_scalar(std::size_t width) {
    if (width > remaining()) {
      return std::nullopt;
    }
    std::uint64_t result = 0;
    for (std::size_t index = 0; index < width; ++index) {
      result |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + index]))
                << (index * 8U);
    }
    offset_ += width;
    return result;
  }

  std::span<const std::byte> bytes_;
  std::size_t offset_ = 0;
};

[[nodiscard]] bool append_count(Encoder& encoder, std::size_t count) {
  if (count > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  encoder.append_u32(static_cast<std::uint32_t>(count));
  return encoder.ok();
}

[[nodiscard]] bool valid_parameter_kind(std::uint32_t value) {
  return value <= static_cast<std::uint32_t>(compiler::ParameterKind::ScalarF32);
}

[[nodiscard]] bool valid_value_kind(std::uint32_t value) {
  return value <= static_cast<std::uint32_t>(compiler::ValueKind::SharedAddress);
}

[[nodiscard]] bool valid_compile_error(std::uint32_t value) {
  return value <= static_cast<std::uint32_t>(backend::cpu::compiler::CompileError::Io);
}

void append_location(Encoder& encoder, compiler::SourceLocation location) {
  encoder.append_u32(location.line);
  encoder.append_u32(location.column);
}

[[nodiscard]] std::optional<compiler::SourceLocation> read_location(Decoder& decoder) {
  const auto line = decoder.read_u32();
  const auto column = decoder.read_u32();
  if (!line.has_value() || !column.has_value()) {
    return std::nullopt;
  }
  return compiler::SourceLocation{.line = *line, .column = *column};
}

[[nodiscard]] std::optional<std::uint32_t> read_count(Decoder& decoder) {
  const auto count = decoder.read_u32();
  if (!count.has_value() || *count > kMaximumCollectionElements) {
    return std::nullopt;
  }
  return count;
}

} // namespace

std::optional<std::vector<std::byte>> encode_request(const compiler::Kernel& kernel) {
  Encoder encoder(kMaximumRequestBytes);
  encoder.append_u64(kRequestMagic);
  encoder.append_u32(kProtocolVersion);
  encoder.append_u32(kernel.schema_version);
  encoder.append_u32(kernel.ptx_major);
  encoder.append_u32(kernel.ptx_minor);
  encoder.append_string(kernel.name);

  if (!append_count(encoder, kernel.parameters.size())) {
    return std::nullopt;
  }
  for (const auto& parameter : kernel.parameters) {
    encoder.append_u32(static_cast<std::uint32_t>(parameter.kind));
    append_location(encoder, parameter.location);
  }

  if (!append_count(encoder, kernel.shared_allocations.size())) {
    return std::nullopt;
  }
  for (const auto& allocation : kernel.shared_allocations) {
    encoder.append_u32(allocation.words);
    append_location(encoder, allocation.location);
  }

  if (!append_count(encoder, kernel.registers.size())) {
    return std::nullopt;
  }
  for (const auto& reg : kernel.registers) {
    encoder.append_u32(static_cast<std::uint32_t>(reg.kind));
    append_location(encoder, reg.location);
  }

  if (!append_count(encoder, kernel.operations.size())) {
    return std::nullopt;
  }
  for (const auto& operation : kernel.operations) {
    encoder.append_u32(static_cast<std::uint32_t>(operation.opcode));
    encoder.append_u32(operation.result);
    for (const auto input : operation.inputs) {
      encoder.append_u32(input);
    }
    encoder.append_u32(operation.input_count);
    encoder.append_u32(operation.attribute);
    encoder.append_u8(operation.flag ? 1U : 0U);
    encoder.append_u32(operation.predicate);
    encoder.append_u8(operation.predicate_negated ? 1U : 0U);
    append_location(encoder, operation.location);
  }
  if (!encoder.ok()) {
    return std::nullopt;
  }
  return std::move(encoder).take();
}

std::optional<compiler::Kernel> decode_request(std::span<const std::byte> bytes) {
  Decoder decoder(bytes);
  const auto magic = decoder.read_u64();
  const auto version = decoder.read_u32();
  const auto schema = decoder.read_u32();
  const auto ptx_major = decoder.read_u32();
  const auto ptx_minor = decoder.read_u32();
  const auto name = decoder.read_string(1024U * 1024U);
  if (magic != kRequestMagic || version != kProtocolVersion || !schema.has_value() ||
      !ptx_major.has_value() || !ptx_minor.has_value() || !name.has_value()) {
    return std::nullopt;
  }

  compiler::Kernel kernel{
      .schema_version = *schema,
      .ptx_major = *ptx_major,
      .ptx_minor = *ptx_minor,
      .name = *name,
      .parameters = {},
      .shared_allocations = {},
      .registers = {},
      .operations = {},
  };
  const auto parameter_count = read_count(decoder);
  if (!parameter_count.has_value()) {
    return std::nullopt;
  }
  kernel.parameters.reserve(*parameter_count);
  for (std::uint32_t index = 0; index < *parameter_count; ++index) {
    const auto kind = decoder.read_u32();
    const auto location = read_location(decoder);
    if (!kind.has_value() || !valid_parameter_kind(*kind) || !location.has_value()) {
      return std::nullopt;
    }
    kernel.parameters.push_back(compiler::Parameter{
        .kind = static_cast<compiler::ParameterKind>(*kind), .location = *location});
  }

  const auto shared_count = read_count(decoder);
  if (!shared_count.has_value()) {
    return std::nullopt;
  }
  kernel.shared_allocations.reserve(*shared_count);
  for (std::uint32_t index = 0; index < *shared_count; ++index) {
    const auto words = decoder.read_u32();
    const auto location = read_location(decoder);
    if (!words.has_value() || !location.has_value()) {
      return std::nullopt;
    }
    kernel.shared_allocations.push_back(
        compiler::SharedAllocation{.words = *words, .location = *location});
  }

  const auto register_count = read_count(decoder);
  if (!register_count.has_value()) {
    return std::nullopt;
  }
  kernel.registers.reserve(*register_count);
  for (std::uint32_t index = 0; index < *register_count; ++index) {
    const auto kind = decoder.read_u32();
    const auto location = read_location(decoder);
    if (!kind.has_value() || !valid_value_kind(*kind) || !location.has_value()) {
      return std::nullopt;
    }
    kernel.registers.push_back(
        compiler::Register{.kind = static_cast<compiler::ValueKind>(*kind), .location = *location});
  }

  const auto operation_count = read_count(decoder);
  if (!operation_count.has_value()) {
    return std::nullopt;
  }
  kernel.operations.reserve(*operation_count);
  for (std::uint32_t index = 0; index < *operation_count; ++index) {
    const auto opcode = decoder.read_u32();
    const auto result = decoder.read_u32();
    std::array<std::uint32_t, 3> inputs{};
    bool valid = opcode.has_value() &&
                 compiler::is_valid_opcode(static_cast<compiler::Opcode>(*opcode)) &&
                 result.has_value();
    for (auto& input : inputs) {
      const auto value = decoder.read_u32();
      valid = valid && value.has_value();
      input = value.value_or(0U);
    }
    const auto input_count = decoder.read_u32();
    const auto attribute = decoder.read_u32();
    const auto flag = decoder.read_u8();
    const auto predicate = decoder.read_u32();
    const auto predicate_negated = decoder.read_u8();
    const auto location = read_location(decoder);
    if (!valid || !input_count.has_value() || *input_count > inputs.size() ||
        !attribute.has_value() || !flag.has_value() || *flag > 1U || !predicate.has_value() ||
        !predicate_negated.has_value() || *predicate_negated > 1U || !location.has_value()) {
      return std::nullopt;
    }
    kernel.operations.push_back(compiler::Operation{
        .opcode = static_cast<compiler::Opcode>(*opcode),
        .result = *result,
        .inputs = inputs,
        .input_count = *input_count,
        .attribute = *attribute,
        .flag = *flag != 0U,
        .predicate = *predicate,
        .predicate_negated = *predicate_negated != 0U,
        .location = *location,
    });
  }
  if (!decoder.finished()) {
    return std::nullopt;
  }
  return kernel;
}

std::optional<std::vector<std::byte>>
encode_response(const backend::cpu::compiler::CompileResult& result, std::int64_t process_id) {
  if (process_id <= 0) {
    return std::nullopt;
  }
  Encoder encoder(kMaximumResponseBytes);
  encoder.append_u64(kResponseMagic);
  encoder.append_u32(kProtocolVersion);
  encoder.append_u64(static_cast<std::uint64_t>(process_id));
  encoder.append_u32(result.ok() ? kResponseSuccess : kResponseFailure);
  if (result.ok()) {
    const auto& artifact = *result.artifact;
    encoder.append_u64(static_cast<std::uint64_t>(artifact.elf.size()));
    encoder.append_bytes(artifact.elf);
    encoder.append_string(artifact.elf_sha256);
    if (!append_count(encoder, artifact.parameters.size())) {
      return std::nullopt;
    }
    for (const auto parameter : artifact.parameters) {
      encoder.append_u32(static_cast<std::uint32_t>(parameter));
    }
    encoder.append_u8(artifact.uses_floating_point ? 1U : 0U);
  } else {
    const backend::cpu::compiler::CompileDiagnostic fallback{
        .error = backend::cpu::compiler::CompileError::Io,
        .location = {},
        .message = "compiler worker returned no diagnostic",
    };
    const auto& diagnostic = result.diagnostic.has_value() ? *result.diagnostic : fallback;
    encoder.append_u32(static_cast<std::uint32_t>(diagnostic.error));
    append_location(encoder, diagnostic.location);
    encoder.append_string(diagnostic.message);
  }
  if (!encoder.ok()) {
    return std::nullopt;
  }
  return std::move(encoder).take();
}

std::optional<DecodedResponse> decode_response(std::span<const std::byte> bytes,
                                               std::string& diagnostic) {
  Decoder decoder(bytes);
  const auto magic = decoder.read_u64();
  const auto version = decoder.read_u32();
  const auto process_id = decoder.read_u64();
  const auto kind = decoder.read_u32();
  if (magic != kResponseMagic || version != kProtocolVersion || !process_id.has_value() ||
      *process_id == 0U ||
      *process_id > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
      !kind.has_value()) {
    diagnostic = "compiler worker response header is truncated or malformed";
    return std::nullopt;
  }

  DecodedResponse response;
  response.process_id = static_cast<std::int64_t>(*process_id);
  if (*kind == kResponseSuccess) {
    const auto elf = decoder.read_bytes(kMaximumResponseBytes);
    const auto elf_sha256 = decoder.read_string(64U);
    const auto parameter_count = read_count(decoder);
    if (!elf.has_value() || elf->empty() || !elf_sha256.has_value() || elf_sha256->size() != 64U ||
        !parameter_count.has_value()) {
      diagnostic = "compiler worker success response is truncated or malformed";
      return std::nullopt;
    }
    std::vector<compiler::ParameterKind> parameters;
    parameters.reserve(*parameter_count);
    for (std::uint32_t index = 0; index < *parameter_count; ++index) {
      const auto parameter = decoder.read_u32();
      if (!parameter.has_value() || !valid_parameter_kind(*parameter)) {
        diagnostic = "compiler worker response has an invalid parameter signature";
        return std::nullopt;
      }
      parameters.push_back(static_cast<compiler::ParameterKind>(*parameter));
    }
    const auto uses_floating_point = decoder.read_u8();
    if (!uses_floating_point.has_value() || *uses_floating_point > 1U || !decoder.finished()) {
      diagnostic = "compiler worker success response is truncated or malformed";
      return std::nullopt;
    }
    if (compiler::sha256_hex(*elf) != *elf_sha256) {
      diagnostic = "compiler worker response ELF digest mismatch";
      return std::nullopt;
    }
    response.compilation.artifact = backend::cpu::compiler::CompiledArtifact{
        .elf = std::move(*elf),
        .elf_sha256 = std::move(*elf_sha256),
        .mlir_text = {},
        .llvm_ir_text = {},
        .parameters = std::move(parameters),
        .uses_floating_point = *uses_floating_point != 0U,
    };
  } else if (*kind == kResponseFailure) {
    const auto error = decoder.read_u32();
    const auto location = read_location(decoder);
    const auto message = decoder.read_string(kMaximumDiagnosticBytes);
    if (!error.has_value() || !valid_compile_error(*error) || !location.has_value() ||
        !message.has_value() || !decoder.finished()) {
      diagnostic = "compiler worker failure response is truncated or malformed";
      return std::nullopt;
    }
    response.compilation.diagnostic = backend::cpu::compiler::CompileDiagnostic{
        .error = static_cast<backend::cpu::compiler::CompileError>(*error),
        .location = *location,
        .message = std::move(*message),
    };
  } else {
    diagnostic = "compiler worker response has an unknown result kind";
    return std::nullopt;
  }
  return response;
}

} // namespace metaflux::service::compiler_worker_protocol
