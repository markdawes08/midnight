#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanDevice;
class VulkanSwapchain;

class VulkanRenderPass final {
public:
    VulkanRenderPass(
        const VulkanDevice& device,
        const VulkanSwapchain& swapchain
    );

    ~VulkanRenderPass();

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    VulkanRenderPass(VulkanRenderPass&&) = delete;
    VulkanRenderPass& operator=(VulkanRenderPass&&) = delete;

    [[nodiscard]] VkRenderPass handle() const noexcept;

private:
    void create_render_pass();

    const VulkanDevice& device_;
    const VulkanSwapchain& swapchain_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
};

}
