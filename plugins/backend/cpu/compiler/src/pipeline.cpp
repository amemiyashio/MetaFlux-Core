#include "metaflux/backend/cpu/compiler.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metaflux::backend::cpu::compiler {
namespace {

std::string signature_payload(const CompiledArtifact& artifact) {
  std::string payload = "cpu-single-cta-v3;fp=";
  payload.push_back(artifact.uses_floating_point ? '1' : '0');
  payload += ";params=";
  for (std::size_t index = 0; index < artifact.parameters.size(); ++index) {
    if (index != 0U) {
      payload.push_back(',');
    }
    payload += std::to_string(static_cast<std::uint32_t>(artifact.parameters[index]));
  }
  return payload;
}

std::optional<std::pair<std::vector<metaflux::compiler::ParameterKind>, bool>>
parse_signature(std::string_view payload) {
  constexpr std::string_view kPrefix = "cpu-single-cta-v3;fp=";
  constexpr std::string_view kParameters = ";params=";
  if (!payload.starts_with(kPrefix) || payload.size() < kPrefix.size() + 1U ||
      (payload[kPrefix.size()] != '0' && payload[kPrefix.size()] != '1') ||
      payload.substr(kPrefix.size() + 1U, kParameters.size()) != kParameters) {
    return std::nullopt;
  }
  const bool uses_fp = payload[kPrefix.size()] == '1';
  auto parameters_text = payload.substr(kPrefix.size() + 1U + kParameters.size());
  std::vector<metaflux::compiler::ParameterKind> parameters;
  while (!parameters_text.empty()) {
    const auto comma = parameters_text.find(',');
    const auto token = parameters_text.substr(0U, comma);
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size() ||
        value > static_cast<std::uint32_t>(metaflux::compiler::ParameterKind::ScalarF32)) {
      return std::nullopt;
    }
    parameters.push_back(static_cast<metaflux::compiler::ParameterKind>(value));
    if (comma == std::string_view::npos) {
      parameters_text = {};
    } else {
      parameters_text.remove_prefix(comma + 1U);
    }
  }
  return std::pair{std::move(parameters), uses_fp};
}

bool kernel_uses_floating_point(const metaflux::compiler::Kernel& kernel) {
  return std::any_of(kernel.operations.begin(), kernel.operations.end(), [](const auto& operation) {
    using metaflux::compiler::Opcode;
    switch (operation.opcode) {
    case Opcode::LoadParameterF32:
    case Opcode::AbsF32:
    case Opcode::SqrtRnF32:
    case Opcode::ExpF32:
    case Opcode::AddRnF32:
    case Opcode::SubRnF32:
    case Opcode::DivRnF32:
    case Opcode::MultiplyRnF32:
    case Opcode::MadRnF32:
    case Opcode::FmaRnF32:
    case Opcode::ConvertRnF32U32:
    case Opcode::ConvertRnF32S32:
    case Opcode::ConvertRziU32F32:
    case Opcode::SetPredicateLtF32:
    case Opcode::LoadGlobalF32:
    case Opcode::StoreGlobalF32:
      return true;
    default:
      return false;
    }
  });
}

std::optional<std::pair<std::string, std::string>>
canonical_identity(const metaflux::compiler::Kernel& kernel, const CompileOptions& options) {
  const auto serialized = metaflux::compiler::serialize_kernel(kernel);
  if (!serialized.ok()) {
    return std::nullopt;
  }
  return std::pair{serialized.text, metaflux::compiler::make_cache_key(
                                        make_cpu_cache_identity(options), serialized.text)};
}

