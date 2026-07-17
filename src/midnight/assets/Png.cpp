#include "midnight/assets/Png.hpp"

#include "midnight/core/File.hpp"

#include <png.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace midnight {
namespace {

class PngImage final {
public:
    PngImage()
    {
        image_.version = PNG_IMAGE_VERSION;
    }

    ~PngImage()
    {
        png_image_free(&image_);
    }

    PngImage(const PngImage&) = delete;
    PngImage& operator=(const PngImage&) = delete;

    PngImage(PngImage&&) = delete;
    PngImage& operator=(PngImage&&) = delete;

    [[nodiscard]] png_image& get() noexcept
    {
        return image_;
    }

private:
    png_image image_{};
};

std::string normalized_path(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

std::runtime_error png_error(
    const std::filesystem::path& path,
    const png_image& image
)
{
    return std::runtime_error(
        "Failed to decode PNG '" +
        normalized_path(path) +
        "': " +
        image.message
    );
}

std::size_t rgba_byte_size(
    std::uint32_t width,
    std::uint32_t height,
    const std::filesystem::path& path
)
{
    if (width == 0 || height == 0) {
        throw std::runtime_error(
            "PNG has zero dimensions: " + normalized_path(path)
        );
    }

    constexpr std::size_t kBytesPerPixel = RgbaImage::bytes_per_pixel;
    constexpr std::size_t kMaximumSize =
        std::numeric_limits<std::size_t>::max();

    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);

    if (width_size > kMaximumSize / height_size ||
        width_size * height_size > kMaximumSize / kBytesPerPixel) {
        throw std::runtime_error(
            "Decoded PNG is too large: " + normalized_path(path)
        );
    }

    return width_size * height_size * kBytesPerPixel;
}

}

RgbaImage load_png_rgba8(const std::filesystem::path& path)
{
    const std::vector<std::byte> encoded_png = read_binary_file(path);

    if (encoded_png.empty()) {
        throw std::runtime_error(
            "PNG file is empty: " + normalized_path(path)
        );
    }

    PngImage png;
    png_image& image = png.get();

    if (!png_image_begin_read_from_memory(
            &image,
            encoded_png.data(),
            encoded_png.size()
        )) {
        throw png_error(path, image);
    }

    image.format = PNG_FORMAT_RGBA;

    RgbaImage decoded{
        .width = image.width,
        .height = image.height,
        .pixels = std::vector<std::uint8_t>(
            rgba_byte_size(image.width, image.height, path)
        )
    };

    if (!png_image_finish_read(
            &image,
            nullptr,
            decoded.pixels.data(),
            0,
            nullptr
        )) {
        throw png_error(path, image);
    }

    return decoded;
}

}
