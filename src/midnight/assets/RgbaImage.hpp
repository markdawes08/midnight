#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace midnight {

struct RgbaImage final {
    static constexpr std::size_t bytes_per_pixel = 4;

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] std::size_t byte_size() const noexcept
    {
        return pixels.size();
    }
};

}
