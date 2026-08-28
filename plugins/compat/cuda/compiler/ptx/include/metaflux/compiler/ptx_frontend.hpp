#ifndef METAFLUX_COMPILER_PTX_FRONTEND_HPP
#define METAFLUX_COMPILER_PTX_FRONTEND_HPP

#include <cstdint>

namespace metaflux::compiler::ptx {

[[nodiscard]] std::uint32_t bootstrap_epoch() noexcept;

}

#endif
