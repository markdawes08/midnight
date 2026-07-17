cmake_minimum_required(VERSION 3.25)

project(midnight
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED COMPONENTS glslangValidator)

find_package(SDL3 CONFIG QUIET)

if(SDL3_FOUND)
    set(MIDNIGHT_SDL_TARGET SDL3::SDL3)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
    set(MIDNIGHT_SDL_TARGET PkgConfig::SDL3)
endif()

set(MIDNIGHT_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(MIDNIGHT_ASSET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets")

set(MIDNIGHT_ASSET_FILES
    assets/README.md
    assets/tilesets/basic_village/SOURCE.md
    assets/tilesets/basic_village/furniture.png
    assets/tilesets/basic_village/house_tileset.png
    assets/tilesets/basic_village/outdoor_tileset.png
    assets/tilesets/basic_village/trees_and_bushes.png
)

set(MIDNIGHT_ASSET_OUTPUTS)

foreach(ASSET_FILE IN LISTS MIDNIGHT_ASSET_FILES)
    set(ASSET_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_FILE}")
    set(ASSET_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ASSET_FILE}")
    get_filename_component(ASSET_OUTPUT_DIRECTORY "${ASSET_OUTPUT_PATH}" DIRECTORY)

    add_custom_command(
        OUTPUT "${ASSET_OUTPUT_PATH}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ASSET_OUTPUT_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ASSET_SOURCE_PATH}"
            "${ASSET_OUTPUT_PATH}"
        DEPENDS "${ASSET_SOURCE_PATH}"
        COMMENT "Copying asset ${ASSET_FILE}"
        VERBATIM
    )

    list(APPEND MIDNIGHT_ASSET_OUTPUTS "${ASSET_OUTPUT_PATH}")
endforeach()

function(midnight_compile_shader OUTPUT_VARIABLE SOURCE_FILE)
    get_filename_component(SHADER_FILE_NAME "${SOURCE_FILE}" NAME)

    set(SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
    set(OUTPUT_PATH "${MIDNIGHT_SHADER_OUTPUT_DIR}/${SHADER_FILE_NAME}.spv")

    add_custom_command(
        OUTPUT "${OUTPUT_PATH}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${MIDNIGHT_SHADER_OUTPUT_DIR}"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V "${SOURCE_PATH}" -o "${OUTPUT_PATH}"
        DEPENDS "${SOURCE_PATH}"
        COMMENT "Compiling shader ${SOURCE_FILE}"
        VERBATIM
    )

    set("${OUTPUT_VARIABLE}" "${OUTPUT_PATH}" PARENT_SCOPE)
endfunction()

midnight_compile_shader(MIDNIGHT_TRIANGLE_VERT_SPV shaders/triangle.vert)
midnight_compile_shader(MIDNIGHT_TRIANGLE_FRAG_SPV shaders/triangle.frag)

add_custom_target(midnight_shaders
    DEPENDS
        "${MIDNIGHT_TRIANGLE_VERT_SPV}"
        "${MIDNIGHT_TRIANGLE_FRAG_SPV}"
)

add_custom_target(midnight_assets
    DEPENDS ${MIDNIGHT_ASSET_OUTPUTS}
)

add_executable(midnight
    src/main.cpp
    src/midnight/core/Application.cpp
    src/midnight/core/File.cpp
    src/midnight/platform/SdlContext.cpp
    src/midnight/platform/Window.cpp
    src/midnight/renderer/vulkan/VulkanBuffer.cpp
    src/midnight/renderer/vulkan/VulkanDevice.cpp
    src/midnight/renderer/vulkan/VulkanFrameRenderer.cpp
    src/midnight/renderer/vulkan/VulkanGraphicsPipeline.cpp
    src/midnight/renderer/vulkan/VulkanImage.cpp
    src/midnight/renderer/vulkan/VulkanInstance.cpp
    src/midnight/renderer/vulkan/VulkanRenderPass.cpp
    src/midnight/renderer/vulkan/VulkanSampler.cpp
    src/midnight/renderer/vulkan/VulkanSurface.cpp
    src/midnight/renderer/vulkan/VulkanSwapchain.cpp
    src/midnight/renderer/vulkan/VulkanTextureDescriptor.cpp
    src/midnight/renderer/vulkan/VulkanTransferContext.cpp
    src/midnight/renderer/vulkan/VulkanUtils.cpp
)

add_dependencies(midnight midnight_assets midnight_shaders)

target_include_directories(midnight
    PRIVATE
        src
)

target_link_libraries(midnight
    PRIVATE
        ${MIDNIGHT_SDL_TARGET}
        Vulkan::Vulkan
)

target_compile_definitions(midnight
    PRIVATE
        "MIDNIGHT_ASSET_DIR=\"${MIDNIGHT_ASSET_OUTPUT_DIR}\""
        "MIDNIGHT_SHADER_DIR=\"${MIDNIGHT_SHADER_OUTPUT_DIR}\""
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
