cmake_minimum_required(VERSION 3.20)
project(hangout64 C)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The C generated from assets, as in core/core.mk.
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(GEN ${CMAKE_BINARY_DIR}/gen)
set(TOOLS ${ROOT}/tools/objload.py ${ROOT}/tools/texload.py)
file(GLOB ASSETS ${ROOT}/assets/model/* ${ROOT}/assets/texture/*)
add_custom_command(OUTPUT ${GEN}/guy_mesh.c
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}
    COMMAND Python3::Interpreter ${ROOT}/tools/obj2c.py ${ROOT}/assets/model/guy.obj ${GEN}/guy_mesh.c guy_mesh 255,255,255
            ${ROOT}/assets/texture/guy_base.png guy_texture ${ROOT}/assets/texture/guy_coat.png
    DEPENDS ${ROOT}/tools/obj2c.py ${TOOLS} ${ASSETS})
add_custom_command(OUTPUT ${GEN}/default_place.c
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}
    COMMAND Python3::Interpreter ${ROOT}/tools/placec.py ${ROOT}/assets/scene/course.json ${GEN}/default_place.c default_place
    DEPENDS ${ROOT}/tools/placec.py ${TOOLS} ${ROOT}/assets/scene/course.json ${ASSETS})

add_executable(hangout64 main.c ${ROOT}/core/world.c ${ROOT}/core/place.c ${GEN}/guy_mesh.c ${GEN}/default_place.c)
target_include_directories(hangout64 PRIVATE ${ROOT}/core)
target_link_libraries(hangout64 PRIVATE SDL3::SDL3 OpenGL::GL)
# Same output dir for every generator/config, so CI knows where to look.
set_target_properties(hangout64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)
if(APPLE)
    target_compile_definitions(hangout64 PRIVATE GL_SILENCE_DEPRECATION)
endif()
