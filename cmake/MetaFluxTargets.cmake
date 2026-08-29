include(CheckIPOSupported)
include(CheckLinkerFlag)

if(METAFLUX_USE_LLD)
  check_linker_flag(C "-fuse-ld=lld" METAFLUX_C_LLD_SUPPORTED)
  if(NOT METAFLUX_C_LLD_SUPPORTED)
    message(FATAL_ERROR "METAFLUX_USE_LLD requires Clang with a working LLD linker")
  endif()
  if(CMAKE_CXX_COMPILER_LOADED)
    check_linker_flag(CXX "-fuse-ld=lld" METAFLUX_CXX_LLD_SUPPORTED)
    if(NOT METAFLUX_CXX_LLD_SUPPORTED)
      message(FATAL_ERROR "METAFLUX_USE_LLD requires Clang++ with a working LLD linker")
    endif()
  endif()
endif()

function(metaflux_configure_target target language)
  if(language STREQUAL "C")
    target_compile_features(${target} PRIVATE c_std_17)
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE C)
  elseif(language STREQUAL "CXX")
    target_compile_features(${target} PRIVATE cxx_std_20)
  else()
    message(FATAL_ERROR "Unknown MetaFlux target language: ${language}")
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(METAFLUX_ENABLE_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()

  if(METAFLUX_USE_LLD)
    target_link_options(${target} PRIVATE -fuse-ld=lld)
  endif()

  if(METAFLUX_ENABLE_FRAME_POINTERS)
    target_compile_options(${target} PRIVATE -fno-omit-frame-pointer)
  endif()

  if(METAFLUX_ENABLE_SANITIZERS)
    target_compile_options(${target} PRIVATE -fno-omit-frame-pointer -fsanitize=address,undefined)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()

  if(METAFLUX_ENABLE_COVERAGE)
    target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  endif()

  if(METAFLUX_PGO_MODE STREQUAL "GENERATE")
    target_compile_options(
      ${target}
      PRIVATE "-fprofile-instr-generate=${METAFLUX_PGO_RAW_PATTERN}"
    )
    target_link_options(
      ${target}
      PRIVATE "-fprofile-instr-generate=${METAFLUX_PGO_RAW_PATTERN}"
    )
  elseif(METAFLUX_PGO_MODE STREQUAL "USE")
    target_compile_options(
      ${target}
      PRIVATE
        "-fprofile-instr-use=${METAFLUX_PGO_PROFILE}"
        -Wno-profile-instr-unprofiled
        -Wno-profile-instr-out-of-date
    )
    target_link_options(${target} PRIVATE "-fprofile-instr-use=${METAFLUX_PGO_PROFILE}")
  endif()

  if(METAFLUX_ENABLE_LTO)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES ${language})
    if(NOT ipo_supported)
      message(FATAL_ERROR "IPO/LTO is unavailable: ${ipo_error}")
    endif()
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set(ipo_compile_options "${CMAKE_${language}_COMPILE_OPTIONS_IPO}")
    if(CMAKE_${language}_COMPILER_ID STREQUAL "Clang" AND
       NOT "${ipo_compile_options}" MATCHES "(^|;)\\-flto=thin($|;)")
      message(FATAL_ERROR "Clang release IPO must resolve to ThinLTO (-flto=thin)")
    endif()
  endif()
endfunction()

function(metaflux_add_component)
  cmake_parse_arguments(
    component
    ""
    "ID;ROLE;OPTION;DIRECTORY;TARGET;LANGUAGE"
    ""
    ${ARGN}
  )

  foreach(required_argument IN ITEMS ID ROLE OPTION DIRECTORY TARGET LANGUAGE)
    if(NOT component_${required_argument})
      message(FATAL_ERROR "metaflux_add_component requires ${required_argument}")
    endif()
  endforeach()

  if(${component_OPTION})
    if(component_LANGUAGE STREQUAL "C")
      metaflux_enforce_c_only_directory("${CMAKE_CURRENT_SOURCE_DIR}/${component_DIRECTORY}")
    endif()
    add_subdirectory("${component_DIRECTORY}")
    if(NOT TARGET ${component_TARGET})
      message(FATAL_ERROR "Component ${component_ID} did not define ${component_TARGET}")
    endif()
    set_target_properties(
      ${component_TARGET}
      PROPERTIES
        METAFLUX_COMPONENT_ID "${component_ID}"
        METAFLUX_COMPONENT_ROLE "${component_ROLE}"
    )
    metaflux_register_component_graph_entry(
      "${component_TARGET}"
      "${component_ID}"
      "${component_ROLE}"
      "${component_LANGUAGE}"
    )
  endif()
endfunction()
