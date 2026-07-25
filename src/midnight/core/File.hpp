#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace midnight {

[[nodiscard]] std::vector<std::byte> read_binary_file(
    const std::filesystem::path& path
);

[[nodiscard]] std::string read_text_file(
    const std::filesystem::path& path
);

void write_text_file_atomically(
    const std::filesystem::path& path,
    const std::string& contents
);

}
