#include "metaflux/backend/api.h"
#include "metaflux/client/protocol.h"
#include "metaflux/compiler/core.hpp"
#include "metaflux/runtime/core.hpp"

#if METAFLUX_HAVE_BUILTIN_CPU_BACKEND
#include "metaflux/backend/cpu.h"
#endif

#include <iostream>
#include <string_view>

namespace {

bool builtin_backends_are_compatible() {
#if METAFLUX_HAVE_BUILTIN_CPU_BACKEND
  const auto* api = mf_cpu_backend_get_api_v1();
  return api != nullptr && api->header.abi_version == MF_BACKEND_ABI_VERSION_1 &&
         api->header.struct_size >= sizeof(mf_backend_api_v1);
#else
  return true;
#endif
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--version") {
    std::cout << "metafluxd 0.1.0-bootstrap\n";
    return 0;
  }

  const auto ready = builtin_backends_are_compatible() &&
                     metaflux::runtime::bootstrap_client_protocol_abi_version() ==
                         MF_CLIENT_PROTOCOL_ABI_VERSION_1 &&
                     metaflux::compiler::bootstrap_epoch() == 1U;
  if (!ready) {
    return 1;
  }

  std::cerr << "metafluxd bootstrap contains no service loop\n";
  return 64;
}
