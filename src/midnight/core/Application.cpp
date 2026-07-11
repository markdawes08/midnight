#include "midnight/core/Application.hpp"

#include "midnight/renderer/Vertex2D.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <iostream>

namespace midnight {
namespace {

constexpr std::array<Vertex2D, 3> kTriangleVertices{{
    Vertex2D{ 0.00f, -0.55f, 0.95f, 0.25f, 0.45f },
    Vertex2D{ 0.55f,  0.45f, 0.25f, 0.90f, 0.70f },
    Vertex2D{-0.55f,  0.45f, 0.35f, 0.45f, 1.00f }
}};

constexpr VkDeviceSize kTriangleVertexBufferSize =
    sizeof(Vertex2D) * kTriangleVertices.size();

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
      triangle_vertex_buffer_(
          vulkan_device_,
          kTriangleVertexBufferSize,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      ),
      vulkan_frame_renderer_(
          vulkan_device_,
          vulkan_swapchain_,
          vulkan_render_pass_,
          vulkan_graphics_pipeline_,
          triangle_vertex_buffer_,
          static_cast<std::uint32_t>(kTriangleVertices.size())
      )
{
    triangle_vertex_buffer_.upload(
        kTriangleVertices.data(),
        kTriangleVertexBufferSize
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
    std::cout << "[Midnight] Rendering a triangle from a Vulkan vertex buffer\n";
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
