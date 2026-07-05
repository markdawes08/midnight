#include "midnight/renderer/vulkan/VulkanDevice.hpp"

#include "midnight/renderer/vulkan/VulkanInstance.hpp"
#include "midnight/renderer/vulkan/VulkanSurface.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace midnight {
namespace {

const std::vector<const char*> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct QueueFamilyIndices final {
    std::optional<std::uint32_t> graphics_family;
    std::optional<std::uint32_t> present_family;

    [[nodiscard]] bool complete() const noexcept
    {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapchainSupportSummary final {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::uint32_t format_count = 0;
    std::uint32_t present_mode_count = 0;

    [[nodiscard]] bool adequate() const noexcept
    {
        return format_count > 0 && present_mode_count > 0;
    }
};

QueueFamilyIndices find_queue_families(
    const VkPhysicalDevice physical_device,
    const VkSurfaceKHR surface
)
{
    QueueFamilyIndices indices{};

    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        nullptr
    );

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_families.data()
    );

    for (std::uint32_t index = 0; index < queue_family_count; ++index) {
        const VkQueueFamilyProperties& family = queue_families[index];

        if (family.queueCount > 0 &&
            (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphics_family = index;
        }

        VkBool32 present_supported = VK_FALSE;

        throw_if_vk_failed(
            vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device,
                index,
                surface,
                &present_supported
            ),
            "vkGetPhysicalDeviceSurfaceSupportKHR"
        );

        if (family.queueCount > 0 && present_supported == VK_TRUE) {
            indices.present_family = index;
        }

        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

bool has_required_device_extensions(const VkPhysicalDevice physical_device)
{
    std::uint32_t extension_count = 0;

    throw_if_vk_failed(
        vkEnumerateDeviceExtensionProperties(
            physical_device,
            nullptr,
            &extension_count,
            nullptr
        ),
        "vkEnumerateDeviceExtensionProperties"
    );

    std::vector<VkExtensionProperties> available_extensions(extension_count);

    throw_if_vk_failed(
        vkEnumerateDeviceExtensionProperties(
            physical_device,
            nullptr,
            &extension_count,
            available_extensions.data()
        ),
        "vkEnumerateDeviceExtensionProperties"
    );

    std::set<std::string> missing_extensions(
        kRequiredDeviceExtensions.begin(),
        kRequiredDeviceExtensions.end()
    );

    for (const VkExtensionProperties& extension : available_extensions) {
        missing_extensions.erase(extension.extensionName);
    }

    return missing_extensions.empty();
}

SwapchainSupportSummary query_swapchain_support(
    const VkPhysicalDevice physical_device,
    const VkSurfaceKHR surface
)
{
    SwapchainSupportSummary summary{};

    throw_if_vk_failed(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physical_device,
            surface,
            &summary.capabilities
        ),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
    );

    throw_if_vk_failed(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &summary.format_count,
            nullptr
        ),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );

    throw_if_vk_failed(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &summary.present_mode_count,
            nullptr
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );

    return summary;
}

bool physical_device_suitable(
    const VkPhysicalDevice physical_device,
    const VkSurfaceKHR surface
)
{
    const QueueFamilyIndices indices =
        find_queue_families(physical_device, surface);

    if (!indices.complete()) {
        return false;
    }

    if (!has_required_device_extensions(physical_device)) {
        return false;
    }

    const SwapchainSupportSummary swapchain_support =
        query_swapchain_support(physical_device, surface);

    return swapchain_support.adequate();
}

int score_physical_device(
    const VkPhysicalDevice physical_device,
    const VkSurfaceKHR surface
)
{
    if (!physical_device_suitable(physical_device, surface)) {
        return -1;
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    int score = 0;

    switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 10000;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 5000;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 2500;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 100;
            break;

        default:
            break;
    }

    score += static_cast<int>(properties.limits.maxImageDimension2D);

    return score;
}

}

VulkanDevice::VulkanDevice(
    const VulkanInstance& instance,
    const VulkanSurface& surface
)
    : instance_(instance),
      surface_(surface)
{
    pick_physical_device();
    create_logical_device();
}

