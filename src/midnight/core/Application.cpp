#include "midnight/core/Application.hpp"

#include "midnight/assets/Png.hpp"
#include "midnight/renderer/Vertex2D.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace midnight {
namespace {

constexpr std::uint32_t kInitialWindowWidth = 1280;
constexpr std::uint32_t kInitialWindowHeight = 720;
constexpr std::uint32_t kOutdoorTilesetWidth = 192;
constexpr std::uint32_t kOutdoorTilesetHeight = 128;
constexpr std::uint32_t kTilesetTileWidth = 16;
constexpr std::uint32_t kTilesetTileHeight = 16;
constexpr std::uint32_t kOutdoorTilesetColumns =
    kOutdoorTilesetWidth / kTilesetTileWidth;
constexpr std::uint32_t kOutdoorTilesetRows =
    kOutdoorTilesetHeight / kTilesetTileHeight;
constexpr std::uint32_t kTilesetPreviewScale = 3;
constexpr std::uint32_t kInitialSelectedTileColumn = 1;
constexpr std::uint32_t kInitialSelectedTileRow = 0;
constexpr std::uint32_t kSelectedRegionPreviewMaxScale = 4;
constexpr std::uint32_t kSelectedRegionPreviewMaxWidth = 256;
constexpr std::uint32_t kSelectedRegionPreviewMaxHeight = 384;
constexpr std::uint32_t kMapCanvasColumns = 8;
constexpr std::uint32_t kMapCanvasRows = 6;
constexpr std::uint32_t kMapCanvasScale = 2;
constexpr std::size_t kOutdoorTilesetByteSize =
    static_cast<std::size_t>(kOutdoorTilesetWidth) *
    static_cast<std::size_t>(kOutdoorTilesetHeight) *
    RgbaImage::bytes_per_pixel;

static_assert(kOutdoorTilesetWidth % kTilesetTileWidth == 0);
static_assert(kOutdoorTilesetHeight % kTilesetTileHeight == 0);
static_assert(kInitialSelectedTileColumn < kOutdoorTilesetColumns);
static_assert(kInitialSelectedTileRow < kOutdoorTilesetRows);
static_assert(kSelectedRegionPreviewMaxWidth >= kOutdoorTilesetWidth);
static_assert(kSelectedRegionPreviewMaxHeight >= kOutdoorTilesetHeight);

struct TextureRegion final {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

constexpr TextureRegion tile_texture_region(
    const std::uint32_t left,
    const std::uint32_t top,
    const std::uint32_t right,
    const std::uint32_t bottom
)
{
    return TextureRegion{
        .left = static_cast<float>(left * kTilesetTileWidth) /
            static_cast<float>(kOutdoorTilesetWidth),
        .top = static_cast<float>(top * kTilesetTileHeight) /
            static_cast<float>(kOutdoorTilesetHeight),
        .right =
            static_cast<float>((right + 1) * kTilesetTileWidth) /
            static_cast<float>(kOutdoorTilesetWidth),
        .bottom =
            static_cast<float>((bottom + 1) * kTilesetTileHeight) /
            static_cast<float>(kOutdoorTilesetHeight)
    };
}

constexpr float kTilesetPreviewHalfWidth =
    static_cast<float>(kOutdoorTilesetWidth * kTilesetPreviewScale) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kTilesetPreviewHalfHeight =
    static_cast<float>(kOutdoorTilesetHeight * kTilesetPreviewScale) /
    static_cast<float>(kInitialWindowHeight);

constexpr float kRightPanelCenterX = 0.70f;
constexpr float kSelectedRegionPreviewCenterY = -1.0f / 3.0f;
constexpr float kMapCanvasCenterY = 0.60f;

constexpr float kMapCanvasHalfWidth =
    static_cast<float>(
        kMapCanvasColumns *
        kTilesetTileWidth *
        kMapCanvasScale
    ) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kMapCanvasHalfHeight =
    static_cast<float>(
        kMapCanvasRows *
        kTilesetTileHeight *
        kMapCanvasScale
    ) /
    static_cast<float>(kInitialWindowHeight);

constexpr float kMapCanvasLeft =
    kRightPanelCenterX - kMapCanvasHalfWidth;
constexpr float kMapCanvasTop =
    kMapCanvasCenterY - kMapCanvasHalfHeight;
constexpr float kMapCanvasRight =
    kRightPanelCenterX + kMapCanvasHalfWidth;
constexpr float kMapCanvasBottom =
    kMapCanvasCenterY + kMapCanvasHalfHeight;
constexpr float kMapCanvasCellWidth =
    (2.0f * kMapCanvasHalfWidth) /
    static_cast<float>(kMapCanvasColumns);
constexpr float kMapCanvasCellHeight =
    (2.0f * kMapCanvasHalfHeight) /
    static_cast<float>(kMapCanvasRows);

constexpr std::uint32_t kSelectionOutlineThickness = 2;
constexpr float kSelectionOutlineRed = 1.0f;
constexpr float kSelectionOutlineGreen = 0.85f;
constexpr float kSelectionOutlineBlue = 0.15f;

constexpr float kAtlasTileWidth =
    (2.0f * kTilesetPreviewHalfWidth) /
    static_cast<float>(kOutdoorTilesetColumns);

constexpr float kAtlasTileHeight =
    (2.0f * kTilesetPreviewHalfHeight) /
    static_cast<float>(kOutdoorTilesetRows);

constexpr float kSelectionOutlineWidth =
    (2.0f * static_cast<float>(kSelectionOutlineThickness)) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kSelectionOutlineHeight =
    (2.0f * static_cast<float>(kSelectionOutlineThickness)) /
    static_cast<float>(kInitialWindowHeight);

constexpr std::uint32_t kGridLineThickness = 1;
constexpr float kTilesetGridRed = 0.22f;
constexpr float kTilesetGridGreen = 0.20f;
constexpr float kTilesetGridBlue = 0.32f;
constexpr float kMapCanvasRed = 0.06f;
constexpr float kMapCanvasGreen = 0.075f;
constexpr float kMapCanvasBlue = 0.12f;
constexpr float kMapGridRed = 0.24f;
constexpr float kMapGridGreen = 0.27f;
constexpr float kMapGridBlue = 0.38f;

constexpr float kGridLineWidth =
    (2.0f * static_cast<float>(kGridLineThickness)) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kGridLineHeight =
    (2.0f * static_cast<float>(kGridLineThickness)) /
    static_cast<float>(kInitialWindowHeight);

constexpr std::array<Vertex2D, 4> kTilesetPreviewVertices{{
    Vertex2D{-kTilesetPreviewHalfWidth, -kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
    Vertex2D{ kTilesetPreviewHalfWidth, -kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
    Vertex2D{ kTilesetPreviewHalfWidth,  kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    Vertex2D{-kTilesetPreviewHalfWidth,  kTilesetPreviewHalfHeight, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f}
}};

constexpr Vertex2D solid_color_vertex(
    const float position_x,
    const float position_y,
    const float red,
    const float green,
    const float blue
)
{
    return Vertex2D{
        position_x,
        position_y,
        red,
        green,
        blue,
        0.0f,
        0.0f,
        0
    };
}

constexpr Vertex2D selection_outline_vertex(
    const float position_x,
    const float position_y
)
{
    return solid_color_vertex(
        position_x,
        position_y,
        kSelectionOutlineRed,
        kSelectionOutlineGreen,
        kSelectionOutlineBlue
    );
}

constexpr std::size_t kTilesetGridLineCount =
    (kOutdoorTilesetColumns + 1) +
    (kOutdoorTilesetRows + 1);

constexpr std::size_t kTilesetGridVertexCount =
    kTilesetGridLineCount * 4;

using TilesetGridVertices =
    std::array<Vertex2D, kTilesetGridVertexCount>;

constexpr void append_grid_line(
    TilesetGridVertices& vertices,
    std::size_t& next_vertex,
    const float left,
    const float top,
    const float right,
    const float bottom
)
{
    vertices[next_vertex++] = solid_color_vertex(
        left,
        top,
        kTilesetGridRed,
        kTilesetGridGreen,
        kTilesetGridBlue
    );
    vertices[next_vertex++] = solid_color_vertex(
        right,
        top,
        kTilesetGridRed,
        kTilesetGridGreen,
        kTilesetGridBlue
    );
    vertices[next_vertex++] = solid_color_vertex(
        right,
        bottom,
        kTilesetGridRed,
        kTilesetGridGreen,
        kTilesetGridBlue
    );
    vertices[next_vertex++] = solid_color_vertex(
        left,
        bottom,
        kTilesetGridRed,
        kTilesetGridGreen,
        kTilesetGridBlue
    );
}

constexpr TilesetGridVertices make_tileset_grid_vertices()
{
    TilesetGridVertices vertices{};
    std::size_t next_vertex = 0;

    for (std::uint32_t column = 0;
         column <= kOutdoorTilesetColumns;
         ++column) {
        const float x =
            -kTilesetPreviewHalfWidth +
            static_cast<float>(column) * kAtlasTileWidth;
        const float left =
            column == kOutdoorTilesetColumns
                ? x - kGridLineWidth
                : x;
        const float right =
            column == kOutdoorTilesetColumns
                ? x
                : x + kGridLineWidth;

        append_grid_line(
            vertices,
            next_vertex,
            left,
            -kTilesetPreviewHalfHeight,
            right,
            kTilesetPreviewHalfHeight
        );
    }

    for (std::uint32_t row = 0;
         row <= kOutdoorTilesetRows;
         ++row) {
        const float y =
            -kTilesetPreviewHalfHeight +
            static_cast<float>(row) * kAtlasTileHeight;
        const float top =
            row == kOutdoorTilesetRows
                ? y - kGridLineHeight
                : y;
        const float bottom =
            row == kOutdoorTilesetRows
                ? y
                : y + kGridLineHeight;

        append_grid_line(
            vertices,
            next_vertex,
            -kTilesetPreviewHalfWidth,
            top,
            kTilesetPreviewHalfWidth,
            bottom
        );
    }

    return vertices;
}

constexpr TilesetGridVertices kTilesetGridVertices =
    make_tileset_grid_vertices();

constexpr TilesetGridVertices kHiddenTilesetGridVertices{};

constexpr std::size_t kMapGridLineCount =
    (kMapCanvasColumns + 1) +
    (kMapCanvasRows + 1);

constexpr std::size_t kMapCanvasQuadCount =
    1 + kMapGridLineCount;

using MapCanvasVertices =
    std::array<Vertex2D, kMapCanvasQuadCount * 4>;

constexpr void append_map_canvas_quad(
    MapCanvasVertices& vertices,
    std::size_t& next_vertex,
    const float left,
    const float top,
    const float right,
    const float bottom,
    const float red,
    const float green,
    const float blue
)
{
    vertices[next_vertex++] =
        solid_color_vertex(left, top, red, green, blue);
    vertices[next_vertex++] =
        solid_color_vertex(right, top, red, green, blue);
    vertices[next_vertex++] =
        solid_color_vertex(right, bottom, red, green, blue);
    vertices[next_vertex++] =
        solid_color_vertex(left, bottom, red, green, blue);
}

constexpr MapCanvasVertices make_map_canvas_vertices()
{
    MapCanvasVertices vertices{};
    std::size_t next_vertex = 0;

    append_map_canvas_quad(
        vertices,
        next_vertex,
        kMapCanvasLeft,
        kMapCanvasTop,
        kMapCanvasRight,
        kMapCanvasBottom,
        kMapCanvasRed,
        kMapCanvasGreen,
        kMapCanvasBlue
    );

    for (std::uint32_t column = 0;
         column <= kMapCanvasColumns;
         ++column) {
        const float x =
            kMapCanvasLeft +
            static_cast<float>(column) * kMapCanvasCellWidth;
        const float left =
            column == kMapCanvasColumns
                ? x - kGridLineWidth
                : x;
        const float right =
            column == kMapCanvasColumns
                ? x
                : x + kGridLineWidth;

        append_map_canvas_quad(
            vertices,
            next_vertex,
            left,
            kMapCanvasTop,
            right,
            kMapCanvasBottom,
            kMapGridRed,
            kMapGridGreen,
            kMapGridBlue
        );
    }

    for (std::uint32_t row = 0;
         row <= kMapCanvasRows;
         ++row) {
        const float y =
            kMapCanvasTop +
            static_cast<float>(row) * kMapCanvasCellHeight;
        const float top =
            row == kMapCanvasRows
                ? y - kGridLineHeight
                : y;
        const float bottom =
            row == kMapCanvasRows
                ? y
                : y + kGridLineHeight;

        append_map_canvas_quad(
            vertices,
            next_vertex,
            kMapCanvasLeft,
            top,
            kMapCanvasRight,
            bottom,
            kMapGridRed,
            kMapGridGreen,
            kMapGridBlue
        );
    }

    return vertices;
}

constexpr MapCanvasVertices kMapCanvasVertices =
    make_map_canvas_vertices();

constexpr std::size_t kTileSelectionVertexCount = 12;

constexpr std::uint32_t selected_region_preview_scale(
    const std::uint32_t column_count,
    const std::uint32_t row_count
)
{
    const std::uint32_t pixel_width =
        column_count * kTilesetTileWidth;
    const std::uint32_t pixel_height =
        row_count * kTilesetTileHeight;

    return std::min(
        kSelectedRegionPreviewMaxScale,
        std::min(
            kSelectedRegionPreviewMaxWidth / pixel_width,
            kSelectedRegionPreviewMaxHeight / pixel_height
        )
    );
}

static_assert(selected_region_preview_scale(1, 1) == 4);
static_assert(selected_region_preview_scale(3, 2) == 4);
static_assert(
    selected_region_preview_scale(
        kOutdoorTilesetColumns,
        kOutdoorTilesetRows
    ) == 1
);

constexpr std::array<Vertex2D, kTileSelectionVertexCount>
make_tile_selection_vertices(
    const std::uint32_t selected_left,
    const std::uint32_t selected_top,
    const std::uint32_t selected_right,
    const std::uint32_t selected_bottom
)
{
    const TextureRegion selected_region =
        tile_texture_region(
            selected_left,
            selected_top,
            selected_right,
            selected_bottom
        );

    const std::uint32_t selected_column_count =
        selected_right - selected_left + 1;
    const std::uint32_t selected_row_count =
        selected_bottom - selected_top + 1;
    const std::uint32_t preview_scale =
        selected_region_preview_scale(
            selected_column_count,
            selected_row_count
        );
    const float preview_half_width =
        static_cast<float>(
            selected_column_count *
            kTilesetTileWidth *
            preview_scale
        ) /
        static_cast<float>(kInitialWindowWidth);
    const float preview_half_height =
        static_cast<float>(
            selected_row_count *
            kTilesetTileHeight *
            preview_scale
        ) /
        static_cast<float>(kInitialWindowHeight);

    const float tile_left =
        -kTilesetPreviewHalfWidth +
        static_cast<float>(selected_left) * kAtlasTileWidth;

    const float tile_top =
        -kTilesetPreviewHalfHeight +
        static_cast<float>(selected_top) * kAtlasTileHeight;

    const float tile_right =
        -kTilesetPreviewHalfWidth +
        static_cast<float>(selected_right + 1) * kAtlasTileWidth;
    const float tile_bottom =
        -kTilesetPreviewHalfHeight +
        static_cast<float>(selected_bottom + 1) * kAtlasTileHeight;

    const float inner_left = tile_left + kSelectionOutlineWidth;
    const float inner_top = tile_top + kSelectionOutlineHeight;
    const float inner_right = tile_right - kSelectionOutlineWidth;
    const float inner_bottom = tile_bottom - kSelectionOutlineHeight;

    return {{
        Vertex2D{
            kRightPanelCenterX - preview_half_width,
            kSelectedRegionPreviewCenterY - preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.left,
            selected_region.top
        },
        Vertex2D{
            kRightPanelCenterX + preview_half_width,
            kSelectedRegionPreviewCenterY - preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.right,
            selected_region.top
        },
        Vertex2D{
            kRightPanelCenterX + preview_half_width,
            kSelectedRegionPreviewCenterY + preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.right,
            selected_region.bottom
        },
        Vertex2D{
            kRightPanelCenterX - preview_half_width,
            kSelectedRegionPreviewCenterY + preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.left,
            selected_region.bottom
        },
        selection_outline_vertex(tile_left, tile_top),
        selection_outline_vertex(tile_right, tile_top),
        selection_outline_vertex(tile_right, tile_bottom),
        selection_outline_vertex(tile_left, tile_bottom),
        selection_outline_vertex(inner_left, inner_top),
        selection_outline_vertex(inner_right, inner_top),
        selection_outline_vertex(inner_right, inner_bottom),
        selection_outline_vertex(inner_left, inner_bottom)
    }};
}

constexpr std::size_t kQuadVertexCount =
    kTilesetPreviewVertices.size() +
    kTilesetGridVertices.size() +
    kMapCanvasVertices.size() +
    kTileSelectionVertexCount;

constexpr std::size_t kTilesetGridVertexByteOffset =
    sizeof(kTilesetPreviewVertices);

constexpr std::size_t kMapCanvasVertexByteOffset =
    sizeof(kTilesetPreviewVertices) +
    sizeof(kTilesetGridVertices);

constexpr std::size_t kTileSelectionVertexByteOffset =
    sizeof(kTilesetPreviewVertices) +
    sizeof(kTilesetGridVertices) +
    sizeof(kMapCanvasVertices);

constexpr std::size_t kQuadIndexCount =
    (
        1 +
        kTilesetGridLineCount +
        kMapCanvasQuadCount +
        1 +
        4
    ) * 6;

using QuadIndices = std::array<std::uint16_t, kQuadIndexCount>;

constexpr void append_quad_indices(
    QuadIndices& indices,
    std::size_t& next_index,
    const std::uint16_t first_vertex
)
{
    indices[next_index++] = first_vertex;
    indices[next_index++] = first_vertex + 1;
    indices[next_index++] = first_vertex + 2;
    indices[next_index++] = first_vertex + 2;
    indices[next_index++] = first_vertex + 3;
    indices[next_index++] = first_vertex;
}

constexpr QuadIndices make_quad_indices()
{
    QuadIndices indices{};
    std::size_t next_index = 0;

    append_quad_indices(indices, next_index, 0);

    const std::uint16_t grid_first_vertex =
        static_cast<std::uint16_t>(kTilesetPreviewVertices.size());

    for (std::size_t line = 0;
         line < kTilesetGridLineCount;
         ++line) {
        append_quad_indices(
            indices,
            next_index,
            static_cast<std::uint16_t>(
                grid_first_vertex + line * 4
            )
        );
    }

    const std::uint16_t map_canvas_first_vertex =
        static_cast<std::uint16_t>(
            kTilesetPreviewVertices.size() +
            kTilesetGridVertices.size()
        );

    for (std::size_t quad = 0;
         quad < kMapCanvasQuadCount;
         ++quad) {
        append_quad_indices(
            indices,
            next_index,
            static_cast<std::uint16_t>(
                map_canvas_first_vertex + quad * 4
            )
        );
    }

    const std::uint16_t selection_first_vertex =
        static_cast<std::uint16_t>(
            kTilesetPreviewVertices.size() +
            kTilesetGridVertices.size() +
            kMapCanvasVertices.size()
        );

    append_quad_indices(
        indices,
        next_index,
        selection_first_vertex
    );

    const std::uint16_t outline_outer =
        selection_first_vertex + 4;
    const std::uint16_t outline_inner =
        outline_outer + 4;

    indices[next_index++] = outline_outer;
    indices[next_index++] = outline_outer + 1;
    indices[next_index++] = outline_inner + 1;
    indices[next_index++] = outline_inner + 1;
    indices[next_index++] = outline_inner;
    indices[next_index++] = outline_outer;

    indices[next_index++] = outline_outer + 1;
    indices[next_index++] = outline_outer + 2;
    indices[next_index++] = outline_inner + 2;
    indices[next_index++] = outline_inner + 2;
    indices[next_index++] = outline_inner + 1;
    indices[next_index++] = outline_outer + 1;

    indices[next_index++] = outline_outer + 2;
    indices[next_index++] = outline_outer + 3;
    indices[next_index++] = outline_inner + 3;
    indices[next_index++] = outline_inner + 3;
    indices[next_index++] = outline_inner + 2;
    indices[next_index++] = outline_outer + 2;

    indices[next_index++] = outline_outer + 3;
    indices[next_index++] = outline_outer;
    indices[next_index++] = outline_inner;
    indices[next_index++] = outline_inner;
    indices[next_index++] = outline_inner + 3;
    indices[next_index++] = outline_outer + 3;

    return indices;
}

constexpr QuadIndices kQuadIndices = make_quad_indices();

constexpr VkDeviceSize kQuadVertexBufferSize =
    sizeof(Vertex2D) * kQuadVertexCount;

constexpr VkDeviceSize kQuadIndexBufferSize =
    sizeof(std::uint16_t) * kQuadIndices.size();

}

Application::Application()
    : sdl_(),
      window_(Window::CreateInfo{
          .title = "Midnight",
          .width = static_cast<int>(kInitialWindowWidth),
          .height = static_cast<int>(kInitialWindowHeight),
          .resizable = true,
          .vulkan = true
      }),
      vulkan_instance_(),
      vulkan_surface_(window_, vulkan_instance_),
      vulkan_device_(vulkan_instance_, vulkan_surface_),
      vulkan_transfer_context_(vulkan_device_),
      vulkan_swapchain_(window_, vulkan_device_, vulkan_surface_),
      vulkan_render_pass_(vulkan_device_, vulkan_swapchain_),
      vulkan_graphics_pipeline_(
          vulkan_device_,
          vulkan_swapchain_,
          vulkan_render_pass_
      ),
      quad_vertex_buffer_(
          vulkan_device_,
          kQuadVertexBufferSize,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      ),
      quad_index_buffer_(
          vulkan_device_,
          kQuadIndexBufferSize,
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      ),
      texture_image_(
          vulkan_device_,
          VulkanImage::CreateInfo{
              .extent = VkExtent2D{
                  .width = kOutdoorTilesetWidth,
                  .height = kOutdoorTilesetHeight
              },
              .format = VK_FORMAT_R8G8B8A8_SRGB,
              .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT,
              .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
          }
      ),
      texture_sampler_(
          vulkan_device_,
          VulkanSampler::CreateInfo{}
      ),
      texture_descriptor_(
          vulkan_device_,
          vulkan_graphics_pipeline_.descriptor_set_layout(),
          texture_image_,
          texture_sampler_
      ),
      vulkan_frame_renderer_(
          vulkan_device_,
          vulkan_swapchain_,
          vulkan_render_pass_,
          vulkan_graphics_pipeline_,
          texture_descriptor_,
          quad_vertex_buffer_,
          quad_index_buffer_,
          static_cast<std::uint32_t>(kQuadIndices.size()),
          VK_INDEX_TYPE_UINT16
      ),
      selected_tile_left_(kInitialSelectedTileColumn),
      selected_tile_top_(kInitialSelectedTileRow),
      selected_tile_right_(kInitialSelectedTileColumn),
      selected_tile_bottom_(kInitialSelectedTileRow)
{
    quad_vertex_buffer_.upload(
        kTilesetPreviewVertices.data(),
        sizeof(kTilesetPreviewVertices)
    );

    upload_tileset_grid_vertices();

    quad_vertex_buffer_.upload(
        kMapCanvasVertices.data(),
        sizeof(kMapCanvasVertices),
        kMapCanvasVertexByteOffset
    );

    upload_tile_selection_vertices();

    quad_index_buffer_.upload(
        kQuadIndices.data(),
        kQuadIndexBufferSize
    );

    const std::filesystem::path outdoor_tileset_path =
        std::filesystem::path(MIDNIGHT_ASSET_DIR) /
        "tilesets/basic_village/outdoor_tileset.png";

    const RgbaImage outdoor_tileset =
        load_png_rgba8(outdoor_tileset_path);

    if (outdoor_tileset.width != kOutdoorTilesetWidth ||
        outdoor_tileset.height != kOutdoorTilesetHeight) {
        throw std::runtime_error(
            "Unexpected outdoor tileset dimensions: expected " +
            std::to_string(kOutdoorTilesetWidth) +
            "x" +
            std::to_string(kOutdoorTilesetHeight) +
            ", got " +
            std::to_string(outdoor_tileset.width) +
            "x" +
            std::to_string(outdoor_tileset.height)
        );
    }

    if (outdoor_tileset.byte_size() != kOutdoorTilesetByteSize) {
        throw std::runtime_error(
            "Unexpected outdoor tileset byte size: expected " +
            std::to_string(kOutdoorTilesetByteSize) +
            ", got " +
            std::to_string(outdoor_tileset.byte_size())
        );
    }

    std::cout << "[Midnight] Decoded PNG: "
              << outdoor_tileset_path.lexically_normal().string()
              << " "
              << outdoor_tileset.width
              << "x"
              << outdoor_tileset.height
              << " RGBA8 ("
              << outdoor_tileset.byte_size()
              << " bytes)"
              << '\n';

    vulkan_transfer_context_.upload_to_new_sampled_image(
        texture_image_,
        outdoor_tileset.pixels.data(),
        static_cast<VkDeviceSize>(outdoor_tileset.byte_size())
    );
}

int Application::run()
{
    print_startup_info();

    while (running_) {
        poll_events();

        if (!running_) {
            break;
        }

        const bool frame_rendered = vulkan_frame_renderer_.draw_frame();

        if (!frame_rendered) {
            std::cout << "[Midnight] Swapchain needs recreation. Exiting for this increment.\n";
            running_ = false;
        }
    }

    return 0;
}

void Application::print_startup_info() const
{
    std::cout << "[Midnight] Application started\n";
    std::cout << "[Midnight] Window size: "
              << window_.width()
              << "x"
              << window_.height()
              << '\n';
    std::cout << "[Midnight] Window pixel size: "
              << window_.pixel_width()
              << "x"
              << window_.pixel_height()
              << '\n';
    std::cout << "[Midnight] Outdoor tileset grid: "
              << kOutdoorTilesetColumns
              << "x"
              << kOutdoorTilesetRows
              << " tiles at "
              << kTilesetTileWidth
              << "x"
              << kTilesetTileHeight
              << " pixels\n";
    std::cout << "[Midnight] Blank map canvas: "
              << kMapCanvasColumns
              << "x"
              << kMapCanvasRows
              << " tiles at "
              << kMapCanvasScale
              << "x\n";
    std::cout << "[Midnight] Rendering the outdoor tileset at "
              << kTilesetPreviewScale
              << "x\n";
    print_tile_selection();
    std::cout << "[Midnight] Use the arrow keys, click, or drag across the atlas to select tiles\n";
    std::cout << "[Midnight] Press G to toggle the atlas grid\n";
    std::cout << "[Midnight] Press Escape or close the window to quit\n";
}

void Application::poll_events()
{
    SDL_Event event{};
    bool tile_selection_drag_update_pending = false;
    float pending_tile_selection_drag_x = 0.0f;
    float pending_tile_selection_drag_y = 0.0f;

    const auto flush_pending_tile_selection_drag = [&]() {
        if (!tile_selection_dragging_ ||
            !tile_selection_drag_update_pending) {
            return;
        }

        tile_selection_drag_update_pending = false;
        update_tile_selection_drag(
            pending_tile_selection_drag_x,
            pending_tile_selection_drag_y
        );
    };

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                flush_pending_tile_selection_drag();

                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running_ = false;
                        break;

                    case SDLK_LEFT:
                        move_tile_selection(-1, 0);
                        break;

                    case SDLK_RIGHT:
                        move_tile_selection(1, 0);
                        break;

                    case SDLK_UP:
                        move_tile_selection(0, -1);
                        break;

                    case SDLK_DOWN:
                        move_tile_selection(0, 1);
                        break;

                    case SDLK_G:
                        if (!event.key.repeat) {
                            toggle_tileset_grid();
                        }
                        break;

                    default:
                        break;
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    flush_pending_tile_selection_drag();
                    begin_tile_selection_drag(
                        event.button.x,
                        event.button.y
                    );
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (tile_selection_dragging_) {
                    if ((event.motion.state & SDL_BUTTON_LMASK) != 0) {
                        pending_tile_selection_drag_x = event.motion.x;
                        pending_tile_selection_drag_y = event.motion.y;
                        tile_selection_drag_update_pending = true;
                    } else {
                        tile_selection_drag_update_pending = false;
                        end_tile_selection_drag(
                            event.motion.x,
                            event.motion.y
                        );
                    }
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    tile_selection_drag_update_pending = false;
                    end_tile_selection_drag(
                        event.button.x,
                        event.button.y
                    );
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                flush_pending_tile_selection_drag();
                window_.refresh_size();
                std::cout << "[Midnight] Window resized: "
                          << window_.width()
                          << "x"
                          << window_.height()
                          << " logical, "
                          << window_.pixel_width()
                          << "x"
                          << window_.pixel_height()
                          << " pixels"
                          << '\n';
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if (tile_selection_dragging_) {
                    flush_pending_tile_selection_drag();
                    tile_selection_dragging_ = false;
                    print_tile_selection();
                }
                break;

            default:
                break;
        }
    }

    flush_pending_tile_selection_drag();
}

void Application::toggle_tileset_grid()
{
    vulkan_device_.wait_idle();

    tileset_grid_visible_ = !tileset_grid_visible_;
    upload_tileset_grid_vertices();

    std::cout << "[Midnight] Atlas grid "
              << (tileset_grid_visible_ ? "shown" : "hidden")
              << '\n';
}

void Application::upload_tileset_grid_vertices()
{
    const TilesetGridVertices& vertices =
        tileset_grid_visible_
            ? kTilesetGridVertices
            : kHiddenTilesetGridVertices;

    quad_vertex_buffer_.upload(
        vertices.data(),
        sizeof(vertices),
        kTilesetGridVertexByteOffset
    );
}

void Application::move_tile_selection(
    const int column_delta,
    const int row_delta
)
{
    const int next_column = std::clamp(
        static_cast<int>(selected_tile_left_) + column_delta,
        0,
        static_cast<int>(kOutdoorTilesetColumns) - 1
    );

    const int next_row = std::clamp(
        static_cast<int>(selected_tile_top_) + row_delta,
        0,
        static_cast<int>(kOutdoorTilesetRows) - 1
    );

    if (set_tile_selection(
            static_cast<std::uint32_t>(next_column),
            static_cast<std::uint32_t>(next_row),
            static_cast<std::uint32_t>(next_column),
            static_cast<std::uint32_t>(next_row)
        )) {
        print_tile_selection();
    }
}

void Application::begin_tile_selection_drag(
    const float x,
    const float y
)
{
    std::uint32_t column = 0;
    std::uint32_t row = 0;

    if (!window_position_to_tile(
            x,
            y,
            false,
            column,
            row
        )) {
        return;
    }

    tile_selection_drag_anchor_column_ = column;
    tile_selection_drag_anchor_row_ = row;
    tile_selection_dragging_ = true;

    (void)set_tile_selection(column, row, column, row);
}

void Application::update_tile_selection_drag(
    const float x,
    const float y
)
{
    if (!tile_selection_dragging_) {
        return;
    }

    std::uint32_t column = 0;
    std::uint32_t row = 0;

    if (!window_position_to_tile(
            x,
            y,
            true,
            column,
            row
        )) {
        return;
    }

    (void)set_tile_selection(
        tile_selection_drag_anchor_column_,
        tile_selection_drag_anchor_row_,
        column,
        row
    );
}

void Application::end_tile_selection_drag(
    const float x,
    const float y
)
{
    if (!tile_selection_dragging_) {
        return;
    }

    update_tile_selection_drag(x, y);
    tile_selection_dragging_ = false;
    print_tile_selection();
}

bool Application::window_position_to_tile(
    const float x,
    const float y,
    const bool clamp_to_atlas,
    std::uint32_t& column,
    std::uint32_t& row
) const
{
    if (window_.width() <= 0 || window_.height() <= 0) {
        return false;
    }

    const float normalized_x =
        (2.0f * x / static_cast<float>(window_.width())) - 1.0f;

    const float normalized_y =
        (2.0f * y / static_cast<float>(window_.height())) - 1.0f;

    const bool position_is_in_atlas =
        normalized_x >= -kTilesetPreviewHalfWidth &&
        normalized_x < kTilesetPreviewHalfWidth &&
        normalized_y >= -kTilesetPreviewHalfHeight &&
        normalized_y < kTilesetPreviewHalfHeight;

    if (!clamp_to_atlas && !position_is_in_atlas) {
        return false;
    }

    const float atlas_x = std::clamp(
        (normalized_x + kTilesetPreviewHalfWidth) /
            (2.0f * kTilesetPreviewHalfWidth),
        0.0f,
        1.0f
    );

    const float atlas_y = std::clamp(
        (normalized_y + kTilesetPreviewHalfHeight) /
            (2.0f * kTilesetPreviewHalfHeight),
        0.0f,
        1.0f
    );

    column = std::min(
        static_cast<std::uint32_t>(
            atlas_x * static_cast<float>(kOutdoorTilesetColumns)
        ),
        kOutdoorTilesetColumns - 1
    );

    row = std::min(
        static_cast<std::uint32_t>(
            atlas_y * static_cast<float>(kOutdoorTilesetRows)
        ),
        kOutdoorTilesetRows - 1
    );

    return true;
}

bool Application::set_tile_selection(
    const std::uint32_t first_column,
    const std::uint32_t first_row,
    const std::uint32_t second_column,
    const std::uint32_t second_row
)
{
    if (first_column >= kOutdoorTilesetColumns ||
        second_column >= kOutdoorTilesetColumns ||
        first_row >= kOutdoorTilesetRows ||
        second_row >= kOutdoorTilesetRows) {
        return false;
    }

    const std::uint32_t left =
        std::min(first_column, second_column);
    const std::uint32_t top =
        std::min(first_row, second_row);
    const std::uint32_t right =
        std::max(first_column, second_column);
    const std::uint32_t bottom =
        std::max(first_row, second_row);

    if (left == selected_tile_left_ &&
        top == selected_tile_top_ &&
        right == selected_tile_right_ &&
        bottom == selected_tile_bottom_) {
        return false;
    }

    vulkan_device_.wait_idle();

    selected_tile_left_ = left;
    selected_tile_top_ = top;
    selected_tile_right_ = right;
    selected_tile_bottom_ = bottom;

    upload_tile_selection_vertices();
    return true;
}

void Application::print_tile_selection() const
{
    const std::uint32_t selected_column_count =
        selected_tile_right_ - selected_tile_left_ + 1;
    const std::uint32_t selected_row_count =
        selected_tile_bottom_ - selected_tile_top_ + 1;

    std::cout << "[Midnight] Selected region: ("
              << selected_tile_left_
              << ", "
              << selected_tile_top_
              << ") to ("
              << selected_tile_right_
              << ", "
              << selected_tile_bottom_
              << "), "
              << selected_column_count
              << "x"
              << selected_row_count
              << " tiles, preview "
              << selected_region_preview_scale(
                     selected_column_count,
                     selected_row_count
                 )
              << "x\n";
}

void Application::upload_tile_selection_vertices()
{
    const std::array<Vertex2D, kTileSelectionVertexCount> vertices =
        make_tile_selection_vertices(
            selected_tile_left_,
            selected_tile_top_,
            selected_tile_right_,
            selected_tile_bottom_
        );

    quad_vertex_buffer_.upload(
        vertices.data(),
        sizeof(vertices),
        kTileSelectionVertexByteOffset
    );
}

}
