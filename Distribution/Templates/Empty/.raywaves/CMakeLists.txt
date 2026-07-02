cmake_minimum_required(VERSION 3.10)
set(CMAKE_C_COMPILER "D:/2D-GameEngine/dist/Tools/zig-cc.bat")
set(CMAKE_CXX_COMPILER "D:/2D-GameEngine/dist/Tools/zig-cxx.bat")
project(NewProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-msse4.2)

set(ENGINE_DIR "D:/2D-GameEngine/dist")
set(PROJECT_SRC_DIR "${CMAKE_SOURCE_DIR}/../GameLogic")

add_library(GameLogic SHARED)

file(GLOB_RECURSE SRC_FILES "${PROJECT_SRC_DIR}/*.cpp")
file(GLOB_RECURSE ENGINE_SRC "${ENGINE_DIR}/Core/Engine/*.cpp")
target_sources(GameLogic PRIVATE ${SRC_FILES} ${ENGINE_SRC})

target_include_directories(GameLogic PRIVATE
    "${ENGINE_DIR}/Core/Engine"
    "${ENGINE_DIR}/Core/raylib/include"
)

target_link_directories(GameLogic PRIVATE "${ENGINE_DIR}/Core/raylib/lib")
target_link_libraries(GameLogic PRIVATE raylib dwmapi)

set_target_properties(GameLogic PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
set_target_properties(GameLogic PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
