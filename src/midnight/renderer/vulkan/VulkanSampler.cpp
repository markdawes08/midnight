#include "midnight/renderer/vulkan/VulkanSampler.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <iostream>

namespace midnight {

VulkanSampler::VulkanSampler(
    const VulkanDevice& device,
    const CreateInfo& create_info
)
    : device_(device)
{
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = create_info.mag_filter;
    sampler_info.minFilter = create_info.min_filter;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = create_info.address_mode;
    sampler_info.addressModeV = create_info.address_mode;
    sampler_info.addressModeW = create_info.address_mode;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    throw_if_vk_failed(
        vkCreateSampler(
            device_.handle(),
            &sampler_info,
            nullptr,
            &sampler_
        ),
        "vkCreateSampler"
    );

    std::cout << "[Midnight] Vulkan sampler created\n";
}

VulkanSampler::~VulkanSampler()
{
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_.handle(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
}

VkSampler VulkanSampler::handle() const noexcept
{
    return sampler_;
}

}
