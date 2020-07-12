cmake_minimum_required(VERSION 3.0.2)
project(vklearn)

add_subdirectory(third_party)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

set(SRCS
"src/Main.cpp"

"src/Engine.cpp"
"include/Engine.h"

"src/Render.cpp"
"include/Render.h"

"src/Viewport.cpp"
"include/Viewport.h"

"src/imgui.cpp"
"src/imgui_demo.cpp"
"src/imgui_draw.cpp"
"src/imgui_impl_sdl.cpp"
"src/imgui_impl_vulkan.cpp"
"src/imgui_widgets.cpp"
)

find_package(Vulkan)

if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Unable to find Vulkan")
endif ()

add_executable(vklearn WIN32 ${SRCS})
target_link_libraries(vklearn glm SDL2-static SDL2main GSL ${Vulkan_LIBRARIES})
target_include_directories(vklearn PRIVATE "include" ${Vulkan_INCLUDE_DIRS})
set_target_properties(vklearn PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vklearn>")