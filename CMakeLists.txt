cmake_minimum_required(VERSION 3.10)
project(ZEngine)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug) 
add_executable(zengine ../src/main.cpp)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--stack,8388608")  # 8MB stack