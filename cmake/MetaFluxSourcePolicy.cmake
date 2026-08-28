set(METAFLUX_SOURCE_ROOTS
    contracts
    runtime
    compiler
    plugins
    services
    tests
    tools
    transports
    kernel
)

set(METAFLUX_FORBIDDEN_LANGUAGE_SOURCES)
foreach(source_root IN LISTS METAFLUX_SOURCE_ROOTS)
  file(
    GLOB_RECURSE forbidden_sources
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/${source_root}/*.asm"
    "${PROJECT_SOURCE_DIR}/${source_root}/*.rs"
    "${PROJECT_SOURCE_DIR}/${source_root}/*.s"
    "${PROJECT_SOURCE_DIR}/${source_root}/*.S"
    "${PROJECT_SOURCE_DIR}/${source_root}/Cargo.lock"
    "${PROJECT_SOURCE_DIR}/${source_root}/Cargo.toml"
  )
  list(APPEND METAFLUX_FORBIDDEN_LANGUAGE_SOURCES ${forbidden_sources})
endforeach()

file(
  GLOB METAFLUX_ROOT_FORBIDDEN_LANGUAGE_SOURCES
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/*.asm"
  "${PROJECT_SOURCE_DIR}/*.rs"
  "${PROJECT_SOURCE_DIR}/*.s"
  "${PROJECT_SOURCE_DIR}/*.S"
  "${PROJECT_SOURCE_DIR}/Cargo.lock"
  "${PROJECT_SOURCE_DIR}/Cargo.toml"
)
list(APPEND
  METAFLUX_FORBIDDEN_LANGUAGE_SOURCES
  ${METAFLUX_ROOT_FORBIDDEN_LANGUAGE_SOURCES}
)

function(metaflux_enforce_c_only_directory directory)
  file(
    GLOB_RECURSE cxx_sources
    CONFIGURE_DEPENDS
    "${directory}/*.cc"
    "${directory}/*.cpp"
    "${directory}/*.cxx"
    "${directory}/*.hh"
    "${directory}/*.hpp"
    "${directory}/*.hxx"
  )
  if(cxx_sources)
    list(JOIN cxx_sources "\n  " cxx_source_list)
    message(
      FATAL_ERROR
      "Application-side boundary ${directory} is C17-only:\n  ${cxx_source_list}"
    )
  endif()
endfunction()

if(METAFLUX_FORBIDDEN_LANGUAGE_SOURCES)
  list(JOIN METAFLUX_FORBIDDEN_LANGUAGE_SOURCES "\n  " forbidden_source_list)
  message(
    FATAL_ERROR
    "Compiler epoch 1 excludes Rust and assembly sources:\n  ${forbidden_source_list}"
  )
endif()
