#ifndef METAFLUX_SERVICE_SERVER_HPP
#define METAFLUX_SERVICE_SERVER_HPP

#include <string_view>

namespace metaflux::service {

[[nodiscard]] int run(std::string_view socket_path);

} // namespace metaflux::service

#endif
