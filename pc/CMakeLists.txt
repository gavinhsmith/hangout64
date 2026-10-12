cmake_minimum_required(VERSION 3.20)
project(hangout64 C)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(hangout64 main.c ../core/world.c ../core/place.c ../core/guy_mesh.c ../core/default_place.c)
target_include_directories(hangout64 PRIVATE ../core)
target_link_libraries(hangout64 PRIVATE SDL3::SDL3 OpenGL::GL)
# Same output dir for every generator/config, so CI knows where to look.
set_target_properties(hangout64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)
if(APPLE)
    target_compile_definitions(hangout64 PRIVATE GL_SILENCE_DEPRECATION)
endif()
