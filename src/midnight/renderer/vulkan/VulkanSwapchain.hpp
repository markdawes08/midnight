#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace midnight {

class VulkanDevice;
class VulkanSurface;
class Window;

class VulkanSwapchain final {
public:
    VulkanSwapchain(
        const Window& window,
        const VulkanDevice& device,
        const VulkanSurface& surface,
        VkSwapchainKHR old_swapchain = VK_NULL_HANDLE
    );

    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    [[nodiscard]] VkSwapchainKHR handle() const noexcept;
    [[nodiscard]] VkFormat image_format() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;

    [[nodiscard]] const std::vector<VkImage>& images() const noexcept;
    [[nodiscard]] const std::vector<VkImageView>& image_views() const noexcept;

    [[nodiscard]] std::uint32_t image_count() const noexcept;

private:
    void create_swapchain(
        const Window& window,
        VkSwapchainKHR old_swapchain
    );
    void create_image_views();
    void destroy_image_views() noexcept;

    const VulkanDevice& device_;
    const VulkanSurface& surface_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;

    VkFormat image_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};

    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
};

}
