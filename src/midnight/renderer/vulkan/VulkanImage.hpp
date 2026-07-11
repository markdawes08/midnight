#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanDevice;

class VulkanImage final {
public:
    struct CreateInfo final {
        VkExtent2D extent{};
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    VulkanImage(const VulkanDevice& device, const CreateInfo& create_info);
    ~VulkanImage();

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    VulkanImage(VulkanImage&&) = delete;
    VulkanImage& operator=(VulkanImage&&) = delete;

    [[nodiscard]] VkImage handle() const noexcept;
    [[nodiscard]] VkImageView image_view() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;

private:
    void create_image(VkImageUsageFlags usage);
    void create_image_view(VkImageAspectFlags aspect_mask);
    void destroy() noexcept;

    const VulkanDevice& device_;
    VkExtent2D extent_{};
    VkFormat format_ = VK_FORMAT_UNDEFINED;

    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView image_view_ = VK_NULL_HANDLE;
};

}
