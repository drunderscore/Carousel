cmake_minimum_required(VERSION 3.16)
project(Carousel)

set(CMAKE_CXX_STANDARD 20)

set(JMP_OPENGL ON)
add_subdirectory(JMP)

# FIXME: I don't believe GLFW comes with the package script thing on Windows... probably need to do something with
#        package registries to make it build out of the box?
find_package(glfw3 REQUIRED)

add_library(imgui
        imgui/imgui.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
        )

target_include_directories(imgui PRIVATE imgui)

# This DOES build on Windows if you manually massage it into building (aka, manually giving it all the paths it wants)
add_executable(Carousel
        src/Application.cpp
        src/main.cpp
        src/NDISourceWindow.cpp
        )

target_include_directories(Carousel SYSTEM PRIVATE imgui ${PROJECT_SOURCE_DIR} JMP/src)
# FIXME: This doesn't link to NDI on Windows properly, because the library is differently named
# FIXME: I believe glfw on Windows is actually glfw3, for some reason
target_link_libraries(Carousel PRIVATE imgui glfw JMP ndi)