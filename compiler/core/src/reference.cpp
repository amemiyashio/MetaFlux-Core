#include "metaflux/compiler/reference.hpp"

#include <cstddef>
#include <limits>

namespace metaflux::compiler::reference {
namespace {

bool count_fits(std::size_t size, std::uint32_t count) noexcept {
  if constexpr (sizeof(std::size_t) < sizeof(std::uint32_t)) {
    if (count > std::numeric_limits<std::size_t>::max()) {
      return false;
    }
  }
  return static_cast<std::size_t>(count) <= size;
}

} // namespace

bool add_u32(std::span<std::uint32_t> destination, std::span<const std::uint32_t> left,
             std::span<const std::uint32_t> right, std::uint32_t count) noexcept {
  if (!count_fits(destination.size(), count) || !count_fits(left.size(), count) ||
      !count_fits(right.size(), count)) {
    return false;
  }
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto offset = static_cast<std::size_t>(index);
    destination[offset] = left[offset] + right[offset];
  }
  return true;
}

bool copy_u32(std::span<std::uint32_t> destination, std::span<const std::uint32_t> source,
              std::uint32_t count) noexcept {
  if (!count_fits(destination.size(), count) || !count_fits(source.size(), count)) {
    return false;
  }
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto offset = static_cast<std::size_t>(index);
    destination[offset] = source[offset];
  }
  return true;
}

} // namespace metaflux::compiler::reference