ArtifactResult result_from_lookup(metaflux::compiler::PersistentArtifactCache& cache,
                                  metaflux::compiler::PersistentCacheLookup lookup,
                                  std::string cache_key, const metaflux::compiler::Kernel& kernel,
                                  ArtifactMode mutable_mode) {
  if (!lookup.hit()) {
    return {
        .artifact = std::nullopt, .cache_error = lookup.error, .compile_diagnostic = std::nullopt};
  }
  const auto signature = parse_signature(lookup.entry->metadata.payload);
  std::vector<metaflux::compiler::ParameterKind> expected_parameters;
  expected_parameters.reserve(kernel.parameters.size());
  for (const auto& parameter : kernel.parameters) {
    expected_parameters.push_back(parameter.kind);
  }
  if (!signature.has_value() ||
      lookup.entry->metadata.kernel_ir_schema != metaflux::compiler::kKernelIrSchemaVersion ||
      lookup.entry->metadata.helper_abi != kCpuHelperAbiVersion ||
      signature->first != expected_parameters ||
      signature->second != kernel_uses_floating_point(kernel)) {
    if (lookup.entry->tier == metaflux::compiler::PersistentCacheTier::MutableUser) {
      lookup.entry->pin.reset();
      static_cast<void>(cache.invalidate(*lookup.entry));
      return {.artifact = std::nullopt,
              .cache_error = metaflux::compiler::PersistentCacheError::Miss,
              .compile_diagnostic = std::nullopt};
    }
    return {.artifact = std::nullopt,
            .cache_error = metaflux::compiler::PersistentCacheError::MetadataMismatch,
            .compile_diagnostic = std::nullopt};
  }
  const auto mode = lookup.entry->tier == metaflux::compiler::PersistentCacheTier::AdministratorAot
                        ? ArtifactMode::AdministratorAot
                        : mutable_mode;
  return {.artifact =
              PreparedArtifact{
                  .mode = mode,
                  .cache_key = std::move(cache_key),
                  .path = lookup.entry->artifact_path,
                  .elf_sha256 = lookup.entry->metadata.artifact_sha256,
                  .parameters = signature->first,
                  .uses_floating_point = signature->second,
                  .cache_pin = std::move(lookup.entry->pin),
              },
          .cache_error = metaflux::compiler::PersistentCacheError::None,
          .compile_diagnostic = std::nullopt};
}

metaflux::compiler::ArtifactDescriptor descriptor_for(const CompiledArtifact& artifact) {
  return {
      .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
      .helper_abi = kCpuHelperAbiVersion,
      .payload = signature_payload(artifact),
  };
}

std::uint64_t reservation_bound(const metaflux::compiler::Kernel& kernel,
                                std::string_view canonical) {
  constexpr std::uint64_t kMinimum = 1024U * 1024U;
  const auto structural = static_cast<std::uint64_t>(canonical.size()) * 32U +
                          static_cast<std::uint64_t>(kernel.operations.size()) * 1024U +
                          static_cast<std::uint64_t>(kernel.registers.size()) * 128U;
  return std::max(kMinimum, structural);
}

CompileResult invoke_compiler(const metaflux::compiler::Kernel& kernel,
                              const CompileOptions& options, const CompileCallback& callback) {
  return callback ? callback() : compile_kernel(kernel, options);
}

ArtifactResult invalid_kernel_result(std::string message) {
  return {.artifact = std::nullopt,
          .cache_error = metaflux::compiler::PersistentCacheError::MetadataMismatch,
          .compile_diagnostic = CompileDiagnostic{
              .error = CompileError::InvalidKernel,
              .location = {},
              .message = std::move(message),
          }};
}

} // namespace

ArtifactResult lookup_cached_artifact(metaflux::compiler::PersistentArtifactCache& cache,
                                      std::uint32_t uid, const metaflux::compiler::Kernel& kernel,
                                      const CompileOptions& options) {
  const auto identity = canonical_identity(kernel, options);
  if (!identity.has_value()) {
    return invalid_kernel_result("Kernel IR verification failed before cache lookup");
  }
  auto lookup = cache.lookup(uid, identity->second, validate_compiled_elf);
  return result_from_lookup(cache, std::move(lookup), identity->second, kernel,
                            ArtifactMode::WarmJit);
}

