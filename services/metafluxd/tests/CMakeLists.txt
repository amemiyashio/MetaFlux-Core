add_executable(metafluxd_cross_process_test client.c)
target_link_libraries(metafluxd_cross_process_test PRIVATE MetaFlux::ClientFastpath)
metaflux_configure_target(metafluxd_cross_process_test C)

add_test(
  NAME metaflux.integration.daemon-cross-process
  COMMAND metafluxd_cross_process_test $<TARGET_FILE:metafluxd>
)
set_tests_properties(
  metaflux.integration.daemon-cross-process
  PROPERTIES LABELS "integration;runtime;daemon" TIMEOUT 60
)

add_executable(metafluxd_noop_stress_test noop_stress.c)
target_link_libraries(metafluxd_noop_stress_test PRIVATE MetaFlux::ClientFastpath)
metaflux_configure_target(metafluxd_noop_stress_test C)

add_test(
  NAME metaflux.stress.daemon-million-noop
  COMMAND metafluxd_noop_stress_test $<TARGET_FILE:metafluxd>
)
set_tests_properties(
  metaflux.stress.daemon-million-noop
  PROPERTIES LABELS "stress;integration;runtime;daemon" TIMEOUT 180
)

add_executable(metafluxd_process_snapshot_test process_snapshot.c)
target_link_libraries(metafluxd_process_snapshot_test PRIVATE MetaFlux::ClientFastpath)
metaflux_configure_target(metafluxd_process_snapshot_test C)

add_test(
  NAME metaflux.integration.daemon-process-snapshot
  COMMAND metafluxd_process_snapshot_test $<TARGET_FILE:metafluxd>
)
set_tests_properties(
  metaflux.integration.daemon-process-snapshot
  PROPERTIES LABELS "integration;runtime;daemon;process-snapshot" TIMEOUT 60
)

add_executable(metafluxd_admission_test admission_test.c)
target_link_libraries(metafluxd_admission_test PRIVATE MetaFlux::ClientFastpath)
metaflux_configure_target(metafluxd_admission_test C)

add_test(
  NAME metaflux.integration.daemon-pre-negotiation-admission
  COMMAND metafluxd_admission_test $<TARGET_FILE:metafluxd>
)
set_tests_properties(
  metaflux.integration.daemon-pre-negotiation-admission
  PROPERTIES LABELS "integration;runtime;daemon;admission" TIMEOUT 60
)

add_executable(
  metafluxd_execution_mode_test
  ../src/compiler_worker_client.cpp
  ../src/compiler_worker_protocol.cpp
  execution_mode_test.cpp
  ../src/execution.cpp
)
target_include_directories(
  metafluxd_execution_mode_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_compile_definitions(
  metafluxd_execution_mode_test
  PRIVATE
    METAFLUX_DAEMON_ADD_PTX="${CMAKE_CURRENT_SOURCE_DIR}/../../../plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
    METAFLUX_DAEMON_EXECUTABLE="$<TARGET_FILE:metafluxd>"
)
target_link_libraries(
  metafluxd_execution_mode_test
  PRIVATE
    MetaFlux::CompilerCore
    MetaFlux::CudaPtxFrontend
    MetaFlux::CpuBackendCompiler
    MetaFlux::CpuBackendRuntime
)
metaflux_configure_target(metafluxd_execution_mode_test CXX)

add_test(
  NAME metaflux.integration.daemon-execution-modes
  COMMAND metafluxd_execution_mode_test
)
set_tests_properties(
  metaflux.integration.daemon-execution-modes
  PROPERTIES LABELS "integration;runtime;daemon;compiler;cpu" TIMEOUT 180
)

add_executable(
  metafluxd_compiler_worker_test
  ../src/compiler_worker_client.cpp
  ../src/compiler_worker_protocol.cpp
  compiler_worker_test.cpp
  ../src/execution.cpp
)
target_include_directories(
  metafluxd_compiler_worker_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_compile_definitions(
  metafluxd_compiler_worker_test
  PRIVATE
    METAFLUX_DAEMON_ADD_PTX="${CMAKE_CURRENT_SOURCE_DIR}/../../../plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
    METAFLUX_DAEMON_CAST_PTX="${CMAKE_CURRENT_SOURCE_DIR}/../../../plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1/cast-i32-f32.ptx"
    METAFLUX_DAEMON_EXP_PTX="${CMAKE_CURRENT_SOURCE_DIR}/../../../plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1/exp-f32.ptx"
    METAFLUX_DAEMON_EXECUTABLE="$<TARGET_FILE:metafluxd>"
)
target_link_libraries(
  metafluxd_compiler_worker_test
  PRIVATE
    MetaFlux::CompilerCore
    MetaFlux::CudaPtxFrontend
    MetaFlux::CpuBackendCompiler
    MetaFlux::CpuBackendRuntime
)
metaflux_configure_target(metafluxd_compiler_worker_test CXX)

add_test(
  NAME metaflux.integration.daemon-compiler-worker
  COMMAND metafluxd_compiler_worker_test
)
set_tests_properties(
  metaflux.integration.daemon-compiler-worker
  PROPERTIES LABELS "integration;runtime;daemon;compiler;cpu;isolation" TIMEOUT 180
)
