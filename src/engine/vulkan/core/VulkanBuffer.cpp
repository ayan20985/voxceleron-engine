#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include <stdexcept>

namespace voxceleron {

VulkanBuffer::VulkanBuffer(VulkanDevice* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : device(device)
    , buffer(VK_NULL_HANDLE)
    , memory(VK_NULL_HANDLE)
    , size(size)
    , mappedData(nullptr) {
    createBuffer(size, usage, properties);
}

VulkanBuffer::~VulkanBuffer() {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device->getDevice(), buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device->getDevice(), memory, nullptr);
    }
}

void VulkanBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device->getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device->getDevice(), buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device->getDevice(), buffer, memory, 0);
}

void* VulkanBuffer::map() {
    if (!mappedData) {
        vkMapMemory(device->getDevice(), memory, 0, size, 0, &mappedData);
    }
    return mappedData;
}

void VulkanBuffer::unmap() {
    if (mappedData) {
        vkUnmapMemory(device->getDevice(), memory);
        mappedData = nullptr;
    }
}

void VulkanBuffer::flush() {
    VkMappedMemoryRange mappedRange{};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = 0;
    mappedRange.size = size;
    vkFlushMappedMemoryRanges(device->getDevice(), 1, &mappedRange);
}

void VulkanBuffer::invalidate() {
    VkMappedMemoryRange mappedRange{};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = 0;
    mappedRange.size = size;
    vkInvalidateMappedMemoryRanges(device->getDevice(), 1, &mappedRange);
}

} // namespace voxceleron 
