# Component registry and dependency-graph export for architecture checks
# (decision-0011). Every boundary target registers itself with a stable component id,
# role, and language; the generated JSON is validated by
# tools/check-component-graph.py so that illegal dependency directions fail the
# build instead of surviving as convention.

function(metaflux_register_component_graph_entry target id role language)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "component graph entry: '${target}' is not a target")
  endif()
  set_property(
    GLOBAL APPEND PROPERTY METAFLUX_COMPONENT_GRAPH_ENTRIES
    "${id}|${role}|${target}|${language}"
  )
endfunction()

function(metaflux_component_graph_target_id target out_var)
  get_property(entries GLOBAL PROPERTY METAFLUX_COMPONENT_GRAPH_ENTRIES)
  set(result "")
  foreach(entry IN LISTS entries)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 2 entry_target)
    if(entry_target STREQUAL target)
      list(GET fields 0 entry_id)
      set(result "${entry_id}")
      break()
    endif()
  endforeach()
  set("${out_var}" "${result}" PARENT_SCOPE)
endfunction()

function(metaflux_write_component_graph output_path)
  get_property(entries GLOBAL PROPERTY METAFLUX_COMPONENT_GRAPH_ENTRIES)

  set(component_ids "")
  set(component_lines "")
  foreach(entry IN LISTS entries)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 entry_id)
    list(GET fields 1 entry_role)
    list(GET fields 2 entry_target)
    list(GET fields 3 entry_language)
    if(entry_id IN_LIST component_ids)
      message(FATAL_ERROR "duplicate component graph id: ${entry_id}")
    endif()
    list(APPEND component_ids "${entry_id}")
    list(
      APPEND component_lines
      "  {\"id\": \"${entry_id}\", \"role\": \"${entry_role}\", \"target\": \"${entry_target}\", \"language\": \"${entry_language}\"}"
    )
  endforeach()

  set(edge_keys "")
  set(edge_lines "")
  foreach(entry IN LISTS entries)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 entry_id)
    list(GET fields 2 entry_target)

    set(dependencies "")
    get_target_property(link_dependencies "${entry_target}" LINK_LIBRARIES)
    if(link_dependencies)
      list(APPEND dependencies ${link_dependencies})
    endif()
    get_target_property(
      interface_dependencies "${entry_target}" INTERFACE_LINK_LIBRARIES
    )
    if(interface_dependencies)
      list(APPEND dependencies ${interface_dependencies})
    endif()

    foreach(dependency IN LISTS dependencies)
      string(
        REGEX REPLACE "^\\$<LINK_ONLY:(.+)>$" "\\1" dependency_name "${dependency}"
      )
      if(
        dependency_name MATCHES "^(PRIVATE|PUBLIC|INTERFACE|debug|optimized|general)$"
        OR NOT TARGET "${dependency_name}"
      )
        continue()
      endif()

      set(canonical "${dependency_name}")
      get_target_property(aliased "${dependency_name}" ALIASED_TARGET)
      if(aliased)
        set(canonical "${aliased}")
      endif()

      metaflux_component_graph_target_id("${canonical}" dependency_id)
      if(dependency_id AND NOT dependency_id STREQUAL entry_id)
        set(edge_key "${entry_id}=>${dependency_id}")
        if(NOT edge_key IN_LIST edge_keys)
          list(APPEND edge_keys "${edge_key}")
          list(APPEND edge_lines "  [\"${entry_id}\", \"${dependency_id}\"]")
        endif()
      endif()
    endforeach()
  endforeach()

  list(JOIN component_lines ",\n" component_json)
  list(JOIN edge_lines ",\n" edge_json)
  file(
    WRITE "${output_path}"
    "{\n\"components\": [\n${component_json}\n],\n\"edges\": [\n${edge_json}\n]\n}\n"
  )
  message(STATUS "MetaFlux component graph written to ${output_path}")
endfunction()
