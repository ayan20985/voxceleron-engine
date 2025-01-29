#pragma once

#include <vulkan/vulkan.h>
#include "VulkanDevice.h"

namespace voxceleron {

class VulkanBuffer {
public:
    VulkanBuffer(VulkanDevice* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~VulkanBuffer();

    VkBuffer getBuffer() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }
    VkDeviceSize getSize() const { return size; }

    void* map();
    void unmap();
    void flush();
    void invalidate();

private:
    VulkanDevice* device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void* mappedData;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
};

} // namespace voxceleron 