set(metaflux_runtime_contract_targets)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(
  METAFLUX_TRANSPORT_SCHEMA_HEADER
  "${CMAKE_CURRENT_BINARY_DIR}/generated/include/metaflux/transport/generated.h"
)
add_custom_command(
  OUTPUT "${METAFLUX_TRANSPORT_SCHEMA_HEADER}"
  COMMAND
    Python3::Interpreter
    "${PROJECT_SOURCE_DIR}/tools/validate-transport-schema.py"
    --root "${PROJECT_SOURCE_DIR}"
    --generate-header "${METAFLUX_TRANSPORT_SCHEMA_HEADER}"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/tools/validate-transport-schema.py"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/manifest.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/device/v1/schema/descriptor.json"
    "${PROJECT_SOURCE_DIR}/contracts/shared/device/v1/schema/ring.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/negotiation.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/vfio_user.json"
    "${PROJECT_SOURCE_DIR}/contracts/uapi/linux/v1/schema/uapi.json"
  VERBATIM
)
add_custom_target(metaflux_transport_schema DEPENDS "${METAFLUX_TRANSPORT_SCHEMA_HEADER}")
add_library(metaflux_transport_schema_projection INTERFACE)
add_library(MetaFlux::TransportSchema ALIAS metaflux_transport_schema_projection)
add_dependencies(metaflux_transport_schema_projection metaflux_transport_schema)
list(APPEND metaflux_runtime_contract_targets metaflux_transport_schema_projection)
target_include_directories(
  metaflux_transport_schema_projection
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shared/device/v1/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/uapi/linux/v1/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
install(
  FILES "${METAFLUX_TRANSPORT_SCHEMA_HEADER}"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/metaflux/transport"
  COMPONENT Runtime
)
install(
  DIRECTORY uapi/linux/v1/include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Runtime
)

set(METAFLUX_BUILD_SHARED_DEVICE_LAYOUT ON)
metaflux_add_component(
  ID contract.shared-device-layout
  ROLE shared-device-layout
  OPTION METAFLUX_BUILD_SHARED_DEVICE_LAYOUT
  DIRECTORY shared/device/v1
  TARGET metaflux_shared_device_layout
  LANGUAGE C
)
list(APPEND metaflux_runtime_contract_targets metaflux_shared_device_layout)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/backend/v1/include/metaflux/backend/api.h")
  add_library(metaflux_backend_plugin_api INTERFACE)
  add_library(MetaFlux::BackendPluginApi ALIAS metaflux_backend_plugin_api)
  set_target_properties(
    metaflux_backend_plugin_api
    PROPERTIES EXPORT_NAME BackendPluginApi
  )
  metaflux_register_component_graph_entry(
    metaflux_backend_plugin_api
    contract.backend-plugin-api
    backend-plugin-api
    C
  )
  target_include_directories(
    metaflux_backend_plugin_api
    INTERFACE
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/plugin/backend/v1/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )
  list(APPEND metaflux_runtime_contract_targets metaflux_backend_plugin_api)
  install(
    DIRECTORY plugin/backend/v1/include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Runtime
  )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/protocol/client/v1/include/metaflux/client/protocol.h")
  add_library(metaflux_client_protocol INTERFACE)
  add_library(MetaFlux::ClientProtocol ALIAS metaflux_client_protocol)
  set_target_properties(metaflux_client_protocol PROPERTIES EXPORT_NAME ClientProtocol)
  metaflux_register_component_graph_entry(
    metaflux_client_protocol
    contract.client-protocol
    client-protocol
    C
  )
  target_include_directories(
    metaflux_client_protocol
    INTERFACE
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/protocol/client/v1/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )
  list(APPEND metaflux_runtime_contract_targets metaflux_client_protocol)
  install(
    DIRECTORY protocol/client/v1/include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Runtime
  )
endif()

if(
  TARGET metaflux_backend_plugin_api
  AND TARGET metaflux_client_protocol
  AND TARGET metaflux_shared_device_layout
)
  add_library(metaflux_contracts INTERFACE)
  add_library(MetaFlux::Contracts ALIAS metaflux_contracts)
  set_target_properties(metaflux_contracts PROPERTIES EXPORT_NAME Contracts)
  target_link_libraries(
    metaflux_contracts
    INTERFACE
      MetaFlux::BackendPluginApi
      MetaFlux::ClientProtocol
      MetaFlux::SharedDeviceLayout
  )
  list(APPEND metaflux_runtime_contract_targets metaflux_contracts)
endif()

if(metaflux_runtime_contract_targets)
  install(
    TARGETS ${metaflux_runtime_contract_targets}
    EXPORT MetaFluxRuntimeTargets
    COMPONENT Runtime
  )
endif()

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(protocol/transport/v1/tests)
  add_subdirectory(shared/device/v1/tests)
  add_subdirectory(protocol/client/v1/tests)
  add_subdirectory(plugin/backend/v1/tests)
endif()
