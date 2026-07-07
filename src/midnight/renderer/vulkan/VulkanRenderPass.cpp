#include "midnight/renderer/vulkan/VulkanRenderPass.hpp"

#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanSwapchain.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <array>
#include <iostream>

namespace midnight {

VulkanRenderPass::VulkanRenderPass(
    const VulkanDevice& device,
    const VulkanSwapchain& swapchain
)
    : device_(device),
      swapchain_(swapchain)
{
    create_render_pass();

    std::cout << "[Midnight] Vulkan render pass created\n";
}

VulkanRenderPass::~VulkanRenderPass()
{
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_.handle(), render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
}

VkRenderPass VulkanRenderPass::handle() const noexcept
{
    return render_pass_;
}

void VulkanRenderPass::create_render_pass()
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_.image_format();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 1> attachments = {
        color_attachment
    };

    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    create_info.pAttachments = attachments.data();
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    throw_if_vk_failed(
        vkCreateRenderPass(
            device_.handle(),
            &create_info,
            nullptr,
            &render_pass_
        ),
        "vkCreateRenderPass"
    );
}

}
