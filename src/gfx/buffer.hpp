#pragma once
#include <volk.h>
#include <cstdint>

namespace gfx {
struct Buffer { VkBuffer buf{}; VkDeviceMemory mem{}; VkDevice device{}; VkDeviceSize size{}; };
Buffer make_host_buffer(VkPhysicalDevice phys, VkDevice dev, VkDeviceSize size, VkBufferUsageFlags usage);
void destroy_buffer(const Buffer& b);
void upload_bytes(const Buffer& b, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
uint32_t find_mem_type(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props);
}
