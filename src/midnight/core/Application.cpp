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
constexpr std::uint32_t kSelectedTilePreviewScale = 4;
constexpr std::size_t kOutdoorTilesetByteSize =
    static_cast<std::size_t>(kOutdoorTilesetWidth) *
    static_cast<std::size_t>(kOutdoorTilesetHeight) *
    RgbaImage::bytes_per_pixel;

static_assert(kOutdoorTilesetWidth % kTilesetTileWidth == 0);
static_assert(kOutdoorTilesetHeight % kTilesetTileHeight == 0);
static_assert(kInitialSelectedTileColumn < kOutdoorTilesetColumns);
static_assert(kInitialSelectedTileRow < kOutdoorTilesetRows);

struct TextureRegion final {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

constexpr TextureRegion tile_texture_region(
    const std::uint32_t column,
    const std::uint32_t row
)
{
    return TextureRegion{
        .left = static_cast<float>(column * kTilesetTileWidth) /
            static_cast<float>(kOutdoorTilesetWidth),
        .top = static_cast<float>(row * kTilesetTileHeight) /
            static_cast<float>(kOutdoorTilesetHeight),
        .right =
            static_cast<float>((column + 1) * kTilesetTileWidth) /
            static_cast<float>(kOutdoorTilesetWidth),
        .bottom =
            static_cast<float>((row + 1) * kTilesetTileHeight) /
            static_cast<float>(kOutdoorTilesetHeight)
    };
}

constexpr float kTilesetPreviewHalfWidth =
    static_cast<float>(kOutdoorTilesetWidth * kTilesetPreviewScale) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kTilesetPreviewHalfHeight =
    static_cast<float>(kOutdoorTilesetHeight * kTilesetPreviewScale) /
    static_cast<float>(kInitialWindowHeight);

constexpr float kSelectedTilePreviewCenterX = 0.65f;
constexpr float kSelectedTilePreviewHalfWidth =
    static_cast<float>(kTilesetTileWidth * kSelectedTilePreviewScale) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kSelectedTilePreviewHalfHeight =
    static_cast<float>(kTilesetTileHeight * kSelectedTilePreviewScale) /
    static_cast<float>(kInitialWindowHeight);

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

constexpr std::uint32_t kTilesetGridThickness = 1;
constexpr float kTilesetGridRed = 0.22f;
constexpr float kTilesetGridGreen = 0.20f;
constexpr float kTilesetGridBlue = 0.32f;

constexpr float kTilesetGridWidth =
    (2.0f * static_cast<float>(kTilesetGridThickness)) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kTilesetGridHeight =
    (2.0f * static_cast<float>(kTilesetGridThickness)) /
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
                ? x - kTilesetGridWidth
                : x;
        const float right =
            column == kOutdoorTilesetColumns
                ? x
                : x + kTilesetGridWidth;

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
                ? y - kTilesetGridHeight
                : y;
        const float bottom =
            row == kOutdoorTilesetRows
                ? y
                : y + kTilesetGridHeight;

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

constexpr std::size_t kTileSelectionVertexCount = 12;

constexpr std::array<Vertex2D, kTileSelectionVertexCount>
make_tile_selection_vertices(
    const std::uint32_t selected_column,
    const std::uint32_t selected_row
)
{
    const TextureRegion selected_region =
        tile_texture_region(selected_column, selected_row);

    const float tile_left =
        -kTilesetPreviewHalfWidth +
        static_cast<float>(selected_column) * kAtlasTileWidth;

    const float tile_top =
        -kTilesetPreviewHalfHeight +
        static_cast<float>(selected_row) * kAtlasTileHeight;

    const float tile_right = tile_left + kAtlasTileWidth;
    const float tile_bottom = tile_top + kAtlasTileHeight;

    const float inner_left = tile_left + kSelectionOutlineWidth;
    const float inner_top = tile_top + kSelectionOutlineHeight;
    const float inner_right = tile_right - kSelectionOutlineWidth;
    const float inner_bottom = tile_bottom - kSelectionOutlineHeight;

    return {{
        Vertex2D{
            kSelectedTilePreviewCenterX - kSelectedTilePreviewHalfWidth,
            -kSelectedTilePreviewHalfHeight,
            1.0f, 1.0f, 1.0f,
            selected_region.left,
            selected_region.top
        },
        Vertex2D{
            kSelectedTilePreviewCenterX + kSelectedTilePreviewHalfWidth,
            -kSelectedTilePreviewHalfHeight,
            1.0f, 1.0f, 1.0f,
            selected_region.right,
            selected_region.top
        },
        Vertex2D{
            kSelectedTilePreviewCenterX + kSelectedTilePreviewHalfWidth,
            kSelectedTilePreviewHalfHeight,
            1.0f, 1.0f, 1.0f,
            selected_region.right,
            selected_region.bottom
        },
        Vertex2D{
            kSelectedTilePreviewCenterX - kSelectedTilePreviewHalfWidth,
            kSelectedTilePreviewHalfHeight,
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
    kTileSelectionVertexCount;

constexpr std::size_t kTilesetGridVertexByteOffset =
    sizeof(kTilesetPreviewVertices);

constexpr std::size_t kTileSelectionVertexByteOffset =
    sizeof(kTilesetPreviewVertices) +
    sizeof(kTilesetGridVertices);

constexpr std::size_t kQuadIndexCount =
    (1 + kTilesetGridLineCount + 1 + 4) * 6;

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

    const std::uint16_t selection_first_vertex =
        static_cast<std::uint16_t>(
            kTilesetPreviewVertices.size() +
            kTilesetGridVertices.size()
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
      selected_tile_column_(kInitialSelectedTileColumn),
      selected_tile_row_(kInitialSelectedTileRow)
{
    quad_vertex_buffer_.upload(
        kTilesetPreviewVertices.data(),
        sizeof(kTilesetPreviewVertices)
    );

    upload_tileset_grid_vertices();

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
    std::cout << "[Midnight] Rendering the outdoor tileset at "
              << kTilesetPreviewScale
              << "x and tile ("
              << selected_tile_column_
              << ", "
              << selected_tile_row_
              << ") at "
              << kSelectedTilePreviewScale
              << "x\n";
    std::cout << "[Midnight] Use the arrow keys or click the atlas to select a tile\n";
    std::cout << "[Midnight] Press G to toggle the atlas grid\n";
    std::cout << "[Midnight] Press Escape or close the window to quit\n";
}

void Application::poll_events()
{
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;

            case SDL_EVENT_KEY_DOWN:
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
                    select_tile_at_window_position(
                        event.button.x,
                        event.button.y
                    );
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
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

            default:
                break;
        }
    }
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
        static_cast<int>(selected_tile_column_) + column_delta,
        0,
        static_cast<int>(kOutdoorTilesetColumns) - 1
    );

    const int next_row = std::clamp(
        static_cast<int>(selected_tile_row_) + row_delta,
        0,
        static_cast<int>(kOutdoorTilesetRows) - 1
    );

    select_tile(
        static_cast<std::uint32_t>(next_column),
        static_cast<std::uint32_t>(next_row)
    );
}

void Application::select_tile(
    const std::uint32_t column,
    const std::uint32_t row
)
{
    if (column >= kOutdoorTilesetColumns ||
        row >= kOutdoorTilesetRows) {
        return;
    }

    if (column == selected_tile_column_ &&
        row == selected_tile_row_) {
        return;
    }

    vulkan_device_.wait_idle();

    selected_tile_column_ = column;
    selected_tile_row_ = row;

    upload_tile_selection_vertices();

    std::cout << "[Midnight] Selected tile: ("
              << selected_tile_column_
              << ", "
              << selected_tile_row_
              << ")\n";
}

void Application::select_tile_at_window_position(
    const float x,
    const float y
)
{
    if (window_.width() <= 0 || window_.height() <= 0) {
        return;
    }

    const float normalized_x =
        (2.0f * x / static_cast<float>(window_.width())) - 1.0f;

    const float normalized_y =
        (2.0f * y / static_cast<float>(window_.height())) - 1.0f;

    if (normalized_x < -kTilesetPreviewHalfWidth ||
        normalized_x >= kTilesetPreviewHalfWidth ||
        normalized_y < -kTilesetPreviewHalfHeight ||
        normalized_y >= kTilesetPreviewHalfHeight) {
        return;
    }

    const float atlas_x =
        (normalized_x + kTilesetPreviewHalfWidth) /
        (2.0f * kTilesetPreviewHalfWidth);

    const float atlas_y =
        (normalized_y + kTilesetPreviewHalfHeight) /
        (2.0f * kTilesetPreviewHalfHeight);

    const std::uint32_t column = std::min(
        static_cast<std::uint32_t>(
            atlas_x * static_cast<float>(kOutdoorTilesetColumns)
        ),
        kOutdoorTilesetColumns - 1
    );

    const std::uint32_t row = std::min(
        static_cast<std::uint32_t>(
            atlas_y * static_cast<float>(kOutdoorTilesetRows)
        ),
        kOutdoorTilesetRows - 1
    );

    select_tile(column, row);
}

void Application::upload_tile_selection_vertices()
{
    const std::array<Vertex2D, kTileSelectionVertexCount> vertices =
        make_tile_selection_vertices(
            selected_tile_column_,
            selected_tile_row_
        );

    quad_vertex_buffer_.upload(
        vertices.data(),
        sizeof(vertices),
        kTileSelectionVertexByteOffset
    );
}

}
