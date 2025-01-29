#include "MeshGenerator.h"
#include "../core/VulkanDevice.h"
#include "../core/VulkanBuffer.h"
#include "../core/VulkanDescriptorSet.h"
#include "../core/VulkanPipeline.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <array>

namespace voxceleron {

MeshGenerator::MeshGenerator(VulkanDevice* device)
    : device(device)
    , computePipeline(VK_NULL_HANDLE)
    , pipelineLayout(VK_NULL_HANDLE)
    , descriptorSetLayout(VK_NULL_HANDLE)
    , descriptorPool(VK_NULL_HANDLE)
    , descriptorSets()
    , currentBufferSet(0)
    , workgroupSizeX(8)
    , workgroupSizeY(8)
    , workgroupSizeZ(8) {
}

MeshGenerator::~MeshGenerator() {
    cleanup();
}

bool MeshGenerator::initialize(const MeshGeneratorCreateInfo& createInfo) {
    std::cout << "MeshGenerator: Starting initialization..." << std::endl;
    
    workgroupSizeX = createInfo.workgroupSizeX;
    workgroupSizeY = createInfo.workgroupSizeY;
    workgroupSizeZ = createInfo.workgroupSizeZ;

    std::cout << "MeshGenerator: Using workgroup sizes: " << workgroupSizeX << "x" << workgroupSizeY << "x" << workgroupSizeZ << std::endl;

    // 1. Create descriptor set layout first
    std::cout << "MeshGenerator: Creating descriptor set layout..." << std::endl;
    if (!createDescriptorSetLayout()) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return false;
    }

    // 2. Create descriptor pool
    std::cout << "MeshGenerator: Creating descriptor pool for " << createInfo.maxNodesInFlight << " nodes..." << std::endl;
    if (!createDescriptorPool(createInfo.maxNodesInFlight)) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }

    // 3. Create buffers
    std::cout << "MeshGenerator: Creating buffers..." << std::endl;
    if (!createBuffers(createInfo)) {
        std::cerr << "Failed to create buffers" << std::endl;
        return false;
    }

    // 4. Allocate and update descriptor sets
    std::cout << "MeshGenerator: Allocating and updating descriptor sets..." << std::endl;
    if (!allocateDescriptorSets()) {
        std::cerr << "Failed to allocate descriptor sets" << std::endl;
        return false;
    }

    // 5. Create pipeline layout
    std::cout << "MeshGenerator: Creating pipeline layout..." << std::endl;
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::ivec3) + 3 * sizeof(uint32_t);
    std::cout << "MeshGenerator: Push constant size: " << pushConstantRange.size << " bytes" << std::endl;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (descriptorSetLayout == VK_NULL_HANDLE) {
        std::cerr << "ERROR: Descriptor set layout is null before pipeline layout creation!" << std::endl;
        return false;
    }
    std::cout << "MeshGenerator: Using descriptor set layout: " << descriptorSetLayout << std::endl;

    VkResult result = vkCreatePipelineLayout(device->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout: " << result << std::endl;
        return false;
    }
    std::cout << "MeshGenerator: Created pipeline layout: " << pipelineLayout << std::endl;

    // 6. Create compute pipeline last
    std::cout << "MeshGenerator: Creating compute pipeline..." << std::endl;
    if (!createComputePipeline()) {
        std::cerr << "Failed to create compute pipeline" << std::endl;
        return false;
    }

    std::cout << "MeshGenerator: Initialization complete" << std::endl;
    return true;
}

void MeshGenerator::cleanup() {
    if (device) {
        device->waitIdle();

        // Clear buffer sets first
        bufferSets.clear();

        // Free descriptor sets before destroying the pool
        if (descriptorPool != VK_NULL_HANDLE && !descriptorSets.empty()) {
            vkFreeDescriptorSets(
                device->getDevice(),
                descriptorPool,
                static_cast<uint32_t>(descriptorSets.size()),
                descriptorSets.data()
            );
            descriptorSets.clear();
        }

        // Destroy descriptor pool
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device->getDevice(), descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        // Destroy pipeline layout
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        // Destroy compute pipeline
        if (computePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device->getDevice(), computePipeline, nullptr);
            computePipeline = VK_NULL_HANDLE;
        }

        // Destroy descriptor set layout
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
    }
}

