file(GLOB_RECURSE HEADS "*.hpp")
file(GLOB_RECURSE SRC "*.cpp")


add_executable(Renderer ${HEADS} ${SRC})
target_include_directories(Renderer PUBLIC ${PROJECT_SOURCE_DIR}/Framework)
target_link_libraries(Renderer FrameworkLib)