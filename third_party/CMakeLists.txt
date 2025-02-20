add_library(${PROJECT_NAME}_dependencies INTERFACE)

if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/mustache)
  return()
endif()

add_subdirectory(mustache)

target_link_libraries(${PROJECT_NAME}_dependencies INTERFACE mustache)

