# Copyright (c) 2018 BaiQiang. All rights reserved.

cmake_minimum_required(VERSION 3.7)
project(text)

set(CMAKE_CXX_STANDARD 11)
set(SOURCE_FILES
        main.cpp
        glad.c
        shader.c
        screenshot.c)

add_executable(text ${SOURCE_FILES})


find_package(OpenGL REQUIRED)
find_package(glfw REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${OPENGL_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

target_link_libraries(text ${OPENGL_LIBRARIES} ${GLFW_LIBRARIES} icudt.lib icuind.lib icuiod.lib icuucd.lib icutud.lib lib/harfbuzz lib/freetype "legacy_stdio_definitions.lib")

add_custom_command(TARGET text PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:text>/res
        )

add_custom_command(TARGET text PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib
        $<TARGET_FILE_DIR:text>/lib
        )

add_custom_command(TARGET text PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts
        $<TARGET_FILE_DIR:text>/fonts
        )