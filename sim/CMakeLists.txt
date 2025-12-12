cmake_minimum_required(VERSION 3.20)
project(racegym_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(racegym_sim SHARED 
    src/sim.cpp
    src/sim.h
    src/renderer.cpp
    src/renderer.h
    src/track.cpp
    src/track.h
    src/physics.cpp
    src/physics.h
    src/vehicle.cpp
    src/vehicle.h
)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

target_link_libraries(racegym_sim PRIVATE glfw OpenGL::GL glm::glm glad::glad)

if(MSVC)
    target_compile_definitions(racegym_sim PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# Output directories (Visual Studio multi-config)
set_target_properties(racegym_sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/
)
