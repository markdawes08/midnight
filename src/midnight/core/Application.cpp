#include "midnight/core/Application.hpp"

#include "midnight/renderer/Vertex2D.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <iostream>

namespace midnight {
namespace {

constexpr std::array<Vertex2D, 4> kQuadVertices{{
    Vertex2D{-0.55f, -0.55f, 0.95f, 0.25f, 0.45f, 0.0f, 0.0f },
    Vertex2D{ 0.55f, -0.55f, 0.25f, 0.90f, 0.70f, 1.0f, 0.0f },
    Vertex2D{ 0.55f,  0.55f, 0.35f, 0.45f, 1.00f, 1.0f, 1.0f },
    Vertex2D{-0.55f,  0.55f, 0.95f, 0.80f, 0.25f, 0.0f, 1.0f }
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
          .width = 1280,
          .height = 720,
          .resizable = true,
          .vulkan = true
      }),
      vulkan_instance_(),
      vulkan_surface_(window_, vulkan_instance_),
      vulkan_device_(vulkan_instance_, vulkan_surface_),
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
              .extent = VkExtent2D{.width = 2, .height = 2},
              .format = VK_FORMAT_R8G8B8A8_SRGB,
              .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT,
              .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
          }
      ),
      vulkan_frame_renderer_(
          vulkan_device_,
          vulkan_swapchain_,
          vulkan_render_pass_,
          vulkan_graphics_pipeline_,
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
    std::cout << "[Midnight] Rendering an indexed quad with UV coordinates\n";
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
