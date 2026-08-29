#ifndef METAFLUX_SERVICE_COMPILER_WORKER_PROTOCOL_HPP
#define METAFLUX_SERVICE_COMPILER_WORKER_PROTOCOL_HPP

#include "metaflux/backend/cpu/compiler.hpp"
#include "metaflux/compiler/kernel_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace metaflux::service::compiler_worker_protocol {

inline constexpr std::size_t kMaximumRequestBytes = 64U * 1024U * 1024U;
inline constexpr std::size_t kMaximumResponseBytes = 257U * 1024U * 1024U;

struct DecodedResponse final {
  backend::cpu::compiler::CompileResult compilation;
  std::int64_t process_id = -1;
};

[[nodiscard]] std::optional<std::vector<std::byte>> encode_request(const compiler::Kernel& kernel);
[[nodiscard]] std::optional<compiler::Kernel> decode_request(std::span<const std::byte> bytes);

[[nodiscard]] std::optional<std::vector<std::byte>>
encode_response(const backend::cpu::compiler::CompileResult& result, std::int64_t process_id);
[[nodiscard]] std::optional<DecodedResponse> decode_response(std::span<const std::byte> bytes,
                                                             std::string& diagnostic);

} // namespace metaflux::service::compiler_worker_protocol

#endif
