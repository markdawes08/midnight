#include "midnight/renderer/vulkan/VulkanTransferContext.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
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
