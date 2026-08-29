find_package(Threads REQUIRED)

add_executable(metaflux_compiler_core_test compiler_core_test.cpp)
target_link_libraries(metaflux_compiler_core_test PRIVATE MetaFlux::CompilerCore)
metaflux_configure_target(metaflux_compiler_core_test CXX)
add_test(NAME metaflux.unit.compiler-core COMMAND metaflux_compiler_core_test)
set_tests_properties(metaflux.unit.compiler-core PROPERTIES LABELS "unit;compiler")

add_executable(metaflux_artifact_cache_test artifact_cache_test.cpp)
target_link_libraries(
  metaflux_artifact_cache_test
  PRIVATE MetaFlux::CompilerCore Threads::Threads
)
metaflux_configure_target(metaflux_artifact_cache_test CXX)
add_test(NAME metaflux.unit.artifact-cache COMMAND metaflux_artifact_cache_test)
set_tests_properties(
  metaflux.unit.artifact-cache
  PROPERTIES LABELS "unit;compiler;cache"
)
