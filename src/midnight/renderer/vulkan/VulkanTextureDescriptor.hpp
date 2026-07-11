#pragma once

#include <vulkan/vulkan.h>

namespace midnight {

class VulkanDevice;
class VulkanImage;
class VulkanSampler;

class VulkanTextureDescriptor final {
public:
    VulkanTextureDescriptor(
        const VulkanDevice& device,
        VkDescriptorSetLayout descriptor_set_layout,
        const VulkanImage& image,
        const VulkanSampler& sampler
    );

    ~VulkanTextureDescriptor();

    VulkanTextureDescriptor(const VulkanTextureDescriptor&) = delete;
    VulkanTextureDescriptor& operator=(const VulkanTextureDescriptor&) = delete;

    VulkanTextureDescriptor(VulkanTextureDescriptor&&) = delete;
    VulkanTextureDescriptor& operator=(VulkanTextureDescriptor&&) = delete;

    [[nodiscard]] VkDescriptorSet handle() const noexcept;

private:
    void create_descriptor_pool();
    void allocate_descriptor_set(VkDescriptorSetLayout descriptor_set_layout);
    void update_descriptor_set(
        const VulkanImage& image,
        const VulkanSampler& sampler
    );
    void destroy() noexcept;

    const VulkanDevice& device_;

    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
};

}
