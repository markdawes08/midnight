#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanDevice;

class VulkanBuffer final {
public:
    VulkanBuffer(
        const VulkanDevice& device,
        VkDeviceSize byte_size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties
    );

    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&&) = delete;
    VulkanBuffer& operator=(VulkanBuffer&&) = delete;

    [[nodiscard]] VkBuffer handle() const noexcept;
    [[nodiscard]] VkDeviceSize byte_size() const noexcept;

    void upload(
        const void* source,
        VkDeviceSize byte_size,
        VkDeviceSize destination_offset = 0
    ) const;

private:
    const VulkanDevice& device_;

    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize byte_size_ = 0;
};

}
