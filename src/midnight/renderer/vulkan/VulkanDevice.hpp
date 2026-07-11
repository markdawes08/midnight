#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace midnight {

class VulkanInstance;
class VulkanSurface;

class VulkanDevice final {
public:
    VulkanDevice(const VulkanInstance& instance, const VulkanSurface& surface);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
    [[nodiscard]] VkDevice handle() const noexcept;

    [[nodiscard]] VkQueue graphics_queue() const noexcept;
    [[nodiscard]] VkQueue present_queue() const noexcept;

    [[nodiscard]] std::uint32_t graphics_queue_family_index() const noexcept;
    [[nodiscard]] std::uint32_t present_queue_family_index() const noexcept;

    [[nodiscard]] std::uint32_t find_memory_type(
        std::uint32_t type_filter,
        VkMemoryPropertyFlags properties
    ) const;

private:
    void pick_physical_device();
    void create_logical_device();

    const VulkanInstance& instance_;
    const VulkanSurface& surface_;

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_properties_{};

    VkDevice device_ = VK_NULL_HANDLE;

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;

    std::uint32_t graphics_queue_family_index_ = 0;
    std::uint32_t present_queue_family_index_ = 0;
};

}
