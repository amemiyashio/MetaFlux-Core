find_package(LLVM 22.1.8 CONFIG REQUIRED)
find_package(MLIR CONFIG REQUIRED)

if(NOT METAFLUX_COMPILER_LINK_SHARED_LLVM)
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

add_library(metaflux_vulkan_compiler STATIC src/lowering.cpp)
add_library(MetaFlux::VulkanCompiler ALIAS metaflux_vulkan_compiler)

target_include_directories(
  metaflux_vulkan_compiler
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/plugins/backend/vulkan/runtime/include"
    "${PROJECT_SOURCE_DIR}/compiler/core/include"
    "${PROJECT_SOURCE_DIR}/contracts/plugin/backend/v1/include"
)
target_include_directories(metaflux_vulkan_compiler SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS})
target_link_libraries(
  metaflux_vulkan_compiler
  PRIVATE
    MetaFlux::CompilerCore
    MetaFlux::VulkanBackend
)
if(METAFLUX_COMPILER_LINK_SHARED_LLVM)
  target_link_libraries(metaflux_vulkan_compiler PRIVATE MLIR LLVM)
else()
  target_link_libraries(
    metaflux_vulkan_compiler
    PRIVATE
      MLIRArithDialect
      MLIRArithToSPIRV
      MLIRControlFlowToSPIRV
      MLIRFuncDialect
      MLIRFuncToSPIRV
      MLIRGPUToSPIRV
      MLIRGPUDialect
      MLIRIR
      MLIRIndexToSPIRV
      MLIRMemRefDialect
      MLIRMemRefToSPIRV
      MLIRParser
      MLIRPass
      MLIRSCFDialect
      MLIRSCFToSPIRV
      MLIRSPIRVDialect
      MLIRSPIRVSerialization
      MLIRSPIRVTarget
      MLIRSPIRVTransforms
      MLIRVectorDialect
      MLIRVectorToSPIRV
      LLVMCore
      LLVMSupport
  )
endif()
metaflux_configure_target(metaflux_vulkan_compiler CXX)
metaflux_register_component_graph_entry(
  metaflux_vulkan_compiler
  compiler.vulkan-lowering
  backend-compiler
  CXX
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_executable(metaflux_vulkan_lowering_test tests/lowering_test.cpp)
  target_link_libraries(metaflux_vulkan_lowering_test PRIVATE MetaFlux::VulkanCompiler)
  metaflux_configure_target(metaflux_vulkan_lowering_test CXX)
  find_program(
    METAFLUX_SPIRV_VAL
    NAMES spirv-val
    HINTS "${METAFLUX_VULKAN_SDK_DIR}/bin"
  )
  if(METAFLUX_SPIRV_VAL)
    target_compile_definitions(
      metaflux_vulkan_lowering_test PRIVATE
      METAFLUX_VULKAN_SPIRV_VAL="${METAFLUX_SPIRV_VAL}"
    )
  endif()
  add_test(NAME metaflux.backend.vulkan-lowering COMMAND metaflux_vulkan_lowering_test)
  set_tests_properties(
    metaflux.backend.vulkan-lowering
    PROPERTIES LABELS "backend;vulkan;compiler;unit"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )
endif()
