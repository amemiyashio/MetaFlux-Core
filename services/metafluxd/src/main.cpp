#include "server.hpp"

#include "compiler_worker.hpp"
#include "execution.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>

#ifndef METAFLUX_PROJECT_VERSION
#error "METAFLUX_PROJECT_VERSION must be supplied by the build"
#endif

namespace {

std::string default_socket_path() {
  if (const char* configured = std::getenv("METAFLUX_SOCKET");
      configured != nullptr && configured[0] != '\0') {
    return configured;
  }
  if (const char* runtime_directory = std::getenv("XDG_RUNTIME_DIR");
      runtime_directory != nullptr && runtime_directory[0] != '\0') {
    return std::string(runtime_directory) + "/metafluxd.sock";
  }
  return "/run/user/" + std::to_string(static_cast<unsigned long long>(geteuid())) +
         "/metafluxd.sock";
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--compiler-worker-v1") {
    std::int64_t parent_process_id = -1;
    const std::string_view encoded_parent(argv[2]);
    const auto parsed = std::from_chars(
        encoded_parent.data(), encoded_parent.data() + encoded_parent.size(), parent_process_id);
    if (encoded_parent.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != encoded_parent.data() + encoded_parent.size()) {
      return 64;
    }
    return metaflux::service::run_compiler_worker_process(parent_process_id);
  }
  if (argc == 2 && std::string_view(argv[1]) == "--version") {
    std::cout << "metafluxd " << METAFLUX_PROJECT_VERSION << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--socket") {
    return metaflux::service::run(argv[2]);
  }
  if (argc == 3 && std::string_view(argv[1]) == "--prewarm-aot") {
    try {
      const auto configuration = metaflux::service::cpu_execution_configuration_from_environment();
      if (!configuration.ok()) {
        std::cerr << "metafluxd: CPU execution configuration failed: " << configuration.diagnostic
                  << '\n';
        return 64;
      }
      const auto result =
          metaflux::service::prewarm_aot_file(*configuration.configuration, argv[2]);
      if (!result.success) {
        std::cerr << "metafluxd: AOT prewarm failed: " << result.diagnostic << '\n';
        return 1;
      }
      std::cout << "metafluxd: AOT prewarm " << (result.compiled ? "compiled" : "hit")
                << " cache-key=" << result.cache_key << '\n';
    } catch (const std::exception& error) {
      std::cerr << "metafluxd: AOT prewarm failed: " << error.what() << '\n';
      return 1;
    }
    return 0;
  }
  if (argc == 1) {
    return metaflux::service::run(default_socket_path());
  }
  std::cerr << "usage: metafluxd [--socket PATH | --prewarm-aot PTX | --version]\n";
  return 64;
}
