#include "midnight/renderer/vulkan/VulkanFrameRenderer.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanSwapchain.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace midnight {

VulkanFrameRenderer::VulkanFrameRenderer(
    const VulkanDevice& device,
    const VulkanSwapchain& swapchain
)
    : device_(device),
      swapchain_(swapchain),
      image_layouts_(
          swapchain.image_count(),
          VK_IMAGE_LAYOUT_UNDEFINED
      )
{
    create_command_pool();
    allocate_command_buffers();
    create_sync_objects();

    std::cout << "[Midnight] Vulkan frame renderer created\n";
}

VulkanFrameRenderer::~VulkanFrameRenderer()
{
    if (device_.handle() != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device_.handle());
    }

    destroy_sync_objects();

    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_.handle(), command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
}

bool VulkanFrameRenderer::draw_frame()
{
    const VkFence frame_fence = in_flight_fences_[current_frame_];

    throw_if_vk_failed(
        vkWaitForFences(
            device_.handle(),
            1,
            &frame_fence,
            VK_TRUE,
            std::numeric_limits<std::uint64_t>::max()
        ),
        "vkWaitForFences"
    );

    std::uint32_t image_index = 0;

    const VkResult acquire_result = vkAcquireNextImageKHR(
        device_.handle(),
        swapchain_.handle(),
        std::numeric_limits<std::uint64_t>::max(),
        image_available_semaphores_[current_frame_],
        VK_NULL_HANDLE,
        &image_index
    );

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }

    if (acquire_result != VK_SUCCESS &&
        acquire_result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error(
            "vkAcquireNextImageKHR failed: " + vk_result_to_string(acquire_result)
        );
    }

    if (image_in_flight_fences_[image_index] != VK_NULL_HANDLE) {
        const VkFence image_fence = image_in_flight_fences_[image_index];

        throw_if_vk_failed(
            vkWaitForFences(
                device_.handle(),
                1,
                &image_fence,
                VK_TRUE,
                std::numeric_limits<std::uint64_t>::max()
            ),
            "vkWaitForFences"
        );
    }

    image_in_flight_fences_[image_index] = frame_fence;

    throw_if_vk_failed(
        vkResetFences(device_.handle(), 1, &frame_fence),
        "vkResetFences"
    );

    const VkCommandBuffer command_buffer = command_buffers_[current_frame_];

    throw_if_vk_failed(
        vkResetCommandBuffer(command_buffer, 0),
        "vkResetCommandBuffer"
    );

    record_command_buffer(command_buffer, image_index);

    const VkSemaphore wait_semaphores[] = {
        image_available_semaphores_[current_frame_]
    };

    const VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_TRANSFER_BIT
    };

    const VkSemaphore signal_semaphores[] = {
        render_finished_semaphores_[current_frame_]
    };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    throw_if_vk_failed(
        vkQueueSubmit(
            device_.graphics_queue(),
            1,
            &submit_info,
            frame_fence
        ),
        "vkQueueSubmit"
    );

    const VkSwapchainKHR swapchains[] = {
        swapchain_.handle()
    };

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    const VkResult present_result = vkQueuePresentKHR(
        device_.present_queue(),
        &present_info
    );

    if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
        present_result == VK_SUBOPTIMAL_KHR) {
        return false;
    }

    if (present_result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkQueuePresentKHR failed: " + vk_result_to_string(present_result)
        );
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;

    return true;
}

void VulkanFrameRenderer::create_command_pool()
{
    VkCommandPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

    std::cout << "[Midnight] Vulkan command pool created\n";
}

void VulkanFrameRenderer::allocate_command_buffers()
{
    command_buffers_.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool_;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount =
        static_cast<std::uint32_t>(command_buffers_.size());

    throw_if_vk_failed(
        vkAllocateCommandBuffers(
            device_.handle(),
            &allocate_info,
            command_buffers_.data()
        ),
        "vkAllocateCommandBuffers"
    );

    std::cout << "[Midnight] Vulkan command buffers allocated: "
              << command_buffers_.size()
              << '\n';
}

void VulkanFrameRenderer::create_sync_objects()
{
    image_available_semaphores_.resize(kMaxFramesInFlight, VK_NULL_HANDLE);
    render_finished_semaphores_.resize(kMaxFramesInFlight, VK_NULL_HANDLE);
    in_flight_fences_.resize(kMaxFramesInFlight, VK_NULL_HANDLE);

    image_in_flight_fences_.resize(
        swapchain_.image_count(),
        VK_NULL_HANDLE
    );

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        throw_if_vk_failed(
            vkCreateSemaphore(
                device_.handle(),
                &semaphore_info,
                nullptr,
                &image_available_semaphores_[index]
            ),
            "vkCreateSemaphore"
        );

        throw_if_vk_failed(
            vkCreateSemaphore(
                device_.handle(),
                &semaphore_info,
                nullptr,
                &render_finished_semaphores_[index]
            ),
            "vkCreateSemaphore"
        );

        throw_if_vk_failed(
            vkCreateFence(
                device_.handle(),
                &fence_info,
                nullptr,
                &in_flight_fences_[index]
            ),
            "vkCreateFence"
        );
    }

    std::cout << "[Midnight] Vulkan frame synchronization created\n";
}

void VulkanFrameRenderer::destroy_sync_objects() noexcept
{
    for (VkFence fence : in_flight_fences_) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device_.handle(), fence, nullptr);
        }
    }

    for (VkSemaphore semaphore : render_finished_semaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_.handle(), semaphore, nullptr);
        }
    }

    for (VkSemaphore semaphore : image_available_semaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_.handle(), semaphore, nullptr);
        }
    }

    in_flight_fences_.clear();
    render_finished_semaphores_.clear();
    image_available_semaphores_.clear();
    image_in_flight_fences_.clear();
}

void VulkanFrameRenderer::record_command_buffer(
    const VkCommandBuffer command_buffer,
    const std::uint32_t image_index
)
{
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    throw_if_vk_failed(
        vkBeginCommandBuffer(command_buffer, &begin_info),
        "vkBeginCommandBuffer"
    );

    const VkImage image = swapchain_.images()[image_index];

    transition_image_layout(
        command_buffer,
        image,
        image_layouts_[image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    VkClearColorValue clear_color{};
    clear_color.float32[0] = 0.035f;
    clear_color.float32[1] = 0.025f;
    clear_color.float32[2] = 0.080f;
    clear_color.float32[3] = 1.000f;

    VkImageSubresourceRange clear_range{};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = 1;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = 1;

    vkCmdClearColorImage(
        command_buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clear_color,
        1,
        &clear_range
    );

    transition_image_layout(
        command_buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    image_layouts_[image_index] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    throw_if_vk_failed(
        vkEndCommandBuffer(command_buffer),
        "vkEndCommandBuffer"
    );
}

void VulkanFrameRenderer::transition_image_layout(
    const VkCommandBuffer command_buffer,
    const VkImage image,
    const VkImageLayout old_layout,
    const VkImageLayout new_layout
)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.image = image;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (
        old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        throw std::runtime_error("Unsupported Vulkan image layout transition");
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage,
        destination_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

}
