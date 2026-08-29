#include "metaflux/backend/cpu/compiled_kernel.hpp"

#include "metaflux/backend/cpu/executor.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace metaflux::backend::cpu {
namespace {

constexpr std::uint64_t kMaximumElfBytes = 256U * 1024U * 1024U;
constexpr std::uint64_t kMaximumGridCtas = 1U << 20U;
constexpr std::uint64_t kMaximumThreadsPerCta = 1024U;

constexpr std::array<std::uint32_t, 64> kSha256Constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U,
};

constexpr std::uint32_t rotate_right(std::uint32_t value, std::uint32_t amount) noexcept {
  return (value >> amount) | (value << (32U - amount));
}

std::string sha256(std::span<const std::byte> input) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(input.size() + 72U);
  for (const auto value : input) {
    bytes.push_back(std::to_integer<std::uint8_t>(value));
  }
  const auto bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
  bytes.push_back(0x80U);
  while (bytes.size() % 64U != 56U) {
    bytes.push_back(0U);
  }
  for (std::uint32_t shift = 56U;; shift -= 8U) {
    bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    if (shift == 0U) {
      break;
    }
  }

  std::array<std::uint32_t, 8> state{
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
  };
  std::array<std::uint32_t, 64> words{};
  for (std::size_t block = 0; block < bytes.size(); block += 64U) {
    for (std::size_t index = 0; index < 16U; ++index) {
      const auto offset = block + index * 4U;
      words[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                     (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                     (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                     static_cast<std::uint32_t>(bytes[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
      const auto value0 = words[index - 15U];
      const auto value1 = words[index - 2U];
      const auto sigma0 = rotate_right(value0, 7U) ^ rotate_right(value0, 18U) ^ (value0 >> 3U);
      const auto sigma1 = rotate_right(value1, 17U) ^ rotate_right(value1, 19U) ^ (value1 >> 10U);
      words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
    }

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];
    auto f = state[5];
    auto g = state[6];
    auto h = state[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
      const auto sum1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
      const auto choose = (e & f) ^ ((~e) & g);
      const auto temporary1 = h + sum1 + choose + kSha256Constants[index] + words[index];
      const auto sum0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
      const auto majority = (a & b) ^ (a & c) ^ (b & c);
      const auto temporary2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    for (std::size_t index = 0; index < state.size(); ++index) {
      state[index] += std::array{a, b, c, d, e, f, g, h}[index];
    }
  }

  constexpr char kHexDigits[] = "0123456789abcdef";
  std::string digest;
  digest.reserve(64U);
  for (const auto word : state) {
    for (std::uint32_t shift = 28U;; shift -= 4U) {
      digest.push_back(kHexDigits[static_cast<std::size_t>((word >> shift) & 0xfU)]);
      if (shift == 0U) {
        break;
      }
    }
  }
  return digest;
}

std::optional<std::vector<std::byte>> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return std::nullopt;
  }
  const auto end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) > kMaximumElfBytes) {
    return std::nullopt;
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!input) {
    return std::nullopt;
  }
  return bytes;
}

std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
         static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U);
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t result = 0;
  for (std::uint32_t index = 0; index < 4U; ++index) {
    result |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
              << (index * 8U);
  }
  return result;
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint64_t result = 0;
  for (std::uint32_t index = 0; index < 8U; ++index) {
    result |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
              << (index * 8U);
  }
  return result;
}

ExecutionResult failure(ExecutionError error) {
  return ExecutionResult{.diagnostic = ExecutionDiagnostic{.error = error}};
}

bool fp_environment_supported() {
  if (!std::numeric_limits<float>::is_iec559 || std::fegetround() != FE_TONEAREST) {
    return false;
  }
#if defined(__i386__) || defined(__x86_64__)
  constexpr unsigned int kDaz = 1U << 6U;
  constexpr unsigned int kFtz = 1U << 15U;
  return (_mm_getcsr() & (kDaz | kFtz)) == 0U;
#else
  return true;
#endif
}

using KernelEntry = std::uint32_t (*)(const std::uint64_t*, const std::uint64_t*,
                                      const std::uint32_t*, const std::uint32_t*, std::uint32_t,
                                      std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                      std::uint32_t);

} // namespace

struct LoadedCompiledKernel::State {
  void* library = nullptr;
  KernelEntry entry = nullptr;
  CompiledKernelSignature signature;
};

