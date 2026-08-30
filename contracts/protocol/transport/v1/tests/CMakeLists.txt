add_executable(metaflux_transport_schema_fixture schema_fixture.c)
target_link_libraries(metaflux_transport_schema_fixture PRIVATE MetaFlux::TransportSchema)
target_include_directories(
  metaflux_transport_schema_fixture
  PRIVATE
    "${PROJECT_SOURCE_DIR}/contracts/shared/device/v1/include"
    "${PROJECT_SOURCE_DIR}/contracts/uapi/linux/v1/include"
)
metaflux_configure_target(metaflux_transport_schema_fixture C)
add_test(NAME metaflux.contract.transport-schema-c COMMAND metaflux_transport_schema_fixture)
set_tests_properties(metaflux.contract.transport-schema-c PROPERTIES LABELS "abi;contract")

if(CMAKE_CXX_COMPILER_LOADED)
  add_executable(metaflux_transport_schema_cpp_fixture schema_fixture.cpp)
  target_link_libraries(metaflux_transport_schema_cpp_fixture PRIVATE MetaFlux::TransportSchema)
  target_include_directories(
    metaflux_transport_schema_cpp_fixture
    PRIVATE
      "${PROJECT_SOURCE_DIR}/contracts/shared/device/v1/include"
      "${PROJECT_SOURCE_DIR}/contracts/uapi/linux/v1/include"
  )
  metaflux_configure_target(metaflux_transport_schema_cpp_fixture CXX)
  add_test(NAME metaflux.contract.transport-schema-cpp COMMAND metaflux_transport_schema_cpp_fixture)
  set_tests_properties(metaflux.contract.transport-schema-cpp PROPERTIES LABELS "abi;contract")
endif()

add_test(
  NAME metaflux.contract.transport-schema-validator
  COMMAND
    Python3::Interpreter -B
    "${PROJECT_SOURCE_DIR}/tools/validate-transport-schema.py"
    --root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(metaflux.contract.transport-schema-validator PROPERTIES LABELS "abi;contract")
