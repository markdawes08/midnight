cmake_minimum_required(VERSION 3.25)

project(midnight
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

find_package(SDL3 CONFIG QUIET)

if(SDL3_FOUND)
    set(MIDNIGHT_SDL_TARGET SDL3::SDL3)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
    set(MIDNIGHT_SDL_TARGET PkgConfig::SDL3)
endif()

add_executable(midnight
    src/main.cpp
    src/midnight/core/Application.cpp
    src/midnight/platform/SdlContext.cpp
    src/midnight/platform/Window.cpp
)

target_include_directories(midnight
    PRIVATE
        src
)

target_link_libraries(midnight
    PRIVATE
        ${MIDNIGHT_SDL_TARGET}
        Vulkan::Vulkan
)

target_compile_options(midnight
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(midnight PRIVATE MIDNIGHT_DEBUG=1)
else()
    target_compile_definitions(midnight PRIVATE MIDNIGHT_DEBUG=0)
endif()
