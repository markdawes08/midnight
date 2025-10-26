#include <volk.h>
#include "gfx/buffer.hpp"
#include <stdexcept>
#include <cstring>

namespace gfx {

uint32_t find_mem_type(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props){
  VkPhysicalDeviceMemoryProperties memProps{}; vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
  for(uint32_t i=0;i<memProps.memoryTypeCount;++i){
    if((typeBits & (1u<<i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
  }
  throw std::runtime_error("No compatible memory type");
}

Buffer make_host_buffer(VkPhysicalDevice phys, VkDevice dev, VkDeviceSize size, VkBufferUsageFlags usage){
  Buffer out{}; out.device = dev; out.size = size;
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = size; bi.usage = usage; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if(vkCreateBuffer(dev, &bi, nullptr, &out.buf) != VK_SUCCESS) throw std::runtime_error("vkCreateBuffer failed");
  VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(dev, out.buf, &req);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = find_mem_type(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if(vkAllocateMemory(dev, &ai, nullptr, &out.mem) != VK_SUCCESS) throw std::runtime_error("vkAllocateMemory failed");
  vkBindBufferMemory(dev, out.buf, out.mem, 0);
  return out;
}

void destroy_buffer(const Buffer& b){
  if(b.buf) vkDestroyBuffer(b.device, b.buf, nullptr);
  if(b.mem) vkFreeMemory(b.device, b.mem, nullptr);
}

void upload_bytes(const Buffer& b, const void* data, VkDeviceSize size, VkDeviceSize offset){
  void* mapped=nullptr; vkMapMemory(b.device, b.mem, offset, size, 0, &mapped);
  std::memcpy(mapped, data, (size_t)size);
  vkUnmapMemory(b.device, b.mem);
}

} // namespace gfx