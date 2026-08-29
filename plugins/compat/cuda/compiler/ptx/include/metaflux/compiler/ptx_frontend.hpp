#ifndef METAFLUX_COMPILER_PTX_FRONTEND_HPP
#define METAFLUX_COMPILER_PTX_FRONTEND_HPP

#include "metaflux/compiler/kernel_ir.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace metaflux::compiler::ptx {

inline constexpr std::uint32_t kPtxIsaMajor = 9;
inline constexpr std::uint32_t kPtxIsaMinor = 0;

[[nodiscard]] std::uint32_t bootstrap_epoch() noexcept;

struct SupportedForm {
  std::string_view id;
  std::string_view spelling;
  std::string_view types;
  std::string_view spaces;
  std::string_view modifiers;
  std::uint32_t minimum_sm;
  std::string_view kernel_ir_op;
  std::string_view oracle;
};

[[nodiscard]] std::span<const SupportedForm> supported_forms() noexcept;

struct ParseResult {
  std::optional<Kernel> kernel;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept { return kernel.has_value() && diagnostics.empty(); }
};

[[nodiscard]] ParseResult parse(std::string_view source);

} // namespace metaflux::compiler::ptx

#endif
