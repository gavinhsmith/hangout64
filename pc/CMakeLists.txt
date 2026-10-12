cmake_minimum_required(VERSION 3.20)
project(hangout64 C)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(hangout64 main.c ../core/cube.c)
target_include_directories(hangout64 PRIVATE ../core)
target_link_libraries(hangout64 PRIVATE SDL3::SDL3 OpenGL::GL)
