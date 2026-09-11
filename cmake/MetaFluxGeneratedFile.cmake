include_guard(GLOBAL)

# Preserve literal source bytes and Ninja timestamps when configure is repeated.
function(metaflux_write_if_different output content)
  if(EXISTS "${output}")
    file(READ "${output}" existing)
    if("${existing}" STREQUAL "${content}")
      return()
    endif()
  endif()
  file(WRITE "${output}" "${content}")
endfunction()
