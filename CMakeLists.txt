cmake_minimum_required(VERSION 3.16)
project(SkyStriker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# raylib 预编译库
set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/raylib-6.2_win64_mingw-w64")
set(RAYLIB_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-cpp-6.0.3")

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(SkyStriker ${GAME_SOURCES})

target_include_directories(SkyStriker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/screens"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
    "${RAYLIB_CPP_DIR}/include"
    "${RAYLIB_DIR}/include"
)

target_link_directories(SkyStriker PRIVATE "${RAYLIB_DIR}/lib")

if(WIN32)
    target_link_libraries(SkyStriker PRIVATE raylib opengl32 gdi32 winmm shell32)
else()
    target_link_libraries(SkyStriker PRIVATE raylib)
endif()

# 输出到 build/，与 build.bat 保持一致
set_target_properties(SkyStriker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
