#include "midnight/platform/Window.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace midnight {

Window::Window(const CreateInfo& create_info)
    : width_(create_info.width),
      height_(create_info.height),
      pixel_width_(create_info.width),
      pixel_height_(create_info.height)
{
    SDL_WindowFlags flags = 0;

    if (create_info.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    if (create_info.vulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }

    const std::string title(create_info.title);

    handle_ = SDL_CreateWindow(
        title.c_str(),
        create_info.width,
        create_info.height,
        flags
    );

    if (handle_ == nullptr) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    refresh_size();
}

Window::~Window()
{
    if (handle_ != nullptr) {
        SDL_DestroyWindow(handle_);
        handle_ = nullptr;
    }
}

SDL_Window* Window::sdl_handle() const noexcept
{
    return handle_;
}

int Window::width() const noexcept
{
    return width_;
}

int Window::height() const noexcept
{
    return height_;
}

int Window::pixel_width() const noexcept
{
    return pixel_width_;
}

int Window::pixel_height() const noexcept
{
    return pixel_height_;
}

void Window::refresh_size() noexcept
{
    int width = 0;
    int height = 0;

    if (SDL_GetWindowSize(handle_, &width, &height)) {
        width_ = width;
        height_ = height;
    }

    int pixel_width = 0;
    int pixel_height = 0;

    if (SDL_GetWindowSizeInPixels(handle_, &pixel_width, &pixel_height)) {
        pixel_width_ = pixel_width;
        pixel_height_ = pixel_height;
    }
}

}
