#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanInstance final {
public:
    VulkanInstance();
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    VulkanInstance(VulkanInstance&&) = delete;
    VulkanInstance& operator=(VulkanInstance&&) = delete;

    [[nodiscard]] VkInstance handle() const noexcept;
    [[nodiscard]] bool validation_enabled() const noexcept;

private:
    void create_instance();
    void setup_debug_messenger();
    void destroy_debug_messenger() noexcept;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

    bool validation_enabled_ = false;
    bool debug_utils_enabled_ = false;
};

}
