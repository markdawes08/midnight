#pragma once

#include "midnight/platform/SdlContext.hpp"
#include "midnight/platform/Window.hpp"
#include "midnight/renderer/vulkan/VulkanBuffer.hpp"
#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanFrameRenderer.hpp"
#include "midnight/renderer/vulkan/VulkanGraphicsPipeline.hpp"
#include "midnight/renderer/vulkan/VulkanImage.hpp"
#include "midnight/renderer/vulkan/VulkanInstance.hpp"
#include "midnight/renderer/vulkan/VulkanRenderPass.hpp"
#include "midnight/renderer/vulkan/VulkanSampler.hpp"
#include "midnight/renderer/vulkan/VulkanSurface.hpp"
#include "midnight/renderer/vulkan/VulkanSwapchain.hpp"
#include "midnight/renderer/vulkan/VulkanTransferContext.hpp"

namespace midnight {

class Application final {
public:
    Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

private:
    void print_startup_info() const;
    void poll_events();

    SdlContext sdl_;
    Window window_;
    VulkanInstance vulkan_instance_;
    VulkanSurface vulkan_surface_;
    VulkanDevice vulkan_device_;
    VulkanTransferContext vulkan_transfer_context_;
    VulkanSwapchain vulkan_swapchain_;
    VulkanRenderPass vulkan_render_pass_;
    VulkanGraphicsPipeline vulkan_graphics_pipeline_;
    VulkanBuffer quad_vertex_buffer_;
    VulkanBuffer quad_index_buffer_;
    VulkanImage texture_image_;
    VulkanSampler texture_sampler_;
    VulkanFrameRenderer vulkan_frame_renderer_;

    bool running_ = true;
};

}
