cmake_minimum_required(VERSION 3.11)
project(ZEngine)

# Set C++ standard and build type
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# Prefer GLVND for OpenGL
set(OpenGL_GL_PREFERENCE GLVND)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLM

include(FetchContent)

FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)

FetchContent_MakeAvailable(glm)



# ImGui source files
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_BACKENDS ${IMGUI_DIR}/backends)
set(IMGUI_MISC ${IMGUI_DIR}/misc/cpp)

set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_BACKENDS}/imgui_impl_sdl2.cpp
    ${IMGUI_BACKENDS}/imgui_impl_opengl3.cpp
    ${IMGUI_MISC}/imgui_stdlib.cpp
)

# Add include directories for ImGui and tinyfiledialogs
include_directories(${IMGUI_DIR} ${IMGUI_BACKENDS} ${IMGUI_MISC} ${CMAKE_CURRENT_SOURCE_DIR}/external/tinyfiledialogs)

# GLAD
add_library(glad STATIC glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)

# Add your sources
set(SRC
    src/main.cpp
    external/tinyfiledialogs.c
)

# Add executable
add_executable(zengine ${SRC} ${IMGUI_SRC})

# Link libraries
target_link_libraries(zengine PRIVATE
    glad
    SDL2::SDL2main
    SDL2::SDL2
    OpenGL::GL
    glm::glm
)

# If on Windows, link additional libraries for tinyfiledialogs
if(WIN32)
    target_link_libraries(zengine PRIVATE comdlg32)
endif()