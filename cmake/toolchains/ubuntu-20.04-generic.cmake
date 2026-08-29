# CMake toolchain file for Ubuntu 20.04/glibc 2.31 generic release builds.
#
# This file selects the target SDK, unwrapped compiler, target linker, and
# generic LLVM/MLIR closure.  The caller must set environment variables
# METAFLUX_TARGET_SDK, METAFLUX_GENERIC_LLVM, METAFLUX_TOOLCHAIN,
# METAFLUX_RAW_CLANG, and METAFLUX_RESOURCE_DIR before invoking cmake.
# Use tools/build-generic-release.sh for a complete invocation.
#
# The target triple is x86_64-unknown-linux-gnu with the system loader at
# /lib64/ld-linux-x86-64.so.2.  Ubuntu 20.04 and glibc 2.31 are the ABI floor.

# --- Required inputs from environment ---
set(_sdk "$ENV{METAFLUX_TARGET_SDK}")
set(_generic "$ENV{METAFLUX_GENERIC_LLVM}")
set(_toolchain "$ENV{METAFLUX_TOOLCHAIN}")
set(_raw_clang "$ENV{METAFLUX_RAW_CLANG}")
set(_resource_dir "$ENV{METAFLUX_RESOURCE_DIR}")

if(_sdk STREQUAL "")
  message(FATAL_ERROR "METAFLUX_TARGET_SDK environment variable must point to the materialized Ubuntu 20.04 target SDK")
endif()
if(_generic STREQUAL "")
  message(FATAL_ERROR "METAFLUX_GENERIC_LLVM environment variable must point to the materialized generic LLVM/MLIR toolchain")
endif()
if(_toolchain STREQUAL "")
  message(FATAL_ERROR "METAFLUX_TOOLCHAIN environment variable must point to the materialized host toolchain")
endif()
if(_raw_clang STREQUAL "")
  message(FATAL_ERROR "METAFLUX_RAW_CLANG environment variable must point to the unwrapped Clang")
endif()
if(_resource_dir STREQUAL "")
  message(FATAL_ERROR "METAFLUX_RESOURCE_DIR environment variable must point to the Clang resource directory")
endif()

set(METAFLUX_TARGET_TRIPLE "x86_64-unknown-linux-gnu")

# --- Unwrapped compiler ---
set(CMAKE_C_COMPILER "${_raw_clang}/bin/clang")
set(CMAKE_CXX_COMPILER "${_raw_clang}/bin/clang++")

# --- Target selection ---
set(CMAKE_C_COMPILER_TARGET "${METAFLUX_TARGET_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "${METAFLUX_TARGET_TRIPLE}")
set(CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN "${_sdk}/usr")
set(CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN "${_sdk}/usr")
set(CMAKE_SYSROOT "${_sdk}")

# --- Search path isolation ---
set(CMAKE_FIND_ROOT_PATH "${_sdk};${_generic}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# --- LLVM/MLIR package config ---
set(LLVM_DIR "${_generic}/lib/cmake/llvm")
set(MLIR_DIR "${_generic}/lib/cmake/mlir")
set(CMAKE_PREFIX_PATH "${_generic}")

# --- Static zlib from the SDK ---
set(ZLIB_USE_STATIC_LIBS ON)
set(ZLIB_INCLUDE_DIR "${_sdk}/usr/include")
set(ZLIB_LIBRARY "${_sdk}/usr/lib/x86_64-linux-gnu/libz.a")

# --- Binutils from the host toolchain ---
set(CMAKE_AR "${_toolchain}/bin/llvm-ar")
set(CMAKE_RANLIB "${_toolchain}/bin/llvm-ranlib")
set(CMAKE_NM "${_toolchain}/bin/llvm-nm")
set(CMAKE_OBJCOPY "${_toolchain}/bin/llvm-objcopy")
set(CMAKE_STRIP "${_toolchain}/bin/llvm-strip")

# --- Compile flags ---
set(_target_compile_flags
  "--target=${METAFLUX_TARGET_TRIPLE}"
  "--sysroot=${_sdk}"
  "--gcc-toolchain=${_sdk}/usr"
  "-resource-dir=${_resource_dir}"
  "-pthread"
  "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
  "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
  "-ffile-prefix-map=${_sdk}=/usr"
  "-ffile-prefix-map=${_generic}=/usr/libexec/metaflux/generic-llvm"
)
string(REPLACE ";" " " _target_compile_flags_str "${_target_compile_flags}")
set(CMAKE_C_FLAGS_INIT "${_target_compile_flags_str}")
set(CMAKE_CXX_FLAGS_INIT "${_target_compile_flags_str}")

# --- Link flags ---
set(_target_link_flags
  "--ld-path=${_generic}/bin/ld.lld"
  "-static-libstdc++"
  "-static-libgcc"
  "-pthread"
  "-Wl,--build-id=sha1,-z,relro,-z,now,-z,noexecstack"
)
string(REPLACE ";" " " _target_link_flags_str "${_target_link_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_target_link_flags_str} -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_target_link_flags_str}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_target_link_flags_str}")

# --- RPATH ---
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_SKIP_INSTALL_RPATH ON)

# --- Install layout ---
set(CMAKE_INSTALL_PREFIX "/usr")
set(CMAKE_INSTALL_LIBDIR "lib")
