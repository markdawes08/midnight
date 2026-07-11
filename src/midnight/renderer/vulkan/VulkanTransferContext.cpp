#include "midnight/renderer/vulkan/VulkanTransferContext.hpp"

#include "midnight/renderer/vulkan/VulkanBuffer.hpp"
#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanImage.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace midnight {

VulkanTransferContext::VulkanTransferContext(const VulkanDevice& device)
    : device_(device)
{
    try {
        create_command_pool();
        allocate_command_buffer();
        create_completion_fence();
    } catch (...) {
        destroy();
        throw;
    }

    std::cout << "[Midnight] Vulkan transfer context created\n";
}

VulkanTransferContext::~VulkanTransferContext()
{
    destroy();
}

void VulkanTransferContext::execute(const CommandRecorder& record_commands)
{
    if (!record_commands) {
        throw std::runtime_error("Cannot execute empty Vulkan transfer commands");
    }

    throw_if_vk_failed(
        vkResetCommandBuffer(command_buffer_, 0),
        "vkResetCommandBuffer"
    );

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    throw_if_vk_failed(
        vkBeginCommandBuffer(command_buffer_, &begin_info),
        "vkBeginCommandBuffer"
    );

    record_commands(command_buffer_);

    throw_if_vk_failed(
        vkEndCommandBuffer(command_buffer_),
        "vkEndCommandBuffer"
    );

    throw_if_vk_failed(
        vkResetFences(device_.handle(), 1, &completion_fence_),
        "vkResetFences"
    );

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer_;

    throw_if_vk_failed(
        vkQueueSubmit(
            device_.graphics_queue(),
            1,
            &submit_info,
            completion_fence_
        ),
        "vkQueueSubmit"
    );

    throw_if_vk_failed(
        vkWaitForFences(
            device_.handle(),
            1,
            &completion_fence_,
            VK_TRUE,
            std::numeric_limits<std::uint64_t>::max()
        ),
        "vkWaitForFences"
    );
}

void VulkanTransferContext::upload_to_new_sampled_image(
    const VulkanImage& destination_image,
    const void* source,
    const VkDeviceSize byte_size
)
{
    VulkanBuffer staging_buffer(
        device_,
        byte_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.upload(source, byte_size);

    execute(
        [&staging_buffer, &destination_image](
            const VkCommandBuffer command_buffer
        ) {
            VkImageMemoryBarrier to_transfer_destination{};
            to_transfer_destination.sType =
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_transfer_destination.srcAccessMask = 0;
            to_transfer_destination.dstAccessMask =
                VK_ACCESS_TRANSFER_WRITE_BIT;
            to_transfer_destination.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            to_transfer_destination.newLayout =
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            to_transfer_destination.srcQueueFamilyIndex =
                VK_QUEUE_FAMILY_IGNORED;
            to_transfer_destination.dstQueueFamilyIndex =
                VK_QUEUE_FAMILY_IGNORED;
            to_transfer_destination.image = destination_image.handle();
            to_transfer_destination.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
            to_transfer_destination.subresourceRange.baseMipLevel = 0;
            to_transfer_destination.subresourceRange.levelCount = 1;
            to_transfer_destination.subresourceRange.baseArrayLayer = 0;
            to_transfer_destination.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &to_transfer_destination
            );

            const VkExtent2D image_extent = destination_image.extent();

            VkBufferImageCopy copy_region{};
            copy_region.bufferOffset = 0;
            copy_region.bufferRowLength = 0;
            copy_region.bufferImageHeight = 0;
            copy_region.imageSubresource.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0};
            copy_region.imageExtent = VkExtent3D{
                .width = image_extent.width,
                .height = image_extent.height,
                .depth = 1
            };

            vkCmdCopyBufferToImage(
                command_buffer,
                staging_buffer.handle(),
                destination_image.handle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy_region
            );

            VkImageMemoryBarrier to_shader_read{};
            to_shader_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.image = destination_image.handle();
            to_shader_read.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
            to_shader_read.subresourceRange.baseMipLevel = 0;
            to_shader_read.subresourceRange.levelCount = 1;
            to_shader_read.subresourceRange.baseArrayLayer = 0;
            to_shader_read.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &to_shader_read
            );
        }
    );

    std::cout << "[Midnight] Vulkan image upload completed: "
              << byte_size
              << " bytes\n";
}

void VulkanTransferContext::create_command_pool()
{
    VkCommandPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags =
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = device_.graphics_queue_family_index();

    throw_if_vk_failed(
        vkCreateCommandPool(
            device_.handle(),
            &create_info,
            nullptr,
            &command_pool_
        ),
        "vkCreateCommandPool"
    );
}

void VulkanTransferContext::allocate_command_buffer()
{
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool_;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;

    throw_if_vk_failed(
        vkAllocateCommandBuffers(
            device_.handle(),
            &allocate_info,
            &command_buffer_
        ),
        "vkAllocateCommandBuffers"
    );
}

void VulkanTransferContext::create_completion_fence()
{
    VkFenceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    throw_if_vk_failed(
        vkCreateFence(
            device_.handle(),
            &create_info,
            nullptr,
            &completion_fence_
        ),
        "vkCreateFence"
    );
}

void VulkanTransferContext::destroy() noexcept
{
    if (completion_fence_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_.handle(), completion_fence_, nullptr);
        completion_fence_ = VK_NULL_HANDLE;
    }

    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_.handle(), command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
        command_buffer_ = VK_NULL_HANDLE;
    }
}

}
