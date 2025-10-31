cmake_minimum_required(VERSION 3.24)
project(midnight LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Helpful warnings in Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# Find packages from vcpkg toolchain
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)


add_executable(midnight_client src/main_client.cpp)

target_link_libraries(midnight_client PRIVATE volk::volk)
target_include_directories(midnight_client PRIVATE src)
target_sources(midnight_client PRIVATE
  src/gfx/context.cpp
)
target_compile_definitions(midnight_client PRIVATE VK_NO_PROTOTYPES GLFW_INCLUDE_VULKAN)


# On Linux we’ll need pthread
find_package(Threads REQUIRED)


# Link
target_link_libraries(midnight_client PRIVATE
Vulkan::Vulkan
glfw
spdlog::spdlog
vk-bootstrap::vk-bootstrap
volk::volk
Threads::Threads)


# Define in debug to enable validation
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
target_compile_definitions(midnight_client PRIVATE MIDNIGHT_VALIDATION=1)
endif()

# Pick a GLSL->SPIR-V compiler
find_program(GLSLC glslc)
find_program(GLSLANG_VALIDATOR glslangValidator)

if (GLSLC)
  set(SHADER_COMPILER ${GLSLC})
  set(SHADER_FLAGS "")
elseif (GLSLANG_VALIDATOR)
  set(SHADER_COMPILER ${GLSLANG_VALIDATOR})
  set(SHADER_FLAGS "-V")  # SPIR-V
else()
  message(FATAL_ERROR "No GLSL compiler found (need glslc or glslangValidator). Install glslang-tools.")
endif()

# Where compiled shaders land, and tell the program where to load from
set(MIDNIGHT_SHADER_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
add_compile_definitions(SHADER_DIR="${MIDNIGHT_SHADER_BIN_DIR}")

# Shader compilation (GLSL -> SPIR-V)
set(MIDNIGHT_SHADER_SOURCES
  assets/shaders/plane.vert
  assets/shaders/solid_green.frag
  assets/shaders/solid_blue.frag
)
set(MIDNIGHT_SHADER_OUTPUTS)
foreach(SRC ${MIDNIGHT_SHADER_SOURCES})
  get_filename_component(FILE_NAME ${SRC} NAME)
  set(SPV "${MIDNIGHT_SHADER_BIN_DIR}/${FILE_NAME}.spv")
  add_custom_command(OUTPUT ${SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MIDNIGHT_SHADER_BIN_DIR}
    COMMAND ${SHADER_COMPILER} ${SHADER_FLAGS} -o ${SPV} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC}
    DEPENDS ${SRC}
    COMMENT "Compiling ${SRC} -> ${SPV}")
  list(APPEND MIDNIGHT_SHADER_OUTPUTS ${SPV})
endforeach()
add_custom_target(shaders DEPENDS ${MIDNIGHT_SHADER_OUTPUTS})
add_dependencies(midnight_client shaders)

# Add new source files
target_sources(midnight_client PRIVATE
  src/gfx/buffer.cpp
  src/gfx/pipeline.cpp
)

# Header include path for GLM (via vcpkg toolchain)
target_include_directories(midnight_client PRIVATE src)