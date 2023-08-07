file(GLOB_RECURSE FRAMEWORK_HEAD "*.hpp" "*.h")
file(GLOB_RECURSE FRAMEWORK_SRC "*.cpp")

add_library(FrameworkLib STATIC ${FRAMEWORK_SRC} ${FRAMEWORK_HEAD})
target_link_libraries(FrameworkLib Vulkan::Vulkan sdl2 vkbootstrap vma glm tinyobjloader imgui stb_image)

add_dependencies(FrameworkLib Shaders)