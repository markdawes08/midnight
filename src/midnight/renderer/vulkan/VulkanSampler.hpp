#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanDevice;

class VulkanSampler final {
public:
    struct CreateInfo final {
        VkFilter min_filter = VK_FILTER_NEAREST;
        VkFilter mag_filter = VK_FILTER_NEAREST;
        VkSamplerAddressMode address_mode =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };

    VulkanSampler(const VulkanDevice& device, const CreateInfo& create_info);
    ~VulkanSampler();

    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;

    VulkanSampler(VulkanSampler&&) = delete;
    VulkanSampler& operator=(VulkanSampler&&) = delete;

    [[nodiscard]] VkSampler handle() const noexcept;

private:
    const VulkanDevice& device_;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

}
