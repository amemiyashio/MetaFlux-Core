add_library(metaflux_compiler_core STATIC src/compiler.cpp)
add_library(MetaFlux::CompilerCore ALIAS metaflux_compiler_core)

target_include_directories(
  metaflux_compiler_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(
  metaflux_compiler_core
  PRIVATE METAFLUX_COMPILER_EPOCH=${METAFLUX_COMPILER_EPOCH}
)
metaflux_configure_target(metaflux_compiler_core CXX)
