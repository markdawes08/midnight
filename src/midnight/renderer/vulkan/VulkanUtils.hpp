#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace midnight {

[[nodiscard]] std::string vk_result_to_string(VkResult result);

[[nodiscard]] std::string vulkan_api_version_to_string(std::uint32_t version);
[[nodiscard]] std::string vulkan_device_type_to_string(VkPhysicalDeviceType type);
[[nodiscard]] std::string vulkan_format_to_string(VkFormat format);
[[nodiscard]] std::string vulkan_present_mode_to_string(VkPresentModeKHR present_mode);

void throw_if_vk_failed(VkResult result, std::string_view operation);

}
