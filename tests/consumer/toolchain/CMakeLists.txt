cmake_minimum_required(VERSION 3.25)

project(MetaFluxToolchainConsumer LANGUAGES CXX)

find_package(LLVM 22.1.8 CONFIG REQUIRED)
find_package(MLIR CONFIG REQUIRED)

add_executable(metaflux_toolchain_consumer main.cpp)
target_include_directories(
  metaflux_toolchain_consumer
  PRIVATE ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS}
)
target_link_libraries(metaflux_toolchain_consumer PRIVATE MLIRIR)
