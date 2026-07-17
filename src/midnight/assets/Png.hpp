#pragma once

#include "midnight/assets/RgbaImage.hpp"

#include <filesystem>

namespace midnight {

[[nodiscard]] RgbaImage load_png_rgba8(
    const std::filesystem::path& path
);

}
