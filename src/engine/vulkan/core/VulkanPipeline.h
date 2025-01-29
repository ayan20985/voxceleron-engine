#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "VulkanDevice.h"
#include "VulkanDescriptorSet.h"

namespace voxceleron {

class VulkanPipeline {
public:
    VulkanPipeline(VulkanDevice* device);
    ~VulkanPipeline();

    VkPipeline getPipeline() const { return pipeline; }
    VkPipelineLayout getLayout() const { return layout; }

    void addShaderStage(VkShaderStageFlagBits stage, const std::string& filename);
    void addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);
    void setDescriptorSetLayout(VkDescriptorSetLayout layout);
    void create();

private:
    VulkanDevice* device;
    VkPipeline pipeline;
    VkPipelineLayout layout;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkPushConstantRange> pushConstantRanges;
    VkDescriptorSetLayout descriptorSetLayout;

    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createPipelineLayout();
    void createComputePipeline();
};

} // namespace voxceleron 