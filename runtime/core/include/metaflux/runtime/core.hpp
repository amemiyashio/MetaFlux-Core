#ifndef METAFLUX_RUNTIME_CORE_HPP
#define METAFLUX_RUNTIME_CORE_HPP

#include <cstdint>

namespace metaflux::runtime {

[[nodiscard]] std::uint32_t bootstrap_client_protocol_abi_version() noexcept;

}

#endif