ArtifactResult acquire_artifact(metaflux::compiler::PersistentArtifactCache& cache,
                                std::uint32_t uid, const metaflux::compiler::Kernel& kernel,
                                const CompileOptions& options, CompileCallback compile) {
  const auto identity = canonical_identity(kernel, options);
  if (!identity.has_value()) {
    return invalid_kernel_result("Kernel IR verification failed before artifact acquisition");
  }
  auto lookup = cache.lookup(uid, identity->second, validate_compiled_elf);
  if (lookup.hit()) {
    auto result = result_from_lookup(cache, std::move(lookup), identity->second, kernel,
                                     ArtifactMode::WarmJit);
    if (result.cache_error != metaflux::compiler::PersistentCacheError::Miss) {
      return result;
    }
  }

  for (std::uint32_t attempt = 0; attempt < 2U; ++attempt) {
    const auto reservation = cache.reserve(
        uid, identity->second, reservation_bound(kernel, identity->first), options.deadline);
    if (!reservation.ok()) {
      if (reservation.error == metaflux::compiler::PersistentCacheError::EntryAvailable) {
        lookup = cache.lookup(uid, identity->second, validate_compiled_elf);
        auto result = result_from_lookup(cache, std::move(lookup), identity->second, kernel,
                                         ArtifactMode::WarmJit);
        if (result.cache_error == metaflux::compiler::PersistentCacheError::Miss) {
          continue;
        }
        return result;
      }
      return {.artifact = std::nullopt,
              .cache_error = reservation.error,
              .compile_diagnostic = std::nullopt};
    }
    auto compiled = invoke_compiler(kernel, options, compile);
    if (!compiled.ok()) {
      cache.cancel(*reservation.reservation);
      return {.artifact = std::nullopt,
              .cache_error = metaflux::compiler::PersistentCacheError::None,
              .compile_diagnostic = std::move(compiled.diagnostic)};
    }
    const auto publish = cache.publish(*reservation.reservation, compiled.artifact->elf,
                                       descriptor_for(*compiled.artifact));
    if (publish != metaflux::compiler::PersistentCacheError::None) {
      return {.artifact = std::nullopt, .cache_error = publish, .compile_diagnostic = std::nullopt};
    }
    lookup = cache.lookup(uid, identity->second, validate_compiled_elf);
    return result_from_lookup(cache, std::move(lookup), identity->second, kernel,
                              ArtifactMode::ColdJit);
  }
  return {.artifact = std::nullopt,
          .cache_error = metaflux::compiler::PersistentCacheError::MetadataMismatch,
          .compile_diagnostic = std::nullopt};
}

ArtifactResult prewarm_aot(metaflux::compiler::PersistentArtifactCache& cache,
                           const metaflux::compiler::Kernel& kernel, const CompileOptions& options,
                           CompileCallback compile) {
  const auto identity = canonical_identity(kernel, options);
  if (!identity.has_value()) {
    return invalid_kernel_result("Kernel IR verification failed before AOT prewarm");
  }
  auto existing = cache.lookup(0U, identity->second, validate_compiled_elf);
  if (existing.hit() &&
      existing.entry->tier == metaflux::compiler::PersistentCacheTier::AdministratorAot) {
    return result_from_lookup(cache, std::move(existing), identity->second, kernel,
                              ArtifactMode::AdministratorAot);
  }
  auto compiled = invoke_compiler(kernel, options, compile);
  if (!compiled.ok()) {
    return {.artifact = std::nullopt,
            .cache_error = metaflux::compiler::PersistentCacheError::None,
            .compile_diagnostic = std::move(compiled.diagnostic)};
  }
  const auto installed = cache.install_aot(identity->second, compiled.artifact->elf,
                                           descriptor_for(*compiled.artifact));
  if (installed != metaflux::compiler::PersistentCacheError::None) {
    return {.artifact = std::nullopt, .cache_error = installed, .compile_diagnostic = std::nullopt};
  }
  auto lookup = cache.lookup(0U, identity->second, validate_compiled_elf);
  return result_from_lookup(cache, std::move(lookup), identity->second, kernel,
                            ArtifactMode::AdministratorAot);
}

} // namespace metaflux::backend::cpu::compiler
