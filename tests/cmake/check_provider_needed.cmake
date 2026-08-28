foreach(required_variable IN ITEMS READELF PROVIDER EXPECTED_SONAME EXPECTED_NEEDED)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

execute_process(
  COMMAND "${READELF}" -dW "${PROVIDER}"
  RESULT_VARIABLE readelf_status
  OUTPUT_VARIABLE dynamic_section
  ERROR_VARIABLE readelf_error
)
if(NOT readelf_status EQUAL 0)
  message(FATAL_ERROR "readelf failed for ${PROVIDER}: ${readelf_error}")
endif()

string(REPLACE "\n" ";" dynamic_lines "${dynamic_section}")
set(observed_needed)
set(observed_soname)
foreach(dynamic_line IN LISTS dynamic_lines)
  if(dynamic_line MATCHES "NEEDED.*\\[([^]]+)\\]")
    list(APPEND observed_needed "${CMAKE_MATCH_1}")
  elseif(dynamic_line MATCHES "SONAME.*\\[([^]]+)\\]")
    set(observed_soname "${CMAKE_MATCH_1}")
  endif()
endforeach()

if(NOT "${observed_soname}" STREQUAL "${EXPECTED_SONAME}")
  message(
    FATAL_ERROR
    "Unexpected SONAME in ${PROVIDER}: '${observed_soname}'; expected '${EXPECTED_SONAME}'"
  )
endif()

set(expected_needed ${EXPECTED_NEEDED})
list(SORT observed_needed)
list(SORT expected_needed)
if(NOT "${observed_needed}" STREQUAL "${expected_needed}")
  message(
    FATAL_ERROR
    "Unexpected DT_NEEDED in ${PROVIDER}: '${observed_needed}'; expected '${expected_needed}'"
  )
endif()
