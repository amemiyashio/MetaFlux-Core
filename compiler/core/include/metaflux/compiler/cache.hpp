#ifndef METAFLUX_COMPILER_CACHE_HPP
#define METAFLUX_COMPILER_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace metaflux::compiler {

inline constexpr std::uint32_t kExecutionArtifactFormatVersion = 1;

struct CacheIdentity {
  std::string toolchain_fingerprint;
  std::uint32_t compiler_epoch = 0;
  std::uint32_t kernel_ir_schema = 0;
  std::string pass_pipeline;
  std::string target_triple;
  std::string cpu_name;
  std::vector<std::string> canonical_features;
  std::string optimization_level;
  std::string fp_semantics;
  std::uint32_t backend_abi = 0;
  std::uint32_t helper_abi = 0;
  std::string pgo_id;
};

[[nodiscard]] std::string make_cache_key(const CacheIdentity& identity,
                                         std::string_view canonical_kernel_ir);
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);
[[nodiscard]] std::string sha256_hex(std::string_view text);

enum class ArtifactKind : std::uint32_t {
  Jit,
  Aot,
};

enum class ExecutionPath : std::uint32_t {
  Interpreter,
  ColdJit,
  WarmJit,
  Aot,
};

enum class ExecutionRequest : std::uint32_t {
  Interpreter,
  Jit,
};

struct ExecutionArtifact {
  std::uint32_t format_version = kExecutionArtifactFormatVersion;
  ArtifactKind kind = ArtifactKind::Jit;
  std::string cache_key;
  std::string canonical_kernel_ir;
  std::string integrity_digest;
};

struct ArtifactSelection {
  ExecutionPath path = ExecutionPath::Interpreter;
  std::string cache_key;
  std::optional<ExecutionArtifact> artifact;
};

// This in-memory class is a phase-1 selection fixture. It never represents a
// compiled executable and is intentionally excluded from JIT/AOT evidence.
class FixtureArtifactCache {
public:
  [[nodiscard]] ArtifactSelection select(const CacheIdentity& identity,
                                         std::string_view canonical_kernel_ir,
                                         ExecutionRequest request);

  void prewarm_aot(const CacheIdentity& identity, std::string_view canonical_kernel_ir);

  [[nodiscard]] std::size_t size() const noexcept { return artifacts_.size(); }

private:
  std::unordered_map<std::string, ExecutionArtifact> artifacts_;
};

} // namespace metaflux::compiler

#endif
