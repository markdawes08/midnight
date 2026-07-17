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
#include "midnight/renderer/vulkan/VulkanTextureDescriptor.hpp"
#include "midnight/renderer/vulkan/VulkanTransferContext.hpp"

#include <cstdint>

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
    void move_tile_selection(int column_delta, int row_delta);
    void upload_selected_tile_preview_vertices();

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
    VulkanTextureDescriptor texture_descriptor_;
    VulkanFrameRenderer vulkan_frame_renderer_;

    std::uint32_t selected_tile_column_ = 0;
    std::uint32_t selected_tile_row_ = 0;
    bool running_ = true;
};

}