VulkanDevice::~VulkanDevice()
{
    if (device_ != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
}

VkPhysicalDevice VulkanDevice::physical_device() const noexcept
{
    return physical_device_;
}

VkDevice VulkanDevice::handle() const noexcept
{
    return device_;
}

VkQueue VulkanDevice::graphics_queue() const noexcept
{
    return graphics_queue_;
}

VkQueue VulkanDevice::present_queue() const noexcept
{
    return present_queue_;
}

std::uint32_t VulkanDevice::graphics_queue_family_index() const noexcept
{
    return graphics_queue_family_index_;
}

std::uint32_t VulkanDevice::present_queue_family_index() const noexcept
{
    return present_queue_family_index_;
}

void VulkanDevice::pick_physical_device()
{
    std::uint32_t physical_device_count = 0;

    throw_if_vk_failed(
        vkEnumeratePhysicalDevices(
            instance_.handle(),
            &physical_device_count,
            nullptr
        ),
        "vkEnumeratePhysicalDevices"
    );

    if (physical_device_count == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);

    throw_if_vk_failed(
        vkEnumeratePhysicalDevices(
            instance_.handle(),
            &physical_device_count,
            physical_devices.data()
        ),
        "vkEnumeratePhysicalDevices"
    );

    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    int best_score = -1;

    std::cout << "[Midnight] Vulkan physical devices found:\n";

    for (const VkPhysicalDevice candidate : physical_devices) {
        VkPhysicalDeviceProperties candidate_properties{};
        vkGetPhysicalDeviceProperties(candidate, &candidate_properties);

        const int candidate_score =
            score_physical_device(candidate, surface_.handle());

        std::cout << "  - " << candidate_properties.deviceName
                  << " [" << vulkan_device_type_to_string(candidate_properties.deviceType)
                  << "] score=" << candidate_score
                  << '\n';

        if (candidate_score > best_score) {
            best_score = candidate_score;
            best_device = candidate;
        }
    }

    if (best_device == VK_NULL_HANDLE || best_score < 0) {
        throw std::runtime_error(
            "No suitable Vulkan physical device found. Need graphics, present, and swapchain support."
        );
    }

    physical_device_ = best_device;
    vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties_);

    const QueueFamilyIndices indices =
        find_queue_families(physical_device_, surface_.handle());

    if (!indices.complete()) {
        throw std::runtime_error("Selected Vulkan device is missing required queue families");
    }

    graphics_queue_family_index_ = indices.graphics_family.value();
    present_queue_family_index_ = indices.present_family.value();

    const SwapchainSupportSummary swapchain_support =
        query_swapchain_support(physical_device_, surface_.handle());

    std::cout << "[Midnight] Selected Vulkan device: "
              << physical_device_properties_.deviceName
              << '\n';

    std::cout << "[Midnight] Device type: "
              << vulkan_device_type_to_string(physical_device_properties_.deviceType)
              << '\n';

    std::cout << "[Midnight] Device API: "
              << vulkan_api_version_to_string(physical_device_properties_.apiVersion)
              << '\n';

    std::cout << "[Midnight] Graphics queue family: "
              << graphics_queue_family_index_
              << '\n';

    std::cout << "[Midnight] Present queue family: "
              << present_queue_family_index_
              << '\n';

    std::cout << "[Midnight] Swapchain support: "
              << swapchain_support.format_count
              << " formats, "
              << swapchain_support.present_mode_count
              << " present modes"
              << '\n';
}

void VulkanDevice::create_logical_device()
{
    const std::set<std::uint32_t> unique_queue_families = {
        graphics_queue_family_index_,
        present_queue_family_index_
    };

    const float queue_priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(unique_queue_families.size());

    for (const std::uint32_t queue_family_index : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;

        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures supported_features{};
    vkGetPhysicalDeviceFeatures(physical_device_, &supported_features);

    VkPhysicalDeviceFeatures enabled_features{};
    enabled_features.samplerAnisotropy = supported_features.samplerAnisotropy;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount =
        static_cast<std::uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount =
        static_cast<std::uint32_t>(kRequiredDeviceExtensions.size());
    create_info.ppEnabledExtensionNames = kRequiredDeviceExtensions.data();
    create_info.pEnabledFeatures = &enabled_features;

    throw_if_vk_failed(
        vkCreateDevice(physical_device_, &create_info, nullptr, &device_),
        "vkCreateDevice"
    );

    vkGetDeviceQueue(
        device_,
        graphics_queue_family_index_,
        0,
        &graphics_queue_
    );

    vkGetDeviceQueue(
        device_,
        present_queue_family_index_,
        0,
        &present_queue_
    );

    std::cout << "[Midnight] Vulkan logical device created\n";

    if (enabled_features.samplerAnisotropy == VK_TRUE) {
        std::cout << "[Midnight] Sampler anisotropy enabled\n";
    } else {
        std::cout << "[Midnight] Sampler anisotropy unavailable\n";
    }
}

}
