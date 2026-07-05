#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace midnight {

class VulkanDevice;
class VulkanSwapchain;

class VulkanFrameRenderer final {
public:
    VulkanFrameRenderer(
        const VulkanDevice& device,
        const VulkanSwapchain& swapchain
    );

    ~VulkanFrameRenderer();

    VulkanFrameRenderer(const VulkanFrameRenderer&) = delete;
    VulkanFrameRenderer& operator=(const VulkanFrameRenderer&) = delete;

    VulkanFrameRenderer(VulkanFrameRenderer&&) = delete;
    VulkanFrameRenderer& operator=(VulkanFrameRenderer&&) = delete;

    [[nodiscard]] bool draw_frame();

private:
    static constexpr std::size_t kMaxFramesInFlight = 2;

    void create_command_pool();
    void allocate_command_buffers();

    void create_sync_objects();
    void destroy_sync_objects() noexcept;

    void record_command_buffer(
        VkCommandBuffer command_buffer,
        std::uint32_t image_index
    );

    static void transition_image_layout(
        VkCommandBuffer command_buffer,
        VkImage image,
        VkImageLayout old_layout,
        VkImageLayout new_layout
    );

    const VulkanDevice& device_;
    const VulkanSwapchain& swapchain_;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> image_in_flight_fences_;

    std::vector<VkImageLayout> image_layouts_;

    std::size_t current_frame_ = 0;
};

}
