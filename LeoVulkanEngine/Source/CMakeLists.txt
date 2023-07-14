
# Add source to this project's executable.
include_directories(Engine)

add_executable(LeoVKEngine
    main.cpp
    Engine/VKEngine.cpp
    Engine/VKEngine.h
    Engine/VKTypes.h
    Engine/VKInitializers.cpp
    Engine/VKInitializers.h)


set_property(TARGET LeoVKEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:LeoVKEngine>")

target_include_directories(LeoVKEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(LeoVKEngine vkbootstrap vma glm tinyobjloader imgui stb_image)
target_link_libraries(LeoVKEngine Vulkan::Vulkan sdl2)

add_dependencies(LeoVKEngine Shaders)
