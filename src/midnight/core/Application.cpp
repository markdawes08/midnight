#include "midnight/core/Application.hpp"

#include "midnight/assets/Png.hpp"
#include "midnight/renderer/Vertex2D.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace midnight {
namespace {

constexpr std::uint32_t kInitialWindowWidth = 1280;
constexpr std::uint32_t kInitialWindowHeight = 720;
constexpr std::uint32_t kOutdoorTilesetWidth = 192;
constexpr std::uint32_t kOutdoorTilesetHeight = 128;
constexpr std::uint32_t kTilesetPreviewScale = 3;
constexpr std::size_t kOutdoorTilesetByteSize =
    static_cast<std::size_t>(kOutdoorTilesetWidth) *
    static_cast<std::size_t>(kOutdoorTilesetHeight) *
    RgbaImage::bytes_per_pixel;

constexpr float kTilesetPreviewHalfWidth =
    static_cast<float>(kOutdoorTilesetWidth * kTilesetPreviewScale) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kTilesetPreviewHalfHeight =
    static_cast<float>(kOutdoorTilesetHeight * kTilesetPreviewScale) /
    static_cast<float>(kInitialWindowHeight);

constexpr std::array<Vertex2D, 4> kQuadVertices{{
    Vertex2D{-kTilesetPreviewHalfWidth, -kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
    Vertex2D{ kTilesetPreviewHalfWidth, -kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
    Vertex2D{ kTilesetPreviewHalfWidth,  kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    Vertex2D{-kTilesetPreviewHalfWidth,  kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f}
}};

constexpr std::array<std::uint16_t, 6> kQuadIndices{{
    0, 1, 2,
    2, 3, 0
}};

constexpr VkDeviceSize kQuadVertexBufferSize =
    sizeof(Vertex2D) * kQuadVertices.size();

constexpr VkDeviceSize kQuadIndexBufferSize =
    sizeof(std::uint16_t) * kQuadIndices.size();

}

Application::Application()
    : sdl_(),
      window_(Window::CreateInfo{
          .title = "Midnight",
          .width = static_cast<int>(kInitialWindowWidth),
          .height = static_cast<int>(kInitialWindowHeight),
          .resizable = true,
          .vulkan = true
      }),
      vulkan_instance_(),
      vulkan_surface_(window_, vulkan_instance_),
      vulkan_device_(vulkan_instance_, vulkan_surface_),
      vulkan_transfer_context_(vulkan_device_),
      vulkan_swapchain_(window_, vulkan_device_, vulkan_surface_),
      vulkan_render_pass_(vulkan_device_, vulkan_swapchain_),
      vulkan_graphics_pipeline_(
          vulkan_device_,
          vulkan_swapchain_,
          vulkan_render_pass_
      ),
      quad_vertex_buffer_(
          vulkan_device_,
          kQuadVertexBufferSize,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      ),
      quad_index_buffer_(
          vulkan_device_,
          kQuadIndexBufferSize,
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      ),
      texture_image_(
          vulkan_device_,
          VulkanImage::CreateInfo{
              .extent = VkExtent2D{
                  .width = kOutdoorTilesetWidth,
                  .height = kOutdoorTilesetHeight
              },
              .format = VK_FORMAT_R8G8B8A8_SRGB,
              .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT,
              .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
          }
      ),
      texture_sampler_(
          vulkan_device_,
          VulkanSampler::CreateInfo{}
      ),
      texture_descriptor_(
          vulkan_device_,
          vulkan_graphics_pipeline_.descriptor_set_layout(),
          texture_image_,
          texture_sampler_
      ),
      vulkan_frame_renderer_(
          vulkan_device_,
          vulkan_swapchain_,
          vulkan_render_pass_,
          vulkan_graphics_pipeline_,
          texture_descriptor_,
          quad_vertex_buffer_,
          quad_index_buffer_,
          static_cast<std::uint32_t>(kQuadIndices.size()),
          VK_INDEX_TYPE_UINT16
      )
{
    quad_vertex_buffer_.upload(
        kQuadVertices.data(),
        kQuadVertexBufferSize
    );

    quad_index_buffer_.upload(
        kQuadIndices.data(),
        kQuadIndexBufferSize
    );

    const std::filesystem::path outdoor_tileset_path =
        std::filesystem::path(MIDNIGHT_ASSET_DIR) /
        "tilesets/basic_village/outdoor_tileset.png";

    const RgbaImage outdoor_tileset =
        load_png_rgba8(outdoor_tileset_path);

    if (outdoor_tileset.width != kOutdoorTilesetWidth ||
        outdoor_tileset.height != kOutdoorTilesetHeight) {
        throw std::runtime_error(
            "Unexpected outdoor tileset dimensions: expected " +
            std::to_string(kOutdoorTilesetWidth) +
            "x" +
            std::to_string(kOutdoorTilesetHeight) +
            ", got " +
            std::to_string(outdoor_tileset.width) +
            "x" +
            std::to_string(outdoor_tileset.height)
        );
    }

    if (outdoor_tileset.byte_size() != kOutdoorTilesetByteSize) {
        throw std::runtime_error(
            "Unexpected outdoor tileset byte size: expected " +
            std::to_string(kOutdoorTilesetByteSize) +
            ", got " +
            std::to_string(outdoor_tileset.byte_size())
        );
    }

    std::cout << "[Midnight] Decoded PNG: "
              << outdoor_tileset_path.lexically_normal().string()
              << " "
              << outdoor_tileset.width
              << "x"
              << outdoor_tileset.height
              << " RGBA8 ("
              << outdoor_tileset.byte_size()
              << " bytes)"
              << '\n';

    vulkan_transfer_context_.upload_to_new_sampled_image(
        texture_image_,
        outdoor_tileset.pixels.data(),
        static_cast<VkDeviceSize>(outdoor_tileset.byte_size())
    );
}

int Application::run()
{
    print_startup_info();

    while (running_) {
        poll_events();

        if (!running_) {
            break;
        }

        const bool frame_rendered = vulkan_frame_renderer_.draw_frame();

        if (!frame_rendered) {
            std::cout << "[Midnight] Swapchain needs recreation. Exiting for this increment.\n";
            running_ = false;
        }
    }

    return 0;
}

void Application::print_startup_info() const
{
    std::cout << "[Midnight] Application started\n";
    std::cout << "[Midnight] Window size: "
              << window_.width()
              << "x"
              << window_.height()
              << '\n';
    std::cout << "[Midnight] Window pixel size: "
              << window_.pixel_width()
              << "x"
              << window_.pixel_height()
              << '\n';
    std::cout << "[Midnight] Rendering the outdoor tileset at "
              << kTilesetPreviewScale
              << "x scale\n";
    std::cout << "[Midnight] Press Escape or close the window to quit\n";
}

void Application::poll_events()
{
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    running_ = false;
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                window_.refresh_size();
                std::cout << "[Midnight] Window resized: "
                          << window_.width()
                          << "x"
                          << window_.height()
                          << " logical, "
                          << window_.pixel_width()
                          << "x"
                          << window_.pixel_height()
                          << " pixels"
                          << '\n';
                break;

            default:
                break;
        }
    }
}

}
