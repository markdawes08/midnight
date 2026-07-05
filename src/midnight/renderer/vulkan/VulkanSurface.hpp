#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanInstance;
class Window;

class VulkanSurface final {
public:
    VulkanSurface(const Window& window, const VulkanInstance& instance);
    ~VulkanSurface();

    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;

    VulkanSurface(VulkanSurface&&) = delete;
    VulkanSurface& operator=(VulkanSurface&&) = delete;

    [[nodiscard]] VkSurfaceKHR handle() const noexcept;

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};

}
