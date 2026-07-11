#include "midnight/renderer/vulkan/VulkanFrameRenderer.hpp"

#include "midnight/renderer/vulkan/VulkanBuffer.hpp"
#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanGraphicsPipeline.hpp"
#include "midnight/renderer/vulkan/VulkanRenderPass.hpp"
#include "midnight/renderer/vulkan/VulkanSwapchain.hpp"
#include "midnight/renderer/vulkan/VulkanTextureDescriptor.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace midnight {

VulkanFrameRenderer::VulkanFrameRenderer(
    const VulkanDevice& device,
    const VulkanSwapchain& swapchain,
    const VulkanRenderPass& render_pass,
    const VulkanGraphicsPipeline& graphics_pipeline,
    const VulkanTextureDescriptor& texture_descriptor,
    const VulkanBuffer& vertex_buffer,
    const VulkanBuffer& index_buffer,
    const std::uint32_t index_count,
    const VkIndexType index_type
)
    : device_(device),
      swapchain_(swapchain),
      render_pass_(render_pass),
      graphics_pipeline_(graphics_pipeline),
      texture_descriptor_(texture_descriptor),
      vertex_buffer_(vertex_buffer),
      index_buffer_(index_buffer),
      index_count_(index_count),
      index_type_(index_type)
{
    create_command_pool();
    create_framebuffers();
    allocate_command_buffers();
    create_sync_objects();

    std::cout << "[Midnight] Vulkan frame renderer created\n";
}

VulkanFrameRenderer::~VulkanFrameRenderer()
{
    if (device_.handle() != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device_.handle());
    }

    destroy_framebuffers();
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
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
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

void VulkanFrameRenderer::create_framebuffers()
{
    framebuffers_.resize(swapchain_.image_views().size(), VK_NULL_HANDLE);

    for (std::size_t index = 0; index < swapchain_.image_views().size(); ++index) {
        const VkImageView attachment = swapchain_.image_views()[index];

        VkFramebufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass_.handle();
        create_info.attachmentCount = 1;
        create_info.pAttachments = &attachment;
        create_info.width = swapchain_.extent().width;
        create_info.height = swapchain_.extent().height;
        create_info.layers = 1;

        throw_if_vk_failed(
            vkCreateFramebuffer(
                device_.handle(),
                &create_info,
                nullptr,
                &framebuffers_[index]
            ),
            "vkCreateFramebuffer"
        );
    }

    std::cout << "[Midnight] Vulkan framebuffers created: "
              << framebuffers_.size()
              << '\n';
}

void VulkanFrameRenderer::destroy_framebuffers() noexcept
{
    for (VkFramebuffer framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_.handle(), framebuffer, nullptr);
        }
    }

    framebuffers_.clear();
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

    VkClearValue clear_value{};
    clear_value.color.float32[0] = 0.035f;
    clear_value.color.float32[1] = 0.025f;
    clear_value.color.float32[2] = 0.080f;
    clear_value.color.float32[3] = 1.000f;

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_.handle();
    render_pass_info.framebuffer = framebuffers_[image_index];
    render_pass_info.renderArea.offset = VkOffset2D{.x = 0, .y = 0};
    render_pass_info.renderArea.extent = swapchain_.extent();
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(
        command_buffer,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_.extent().width);
    viewport.height = static_cast<float>(swapchain_.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = VkOffset2D{.x = 0, .y = 0};
    scissor.extent = swapchain_.extent();

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics_pipeline_.handle()
    );

    const VkDescriptorSet texture_descriptor_set =
        texture_descriptor_.handle();

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics_pipeline_.layout(),
        0,
        1,
        &texture_descriptor_set,
        0,
        nullptr
    );

    const VkBuffer vertex_buffers[] = {
        vertex_buffer_.handle()
    };

    const VkDeviceSize vertex_buffer_offsets[] = {
        0
    };

    vkCmdBindVertexBuffers(
        command_buffer,
        0,
        1,
        vertex_buffers,
        vertex_buffer_offsets
    );

    vkCmdBindIndexBuffer(
        command_buffer,
        index_buffer_.handle(),
        0,
        index_type_
    );

    vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    throw_if_vk_failed(
        vkEndCommandBuffer(command_buffer),
        "vkEndCommandBuffer"
    );
}

}
