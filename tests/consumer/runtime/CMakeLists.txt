cmake_minimum_required(VERSION 3.25)

project(MetaFluxRuntimeConsumer LANGUAGES C CXX)

find_package(MetaFlux CONFIG REQUIRED)

add_executable(metaflux_fastpath_consumer fastpath.c)
target_link_libraries(metaflux_fastpath_consumer PRIVATE MetaFlux::ClientFastpath)

add_executable(metaflux_runtime_consumer runtime.cpp)
target_link_libraries(metaflux_runtime_consumer PRIVATE MetaFlux::RuntimeCore)
