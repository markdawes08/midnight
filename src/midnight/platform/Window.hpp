#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <string_view>

namespace midnight {

class Window final {
public:
    struct CreateInfo {
        std::string_view title;
        int width = 1280;
        int height = 720;
        bool resizable = true;
        bool vulkan = true;
    };

    explicit Window(const CreateInfo& create_info);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] SDL_Window* sdl_handle() const noexcept;

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

    [[nodiscard]] int pixel_width() const noexcept;
    [[nodiscard]] int pixel_height() const noexcept;

    void refresh_size() noexcept;

private:
    SDL_Window* handle_ = nullptr;

    int width_ = 0;
    int height_ = 0;

    int pixel_width_ = 0;
    int pixel_height_ = 0;
};

}
