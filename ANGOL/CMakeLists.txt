cmake_minimum_required(VERSION 3.20)
project(angol_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(angol_cpp WIN32
    src/cpp/AngolGame.cpp
)

target_link_libraries(angol_cpp PRIVATE gdiplus msimg32 winmm)

add_custom_command(TARGET angol_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/1520056982297.jfif"
        "$<TARGET_FILE_DIR:angol_cpp>/1520056982297.jfif"
)
