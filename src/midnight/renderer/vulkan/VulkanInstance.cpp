#include "midnight/renderer/vulkan/VulkanInstance.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace midnight {
namespace {

constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

std::string vk_result_to_string(const VkResult result)
{
    switch (result) {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        default:
            return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

std::string api_version_to_string(const uint32_t version)
{
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_API_VERSION_MINOR(version)) + "." +
           std::to_string(VK_API_VERSION_PATCH(version));
}

uint32_t choose_instance_api_version()
{
    uint32_t loader_version = VK_API_VERSION_1_0;

    const VkResult result = vkEnumerateInstanceVersion(&loader_version);

    if (result != VK_SUCCESS) {
        return VK_API_VERSION_1_0;
    }

    if (loader_version >= VK_API_VERSION_1_2) {
        return VK_API_VERSION_1_2;
    }

    return loader_version;
}

bool layer_available(const char* layer_name)
{
    uint32_t layer_count = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkEnumerateInstanceLayerProperties failed: " + vk_result_to_string(result)
        );
    }

    std::vector<VkLayerProperties> layers(layer_count);

    result = vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkEnumerateInstanceLayerProperties failed: " + vk_result_to_string(result)
        );
    }

    return std::any_of(
        layers.begin(),
        layers.end(),
        [layer_name](const VkLayerProperties& layer) {
            return std::strcmp(layer.layerName, layer_name) == 0;
        }
    );
}

bool extension_available(const char* extension_name)
{
    uint32_t extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        nullptr
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkEnumerateInstanceExtensionProperties failed: " + vk_result_to_string(result)
        );
    }

    std::vector<VkExtensionProperties> extensions(extension_count);

    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &extension_count,
        extensions.data()
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkEnumerateInstanceExtensionProperties failed: " + vk_result_to_string(result)
        );
    }

    return std::any_of(
        extensions.begin(),
        extensions.end(),
        [extension_name](const VkExtensionProperties& extension) {
            return std::strcmp(extension.extensionName, extension_name) == 0;
        }
    );
}

void push_unique_extension(std::vector<const char*>& extensions, const char* extension)
{
    const auto found = std::find_if(
        extensions.begin(),
        extensions.end(),
        [extension](const char* existing) {
            return std::strcmp(existing, extension) == 0;
        }
    );

    if (found == extensions.end()) {
        extensions.push_back(extension);
    }
}

std::vector<const char*> required_instance_extensions(const bool enable_debug_utils)
{
    Uint32 sdl_extension_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);

    if (sdl_extensions == nullptr) {
        throw std::runtime_error(
            std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError()
        );
    }

    std::vector<const char*> extensions;
    extensions.reserve(static_cast<std::size_t>(sdl_extension_count) + 1);

    for (Uint32 index = 0; index < sdl_extension_count; ++index) {
        push_unique_extension(extensions, sdl_extensions[index]);
    }

    if (enable_debug_utils) {
        push_unique_extension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    for (const char* extension : extensions) {
        if (!extension_available(extension)) {
            throw std::runtime_error(
                std::string("Required Vulkan instance extension is not available: ") +
                extension
            );
        }
    }

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
)
{
    (void)message_severity;
    (void)message_type;
    (void)user_data;

    std::cerr << "[Vulkan validation] " << callback_data->pMessage << '\n';

    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT make_debug_messenger_create_info()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;

    return create_info;
}

}

VulkanInstance::VulkanInstance()
{
    create_instance();
    setup_debug_messenger();
}

VulkanInstance::~VulkanInstance()
{
    destroy_debug_messenger();

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

VkInstance VulkanInstance::handle() const noexcept
{
    return instance_;
}

bool VulkanInstance::validation_enabled() const noexcept
{
    return validation_enabled_;
}

void VulkanInstance::create_instance()
{
#if MIDNIGHT_DEBUG
    validation_enabled_ = layer_available(kValidationLayerName);

    if (!validation_enabled_) {
        std::cerr << "[Midnight] Vulkan validation layer not found. Continuing without validation.\n";
    }
#else
    validation_enabled_ = false;
#endif

    debug_utils_enabled_ =
        validation_enabled_ &&
        extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (validation_enabled_ && !debug_utils_enabled_) {
        std::cerr << "[Midnight] VK_EXT_debug_utils not found. Validation messages will not be captured.\n";
    }

    const std::vector<const char*> extensions =
        required_instance_extensions(debug_utils_enabled_);

    std::vector<const char*> layers;

    if (validation_enabled_) {
        layers.push_back(kValidationLayerName);
    }

    const uint32_t api_version = choose_instance_api_version();

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Midnight";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Midnight Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = api_version;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    if (validation_enabled_ && debug_utils_enabled_) {
        debug_create_info = make_debug_messenger_create_info();
        create_info.pNext = &debug_create_info;
    }

    const VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("vkCreateInstance failed: " + vk_result_to_string(result));
    }

    std::cout << "[Midnight] Vulkan instance created. API "
              << api_version_to_string(api_version)
              << '\n';

    std::cout << "[Midnight] Vulkan instance extensions:\n";

    for (const char* extension : extensions) {
        std::cout << "  - " << extension << '\n';
    }

    if (validation_enabled_) {
        std::cout << "[Midnight] Vulkan validation enabled\n";
    } else {
        std::cout << "[Midnight] Vulkan validation disabled\n";
    }
}

void VulkanInstance::setup_debug_messenger()
{
    if (!validation_enabled_ || !debug_utils_enabled_) {
        return;
    }

    const auto create_debug_utils_messenger =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT")
        );

    if (create_debug_utils_messenger == nullptr) {
        std::cerr << "[Midnight] vkCreateDebugUtilsMessengerEXT was not available\n";
        return;
    }

    const VkDebugUtilsMessengerCreateInfoEXT create_info =
        make_debug_messenger_create_info();

    const VkResult result = create_debug_utils_messenger(
        instance_,
        &create_info,
        nullptr,
        &debug_messenger_
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkCreateDebugUtilsMessengerEXT failed: " + vk_result_to_string(result)
        );
    }

    std::cout << "[Midnight] Vulkan debug messenger created\n";
}

void VulkanInstance::destroy_debug_messenger() noexcept
{
    if (debug_messenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) {
        return;
    }

    const auto destroy_debug_utils_messenger =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT")
        );

    if (destroy_debug_utils_messenger != nullptr) {
        destroy_debug_utils_messenger(instance_, debug_messenger_, nullptr);
    }

    debug_messenger_ = VK_NULL_HANDLE;
}

}