ElfValidationResult validate_x86_64_pic_elf(std::span<const std::byte> bytes) {
  constexpr std::size_t kElfHeaderSize = 64U;
  constexpr std::size_t kProgramHeaderSize = 56U;
  if (bytes.size() < kElfHeaderSize) {
    return {.diagnostic = "ELF header is truncated"};
  }
  if (std::to_integer<std::uint8_t>(bytes[0]) != 0x7fU || std::to_integer<char>(bytes[1]) != 'E' ||
      std::to_integer<char>(bytes[2]) != 'L' || std::to_integer<char>(bytes[3]) != 'F') {
    return {.diagnostic = "artifact does not have ELF magic"};
  }
  if (std::to_integer<std::uint8_t>(bytes[4]) != 2U ||
      std::to_integer<std::uint8_t>(bytes[5]) != 1U ||
      std::to_integer<std::uint8_t>(bytes[6]) != 1U) {
    return {.diagnostic = "artifact is not little-endian ELF64"};
  }
  if (read_u16(bytes, 16U) != 3U || read_u16(bytes, 18U) != 62U || read_u32(bytes, 20U) != 1U) {
    return {.diagnostic = "artifact is not an x86_64 ET_DYN object"};
  }
  const auto program_offset = read_u64(bytes, 32U);
  const auto program_entry_size = read_u16(bytes, 54U);
  const auto program_count = read_u16(bytes, 56U);
  if (program_entry_size != kProgramHeaderSize || program_count == 0U ||
      program_offset > bytes.size() ||
      static_cast<std::uint64_t>(program_count) >
          (static_cast<std::uint64_t>(bytes.size()) - program_offset) / kProgramHeaderSize) {
    return {.diagnostic = "ELF program-header table is malformed"};
  }

  bool has_dynamic = false;
  bool has_executable_load = false;
  for (std::uint16_t index = 0; index < program_count; ++index) {
    const auto offset = static_cast<std::size_t>(program_offset) +
                        static_cast<std::size_t>(index) * kProgramHeaderSize;
    const auto type = read_u32(bytes, offset);
    const auto flags = read_u32(bytes, offset + 4U);
    const auto file_offset = read_u64(bytes, offset + 8U);
    const auto file_size = read_u64(bytes, offset + 32U);
    const auto memory_size = read_u64(bytes, offset + 40U);
    if (file_size > memory_size || file_offset > bytes.size() ||
        file_size > static_cast<std::uint64_t>(bytes.size()) - file_offset) {
      return {.diagnostic = "ELF segment range is malformed"};
    }
    if (type == 2U) {
      has_dynamic = true;
    }
    if (type == 1U && (flags & 1U) != 0U) {
      has_executable_load = true;
      if ((flags & 2U) != 0U) {
        return {.diagnostic = "ELF contains a writable executable segment"};
      }
    }
  }
  if (!has_dynamic || !has_executable_load) {
    return {.diagnostic = "ELF lacks its dynamic or executable load segment"};
  }
  return {.valid = true, .diagnostic = {}};
}

std::string sha256_file(const std::filesystem::path& path) {
  const auto bytes = read_file(path);
  return bytes.has_value() ? sha256(*bytes) : std::string{};
}

LoadedCompiledKernel::LoadedCompiledKernel() noexcept = default;

LoadedCompiledKernel::LoadedCompiledKernel(std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}

LoadedCompiledKernel::~LoadedCompiledKernel() {
  if (state_ != nullptr && state_->library != nullptr) {
    static_cast<void>(dlclose(state_->library));
  }
}

LoadedCompiledKernel::LoadedCompiledKernel(LoadedCompiledKernel&&) noexcept = default;
LoadedCompiledKernel& LoadedCompiledKernel::operator=(LoadedCompiledKernel&&) noexcept = default;

bool LoadedCompiledKernel::valid() const noexcept {
  return state_ != nullptr && state_->library != nullptr && state_->entry != nullptr;
}

ExecutionResult LoadedCompiledKernel::launch(std::span<const Argument> arguments,
                                             LaunchDimensions launch) const {
  return this->launch(default_cpu_executor(), arguments, launch, std::stop_token{});
}

ExecutionResult LoadedCompiledKernel::launch(std::span<const Argument> arguments,
                                             LaunchDimensions launch,
                                             std::stop_token cancellation) const {
  return this->launch(default_cpu_executor(), arguments, launch, cancellation);
}

ExecutionResult LoadedCompiledKernel::launch(CpuExecutor& executor,
                                             std::span<const Argument> arguments,
                                             LaunchDimensions launch) const {
  return this->launch(executor, arguments, launch, std::stop_token{});
}

