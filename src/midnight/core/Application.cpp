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
#include <utility>

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
constexpr std::uint32_t kTilesetPreviewScale = 2;
constexpr std::uint32_t kInitialSelectedTileColumn = 1;
constexpr std::uint32_t kInitialSelectedTileRow = 0;
constexpr std::uint32_t kSelectedRegionPreviewMaxScale = 3;
constexpr std::uint32_t kSelectedRegionPreviewMaxWidth = 192;
constexpr std::uint32_t kSelectedRegionPreviewMaxHeight = 256;
constexpr std::uint32_t kMapCanvasColumns = 16;
constexpr std::uint32_t kMapCanvasRows = 12;
constexpr std::uint32_t kMapCanvasScale = 2;
constexpr std::uint64_t kSwapchainResizeSettleMilliseconds = 250;
constexpr std::size_t kMapCanvasCellCount =
    static_cast<std::size_t>(kMapCanvasColumns) *
    static_cast<std::size_t>(kMapCanvasRows);
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

constexpr float kTilesetPreviewCenterX = -0.60f;
constexpr float kTilesetPreviewCenterY = -0.25f;
constexpr float kTilesetPreviewLeft =
    kTilesetPreviewCenterX - kTilesetPreviewHalfWidth;
constexpr float kTilesetPreviewTop =
    kTilesetPreviewCenterY - kTilesetPreviewHalfHeight;
constexpr float kTilesetPreviewRight =
    kTilesetPreviewCenterX + kTilesetPreviewHalfWidth;
constexpr float kTilesetPreviewBottom =
    kTilesetPreviewCenterY + kTilesetPreviewHalfHeight;

constexpr float kSelectedRegionPreviewCenterX = -0.60f;
constexpr float kSelectedRegionPreviewCenterY = 7.0f / 12.0f;
constexpr float kSelectedRegionPreviewMaxHalfWidth =
    static_cast<float>(kSelectedRegionPreviewMaxWidth) /
    static_cast<float>(kInitialWindowWidth);
constexpr float kSelectedRegionPreviewMaxHalfHeight =
    static_cast<float>(kSelectedRegionPreviewMaxHeight) /
    static_cast<float>(kInitialWindowHeight);
constexpr float kMapCanvasCenterX = 0.35f;
constexpr float kMapCanvasCenterY = 0.0f;

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
    kMapCanvasCenterX - kMapCanvasHalfWidth;
constexpr float kMapCanvasTop =
    kMapCanvasCenterY - kMapCanvasHalfHeight;
constexpr float kMapCanvasRight =
    kMapCanvasCenterX + kMapCanvasHalfWidth;
constexpr float kMapCanvasBottom =
    kMapCanvasCenterY + kMapCanvasHalfHeight;
constexpr float kMapCanvasCellWidth =
    (2.0f * kMapCanvasHalfWidth) /
    static_cast<float>(kMapCanvasColumns);
constexpr float kMapCanvasCellHeight =
    (2.0f * kMapCanvasHalfHeight) /
    static_cast<float>(kMapCanvasRows);

static_assert(kTilesetPreviewLeft >= -1.0f);
static_assert(kTilesetPreviewTop >= -1.0f);
static_assert(kTilesetPreviewRight <= 1.0f);
static_assert(kTilesetPreviewBottom <= 1.0f);
static_assert(kMapCanvasLeft >= -1.0f);
static_assert(kMapCanvasTop >= -1.0f);
static_assert(kMapCanvasRight <= 1.0f);
static_assert(kMapCanvasBottom <= 1.0f);
static_assert(
    kSelectedRegionPreviewCenterX -
        kSelectedRegionPreviewMaxHalfWidth >= -1.0f
);
static_assert(
    kSelectedRegionPreviewCenterX +
        kSelectedRegionPreviewMaxHalfWidth <= 1.0f
);
static_assert(
    kSelectedRegionPreviewCenterY -
        kSelectedRegionPreviewMaxHalfHeight >= -1.0f
);
static_assert(
    kSelectedRegionPreviewCenterY +
        kSelectedRegionPreviewMaxHalfHeight <= 1.0f
);
static_assert(kTilesetPreviewRight < kMapCanvasLeft);
static_assert(
    kTilesetPreviewBottom <
        kSelectedRegionPreviewCenterY -
            kSelectedRegionPreviewMaxHalfHeight
);

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
constexpr std::uint32_t kMapHoverOutlineThickness = 2;
constexpr float kMapHoverRed = 0.10f;
constexpr float kMapHoverGreen = 0.85f;
constexpr float kMapHoverBlue = 1.0f;

constexpr float kGridLineWidth =
    (2.0f * static_cast<float>(kGridLineThickness)) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kGridLineHeight =
    (2.0f * static_cast<float>(kGridLineThickness)) /
    static_cast<float>(kInitialWindowHeight);

constexpr float kMapHoverOutlineWidth =
    (2.0f * static_cast<float>(kMapHoverOutlineThickness)) /
    static_cast<float>(kInitialWindowWidth);

constexpr float kMapHoverOutlineHeight =
    (2.0f * static_cast<float>(kMapHoverOutlineThickness)) /
    static_cast<float>(kInitialWindowHeight);

