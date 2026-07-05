#include "midnight/renderer/vulkan/VulkanSurface.hpp"

#include "midnight/platform/Window.hpp"
#include "midnight/renderer/vulkan/VulkanInstance.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace midnight {

VulkanSurface::VulkanSurface(const Window& window, const VulkanInstance& instance)
    : instance_(instance.handle())
{
    if (!SDL_Vulkan_CreateSurface(window.sdl_handle(), instance_, nullptr, &surface_)) {
        throw std::runtime_error(
            std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError()
        );
    }

    std::cout << "[Midnight] Vulkan surface created\n";
}

VulkanSurface::~VulkanSurface()
{
    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

VkSurfaceKHR VulkanSurface::handle() const noexcept
{
    return surface_;
}

}
