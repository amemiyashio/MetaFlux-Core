set(metaflux_runtime_contract_targets)

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

if(TARGET metaflux_backend_plugin_api AND TARGET metaflux_client_protocol)
  add_library(metaflux_contracts INTERFACE)
  add_library(MetaFlux::Contracts ALIAS metaflux_contracts)
  set_target_properties(metaflux_contracts PROPERTIES EXPORT_NAME Contracts)
  target_link_libraries(
    metaflux_contracts
    INTERFACE MetaFlux::BackendPluginApi MetaFlux::ClientProtocol
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