constexpr std::array<Vertex2D, 4> kTilesetPreviewVertices{{
    Vertex2D{kTilesetPreviewLeft,  kTilesetPreviewTop,    1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
    Vertex2D{kTilesetPreviewRight, kTilesetPreviewTop,    1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
    Vertex2D{kTilesetPreviewRight, kTilesetPreviewBottom, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    Vertex2D{kTilesetPreviewLeft,  kTilesetPreviewBottom, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f}
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

constexpr Vertex2D map_hover_vertex(
    const float position_x,
    const float position_y
)
{
    return solid_color_vertex(
        position_x,
        position_y,
        kMapHoverRed,
        kMapHoverGreen,
        kMapHoverBlue
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
            kTilesetPreviewLeft +
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
            kTilesetPreviewTop,
            right,
            kTilesetPreviewBottom
        );
    }

    for (std::uint32_t row = 0;
         row <= kOutdoorTilesetRows;
         ++row) {
        const float y =
            kTilesetPreviewTop +
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
            kTilesetPreviewLeft,
            top,
            kTilesetPreviewRight,
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

constexpr std::size_t kMapCanvasBackgroundVertexCount = 4;
constexpr std::size_t kMapGridVertexCount =
    kMapGridLineCount * 4;

using MapCanvasVertices =
    std::array<Vertex2D, kMapCanvasQuadCount * 4>;

using MapGridVertices =
    std::array<Vertex2D, kMapGridVertexCount>;

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

constexpr MapGridVertices kHiddenMapGridVertices{};

static_assert(
    kMapCanvasVertices.size() ==
    kMapCanvasBackgroundVertexCount + kMapGridVertexCount
);

using MapTileCellVertices = std::array<Vertex2D, 4>;

constexpr MapTileCellVertices make_map_tile_vertices(
    const std::uint32_t map_column,
    const std::uint32_t map_row,
    const std::uint32_t tileset_column,
    const std::uint32_t tileset_row
)
{
    const float left =
        kMapCanvasLeft +
        static_cast<float>(map_column) * kMapCanvasCellWidth;
    const float top =
        kMapCanvasTop +
        static_cast<float>(map_row) * kMapCanvasCellHeight;
    const float right = left + kMapCanvasCellWidth;
    const float bottom = top + kMapCanvasCellHeight;
    const TextureRegion texture_region =
        tile_texture_region(
            tileset_column,
            tileset_row,
            tileset_column,
            tileset_row
        );

    return {{
        Vertex2D{
            left, top,
            1.0f, 1.0f, 1.0f,
            texture_region.left,
            texture_region.top
        },
        Vertex2D{
            right, top,
            1.0f, 1.0f, 1.0f,
            texture_region.right,
            texture_region.top
        },
        Vertex2D{
            right, bottom,
            1.0f, 1.0f, 1.0f,
            texture_region.right,
            texture_region.bottom
        },
        Vertex2D{
            left, bottom,
            1.0f, 1.0f, 1.0f,
            texture_region.left,
            texture_region.bottom
        }
    }};
}

constexpr MapTileCellVertices kEmptyMapTileCellVertices{};

constexpr std::size_t kMapTileVertexCount =
    kMapCanvasCellCount * MapTileCellVertices{}.size();

using MapTileVertices =
    std::array<Vertex2D, kMapTileVertexCount>;

constexpr MapTileVertices kEmptyMapTileVertices{};

constexpr std::size_t kMapHoverVertexCount = 8;

using MapHoverVertices =
    std::array<Vertex2D, kMapHoverVertexCount>;

constexpr MapHoverVertices make_map_hover_vertices(
    const std::uint32_t column,
    const std::uint32_t row
)
{
    const float left =
        kMapCanvasLeft +
        static_cast<float>(column) * kMapCanvasCellWidth;
    const float top =
        kMapCanvasTop +
        static_cast<float>(row) * kMapCanvasCellHeight;
    const float right = left + kMapCanvasCellWidth;
    const float bottom = top + kMapCanvasCellHeight;

    const float inner_left = left + kMapHoverOutlineWidth;
    const float inner_top = top + kMapHoverOutlineHeight;
    const float inner_right = right - kMapHoverOutlineWidth;
    const float inner_bottom = bottom - kMapHoverOutlineHeight;

    return {{
        map_hover_vertex(left, top),
        map_hover_vertex(right, top),
        map_hover_vertex(right, bottom),
        map_hover_vertex(left, bottom),
        map_hover_vertex(inner_left, inner_top),
        map_hover_vertex(inner_right, inner_top),
        map_hover_vertex(inner_right, inner_bottom),
        map_hover_vertex(inner_left, inner_bottom)
    }};
}

constexpr MapHoverVertices kHiddenMapHoverVertices{};

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

static_assert(selected_region_preview_scale(1, 1) == 3);
static_assert(selected_region_preview_scale(3, 2) == 3);
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
        kTilesetPreviewLeft +
        static_cast<float>(selected_left) * kAtlasTileWidth;

    const float tile_top =
        kTilesetPreviewTop +
        static_cast<float>(selected_top) * kAtlasTileHeight;

    const float tile_right =
        kTilesetPreviewLeft +
        static_cast<float>(selected_right + 1) * kAtlasTileWidth;
    const float tile_bottom =
        kTilesetPreviewTop +
        static_cast<float>(selected_bottom + 1) * kAtlasTileHeight;

    const float inner_left = tile_left + kSelectionOutlineWidth;
    const float inner_top = tile_top + kSelectionOutlineHeight;
    const float inner_right = tile_right - kSelectionOutlineWidth;
    const float inner_bottom = tile_bottom - kSelectionOutlineHeight;

    return {{
        Vertex2D{
            kSelectedRegionPreviewCenterX - preview_half_width,
            kSelectedRegionPreviewCenterY - preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.left,
            selected_region.top
        },
        Vertex2D{
            kSelectedRegionPreviewCenterX + preview_half_width,
            kSelectedRegionPreviewCenterY - preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.right,
            selected_region.top
        },
        Vertex2D{
            kSelectedRegionPreviewCenterX + preview_half_width,
            kSelectedRegionPreviewCenterY + preview_half_height,
            1.0f, 1.0f, 1.0f,
            selected_region.right,
            selected_region.bottom
        },
        Vertex2D{
            kSelectedRegionPreviewCenterX - preview_half_width,
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
    kTileSelectionVertexCount +
    kMapHoverVertexCount +
    kMapTileVertexCount;

constexpr std::size_t kTilesetGridVertexByteOffset =
    sizeof(kTilesetPreviewVertices);

constexpr std::size_t kMapCanvasVertexByteOffset =
    sizeof(kTilesetPreviewVertices) +
    sizeof(kTilesetGridVertices);

constexpr std::size_t kMapGridVertexByteOffset =
    kMapCanvasVertexByteOffset +
    sizeof(Vertex2D) * kMapCanvasBackgroundVertexCount;

constexpr std::size_t kTileSelectionVertexByteOffset =
    sizeof(kTilesetPreviewVertices) +
    sizeof(kTilesetGridVertices) +
    sizeof(kMapCanvasVertices);

constexpr std::size_t kMapHoverVertexByteOffset =
    kTileSelectionVertexByteOffset +
    sizeof(Vertex2D) * kTileSelectionVertexCount;

constexpr std::size_t kMapTileVertexByteOffset =
    kMapHoverVertexByteOffset +
    sizeof(Vertex2D) * kMapHoverVertexCount;

constexpr std::size_t kQuadIndexCount =
    (
        1 +
        kTilesetGridLineCount +
        kMapCanvasQuadCount +
        kMapCanvasCellCount +
        1 +
        4 +
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

constexpr void append_outline_indices(
    QuadIndices& indices,
    std::size_t& next_index,
    const std::uint16_t outer
)
{
    const std::uint16_t inner = outer + 4;

    indices[next_index++] = outer;
    indices[next_index++] = outer + 1;
    indices[next_index++] = inner + 1;
    indices[next_index++] = inner + 1;
    indices[next_index++] = inner;
    indices[next_index++] = outer;

    indices[next_index++] = outer + 1;
    indices[next_index++] = outer + 2;
    indices[next_index++] = inner + 2;
    indices[next_index++] = inner + 2;
    indices[next_index++] = inner + 1;
    indices[next_index++] = outer + 1;

    indices[next_index++] = outer + 2;
    indices[next_index++] = outer + 3;
    indices[next_index++] = inner + 3;
    indices[next_index++] = inner + 3;
    indices[next_index++] = inner + 2;
    indices[next_index++] = outer + 2;

    indices[next_index++] = outer + 3;
    indices[next_index++] = outer;
    indices[next_index++] = inner;
    indices[next_index++] = inner;
    indices[next_index++] = inner + 3;
    indices[next_index++] = outer + 3;
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

    append_quad_indices(
        indices,
        next_index,
        map_canvas_first_vertex
    );

    const std::uint16_t map_tile_first_vertex =
        map_canvas_first_vertex +
        static_cast<std::uint16_t>(
            kMapCanvasVertices.size() +
            kTileSelectionVertexCount +
            kMapHoverVertexCount
        );

    for (std::size_t cell = 0;
         cell < kMapCanvasCellCount;
         ++cell) {
        append_quad_indices(
            indices,
            next_index,
            static_cast<std::uint16_t>(
                map_tile_first_vertex + cell * 4
            )
        );
    }

    for (std::size_t quad = 1;
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

    append_outline_indices(
        indices,
        next_index,
        selection_first_vertex + 4
    );

    const std::uint16_t map_hover_first_vertex =
        selection_first_vertex +
        static_cast<std::uint16_t>(kTileSelectionVertexCount);

    append_outline_indices(
        indices,
        next_index,
        map_hover_first_vertex
    );

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
      map_tiles_(kMapCanvasCellCount),
      selected_tile_left_(kInitialSelectedTileColumn),
      selected_tile_top_(kInitialSelectedTileRow),
      selected_tile_right_(kInitialSelectedTileColumn),
      selected_tile_bottom_(kInitialSelectedTileRow)
{
    swapchain_resources_ = create_swapchain_resources();
    swapchain_window_pixel_width_ = window_.pixel_width();
    swapchain_window_pixel_height_ = window_.pixel_height();

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
    upload_map_hover_vertices();

    quad_vertex_buffer_.upload(
        kEmptyMapTileVertices.data(),
        sizeof(kEmptyMapTileVertices),
        kMapTileVertexByteOffset
    );

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

Application::~Application() noexcept
{
    try {
        vulkan_device_.wait_idle();
    } catch (const std::exception& error) {
        std::cerr << "[Midnight] Vulkan shutdown wait failed: "
                  << error.what()
                  << '\n';
    }
}

int Application::run()
{
    print_startup_info();

    while (running_) {
        poll_events();

        if (!running_) {
            break;
        }

        window_.refresh_size();

        if (window_.pixel_width() !=
                swapchain_window_pixel_width_ ||
            window_.pixel_height() !=
                swapchain_window_pixel_height_) {
            request_swapchain_recreation(true);
        }

        if (swapchain_recreation_pending_) {
            if (SDL_GetTicks() <
                swapchain_recreation_not_before_ticks_) {
                SDL_Delay(16);
                continue;
            }

            if (!recreate_swapchain_resources()) {
                SDL_Delay(16);
                continue;
            }
        }

        const bool swapchain_ready =
            swapchain_resources_.frame_renderer->draw_frame();

        if (swapchain_resources_.frame_renderer
                ->consume_present_completion_observed()) {
            release_retired_swapchain_resources();
        }

        if (!swapchain_ready) {
            request_swapchain_recreation(false);
        }
    }

    return 0;
}

Application::SwapchainResources
Application::create_swapchain_resources(
    const VkSwapchainKHR old_swapchain
)
{
    SwapchainResources resources{};

    resources.swapchain = std::make_unique<VulkanSwapchain>(
        window_,
        vulkan_device_,
        vulkan_surface_,
        old_swapchain
    );
    resources.render_pass = std::make_unique<VulkanRenderPass>(
        vulkan_device_,
        *resources.swapchain
    );
    resources.graphics_pipeline =
        std::make_unique<VulkanGraphicsPipeline>(
            vulkan_device_,
            *resources.swapchain,
            *resources.render_pass
        );
    resources.texture_descriptor =
        std::make_unique<VulkanTextureDescriptor>(
            vulkan_device_,
            resources.graphics_pipeline->descriptor_set_layout(),
            texture_image_,
            texture_sampler_
        );
    resources.frame_renderer =
        std::make_unique<VulkanFrameRenderer>(
            vulkan_device_,
            *resources.swapchain,
            *resources.render_pass,
            *resources.graphics_pipeline,
            *resources.texture_descriptor,
            quad_vertex_buffer_,
            quad_index_buffer_,
            static_cast<std::uint32_t>(kQuadIndices.size()),
            VK_INDEX_TYPE_UINT16
        );

    return resources;
}

bool Application::recreate_swapchain_resources()
{
    window_.refresh_size();

    const SDL_WindowFlags window_flags =
        SDL_GetWindowFlags(window_.sdl_handle());
    const bool window_unavailable =
        (window_flags &
            (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) != 0;

    if (window_unavailable ||
        window_.pixel_width() <= 0 ||
        window_.pixel_height() <= 0) {
        return false;
    }

    const VkSwapchainKHR old_swapchain =
        swapchain_resources_.swapchain->handle();
    SwapchainResources replacement =
        create_swapchain_resources(old_swapchain);

    retired_swapchain_resources_.push_back(
        std::move(swapchain_resources_)
    );
    swapchain_resources_ = std::move(replacement);

    swapchain_window_pixel_width_ = window_.pixel_width();
    swapchain_window_pixel_height_ = window_.pixel_height();
    swapchain_recreation_pending_ = false;

    std::cout << "[Midnight] Vulkan swapchain resources recreated\n";

    return true;
}

void Application::request_swapchain_recreation(
    const bool wait_for_stable_size,
    const bool restart_settle_delay
)
{
    const bool request_was_pending =
        swapchain_recreation_pending_;
    const bool requested_size_changed =
        window_.pixel_width() != pending_swapchain_pixel_width_ ||
        window_.pixel_height() != pending_swapchain_pixel_height_;

    swapchain_recreation_pending_ = true;

    if (wait_for_stable_size &&
        (!request_was_pending ||
         requested_size_changed ||
         restart_settle_delay)) {
        pending_swapchain_pixel_width_ = window_.pixel_width();
        pending_swapchain_pixel_height_ = window_.pixel_height();
        swapchain_recreation_not_before_ticks_ =
            SDL_GetTicks() +
            kSwapchainResizeSettleMilliseconds;
    } else if (!wait_for_stable_size &&
               !request_was_pending) {
        pending_swapchain_pixel_width_ = window_.pixel_width();
        pending_swapchain_pixel_height_ = window_.pixel_height();
        swapchain_recreation_not_before_ticks_ = SDL_GetTicks();
    }
}

void Application::release_retired_swapchain_resources()
{
    if (retired_swapchain_resources_.empty()) {
        return;
    }

    const std::size_t released_count =
        retired_swapchain_resources_.size();
    retired_swapchain_resources_.clear();

    std::cout << "[Midnight] Retired Vulkan swapchain resources released: "
              << released_count
              << '\n';
}

void Application::wait_for_rendering_resources()
{
    swapchain_resources_.frame_renderer->wait_for_in_flight_frames();

    for (SwapchainResources& resources :
         retired_swapchain_resources_) {
        resources.frame_renderer->wait_for_in_flight_frames();
    }

    if (swapchain_resources_.frame_renderer
            ->consume_present_completion_observed()) {
        release_retired_swapchain_resources();
    }
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
    std::cout << "[Midnight] Move the cursor across the map to highlight cells\n";
    std::cout << "[Midnight] Left-click or drag across the map to paint the selected region\n";
    std::cout << "[Midnight] Right-click or drag across the map to erase tiles\n";
    std::cout << "[Midnight] Middle-click a painted map tile to select it\n";
    std::cout << "[Midnight] Press F over the map to flood-fill with a 1x1 selection\n";
    std::cout << "[Midnight] Press Ctrl+Z to undo and Ctrl+Shift+Z to redo map edits\n";
    std::cout << "[Midnight] Press G to toggle the atlas grid\n";
    std::cout << "[Midnight] Press M to toggle the map grid\n";
    std::cout << "[Midnight] Press Escape or close the window to quit\n";
}

void Application::poll_events()
{
    SDL_Event event{};
    bool tile_selection_drag_update_pending = false;
    float pending_tile_selection_drag_x = 0.0f;
    float pending_tile_selection_drag_y = 0.0f;
    bool map_hover_update_pending = false;
    float pending_map_hover_x = 0.0f;
    float pending_map_hover_y = 0.0f;

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

    const auto flush_pending_map_hover = [&]() {
        if (!map_hover_update_pending) {
            return;
        }

        map_hover_update_pending = false;
        update_map_hover(
            pending_map_hover_x,
            pending_map_hover_y
        );
    };

    const auto queue_current_map_hover = [&]() {
        if (SDL_GetMouseFocus() != window_.sdl_handle()) {
            map_hover_update_pending = false;
            clear_map_hover();
            return;
        }

        (void)SDL_GetMouseState(
            &pending_map_hover_x,
            &pending_map_hover_y
        );
        map_hover_update_pending = true;
    };

    const auto finish_pointer_gestures = [&]() {
        if (tile_selection_dragging_) {
            flush_pending_tile_selection_drag();
            tile_selection_dragging_ = false;
            print_tile_selection();
        }

        finish_map_edit();
        map_paint_dragging_ = false;
        map_erase_dragging_ = false;
        tile_selection_drag_update_pending = false;
    };

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
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

                    case SDLK_M:
                        if (!event.key.repeat) {
                            toggle_map_grid();
                        }
                        break;

                    case SDLK_F:
                        if (!event.key.repeat) {
                            flush_pending_map_hover();
                            flood_fill_map();
                        }
                        break;

                    case SDLK_Z:
                        if (!event.key.repeat &&
                            (event.key.mod & SDL_KMOD_CTRL) != 0) {
                            if ((event.key.mod & SDL_KMOD_SHIFT) != 0) {
                                redo_map_edit();
                            } else {
                                undo_map_edit();
                            }
                        }
                        break;

                    default:
                        break;
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                pending_map_hover_x = event.button.x;
                pending_map_hover_y = event.button.y;
                map_hover_update_pending = true;

                if (event.button.button == SDL_BUTTON_LEFT &&
                    !map_erase_dragging_) {
                    flush_pending_tile_selection_drag();
                    map_paint_dragging_ = false;
                    begin_map_edit();
                    map_paint_dragging_ = paint_map_selection(
                        event.button.x,
                        event.button.y
                    );

                    if (!map_paint_dragging_) {
                        finish_map_edit();
                        begin_tile_selection_drag(
                            event.button.x,
                            event.button.y
                        );
                    }
                } else if (event.button.button == SDL_BUTTON_RIGHT &&
                           !map_paint_dragging_ &&
                           !tile_selection_dragging_) {
                    map_erase_dragging_ = false;
                    begin_map_edit();
                    map_erase_dragging_ = erase_map_tile(
                        event.button.x,
                        event.button.y
                    );

                    if (!map_erase_dragging_) {
                        finish_map_edit();
                    }
                } else if (event.button.button == SDL_BUTTON_MIDDLE &&
                           !map_paint_dragging_ &&
                           !map_erase_dragging_ &&
                           !tile_selection_dragging_) {
                    pick_map_tile(
                        event.button.x,
                        event.button.y
                    );
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (map_paint_dragging_) {
                    if ((event.motion.state & SDL_BUTTON_LMASK) != 0) {
                        (void)paint_map_selection(
                            event.motion.x,
                            event.motion.y
                        );
                    } else {
                        finish_map_edit();
                        map_paint_dragging_ = false;
                    }
                }

                if (map_erase_dragging_) {
                    if ((event.motion.state & SDL_BUTTON_RMASK) != 0) {
                        (void)erase_map_tile(
                            event.motion.x,
                            event.motion.y
                        );
                    } else {
                        finish_map_edit();
                        map_erase_dragging_ = false;
                    }
                }

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

                pending_map_hover_x = event.motion.x;
                pending_map_hover_y = event.motion.y;
                map_hover_update_pending = true;
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                pending_map_hover_x = event.button.x;
                pending_map_hover_y = event.button.y;
                map_hover_update_pending = true;

                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (map_paint_dragging_) {
                        (void)paint_map_selection(
                            event.button.x,
                            event.button.y
                        );
                    }

                    if (map_paint_dragging_) {
                        finish_map_edit();
                    }

                    map_paint_dragging_ = false;
                    tile_selection_drag_update_pending = false;
                    end_tile_selection_drag(
                        event.button.x,
                        event.button.y
                    );
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (map_erase_dragging_) {
                        (void)erase_map_tile(
                            event.button.x,
                            event.button.y
                        );
                    }

                    if (map_erase_dragging_) {
                        finish_map_edit();
                    }

                    map_erase_dragging_ = false;
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: {
                finish_pointer_gestures();
                map_hover_update_pending = false;

                const int old_width = window_.width();
                const int old_height = window_.height();
                const int old_pixel_width = window_.pixel_width();
                const int old_pixel_height = window_.pixel_height();

                window_.refresh_size();

                const bool pixel_size_changed =
                    window_.pixel_width() !=
                        swapchain_window_pixel_width_ ||
                    window_.pixel_height() !=
                        swapchain_window_pixel_height_;
                const bool pending_size_changed =
                    swapchain_recreation_pending_ &&
                    (window_.pixel_width() !=
                         pending_swapchain_pixel_width_ ||
                     window_.pixel_height() !=
                         pending_swapchain_pixel_height_);
                const bool window_state_changed =
                    event.type ==
                        SDL_EVENT_WINDOW_MAXIMIZED ||
                    event.type ==
                        SDL_EVENT_WINDOW_RESTORED ||
                    event.type ==
                        SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
                    event.type ==
                        SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
                    event.type ==
                        SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
                    event.type ==
                        SDL_EVENT_WINDOW_LEAVE_FULLSCREEN;

                if (pixel_size_changed ||
                    pending_size_changed ||
                    window_state_changed) {
                    request_swapchain_recreation(
                        true,
                        window_state_changed
                    );
                }

                if (old_width != window_.width() ||
                    old_height != window_.height() ||
                    old_pixel_width != window_.pixel_width() ||
                    old_pixel_height != window_.pixel_height()) {
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
                }

                queue_current_map_hover();
                break;
            }

            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_HIDDEN:
                finish_pointer_gestures();
                window_.refresh_size();
                request_swapchain_recreation(true, true);
                map_hover_update_pending = false;
                clear_map_hover();
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                finish_pointer_gestures();
                map_hover_update_pending = false;
                clear_map_hover();
                break;

            case SDL_EVENT_WINDOW_MOUSE_ENTER:
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                queue_current_map_hover();
                break;

            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                map_hover_update_pending = false;
                clear_map_hover();
                break;

            default:
                break;
        }
    }

    flush_pending_tile_selection_drag();
    flush_pending_map_hover();
}

void Application::begin_map_edit()
{
    if (map_edit_active_) {
        return;
    }

    active_map_edit_before_ = map_tiles_;
    map_edit_active_ = true;
}

void Application::finish_map_edit()
{
    if (!map_edit_active_) {
        return;
    }

    if (active_map_edit_before_ != map_tiles_) {
        map_undo_stack_.push_back(
            std::move(active_map_edit_before_)
        );
        map_redo_stack_.clear();
    } else {
        active_map_edit_before_.clear();
    }

    map_edit_active_ = false;
}

void Application::undo_map_edit()
{
    if (map_edit_active_) {
        return;
    }

    if (map_undo_stack_.empty()) {
        std::cout << "[Midnight] Nothing to undo\n";
        return;
    }

    wait_for_rendering_resources();

    map_redo_stack_.push_back(std::move(map_tiles_));
    map_tiles_ = std::move(map_undo_stack_.back());
    map_undo_stack_.pop_back();

    for (std::uint32_t row = 0;
         row < kMapCanvasRows;
         ++row) {
        for (std::uint32_t column = 0;
             column < kMapCanvasColumns;
             ++column) {
            upload_map_tile_vertices(column, row);
        }
    }

    std::cout << "[Midnight] Undid map edit\n";
}

void Application::redo_map_edit()
{
    if (map_edit_active_) {
        return;
    }

    if (map_redo_stack_.empty()) {
        std::cout << "[Midnight] Nothing to redo\n";
        return;
    }

    wait_for_rendering_resources();

    map_undo_stack_.push_back(std::move(map_tiles_));
    map_tiles_ = std::move(map_redo_stack_.back());
    map_redo_stack_.pop_back();

    for (std::uint32_t row = 0;
         row < kMapCanvasRows;
         ++row) {
        for (std::uint32_t column = 0;
             column < kMapCanvasColumns;
             ++column) {
            upload_map_tile_vertices(column, row);
        }
    }

    std::cout << "[Midnight] Redid map edit\n";
}

void Application::flood_fill_map()
{
    if (!map_hover_visible_ ||
        tile_selection_dragging_ ||
        map_paint_dragging_ ||
        map_erase_dragging_ ||
        map_edit_active_) {
        return;
    }

    if (selected_tile_left_ != selected_tile_right_ ||
        selected_tile_top_ != selected_tile_bottom_) {
        std::cout << "[Midnight] Flood fill requires a 1x1 atlas selection\n";
        return;
    }

    const MapTile replacement{
        .tileset_column = selected_tile_left_,
        .tileset_row = selected_tile_top_,
        .occupied = true
    };
    const std::size_t start_index =
        static_cast<std::size_t>(hovered_map_row_) *
            kMapCanvasColumns +
        hovered_map_column_;
    const MapTile target = map_tiles_.at(start_index);

    if (target == replacement) {
        return;
    }

    const auto matches_target = [&target](
        const MapTile& map_tile
    ) {
        if (!target.occupied) {
            return !map_tile.occupied;
        }

        return map_tile == target;
    };

    std::array<bool, kMapCanvasCellCount> queued{};
    std::vector<std::size_t> pending_cells;
    std::vector<std::size_t> filled_cells;
    pending_cells.reserve(kMapCanvasCellCount);
    filled_cells.reserve(kMapCanvasCellCount);

    const auto queue_cell = [&](
        const std::size_t cell_index
    ) {
        if (queued.at(cell_index) ||
            !matches_target(map_tiles_.at(cell_index))) {
            return;
        }

        queued.at(cell_index) = true;
        pending_cells.push_back(cell_index);
    };

    queue_cell(start_index);

    while (!pending_cells.empty()) {
        const std::size_t cell_index = pending_cells.back();
        pending_cells.pop_back();
        filled_cells.push_back(cell_index);

        const std::uint32_t column =
            static_cast<std::uint32_t>(
                cell_index % kMapCanvasColumns
            );
        const std::uint32_t row =
            static_cast<std::uint32_t>(
                cell_index / kMapCanvasColumns
            );

        if (column > 0) {
            queue_cell(cell_index - 1);
        }

        if (column + 1 < kMapCanvasColumns) {
            queue_cell(cell_index + 1);
        }

        if (row > 0) {
            queue_cell(cell_index - kMapCanvasColumns);
        }

        if (row + 1 < kMapCanvasRows) {
            queue_cell(cell_index + kMapCanvasColumns);
        }
    }

    begin_map_edit();
    wait_for_rendering_resources();

    for (const std::size_t cell_index : filled_cells) {
        const std::uint32_t column =
            static_cast<std::uint32_t>(
                cell_index % kMapCanvasColumns
            );
        const std::uint32_t row =
            static_cast<std::uint32_t>(
                cell_index / kMapCanvasColumns
            );

        map_tiles_.at(cell_index) = replacement;
        upload_map_tile_vertices(column, row);
    }

    finish_map_edit();

    std::cout << "[Midnight] Flood-filled "
              << filled_cells.size()
              << " connected map cells with atlas tile ("
              << replacement.tileset_column
              << ", "
              << replacement.tileset_row
              << ")\n";
}

bool Application::paint_map_selection(
    const float x,
    const float y
)
{
    std::uint32_t column = 0;
    std::uint32_t row = 0;

    if (!window_position_to_map_cell(
            x,
            y,
            column,
            row
        )) {
        return false;
    }

    if (map_paint_dragging_ &&
        column == last_map_paint_column_ &&
        row == last_map_paint_row_) {
        return true;
    }

    last_map_paint_column_ = column;
    last_map_paint_row_ = row;

    const std::uint32_t selected_column_count =
        selected_tile_right_ - selected_tile_left_ + 1;
    const std::uint32_t selected_row_count =
        selected_tile_bottom_ - selected_tile_top_ + 1;
    const std::uint32_t painted_column_count = std::min(
        selected_column_count,
        kMapCanvasColumns - column
    );
    const std::uint32_t painted_row_count = std::min(
        selected_row_count,
        kMapCanvasRows - row
    );

    const auto tile_matches = [](
        const MapTile& map_tile,
        const std::uint32_t tileset_column,
        const std::uint32_t tileset_row
    ) {
        return map_tile.occupied &&
            map_tile.tileset_column == tileset_column &&
            map_tile.tileset_row == tileset_row;
    };

    bool tile_will_change = false;

    for (std::uint32_t row_offset = 0;
         row_offset < painted_row_count && !tile_will_change;
         ++row_offset) {
        for (std::uint32_t column_offset = 0;
             column_offset < painted_column_count;
             ++column_offset) {
            const std::uint32_t map_column =
                column + column_offset;
            const std::uint32_t map_row =
                row + row_offset;
            const std::size_t cell_index =
                static_cast<std::size_t>(map_row) *
                    kMapCanvasColumns +
                map_column;
            const MapTile& map_tile =
                map_tiles_.at(cell_index);

            if (!tile_matches(
                    map_tile,
                    selected_tile_left_ + column_offset,
                    selected_tile_top_ + row_offset
                )) {
                tile_will_change = true;
                break;
            }
        }
    }

    if (!tile_will_change) {
        return true;
    }

    wait_for_rendering_resources();

    for (std::uint32_t row_offset = 0;
         row_offset < painted_row_count;
         ++row_offset) {
        for (std::uint32_t column_offset = 0;
             column_offset < painted_column_count;
             ++column_offset) {
            const std::uint32_t map_column =
                column + column_offset;
            const std::uint32_t map_row =
                row + row_offset;
            const std::uint32_t tileset_column =
                selected_tile_left_ + column_offset;
            const std::uint32_t tileset_row =
                selected_tile_top_ + row_offset;
            const std::size_t cell_index =
                static_cast<std::size_t>(map_row) *
                    kMapCanvasColumns +
                map_column;
            MapTile& map_tile = map_tiles_.at(cell_index);

            if (tile_matches(
                    map_tile,
                    tileset_column,
                    tileset_row
                )) {
                continue;
            }

            map_tile.tileset_column = tileset_column;
            map_tile.tileset_row = tileset_row;
            map_tile.occupied = true;

            upload_map_tile_vertices(map_column, map_row);
        }
    }

    std::cout << "[Midnight] Painted "
              << selected_column_count
              << "x"
              << selected_row_count
              << " atlas region at map cell ("
              << column
              << ", "
              << row
              << ")";

    if (painted_column_count != selected_column_count ||
        painted_row_count != selected_row_count) {
        std::cout << ", clipped to "
                  << painted_column_count
                  << "x"
                  << painted_row_count;
    }

    std::cout << '\n';

    return true;
}

bool Application::erase_map_tile(
    const float x,
    const float y
)
{
    std::uint32_t column = 0;
    std::uint32_t row = 0;

    if (!window_position_to_map_cell(
            x,
            y,
            column,
            row
        )) {
        return false;
    }

    if (map_erase_dragging_ &&
        column == last_map_erase_column_ &&
        row == last_map_erase_row_) {
        return true;
    }

    last_map_erase_column_ = column;
    last_map_erase_row_ = row;

    const std::size_t cell_index =
        static_cast<std::size_t>(row) * kMapCanvasColumns +
        column;
    MapTile& map_tile = map_tiles_.at(cell_index);

    if (!map_tile.occupied) {
        return true;
    }

    wait_for_rendering_resources();

    map_tile = MapTile{};
    upload_map_tile_vertices(column, row);

    std::cout << "[Midnight] Erased map cell ("
              << column
              << ", "
              << row
              << ")\n";

    return true;
}

void Application::pick_map_tile(
    const float x,
    const float y
)
{
    std::uint32_t column = 0;
    std::uint32_t row = 0;

    if (!window_position_to_map_cell(
            x,
            y,
            column,
            row
        )) {
        return;
    }

    const std::size_t cell_index =
        static_cast<std::size_t>(row) * kMapCanvasColumns +
        column;
    const MapTile& map_tile = map_tiles_.at(cell_index);

    if (!map_tile.occupied) {
        return;
    }

    (void)set_tile_selection(
        map_tile.tileset_column,
        map_tile.tileset_row,
        map_tile.tileset_column,
        map_tile.tileset_row
    );

    std::cout << "[Midnight] Picked atlas tile ("
              << map_tile.tileset_column
              << ", "
              << map_tile.tileset_row
              << ") from map cell ("
              << column
              << ", "
              << row
              << ")\n";
}

void Application::upload_map_tile_vertices(
    const std::uint32_t column,
    const std::uint32_t row
)
{
    const std::size_t cell_index =
        static_cast<std::size_t>(row) * kMapCanvasColumns +
        column;
    const MapTile& map_tile = map_tiles_.at(cell_index);
    const MapTileCellVertices vertices =
        map_tile.occupied
            ? make_map_tile_vertices(
                  column,
                  row,
                  map_tile.tileset_column,
                  map_tile.tileset_row
              )
            : kEmptyMapTileCellVertices;

    quad_vertex_buffer_.upload(
        vertices.data(),
        sizeof(vertices),
        kMapTileVertexByteOffset +
            static_cast<VkDeviceSize>(cell_index) *
            sizeof(vertices)
    );
}

void Application::update_map_hover(
    const float x,
    const float y
)
{
    std::uint32_t column = 0;
    std::uint32_t row = 0;

    if (!window_position_to_map_cell(
            x,
            y,
            column,
            row
        )) {
        clear_map_hover();
        return;
    }

    if (map_hover_visible_ &&
        column == hovered_map_column_ &&
        row == hovered_map_row_) {
        return;
    }

    wait_for_rendering_resources();

    hovered_map_column_ = column;
    hovered_map_row_ = row;
    map_hover_visible_ = true;

    upload_map_hover_vertices();
}

void Application::clear_map_hover()
{
    if (!map_hover_visible_) {
        return;
    }

    wait_for_rendering_resources();

    map_hover_visible_ = false;
    upload_map_hover_vertices();
}

bool Application::window_position_to_map_cell(
    const float x,
    const float y,
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

    if (normalized_x < kMapCanvasLeft ||
        normalized_x >= kMapCanvasRight ||
        normalized_y < kMapCanvasTop ||
        normalized_y >= kMapCanvasBottom) {
        return false;
    }

    const float map_x =
        (normalized_x - kMapCanvasLeft) /
        (kMapCanvasRight - kMapCanvasLeft);
    const float map_y =
        (normalized_y - kMapCanvasTop) /
        (kMapCanvasBottom - kMapCanvasTop);

    column = std::min(
        static_cast<std::uint32_t>(
            map_x * static_cast<float>(kMapCanvasColumns)
        ),
        kMapCanvasColumns - 1
    );
    row = std::min(
        static_cast<std::uint32_t>(
            map_y * static_cast<float>(kMapCanvasRows)
        ),
        kMapCanvasRows - 1
    );

    return true;
}

void Application::upload_map_hover_vertices()
{
    const MapHoverVertices vertices =
        map_hover_visible_
            ? make_map_hover_vertices(
                  hovered_map_column_,
                  hovered_map_row_
              )
            : kHiddenMapHoverVertices;

    quad_vertex_buffer_.upload(
        vertices.data(),
        sizeof(vertices),
        kMapHoverVertexByteOffset
    );
}

void Application::toggle_tileset_grid()
{
    wait_for_rendering_resources();

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

void Application::toggle_map_grid()
{
    wait_for_rendering_resources();

    map_grid_visible_ = !map_grid_visible_;
    upload_map_grid_vertices();

    std::cout << "[Midnight] Map grid "
              << (map_grid_visible_ ? "shown" : "hidden")
              << '\n';
}

void Application::upload_map_grid_vertices()
{
    const Vertex2D* vertices =
        map_grid_visible_
            ? kMapCanvasVertices.data() +
                  kMapCanvasBackgroundVertexCount
            : kHiddenMapGridVertices.data();

    quad_vertex_buffer_.upload(
        vertices,
        sizeof(kHiddenMapGridVertices),
        kMapGridVertexByteOffset
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
        normalized_x >= kTilesetPreviewLeft &&
        normalized_x < kTilesetPreviewRight &&
        normalized_y >= kTilesetPreviewTop &&
        normalized_y < kTilesetPreviewBottom;

    if (!clamp_to_atlas && !position_is_in_atlas) {
        return false;
    }

    const float atlas_x = std::clamp(
        (normalized_x - kTilesetPreviewLeft) /
            (kTilesetPreviewRight - kTilesetPreviewLeft),
        0.0f,
        1.0f
    );

    const float atlas_y = std::clamp(
        (normalized_y - kTilesetPreviewTop) /
            (kTilesetPreviewBottom - kTilesetPreviewTop),
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

    wait_for_rendering_resources();

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
