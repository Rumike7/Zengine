cmake_minimum_required(VERSION 3.10)
project(ZEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Add executable
add_executable(zengine ../src/main.cpp)

# Link SDL2
target_link_libraries(zengine PRIVATE ${SDL2_LIBRARIES})

# Windows-specific: Ensure console compatibility
if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--stack,8388608 -mconsole")
endif()