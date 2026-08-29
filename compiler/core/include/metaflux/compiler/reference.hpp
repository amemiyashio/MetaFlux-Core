#ifndef METAFLUX_COMPILER_REFERENCE_HPP
#define METAFLUX_COMPILER_REFERENCE_HPP

#include <cstdint>
#include <span>

namespace metaflux::compiler::reference {

[[nodiscard]] bool add_u32(std::span<std::uint32_t> destination,
                           std::span<const std::uint32_t> left,
                           std::span<const std::uint32_t> right, std::uint32_t count) noexcept;

[[nodiscard]] bool copy_u32(std::span<std::uint32_t> destination,
                            std::span<const std::uint32_t> source, std::uint32_t count) noexcept;

} // namespace metaflux::compiler::reference

#endif
