#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <vector>

namespace midnight {

class VulkanDevice;
class VulkanRenderPass;
class VulkanSwapchain;

class VulkanGraphicsPipeline final {
public:
    VulkanGraphicsPipeline(
        const VulkanDevice& device,
        const VulkanSwapchain& swapchain,
        const VulkanRenderPass& render_pass
    );

    ~VulkanGraphicsPipeline();

    VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
    VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

    VulkanGraphicsPipeline(VulkanGraphicsPipeline&&) = delete;
    VulkanGraphicsPipeline& operator=(VulkanGraphicsPipeline&&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept;
    [[nodiscard]] VkPipelineLayout layout() const noexcept;
    [[nodiscard]] VkDescriptorSetLayout descriptor_set_layout() const noexcept;

private:
    [[nodiscard]] VkShaderModule create_shader_module(
        const std::filesystem::path& path
    ) const;

    void create_descriptor_set_layout();
    void create_pipeline_layout();
    void create_graphics_pipeline();
    void destroy() noexcept;

    const VulkanDevice& device_;
    const VulkanSwapchain& swapchain_;
    const VulkanRenderPass& render_pass_;

    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
};

}
