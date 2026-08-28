foreach(required_variable IN ITEMS READELF PROVIDER EXPECTED_SYMBOL EXPECTED_VERSION)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

execute_process(
  COMMAND "${READELF}" --dyn-syms --wide "${PROVIDER}"
  RESULT_VARIABLE readelf_status
  OUTPUT_VARIABLE dynamic_symbols
  ERROR_VARIABLE readelf_error
)
if(NOT readelf_status EQUAL 0)
  message(FATAL_ERROR "readelf failed for ${PROVIDER}: ${readelf_error}")
endif()

string(
  REGEX MATCHALL
  "mf_[A-Za-z0-9_]+(@@?[A-Za-z0-9_.]+)?"
  observed_exports
  "${dynamic_symbols}"
)
list(SORT observed_exports)
set(expected_export "${EXPECTED_SYMBOL}@@${EXPECTED_VERSION}")
if(NOT "${observed_exports}" STREQUAL "${expected_export}")
  message(
    FATAL_ERROR
    "Unexpected MetaFlux exports in ${PROVIDER}: '${observed_exports}'; expected '${expected_export}'"
  )
endif()
