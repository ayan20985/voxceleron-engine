#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace voxceleron {

class VulkanDevice {
public:
    VulkanDevice(VkPhysicalDevice physicalDevice, VkDevice device);
    ~VulkanDevice();

    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkCommandBuffer beginSingleTimeCommands();
    bool endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void waitIdle() const;

private:
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;

    void createCommandPool();
};

} // namespace voxceleron 