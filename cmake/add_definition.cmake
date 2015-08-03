# on startup, this is unset, but we'll set it to an empty string anyway
set_property(GLOBAL PROPERTY PROJECT_CONFIG_DEFINITIONS "")

function(add_config_define definition)
  message("Add definition ${definition}") 
  if(ARGN)
    set_property(GLOBAL APPEND PROPERTY PROJECT_CONFIG_DEFINITIONS "${definition} ${ARGN}")
  else()
    set_property(GLOBAL APPEND PROPERTY PROJECT_CONFIG_DEFINITIONS "${definition}")
  endif()

endfunction()
