#include "midnight/renderer/vulkan/VulkanTextureDescriptor.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanImage.hpp"
#include "midnight/renderer/vulkan/VulkanSampler.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <iostream>

namespace midnight {

VulkanTextureDescriptor::VulkanTextureDescriptor(
    const VulkanDevice& device,
    const VkDescriptorSetLayout descriptor_set_layout,
    const VulkanImage& image,
    const VulkanSampler& sampler
)
    : device_(device)
{
    try {
        create_descriptor_pool();
        allocate_descriptor_set(descriptor_set_layout);
        update_descriptor_set(image, sampler);
    } catch (...) {
        destroy();
        throw;
    }

    std::cout << "[Midnight] Vulkan texture descriptor created\n";
}

VulkanTextureDescriptor::~VulkanTextureDescriptor()
{
    destroy();
}

VkDescriptorSet VulkanTextureDescriptor::handle() const noexcept
{
    return descriptor_set_;
}

void VulkanTextureDescriptor::create_descriptor_pool()
{
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.maxSets = 1;
    create_info.poolSizeCount = 1;
    create_info.pPoolSizes = &pool_size;

    throw_if_vk_failed(
        vkCreateDescriptorPool(
            device_.handle(),
            &create_info,
            nullptr,
            &descriptor_pool_
        ),
        "vkCreateDescriptorPool"
    );
}

void VulkanTextureDescriptor::allocate_descriptor_set(
    const VkDescriptorSetLayout descriptor_set_layout
)
{
    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = descriptor_pool_;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;

    throw_if_vk_failed(
        vkAllocateDescriptorSets(
            device_.handle(),
            &allocate_info,
            &descriptor_set_
        ),
        "vkAllocateDescriptorSets"
    );
}

void VulkanTextureDescriptor::update_descriptor_set(
    const VulkanImage& image,
    const VulkanSampler& sampler
)
{
    VkDescriptorImageInfo image_info{};
    image_info.sampler = sampler.handle();
    image_info.imageView = image.image_view();
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        device_.handle(),
        1,
        &descriptor_write,
        0,
        nullptr
    );
}

void VulkanTextureDescriptor::destroy() noexcept
{
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_.handle(), descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
        descriptor_set_ = VK_NULL_HANDLE;
    }
}

}
