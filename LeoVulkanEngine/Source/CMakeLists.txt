# Add source to this project's executable.
include_directories(Engine)

file(GLOB_RECURSE ENGINE_HEADS "Engine/*.h")
file(GLOB_RECURSE ENGINE_SOURCES "Engine/*.cpp")

add_executable(LeoVKEngine
    main.cpp ${ENGINE_HEADS} ${ENGINE_SOURCES})


set_property(TARGET LeoVKEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:LeoVKEngine>")

target_include_directories(LeoVKEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(LeoVKEngine vkbootstrap vma glm tinyobjloader imgui stb_image)
target_link_libraries(LeoVKEngine Vulkan::Vulkan sdl2 fastgltf::fastgltf)

add_dependencies(LeoVKEngine Shaders)
