#include "midnight/renderer/vulkan/VulkanBuffer.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace midnight {

VulkanBuffer::VulkanBuffer(
    const VulkanDevice& device,
    const VkDeviceSize byte_size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags memory_properties
)
    : device_(device),
      byte_size_(byte_size)
{
    if (byte_size_ == 0) {
        throw std::runtime_error("Cannot create a zero-sized Vulkan buffer");
    }

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = byte_size_;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    throw_if_vk_failed(
        vkCreateBuffer(
            device_.handle(),
            &buffer_info,
            nullptr,
            &buffer_
        ),
        "vkCreateBuffer"
    );

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(
        device_.handle(),
        buffer_,
        &memory_requirements
    );

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = device_.find_memory_type(
        memory_requirements.memoryTypeBits,
        memory_properties
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
        vkBindBufferMemory(
            device_.handle(),
            buffer_,
            memory_,
            0
        ),
        "vkBindBufferMemory"
    );

    std::cout << "[Midnight] Vulkan buffer created: "
              << byte_size_
              << " bytes"
              << '\n';
}

VulkanBuffer::~VulkanBuffer()
{
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_.handle(), buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }

    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_.handle(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

VkBuffer VulkanBuffer::handle() const noexcept
{
    return buffer_;
}

VkDeviceSize VulkanBuffer::byte_size() const noexcept
{
    return byte_size_;
}

void VulkanBuffer::upload(
    const void* source,
    const VkDeviceSize byte_size,
    const VkDeviceSize destination_offset
) const
{
    if (byte_size == 0) {
        return;
    }

    if (source == nullptr) {
        throw std::runtime_error("Cannot upload from a null source pointer");
    }

    if (destination_offset > byte_size_ || byte_size > byte_size_ - destination_offset) {
        throw std::runtime_error("Vulkan buffer upload would write past the end of the buffer");
    }

    void* mapped_memory = nullptr;

    throw_if_vk_failed(
        vkMapMemory(
            device_.handle(),
            memory_,
            destination_offset,
            byte_size,
            0,
            &mapped_memory
        ),
        "vkMapMemory"
    );

    std::memcpy(mapped_memory, source, static_cast<std::size_t>(byte_size));

    vkUnmapMemory(device_.handle(), memory_);
}

}