bool MeshGenerator::createDescriptorPool(uint32_t maxSets) {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = maxSets; // Voxel buffer
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = maxSets; // Vertex buffer
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = maxSets; // Index buffer
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = maxSets; // Counter buffer

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool MeshGenerator::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

    // Ensure all pImmutableSamplers are nullptr first
    for (auto& binding : bindings) {
        binding.pImmutableSamplers = nullptr;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    // Binding 0: Voxel buffer (read-only)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Binding 1: Vertex buffer (read/write)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Binding 2: Index buffer (read/write)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Binding 3: Counter buffer (read/write)
    // This must match the shader's declaration: layout(std430, binding = 3) buffer CounterBuffer
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.flags = 0; // No special flags needed

    VkResult result = vkCreateDescriptorSetLayout(device->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout: " << result << std::endl;
        return false;
    }

    std::cout << "Successfully created descriptor set layout with 4 bindings" << std::endl;
    return true;
}

bool MeshGenerator::createComputePipeline() {
    std::cout << "Creating compute pipeline..." << std::endl;
    
    // Load and validate shader
    std::vector<char> shaderCode;
    if (!loadShaderFile("shaders/mesh_generator.comp.spv", shaderCode)) {
        std::cerr << "Failed to load compute shader" << std::endl;
        return false;
    }
    std::cout << "Loaded compute shader successfully" << std::endl;

    // Create shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    VkResult moduleResult = vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &shaderModule);
    if (moduleResult != VK_SUCCESS) {
        std::cerr << "Failed to create shader module: " << moduleResult << std::endl;
        return false;
    }
    std::cout << "Created shader module successfully" << std::endl;

    // Configure shader stage
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT; // Add for debugging
    
    // Validate pipeline layout
    if (pipelineLayout == VK_NULL_HANDLE) {
        std::cerr << "Pipeline layout is null!" << std::endl;
        vkDestroyShaderModule(device->getDevice(), shaderModule, nullptr);
        return false;
    }
    
    std::cout << "Creating compute pipeline with layout: " << pipelineLayout << std::endl;
    VkResult result = vkCreateComputePipelines(device->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
    
    // Cleanup shader module
    vkDestroyShaderModule(device->getDevice(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline: " << result << std::endl;
        return false;
    }

    std::cout << "Created compute pipeline successfully" << std::endl;
    return true;
}

bool MeshGenerator::createBuffers(const MeshGeneratorCreateInfo& createInfo) {
    for (uint32_t i = 0; i < createInfo.maxNodesInFlight; ++i) {
        auto bufferSet = std::make_unique<BufferSet>();

        // Create voxel buffer
        bufferSet->voxelBuffer = std::make_unique<VulkanBuffer>(
            device,
            MAX_VOXELS * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Create vertex buffer
        bufferSet->vertexBuffer = std::make_unique<VulkanBuffer>(
            device,
            MAX_VERTICES * sizeof(float) * 3,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Create index buffer
        bufferSet->indexBuffer = std::make_unique<VulkanBuffer>(
            device,
            MAX_INDICES * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Create counter buffer
        bufferSet->counterBuffer = std::make_unique<VulkanBuffer>(
            device,
            sizeof(uint32_t) * 2,  // One for vertex count, one for index count
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        bufferSets.push_back(std::move(bufferSet));
    }

    return true;
}

bool MeshGenerator::allocateDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(bufferSets.size(), descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(layouts.size());
    if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets" << std::endl;
        return false;
    }

    // Update descriptor sets
    for (size_t i = 0; i < bufferSets.size(); i++) {
        std::array<VkDescriptorBufferInfo, 4> bufferInfos{};
        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

        // Voxel buffer
        bufferInfos[0].buffer = bufferSets[i]->voxelBuffer->getBuffer();
        bufferInfos[0].offset = 0;
        bufferInfos[0].range = VK_WHOLE_SIZE;

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfos[0];

        // Vertex buffer
        bufferInfos[1].buffer = bufferSets[i]->vertexBuffer->getBuffer();
        bufferInfos[1].offset = 0;
        bufferInfos[1].range = VK_WHOLE_SIZE;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &bufferInfos[1];

        // Index buffer
        bufferInfos[2].buffer = bufferSets[i]->indexBuffer->getBuffer();
        bufferInfos[2].offset = 0;
        bufferInfos[2].range = VK_WHOLE_SIZE;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &bufferInfos[2];

        // Counter buffer
        bufferInfos[3].buffer = bufferSets[i]->counterBuffer->getBuffer();
        bufferInfos[3].offset = 0;
        bufferInfos[3].range = VK_WHOLE_SIZE;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &bufferInfos[3];

        vkUpdateDescriptorSets(device->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    return true;
}

bool MeshGenerator::loadShaderFile(const std::string& filename, std::vector<char>& buffer) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filename << std::endl;
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        std::cerr << "Shader file is empty: " << filename << std::endl;
        return false;
    }

    buffer.resize(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    if (buffer.size() % 4 != 0) {
        std::cerr << "Shader file size is not a multiple of 4: " << filename << std::endl;
        return false;
    }

    std::cout << "Successfully loaded shader file: " << filename << " (size: " << fileSize << " bytes)" << std::endl;
    return true;
}

bool MeshGenerator::generateMesh(
    const void* inputData,
    size_t inputSize,
    uint32_t lodLevel,
    float lodTransitionFactor,
    const float* position,
    float size,
    void* outputData,
    size_t* outputSize
) {
    // TODO: Implement mesh generation
    return false;
}

} // namespace voxceleron