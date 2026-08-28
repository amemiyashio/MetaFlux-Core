#include "metaflux/compiler/ptx_frontend.hpp"

#include "metaflux/compiler/core.hpp"

namespace metaflux::compiler::ptx {

std::uint32_t bootstrap_epoch() noexcept { return metaflux::compiler::bootstrap_epoch(); }

} // namespace metaflux::compiler::ptx
