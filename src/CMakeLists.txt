find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE ${LLVM_INCLUDE_DIRS}
                           ${CLANG_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
  clangTooling
  clangFrontend
)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_interface)
