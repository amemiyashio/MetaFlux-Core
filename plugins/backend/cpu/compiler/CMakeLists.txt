find_package(LLVM 22.1.8 CONFIG REQUIRED)
find_package(MLIR CONFIG REQUIRED)
find_program(METAFLUX_LLD_EXECUTABLE NAMES ld.lld REQUIRED)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/metaflux/backend/cpu")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/metaflux/backend/cpu/compiler_build_identity.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/metaflux/backend/cpu/compiler_build_identity.hpp"
  @ONLY
)

set(
  METAFLUX_RUNTIME_LLD_PATH
  ""
  CACHE FILEPATH
  "Absolute ld.lld path embedded in CPU compiler artifacts at runtime"
)
if(NOT METAFLUX_RUNTIME_LLD_PATH)
  set(METAFLUX_RUNTIME_LLD_PATH "${METAFLUX_LLD_EXECUTABLE}")
endif()
if(NOT IS_ABSOLUTE "${METAFLUX_RUNTIME_LLD_PATH}")
  message(FATAL_ERROR "METAFLUX_RUNTIME_LLD_PATH must be absolute")
endif()

if(NOT METAFLUX_COMPILER_LINK_SHARED_LLVM)
  # The MLIR export set names the monolithic LLVM dylib on every component.
  # Static compiler packaging supplies the precise LLVM component archives
  # below, so remove that aggregate edge from the imported MLIR targets.
  get_property(mlir_imported_targets DIRECTORY PROPERTY IMPORTED_TARGETS)
  foreach(mlir_target IN LISTS mlir_imported_targets)
    if(mlir_target MATCHES "^MLIR")
      get_target_property(mlir_interface_libraries ${mlir_target} INTERFACE_LINK_LIBRARIES)
      if(mlir_interface_libraries)
        list(REMOVE_ITEM mlir_interface_libraries LLVM)
        set_property(
          TARGET ${mlir_target}
          PROPERTY INTERFACE_LINK_LIBRARIES "${mlir_interface_libraries}"
        )
      endif()
    endif()
  endforeach()
endif()

add_library(
  metaflux_cpu_backend_compiler STATIC
  src/compiler.cpp
  src/pipeline.cpp
)
add_library(MetaFlux::CpuBackendCompiler ALIAS metaflux_cpu_backend_compiler)

target_include_directories(
  metaflux_cpu_backend_compiler
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_include_directories(
  metaflux_cpu_backend_compiler
  SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS}
)
target_compile_definitions(
  metaflux_cpu_backend_compiler
  PRIVATE METAFLUX_DEFAULT_LLD_PATH="${METAFLUX_RUNTIME_LLD_PATH}"
)
target_link_libraries(
  metaflux_cpu_backend_compiler
  PUBLIC MetaFlux::CompilerCore
  PRIVATE MetaFlux::CpuBackendRuntime
)
if(METAFLUX_COMPILER_LINK_SHARED_LLVM)
  target_link_libraries(metaflux_cpu_backend_compiler PRIVATE MLIR LLVM)
else()
  target_link_libraries(
    metaflux_cpu_backend_compiler
    PRIVATE
      MLIRIR
      MLIRBuiltinToLLVMIRTranslation
      MLIRLLVMDialect
      MLIRLLVMToLLVMIRTranslation
      MLIRParser
      MLIRTargetLLVMIRExport
      LLVMCore
      LLVMIRReader
      LLVMPasses
      LLVMTarget
      LLVMX86CodeGen
      LLVMX86Desc
      LLVMX86Info
  )
endif()
metaflux_configure_target(metaflux_cpu_backend_compiler CXX)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
