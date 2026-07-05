#include "midnight/renderer/vulkan/VulkanSwapchain.hpp"

#include "midnight/platform/Window.hpp"
#include "midnight/renderer/vulkan/VulkanDevice.hpp"
#include "midnight/renderer/vulkan/VulkanSurface.hpp"
#include "midnight/renderer/vulkan/VulkanUtils.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace midnight {
namespace {

struct SwapchainSupportDetails final {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

SwapchainSupportDetails query_swapchain_support(
    const VkPhysicalDevice physical_device,
    const VkSurfaceKHR surface
)
{
    SwapchainSupportDetails details{};

    throw_if_vk_failed(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physical_device,
            surface,
            &details.capabilities
        ),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
    );

    std::uint32_t format_count = 0;

    throw_if_vk_failed(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &format_count,
            nullptr
        ),
        "vkGetPhysicalDeviceSurfaceFormatsKHR"
    );

    details.formats.resize(format_count);

    if (format_count > 0) {
        throw_if_vk_failed(
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physical_device,
                surface,
                &format_count,
                details.formats.data()
            ),
            "vkGetPhysicalDeviceSurfaceFormatsKHR"
        );
    }

    std::uint32_t present_mode_count = 0;

    throw_if_vk_failed(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &present_mode_count,
            nullptr
        ),
        "vkGetPhysicalDeviceSurfacePresentModesKHR"
    );

    details.present_modes.resize(present_mode_count);

    if (present_mode_count > 0) {
        throw_if_vk_failed(
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physical_device,
                surface,
                &present_mode_count,
                details.present_modes.data()
            ),
            "vkGetPhysicalDeviceSurfacePresentModesKHR"
        );
    }

    return details;
}

VkSurfaceFormatKHR choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats
)
{
    if (available_formats.empty()) {
        throw std::runtime_error("Swapchain has no available surface formats");
    }

    for (const VkSurfaceFormatKHR& format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    for (const VkSurfaceFormatKHR& format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return available_formats.front();
}

VkPresentModeKHR choose_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes
)
{
    for (const VkPresentModeKHR present_mode : available_present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_extent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    const Window& window
)
{
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    if (window.pixel_width() <= 0 || window.pixel_height() <= 0) {
        throw std::runtime_error("Cannot create swapchain for a zero-sized window");
    }

    VkExtent2D actual_extent{
        .width = static_cast<std::uint32_t>(window.pixel_width()),
        .height = static_cast<std::uint32_t>(window.pixel_height())
    };

    actual_extent.width = std::clamp(
        actual_extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
    );

    actual_extent.height = std::clamp(
        actual_extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
    );

    return actual_extent;
}

std::uint32_t choose_image_count(const VkSurfaceCapabilitiesKHR& capabilities)
{
    std::uint32_t image_count = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    return image_count;
}

VkCompositeAlphaFlagBitsKHR choose_composite_alpha(
    const VkSurfaceCapabilitiesKHR& capabilities
)
{
    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }

    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }

    throw std::runtime_error("Surface does not expose a supported composite alpha mode");
}

}

VulkanSwapchain::VulkanSwapchain(
    const Window& window,
    const VulkanDevice& device,
    const VulkanSurface& surface
)
    : device_(device),
      surface_(surface)
{
    create_swapchain(window);
    create_image_views();
}

VulkanSwapchain::~VulkanSwapchain()
{
    destroy_image_views();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_.handle(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

VkSwapchainKHR VulkanSwapchain::handle() const noexcept
{
    return swapchain_;
}

VkFormat VulkanSwapchain::image_format() const noexcept
{
    return image_format_;
}

VkExtent2D VulkanSwapchain::extent() const noexcept
{
    return extent_;
}

const std::vector<VkImage>& VulkanSwapchain::images() const noexcept
{
    return images_;
}

const std::vector<VkImageView>& VulkanSwapchain::image_views() const noexcept
{
    return image_views_;
}

std::uint32_t VulkanSwapchain::image_count() const noexcept
{
    return static_cast<std::uint32_t>(images_.size());
}

void VulkanSwapchain::create_swapchain(const Window& window)
{
    const SwapchainSupportDetails support = query_swapchain_support(
        device_.physical_device(),
        surface_.handle()
    );

    const VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats);
    const VkPresentModeKHR present_mode = choose_present_mode(support.present_modes);
    const VkExtent2D swap_extent = choose_extent(support.capabilities, window);
    const std::uint32_t image_count = choose_image_count(support.capabilities);

    const std::uint32_t queue_family_indices[] = {
        device_.graphics_queue_family_index(),
        device_.present_queue_family_index()
    };

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_.handle();
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swap_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = choose_composite_alpha(support.capabilities);
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (device_.graphics_queue_family_index() != device_.present_queue_family_index()) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    throw_if_vk_failed(
        vkCreateSwapchainKHR(device_.handle(), &create_info, nullptr, &swapchain_),
        "vkCreateSwapchainKHR"
    );

    image_format_ = surface_format.format;
    extent_ = swap_extent;

    std::uint32_t actual_image_count = 0;

    throw_if_vk_failed(
        vkGetSwapchainImagesKHR(
            device_.handle(),
            swapchain_,
            &actual_image_count,
            nullptr
        ),
        "vkGetSwapchainImagesKHR"
    );

    images_.resize(actual_image_count);

    throw_if_vk_failed(
        vkGetSwapchainImagesKHR(
            device_.handle(),
            swapchain_,
            &actual_image_count,
            images_.data()
        ),
        "vkGetSwapchainImagesKHR"
    );

    std::cout << "[Midnight] Vulkan swapchain created\n";
    std::cout << "[Midnight] Swapchain extent: "
              << extent_.width
              << "x"
              << extent_.height
              << '\n';
    std::cout << "[Midnight] Swapchain format: "
              << vulkan_format_to_string(image_format_)
              << '\n';
    std::cout << "[Midnight] Swapchain present mode: "
              << vulkan_present_mode_to_string(present_mode)
              << '\n';
    std::cout << "[Midnight] Swapchain images: "
              << images_.size()
              << '\n';
}

void VulkanSwapchain::create_image_views()
{
    image_views_.resize(images_.size());

    for (std::size_t index = 0; index < images_.size(); ++index) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = images_[index];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = image_format_;

        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        throw_if_vk_failed(
            vkCreateImageView(
                device_.handle(),
                &create_info,
                nullptr,
                &image_views_[index]
            ),
            "vkCreateImageView"
        );
    }

    std::cout << "[Midnight] Swapchain image views created: "
              << image_views_.size()
              << '\n';
}

void VulkanSwapchain::destroy_image_views() noexcept
{
    for (VkImageView image_view : image_views_) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_.handle(), image_view, nullptr);
        }
    }

    image_views_.clear();
}

}
