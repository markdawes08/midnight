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
#include <memory>
#include <optional>
#include <vector>

namespace midnight {

class Application final {
public:
    Application();
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

private:
    struct SwapchainResources final {
        std::unique_ptr<VulkanSwapchain> swapchain;
        std::unique_ptr<VulkanRenderPass> render_pass;
        std::unique_ptr<VulkanGraphicsPipeline> graphics_pipeline;
        std::unique_ptr<VulkanTextureDescriptor> texture_descriptor;
        std::unique_ptr<VulkanFrameRenderer> frame_renderer;
    };

    struct MapTile final {
        std::uint32_t tileset_column = 0;
        std::uint32_t tileset_row = 0;
        bool occupied = false;

        bool operator==(const MapTile&) const = default;
    };

    struct MapAreaSelectionState final {
        std::uint32_t left = 0;
        std::uint32_t top = 0;
        std::uint32_t right = 0;
        std::uint32_t bottom = 0;
        bool visible = false;

        bool operator==(const MapAreaSelectionState&) const = default;
    };

    struct MapEditSnapshot final {
        std::vector<MapTile> tiles;
        std::optional<MapAreaSelectionState> area_selection;
    };

    void print_startup_info() const;
    void poll_events();
    [[nodiscard]] SwapchainResources create_swapchain_resources(
        VkSwapchainKHR old_swapchain = VK_NULL_HANDLE
    );
    [[nodiscard]] bool recreate_swapchain_resources();
    void request_swapchain_recreation(
        bool wait_for_stable_size,
        bool restart_settle_delay = false
    );
    void release_retired_swapchain_resources();
    void wait_for_rendering_resources();
    void begin_map_edit(bool include_area_selection = false);
    void finish_map_edit();
    void undo_map_edit();
    void redo_map_edit();
    [[nodiscard]] MapAreaSelectionState
        capture_map_area_selection_state() const;
    void apply_map_area_selection_state(
        const MapAreaSelectionState& state
    );
    void flood_fill_map();
    void delete_selected_map_area();
    void move_selected_map_area(int column_delta, int row_delta);
    [[nodiscard]] bool begin_map_rectangle_paint(float x, float y);
    void update_map_rectangle_paint(float x, float y);
    void apply_map_rectangle_paint();
    void finish_map_rectangle_paint();
    [[nodiscard]] bool begin_map_area_selection_drag(float x, float y);
    void update_map_area_selection_drag(float x, float y);
    void finish_map_area_selection_drag();
    void upload_map_area_selection_vertices();
    [[nodiscard]] bool paint_map_selection(float x, float y);
    [[nodiscard]] bool erase_map_tile(float x, float y);
    void pick_map_tile(float x, float y);
    void upload_map_tile_vertices(
        std::uint32_t column,
        std::uint32_t row
    );
    void update_map_hover(float x, float y);
    void clear_map_hover();
    [[nodiscard]] bool window_position_to_map_cell(
        float x,
        float y,
        std::uint32_t& column,
        std::uint32_t& row,
        bool clamp_to_map = false
    ) const;
    void upload_map_hover_vertices();
    void toggle_tileset_grid();
    void upload_tileset_grid_vertices();
    void toggle_map_grid();
    void upload_map_grid_vertices();
    void move_tile_selection(int column_delta, int row_delta);
    void begin_tile_selection_drag(float x, float y);
    void update_tile_selection_drag(float x, float y);
    void end_tile_selection_drag(float x, float y);
    [[nodiscard]] bool window_position_to_tile(
        float x,
        float y,
        bool clamp_to_atlas,
        std::uint32_t& column,
        std::uint32_t& row
    ) const;
    bool set_tile_selection(
        std::uint32_t first_column,
        std::uint32_t first_row,
        std::uint32_t second_column,
        std::uint32_t second_row
    );
    void print_tile_selection() const;
    void upload_tile_selection_vertices();

    SdlContext sdl_;
    Window window_;
    VulkanInstance vulkan_instance_;
    VulkanSurface vulkan_surface_;
    VulkanDevice vulkan_device_;
    VulkanTransferContext vulkan_transfer_context_;
    VulkanBuffer quad_vertex_buffer_;
    VulkanBuffer quad_index_buffer_;
    VulkanImage texture_image_;
    VulkanSampler texture_sampler_;
    SwapchainResources swapchain_resources_;
    std::vector<SwapchainResources> retired_swapchain_resources_;
    std::vector<MapTile> map_tiles_;
    std::vector<MapTile> active_map_edit_before_;
    std::optional<MapAreaSelectionState>
        active_map_area_selection_before_;
    std::vector<MapEditSnapshot> map_undo_stack_;
    std::vector<MapEditSnapshot> map_redo_stack_;

    std::uint32_t selected_tile_left_ = 0;
    std::uint32_t selected_tile_top_ = 0;
    std::uint32_t selected_tile_right_ = 0;
    std::uint32_t selected_tile_bottom_ = 0;
    std::uint32_t tile_selection_drag_anchor_column_ = 0;
    std::uint32_t tile_selection_drag_anchor_row_ = 0;
    std::uint32_t hovered_map_column_ = 0;
    std::uint32_t hovered_map_row_ = 0;
    std::uint32_t last_map_paint_column_ = 0;
    std::uint32_t last_map_paint_row_ = 0;
    std::uint32_t last_map_erase_column_ = 0;
    std::uint32_t last_map_erase_row_ = 0;
    std::uint32_t map_rectangle_anchor_column_ = 0;
    std::uint32_t map_rectangle_anchor_row_ = 0;
    std::uint32_t map_rectangle_end_column_ = 0;
    std::uint32_t map_rectangle_end_row_ = 0;
    std::uint32_t map_rectangle_tileset_left_ = 0;
    std::uint32_t map_rectangle_tileset_top_ = 0;
    std::uint32_t map_rectangle_tileset_right_ = 0;
    std::uint32_t map_rectangle_tileset_bottom_ = 0;
    std::uint32_t map_area_selection_anchor_column_ = 0;
    std::uint32_t map_area_selection_anchor_row_ = 0;
    std::uint32_t map_area_selection_left_ = 0;
    std::uint32_t map_area_selection_top_ = 0;
    std::uint32_t map_area_selection_right_ = 0;
    std::uint32_t map_area_selection_bottom_ = 0;
    bool tileset_grid_visible_ = true;
    bool map_grid_visible_ = true;
    bool tile_selection_dragging_ = false;
    bool map_paint_dragging_ = false;
    bool map_rectangle_dragging_ = false;
    bool map_area_selection_dragging_ = false;
    bool map_erase_dragging_ = false;
    bool map_edit_active_ = false;
    bool map_hover_visible_ = false;
    bool map_area_selection_visible_ = false;
    bool swapchain_recreation_pending_ = false;
    std::uint64_t swapchain_recreation_not_before_ticks_ = 0;
    int swapchain_window_pixel_width_ = 0;
    int swapchain_window_pixel_height_ = 0;
    int pending_swapchain_pixel_width_ = 0;
    int pending_swapchain_pixel_height_ = 0;
    bool running_ = true;
};

}
