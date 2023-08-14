file(GLOB_RECURSE Asset_HEAD "AssetsLoader/*.hpp")
file(GLOB_RECURSE Asset_SRC "AssetsLoader/*.cpp")

add_library(AssetLib STATIC ${Asset_SRC} ${Asset_HEAD})
target_include_directories(AssetLib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AssetLib PRIVATE json lz4 glm)

add_executable(AssetBaker "AssetsLoader/AssetBaker.cpp")
#set_property(TARGET baker PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:extra>")
target_include_directories(AssetBaker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AssetBaker PUBLIC tinyobjloader stb_image json lz4 AssetLib tinyGLTF nvtt glm)

file(GLOB_RECURSE FRAMEWORK_HEAD "VulkanObjects/*.hpp" "VulkanObjects/*.h")
file(GLOB_RECURSE FRAMEWORK_SRC "VulkanObjects/*.cpp")

add_library(FrameworkLib STATIC ${FRAMEWORK_SRC} ${FRAMEWORK_HEAD})
target_link_libraries(FrameworkLib AssetLib Vulkan::Vulkan sdl2 vkbootstrap vma glm tinyobjloader imgui stb_image)

add_dependencies(FrameworkLib Shaders)