ExecutionResult LoadedCompiledKernel::launch(CpuExecutor& executor,
                                             std::span<const Argument> arguments,
                                             LaunchDimensions launch,
                                             std::stop_token cancellation) const {
  if (cancellation.stop_requested()) {
    return failure(ExecutionError::Cancelled);
  }
  if (!valid()) {
    return failure(ExecutionError::InvalidArtifact);
  }
  if (arguments.size() != state_->signature.parameters.size()) {
    return failure(ExecutionError::ArgumentCount);
  }
  if (launch.grid_x == 0U || launch.grid_y == 0U || launch.block_x == 0U || launch.block_y == 0U ||
      static_cast<std::uint64_t>(launch.grid_x) * launch.grid_y > kMaximumGridCtas ||
      static_cast<std::uint64_t>(launch.block_x) * launch.block_y > kMaximumThreadsPerCta) {
    return failure(ExecutionError::InvalidLaunch);
  }
  if (state_->signature.uses_floating_point && !fp_environment_supported()) {
    return failure(ExecutionError::UnsupportedFpEnvironment);
  }

  std::vector<std::uint64_t> addresses(arguments.size(), 0U);
  std::vector<std::uint64_t> sizes(arguments.size(), 0U);
  std::vector<std::uint32_t> writable(arguments.size(), 0U);
  std::vector<std::uint32_t> scalars(arguments.size(), 0U);
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    switch (state_->signature.parameters[index]) {
    case CompiledParameterKind::BufferU32: {
      if (!std::holds_alternative<BufferArgument>(arguments[index])) {
        return failure(ExecutionError::ArgumentType);
      }
      const auto buffer = std::get<BufferArgument>(arguments[index]);
      addresses[index] =
          static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(buffer.words.data()));
      sizes[index] = static_cast<std::uint64_t>(buffer.words.size());
      writable[index] = buffer.writable ? 1U : 0U;
      break;
    }
    case CompiledParameterKind::ScalarU32:
      if (!std::holds_alternative<std::uint32_t>(arguments[index])) {
        return failure(ExecutionError::ArgumentType);
      }
      scalars[index] = std::get<std::uint32_t>(arguments[index]);
      break;
    case CompiledParameterKind::ScalarF32:
      if (!std::holds_alternative<Float32Argument>(arguments[index])) {
        return failure(ExecutionError::ArgumentType);
      }
      scalars[index] = std::get<Float32Argument>(arguments[index]).bits;
      break;
    }
  }

  const auto cta_count = static_cast<std::uint64_t>(launch.grid_x) * launch.grid_y;
  return executor.run_ctas(
      cta_count,
      [this, addresses = std::move(addresses), sizes = std::move(sizes),
       writable = std::move(writable), scalars = std::move(scalars),
       arguments_size = arguments.size(), launch,
       cancellation](std::uint64_t cta_index, std::uint32_t) {
        if (cancellation.stop_requested()) {
          return failure(ExecutionError::Cancelled);
        }
        const auto status = static_cast<CompiledStatus>(state_->entry(
            addresses.data(), sizes.data(), writable.data(), scalars.data(),
            static_cast<std::uint32_t>(arguments_size), static_cast<std::uint32_t>(cta_index),
            launch.grid_x, launch.block_x, launch.grid_y, launch.block_y));
        switch (status) {
        case CompiledStatus::Success:
          return ExecutionResult{};
        case CompiledStatus::InvalidLaunch:
          return failure(ExecutionError::InvalidLaunch);
        case CompiledStatus::ArgumentCount:
          return failure(ExecutionError::ArgumentCount);
        case CompiledStatus::AddressOverflow:
          return failure(ExecutionError::AddressOverflow);
        case CompiledStatus::MisalignedAddress:
          return failure(ExecutionError::MisalignedAddress);
        case CompiledStatus::OutOfBounds:
          return failure(ExecutionError::OutOfBounds);
        case CompiledStatus::WriteToReadOnly:
          return failure(ExecutionError::WriteToReadOnly);
        }
        return failure(ExecutionError::InvalidArtifact);
      },
      cancellation);
}

LoadCompiledKernelResult load_compiled_kernel(const std::filesystem::path& path,
                                              std::string_view expected_sha256,
                                              CompiledKernelSignature signature) {
  const auto bytes = read_file(path);
  if (!bytes.has_value()) {
    return {.kernel = {}, .diagnostic = "compiled ELF cannot be read"};
  }
  const auto validation = validate_x86_64_pic_elf(*bytes);
  if (!validation.valid) {
    return {.kernel = {}, .diagnostic = validation.diagnostic};
  }
  if (expected_sha256.size() != 64U || sha256(*bytes) != expected_sha256) {
    return {.kernel = {}, .diagnostic = "compiled ELF digest mismatch"};
  }

  void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (library == nullptr) {
    const char* error = dlerror();
    return {.kernel = {}, .diagnostic = error == nullptr ? "dlopen failed" : error};
  }
  static_cast<void>(dlerror());
  void* symbol = dlsym(library, kCompiledKernelEntrySymbol);
  const char* symbol_error = dlerror();
  if (symbol == nullptr || symbol_error != nullptr) {
    const std::string diagnostic =
        symbol_error == nullptr ? "compiled entry symbol is absent" : symbol_error;
    static_cast<void>(dlclose(library));
    return {.kernel = {}, .diagnostic = diagnostic};
  }
  KernelEntry entry = nullptr;
  static_assert(sizeof(entry) == sizeof(symbol));
  std::memcpy(&entry, &symbol, sizeof(entry));

  auto state = std::make_unique<LoadedCompiledKernel::State>();
  state->library = library;
  state->entry = entry;
  state->signature = std::move(signature);
  return {.kernel = LoadedCompiledKernel(std::move(state)), .diagnostic = {}};
}

} // namespace metaflux::backend::cpu
