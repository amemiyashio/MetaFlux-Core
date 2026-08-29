#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t kMaximumInputBytes = 1024U * 1024U;

bool equivalent_diagnostics(const metaflux::compiler::Diagnostic& left,
                            const metaflux::compiler::Diagnostic& right) {
  return left.code == right.code && left.location == right.location &&
         left.message == right.message && left.form == right.form;
}

std::string read_input(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open fuzz input");
  }
  std::string source{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  if (input.bad()) {
    throw std::runtime_error("failed while reading fuzz input");
  }
  if (source.size() > kMaximumInputBytes) {
    throw std::runtime_error("fuzz input exceeds the 1 MiB driver limit");
  }
  return source;
}

int exercise(std::string_view source) {
  const auto first = metaflux::compiler::ptx::parse(source);
  const auto second = metaflux::compiler::ptx::parse(source);
  if (first.ok() != second.ok() || first.diagnostics.size() != second.diagnostics.size()) {
    std::cerr << "PTX parser produced a nondeterministic result\n";
    return 1;
  }
  for (std::size_t index = 0; index < first.diagnostics.size(); ++index) {
    if (!equivalent_diagnostics(first.diagnostics[index], second.diagnostics[index])) {
      std::cerr << "PTX parser produced nondeterministic diagnostics\n";
      return 1;
    }
  }

  if (!first.ok()) {
    std::cout << "outcome=rejected input_bytes=" << source.size()
              << " diagnostics=" << first.diagnostics.size() << '\n';
    return 0;
  }

  const auto first_serialized = metaflux::compiler::serialize_kernel(*first.kernel);
  const auto second_serialized = metaflux::compiler::serialize_kernel(*second.kernel);
  if (!first_serialized.ok() || !second_serialized.ok() ||
      first_serialized.text != second_serialized.text) {
    std::cerr << "accepted PTX did not produce deterministic verified Kernel IR\n";
    return 1;
  }
  std::cout << "outcome=accepted input_bytes=" << source.size()
            << " kernel_ir_bytes=" << first_serialized.text.size() << '\n';
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: metaflux_cuda_ptx_fuzz_driver INPUT.ptx\n";
    return 2;
  }
  try {
    return exercise(read_input(argv[1]));
  } catch (const std::exception& error) {
    std::cerr << "PTX fuzz driver failure: " << error.what() << '\n';
    return 1;
  }
}
