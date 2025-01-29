#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"

namespace voxceleron {

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet(VulkanDevice* device);
    ~VulkanDescriptorSet();

    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    VkDescriptorSetLayout getLayout() const { return layout; }

    void addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags);
    void create();
    void update(const std::vector<VkDescriptorBufferInfo>& bufferInfos);

private:
    VulkanDevice* device;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout layout;
    VkDescriptorPool pool;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void createLayout();
    void createPool();
    void allocateDescriptorSet();
};

} // namespace voxceleron 