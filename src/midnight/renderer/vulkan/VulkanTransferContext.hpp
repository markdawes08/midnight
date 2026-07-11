#pragma once

#include <vulkan/vulkan.h>

#include <functional>

namespace midnight {

class VulkanDevice;

class VulkanTransferContext final {
public:
    using CommandRecorder = std::function<void(VkCommandBuffer)>;

    explicit VulkanTransferContext(const VulkanDevice& device);
    ~VulkanTransferContext();

    VulkanTransferContext(const VulkanTransferContext&) = delete;
    VulkanTransferContext& operator=(const VulkanTransferContext&) = delete;

    VulkanTransferContext(VulkanTransferContext&&) = delete;
    VulkanTransferContext& operator=(VulkanTransferContext&&) = delete;

    void execute(const CommandRecorder& record_commands);

private:
    void create_command_pool();
    void allocate_command_buffer();
    void create_completion_fence();
    void destroy() noexcept;

    const VulkanDevice& device_;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
    VkFence completion_fence_ = VK_NULL_HANDLE;
};

}
