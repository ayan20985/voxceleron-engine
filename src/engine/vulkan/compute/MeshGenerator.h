#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <string>
#include "../core/VulkanDevice.h"
#include "../core/VulkanBuffer.h"
#include "../core/VulkanDescriptorSet.h"

namespace voxceleron {

class VulkanDevice;
class VulkanBuffer;
class VulkanDescriptorSet;

struct MeshGeneratorCreateInfo {
    uint32_t maxVerticesPerNode;
    uint32_t maxNodesInFlight;
    uint32_t workgroupSizeX;
    uint32_t workgroupSizeY;
    uint32_t workgroupSizeZ;
};

class MeshGenerator {
public:
    MeshGenerator(VulkanDevice* device);
    ~MeshGenerator();

    bool initialize(const MeshGeneratorCreateInfo& createInfo);
    void cleanup();

    bool generateMesh(
        const void* inputData,
        size_t inputSize,
        uint32_t lodLevel,
        float lodTransitionFactor,
        const float* position,
        float size,
        void* outputData,
        size_t* outputSize
    );

private:
    struct BufferSet {
        std::unique_ptr<VulkanBuffer> voxelBuffer;
        std::unique_ptr<VulkanBuffer> vertexBuffer;
        std::unique_ptr<VulkanBuffer> indexBuffer;
        std::unique_ptr<VulkanBuffer> counterBuffer;

        BufferSet() {}
    };

    static constexpr uint32_t MAX_BUFFER_SETS = 16;
    static constexpr uint32_t MAX_VOXELS = 512 * 512 * 512;
    static constexpr uint32_t MAX_VERTICES = MAX_VOXELS * 24;  // 24 vertices per voxel (worst case)
    static constexpr uint32_t MAX_INDICES = MAX_VERTICES * 1.5; // Assuming triangles

    VulkanDevice* device;
    VkPipeline computePipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<std::unique_ptr<BufferSet>> bufferSets;
    std::vector<VkDescriptorSet> descriptorSets;
    size_t currentBufferSet;
    uint32_t workgroupSizeX;
    uint32_t workgroupSizeY;
    uint32_t workgroupSizeZ;

    bool createDescriptorSetLayout();
    bool createComputePipeline();
    bool createBuffers(const MeshGeneratorCreateInfo& createInfo);
    bool createDescriptorPool(uint32_t maxSets);
    bool allocateDescriptorSets();
    bool loadShaderFile(const std::string& filename, std::vector<char>& buffer);
};

} // namespace voxceleron 