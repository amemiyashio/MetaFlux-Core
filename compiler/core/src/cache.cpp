#include "metaflux/compiler/cache.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace metaflux::compiler {
namespace {

constexpr std::array<std::uint32_t, 64> kSha256Constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U,
};

constexpr std::uint32_t rotate_right(std::uint32_t value, std::uint32_t amount) noexcept {
  return (value >> amount) | (value << (32U - amount));
}

std::string sha256_impl(std::span<const std::byte> input) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(input.size() + 72U);
  for (const auto value : input) {
    bytes.push_back(std::to_integer<std::uint8_t>(value));
  }
  const auto bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
  bytes.push_back(0x80U);
  while (bytes.size() % 64U != 56U) {
    bytes.push_back(0U);
  }
  for (std::uint32_t shift = 56U;; shift -= 8U) {
    bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    if (shift == 0U) {
      break;
    }
  }

  std::array<std::uint32_t, 8> state{
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
  };
  std::array<std::uint32_t, 64> words{};
  for (std::size_t block = 0; block < bytes.size(); block += 64U) {
    for (std::size_t index = 0; index < 16U; ++index) {
      const auto offset = block + index * 4U;
      words[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                     (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                     (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                     static_cast<std::uint32_t>(bytes[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
      const auto value0 = words[index - 15U];
      const auto value1 = words[index - 2U];
      const auto sigma0 = rotate_right(value0, 7U) ^ rotate_right(value0, 18U) ^ (value0 >> 3U);
      const auto sigma1 = rotate_right(value1, 17U) ^ rotate_right(value1, 19U) ^ (value1 >> 10U);
      words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
    }

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];
    auto f = state[5];
    auto g = state[6];
    auto h = state[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
      const auto sum1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
      const auto choose = (e & f) ^ ((~e) & g);
      const auto temporary1 = h + sum1 + choose + kSha256Constants[index] + words[index];
      const auto sum0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
      const auto majority = (a & b) ^ (a & c) ^ (b & c);
      const auto temporary2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
  }

  constexpr char kHexDigits[] = "0123456789abcdef";
  std::string digest;
  digest.reserve(64U);
  for (const auto word : state) {
    for (std::uint32_t shift = 28U;; shift -= 4U) {
      const auto nibble = static_cast<std::size_t>((word >> shift) & 0xfU);
      digest.push_back(kHexDigits[nibble]);
      if (shift == 0U) {
        break;
      }
    }
  }
  return digest;
}

void append_field(std::string& encoding, std::string_view name, std::string_view value) {
  encoding.append(name);
  encoding.push_back(':');
  encoding.append(std::to_string(value.size()));
  encoding.push_back(':');
  encoding.append(value);
  encoding.push_back('\n');
}

void append_number(std::string& encoding, std::string_view name, std::uint32_t value) {
  append_field(encoding, name, std::to_string(value));
}

std::string artifact_digest(std::string_view cache_key, std::string_view canonical_kernel_ir,
                            ArtifactKind kind) {
  std::string encoding("metaflux-execution-artifact-v1\n");
  append_field(encoding, "cache_key", cache_key);
  append_field(encoding, "kernel_ir", canonical_kernel_ir);
  append_number(encoding, "kind", static_cast<std::uint32_t>(kind));
  return sha256_hex(encoding);
}

ExecutionArtifact make_artifact(std::string cache_key, std::string_view canonical_kernel_ir,
                                ArtifactKind kind) {
  ExecutionArtifact artifact{
      .format_version = kExecutionArtifactFormatVersion,
      .kind = kind,
      .cache_key = std::move(cache_key),
      .canonical_kernel_ir = std::string(canonical_kernel_ir),
      .integrity_digest = {},
  };
  artifact.integrity_digest =
      artifact_digest(artifact.cache_key, artifact.canonical_kernel_ir, artifact.kind);
  return artifact;
}

bool valid_artifact(const ExecutionArtifact& artifact, std::string_view cache_key,
                    std::string_view canonical_kernel_ir) {
  return artifact.format_version == kExecutionArtifactFormatVersion &&
         artifact.cache_key == cache_key && artifact.canonical_kernel_ir == canonical_kernel_ir &&
         artifact.integrity_digest ==
             artifact_digest(artifact.cache_key, artifact.canonical_kernel_ir, artifact.kind);
}

} // namespace

std::string make_cache_key(const CacheIdentity& identity, std::string_view canonical_kernel_ir) {
  std::string encoding("metaflux-cache-identity-v1\n");
  append_field(encoding, "toolchain", identity.toolchain_fingerprint);
  append_number(encoding, "compiler_epoch", identity.compiler_epoch);
  append_number(encoding, "kernel_ir_schema", identity.kernel_ir_schema);
  append_field(encoding, "pipeline", identity.pass_pipeline);
  append_field(encoding, "target_triple", identity.target_triple);
  append_field(encoding, "cpu_name", identity.cpu_name);

  auto features = identity.canonical_features;
  std::sort(features.begin(), features.end());
  features.erase(std::unique(features.begin(), features.end()), features.end());
  append_number(encoding, "feature_count", static_cast<std::uint32_t>(features.size()));
  for (const auto& feature : features) {
    append_field(encoding, "feature", feature);
  }

  append_field(encoding, "optimization", identity.optimization_level);
  append_field(encoding, "fp_semantics", identity.fp_semantics);
  append_number(encoding, "backend_abi", identity.backend_abi);
  append_number(encoding, "helper_abi", identity.helper_abi);
  append_field(encoding, "pgo_id", identity.pgo_id);
  append_field(encoding, "kernel_ir", canonical_kernel_ir);
  return "mf-cache-v1-" + sha256_hex(encoding);
}

std::string sha256_hex(std::span<const std::byte> bytes) { return sha256_impl(bytes); }

std::string sha256_hex(std::string_view text) {
  return sha256_impl(std::as_bytes(std::span{text.data(), text.size()}));
}

ArtifactSelection FixtureArtifactCache::select(const CacheIdentity& identity,
                                               std::string_view canonical_kernel_ir,
                                               ExecutionRequest request) {
  const auto key = make_cache_key(identity, canonical_kernel_ir);
  if (request == ExecutionRequest::Interpreter) {
    return ArtifactSelection{
        .path = ExecutionPath::Interpreter,
        .cache_key = key,
        .artifact = std::nullopt,
    };
  }

  auto existing = artifacts_.find(key);
  if (existing != artifacts_.end() && !valid_artifact(existing->second, key, canonical_kernel_ir)) {
    artifacts_.erase(existing);
    existing = artifacts_.end();
  }
  if (existing != artifacts_.end()) {
    return ArtifactSelection{
        .path = existing->second.kind == ArtifactKind::Aot ? ExecutionPath::Aot
                                                           : ExecutionPath::WarmJit,
        .cache_key = key,
        .artifact = existing->second,
    };
  }

  auto artifact = make_artifact(key, canonical_kernel_ir, ArtifactKind::Jit);
  const auto [inserted, did_insert] = artifacts_.emplace(key, artifact);
  static_cast<void>(did_insert);
  return ArtifactSelection{
      .path = ExecutionPath::ColdJit,
      .cache_key = key,
      .artifact = inserted->second,
  };
}

void FixtureArtifactCache::prewarm_aot(const CacheIdentity& identity,
                                       std::string_view canonical_kernel_ir) {
  const auto key = make_cache_key(identity, canonical_kernel_ir);
  auto artifact = make_artifact(key, canonical_kernel_ir, ArtifactKind::Aot);
  artifacts_.insert_or_assign(key, std::move(artifact));
}

} // namespace metaflux::compiler
