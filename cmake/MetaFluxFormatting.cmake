find_program(METAFLUX_CLANG_FORMAT_EXECUTABLE NAMES clang-format clang-format-22)

file(
  GLOB_RECURSE METAFLUX_FORMAT_SOURCES
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/contracts/*.h"
  "${PROJECT_SOURCE_DIR}/runtime/*.c"
  "${PROJECT_SOURCE_DIR}/runtime/*.h"
  "${PROJECT_SOURCE_DIR}/runtime/*.cpp"
  "${PROJECT_SOURCE_DIR}/runtime/*.hpp"
  "${PROJECT_SOURCE_DIR}/compiler/*.cpp"
  "${PROJECT_SOURCE_DIR}/compiler/*.hpp"
  "${PROJECT_SOURCE_DIR}/plugins/*.c"
  "${PROJECT_SOURCE_DIR}/plugins/*.h"
  "${PROJECT_SOURCE_DIR}/plugins/*.cpp"
  "${PROJECT_SOURCE_DIR}/plugins/*.hpp"
  "${PROJECT_SOURCE_DIR}/services/*.cpp"
  "${PROJECT_SOURCE_DIR}/tests/*.c"
  "${PROJECT_SOURCE_DIR}/tests/*.cpp"
)

if(METAFLUX_CLANG_FORMAT_EXECUTABLE)
  add_custom_target(
    metaflux-format-check
    COMMAND "${METAFLUX_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${METAFLUX_FORMAT_SOURCES}
    COMMENT "Checking C and C++ formatting"
    VERBATIM
  )
else()
  add_custom_target(
    metaflux-format-check
    COMMAND "${CMAKE_COMMAND}" -E echo "clang-format 22 is required for format checks"
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM
  )
endif()

