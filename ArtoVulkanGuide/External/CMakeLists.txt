find_package(Vulkan REQUIRED)
include(ExternalProject)

add_library(glm INTERFACE)
add_library(vma INTERFACE)
#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)
target_include_directories(glm INTERFACE glm )

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb_image")

add_library(json INTERFACE)
target_include_directories(json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json" )

add_library(tinyobjloader STATIC)
target_sources(tinyobjloader PRIVATE
    tinyobjloader/tiny_obj_loader.h
    tinyobjloader/tiny_obj_loader.cc)

add_library(vkbootstrap STATIC)
target_sources(vkbootstrap PRIVATE
    vkbootstrap/VkBootstrap.h
    vkbootstrap/VkBootstrap.cpp)
target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)

add_library(spirv_reflect STATIC)
target_sources(spirv_reflect PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.c")

add_library(lz4 STATIC)
target_sources(lz4 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4.c")
target_include_directories(lz4 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lz4")

target_include_directories(tinyobjloader PUBLIC tinyobjloader)

add_library(sdl2 INTERFACE)
set(SDL2_DIR "${CMAKE_CURRENT_LIST_DIR}/SDL2/cmake")
find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(sdl2 INTERFACE SDL2::SDL2 SDL2::SDL2main)

add_subdirectory(fastgltf)
add_subdirectory(tinygltfloader)

ExternalProject_Add(nvtools
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nvidia-texture-tools"
    BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nvLibs"
    CMAKE_ARGS -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/nvLibs -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/nvLibs -DCMAKE_CXX_STANDARD=17)

add_library(nvtt INTERFACE)
target_include_directories(nvtt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nvidia-texture-tools/src/nvtt")

set(nvttFolder "${CMAKE_CURRENT_SOURCE_DIR}/nvLibs/$<IF:$<CONFIG:Debug>,Debug,Release>")

target_link_libraries(nvtt INTERFACE "${nvttFolder}/nvtt.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/bc6h.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/bc7.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/EtcLib.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/nvcore.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/nvimage.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/nvmath.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/nvsquish.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/nvthread.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/posh.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/rg_etc1.lib")
target_link_libraries(nvtt INTERFACE "${nvttFolder}/squishd.lib")
#add_subdirectory(nvidia-texture-tools)

set(TRACY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tracy" CACHE FILEPATH "Path to Tracy")
add_library(tracy OBJECT "${TRACY_DIR}/TracyClient.cpp")
target_include_directories(tracy PUBLIC "${TRACY_DIR}")
target_compile_definitions(tracy PUBLIC TRACY_ENABLE)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE 
    imgui/imgui.h
    imgui/imgui.cpp

    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp

    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_sdl.cpp
    )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)

add_library(fmt_lib STATIC)
target_include_directories(fmt_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/fmt_lib")
file(GLOB FMT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/fmt_lib/fmt/*")
target_sources(fmt_lib PRIVATE ${FMT_FILES})