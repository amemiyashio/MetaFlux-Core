foreach(required_variable IN ITEMS READELF PROVIDER)
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

foreach(symbol IN ITEMS cuStreamQuery_ptsz cuStreamSynchronize_ptsz cuEventRecord_ptsz)
  string(REGEX MATCHALL "${symbol}@@libcuda\\.so\\.1" matches "${dynamic_symbols}")
  list(LENGTH matches match_count)
  if(NOT match_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one ${symbol}@@libcuda.so.1 export, found ${match_count}")
  endif()
endforeach()
