#include "midnight/core/File.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace midnight {
namespace {

std::string path_to_string(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

}

std::vector<std::byte> read_binary_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        throw std::runtime_error("Failed to open binary file: " + path_to_string(path));
    }

    const std::ifstream::pos_type end_position = file.tellg();

    if (end_position < 0) {
        throw std::runtime_error("Failed to determine binary file size: " + path_to_string(path));
    }

    const auto size = static_cast<std::size_t>(end_position);
    std::vector<std::byte> data(size);

    file.seekg(0, std::ios::beg);

    if (size > 0) {
        file.read(
            reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(data.size())
        );

        if (!file) {
            throw std::runtime_error("Failed to read binary file: " + path_to_string(path));
        }
    }

    return data;
}

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream file(path);

    if (!file) {
        throw std::runtime_error("Failed to open text file: " + path_to_string(path));
    }

    std::ostringstream stream;
    stream << file.rdbuf();

    if (!file.good() && !file.eof()) {
        throw std::runtime_error("Failed to read text file: " + path_to_string(path));
    }

    return stream.str();
}

}
