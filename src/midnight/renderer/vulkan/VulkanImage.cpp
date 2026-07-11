#include "midnight/renderer/vulkan/VulkanImage.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <iostream>
#include <stdexcept>

namespace midnight {

VulkanImage::VulkanImage(
    const VulkanDevice& device,
    const CreateInfo& create_info
)
    : device_(device),
      extent_(create_info.extent),
      format_(create_info.format)
{
    if (extent_.width == 0 || extent_.height == 0) {
        throw std::runtime_error("Cannot create a zero-sized Vulkan image");
    }

    if (format_ == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("Cannot create a Vulkan image with an undefined format");
    }

    if (create_info.usage == 0) {
        throw std::runtime_error("Cannot create a Vulkan image without a usage");
    }

    if (create_info.aspect_mask == 0) {
        throw std::runtime_error("Cannot create a Vulkan image view without an aspect mask");
    }

    try {
        create_image(create_info.usage);
        create_image_view(create_info.aspect_mask);
    } catch (...) {
        destroy();
        throw;
    }

    std::cout << "[Midnight] Vulkan image created: "
              << extent_.width
              << "x"
              << extent_.height
              << " "
              << vulkan_format_to_string(format_)
              << '\n';
}

VulkanImage::~VulkanImage()
{
    destroy();
}

VkImage VulkanImage::handle() const noexcept
{
    return image_;
}

VkImageView VulkanImage::image_view() const noexcept
{
    return image_view_;
}

VkExtent2D VulkanImage::extent() const noexcept
{
    return extent_;
}

VkFormat VulkanImage::format() const noexcept
{
    return format_;
}

void VulkanImage::create_image(const VkImageUsageFlags usage)
{
    VkImageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.format = format_;
    create_info.extent = VkExtent3D{
        .width = extent_.width,
        .height = extent_.height,
        .depth = 1
    };
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    throw_if_vk_failed(
        vkCreateImage(
            device_.handle(),
            &create_info,
            nullptr,
            &image_
        ),
        "vkCreateImage"
    );

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(
        device_.handle(),
        image_,
        &memory_requirements
    );

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = device_.find_memory_type(
        memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    throw_if_vk_failed(
        vkAllocateMemory(
            device_.handle(),
            &allocate_info,
            nullptr,
            &memory_
        ),
        "vkAllocateMemory"
    );

    throw_if_vk_failed(
        vkBindImageMemory(
            device_.handle(),
            image_,
            memory_,
            0
        ),
        "vkBindImageMemory"
    );
}

void VulkanImage::create_image_view(const VkImageAspectFlags aspect_mask)
{
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image_;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format_;
    create_info.subresourceRange.aspectMask = aspect_mask;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    throw_if_vk_failed(
        vkCreateImageView(
            device_.handle(),
            &create_info,
            nullptr,
            &image_view_
        ),
        "vkCreateImageView"
    );
}

void VulkanImage::destroy() noexcept
{
    if (image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_.handle(), image_view_, nullptr);
        image_view_ = VK_NULL_HANDLE;
    }

    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_.handle(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }

    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_.handle(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

}
