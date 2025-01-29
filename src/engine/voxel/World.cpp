#include "World.h"
#include "WorldRenderer.h"
#include "../core/Camera.h"
#include "../vulkan/core/VulkanContext.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

namespace voxceleron {

World::World(VulkanContext* context)
    : context(context)
    , device(context->getDevice())
    , physicalDevice(context->getPhysicalDevice())
    , descriptorPool(VK_NULL_HANDLE)
    , descriptorSetLayout(VK_NULL_HANDLE)
    , pipelineLayout(VK_NULL_HANDLE)
    , computePipeline(VK_NULL_HANDLE)
    , computeQueue(VK_NULL_HANDLE)
    , commandPool(VK_NULL_HANDLE) {
    std::cout << "World: Creating world instance" << std::endl;
}

World::~World() {
    std::cout << "World: Destroying world instance" << std::endl;
    cleanup();
}

bool World::initialize() {
    std::cout << "World: Starting initialization..." << std::endl;

    // Create root node
    root = std::make_unique<OctreeNode>();
    root->position = glm::ivec3(0);
    root->size = 1 << MAX_LEVEL;
    root->level = 0;
    root->isLeaf = true;

    // Create renderer
    renderer = std::make_unique<WorldRenderer>();
    if (!renderer->initialize(device, physicalDevice)) {
        std::cerr << "World: Failed to initialize renderer" << std::endl;
        return false;
    }

    // Create compute pipeline for mesh generation
    if (!createComputePipeline()) {
        std::cerr << "World: Failed to create compute pipeline" << std::endl;
        return false;
    }

    std::cout << "World: Initialization complete" << std::endl;
    return true;
}

void World::cleanup() {
    std::cout << "World: Starting cleanup..." << std::endl;

    // Clean up renderer
    if (renderer) {
        renderer.reset();
    }

    // Clean up mesh data
    for (auto& [node, meshData] : meshes) {
        cleanupMeshData(meshData);
    }
    meshes.clear();

    // Clean up Vulkan resources
    if (device != VK_NULL_HANDLE) {
        if (computePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, computePipeline, nullptr);
            computePipeline = VK_NULL_HANDLE;
        }

        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
    }

    // Clean up octree
    root.reset();

    std::cout << "World: Cleanup complete" << std::endl;
}

void World::setVoxel(const glm::ivec3& pos, const Voxel& voxel) {
    OctreeNode* node = findNode(pos, true);
    if (!node) return;

    // Calculate local position within the node
    glm::ivec3 localPos = pos - node->position;
    uint32_t index = (localPos.x & 1) | ((localPos.y & 1) << 1) | ((localPos.z & 1) << 2);

    // Initialize data vector if empty
    if (node->nodeData.leaf.data.empty()) {
        node->nodeData.leaf.data.resize(8, 0);  // Initialize with 8 empty voxels
    }

    // Pack voxel data into uint32_t
    uint32_t packedVoxel = (voxel.color & 0xFFFFFF00) | (voxel.type & 0xFF);
    node->nodeData.leaf.data[index] = packedVoxel;
    node->needsUpdate = true;
}

Voxel World::getVoxel(const glm::ivec3& pos) const {
    const OctreeNode* node = findNode(pos);
    if (!node || node->nodeData.leaf.data.empty()) {
        return Voxel{0, 0};  // Return empty voxel if node doesn't exist
    }

    // Calculate local position within the node
    glm::ivec3 localPos = pos - node->position;
    uint32_t index = (localPos.x & 1) | ((localPos.y & 1) << 1) | ((localPos.z & 1) << 2);

    // Unpack voxel data from uint32_t
    uint32_t packedVoxel = node->nodeData.leaf.data[index];
    return Voxel{
        static_cast<uint32_t>(packedVoxel & 0xFF),           // type
        static_cast<uint32_t>(packedVoxel & 0xFFFFFF00)      // color
    };
}

void World::updateLOD(const glm::vec3& viewerPos) {
    if (!root) return;

    // Update LOD levels based on distance from viewer
    std::function<void(OctreeNode*, const glm::vec3&)> updateNode = 
        [&](OctreeNode* node, const glm::vec3& pos) {
            if (!node) return;

            // Calculate distance to viewer
            glm::vec3 center = glm::vec3(node->position) + glm::vec3(node->size / 2);
            float distance = glm::length(center - pos);

            // Determine desired LOD level based on distance
            float factor = distance / (node->size * 2.0f);
            uint32_t desiredLevel = static_cast<uint32_t>(glm::log2(factor));
            desiredLevel = glm::clamp(desiredLevel, 0u, MAX_LEVEL);

            // Split or merge based on desired level
            if (desiredLevel > node->level && !node->isLeaf) {
                // Node is too detailed, try to merge
                optimizeNode(node);
            } else if (desiredLevel < node->level && node->isLeaf) {
                // Node needs more detail, split
                subdivideNode(node);
            }

            // Recursively update children
            if (!node->isLeaf) {
                for (uint8_t i = 0; i < 8; ++i) {
                    if (node->childMask & (1 << i)) {
                        updateNode(node->nodeData.internal.children[i].get(), pos);
                    }
                }
            }
        };

    updateNode(root.get(), viewerPos);
}

void World::generateMeshes(const glm::vec3& viewerPos) {
    if (!root) return;

    // Queue of nodes that need mesh updates
    std::vector<OctreeNode*> updateQueue;

    // Collect nodes that need updates
    std::function<void(OctreeNode*)> collectNodes = [&](OctreeNode* node) {
        if (!node) return;

        if (node->needsUpdate) {
            updateQueue.push_back(node);
        }

        if (!node->isLeaf) {
            for (uint8_t i = 0; i < 8; ++i) {
                if (node->childMask & (1 << i)) {
                    collectNodes(node->nodeData.internal.children[i].get());
                }
            }
        }
    };

    collectNodes(root.get());

    // Sort nodes by distance to viewer (closest first)
    std::sort(updateQueue.begin(), updateQueue.end(),
        [&viewerPos](const OctreeNode* a, const OctreeNode* b) {
            glm::vec3 centerA = glm::vec3(a->position) + glm::vec3(a->size / 2);
            glm::vec3 centerB = glm::vec3(b->position) + glm::vec3(b->size / 2);
            float distA = glm::length(centerA - viewerPos);
            float distB = glm::length(centerB - viewerPos);
            return distA < distB;
        });

    // Generate meshes for nodes that need updates
    for (auto* node : updateQueue) {
        if (generateMeshForNode(node)) {
            node->needsUpdate = false;
        }
    }
}

void World::prepareFrame(const Camera& camera) {
    if (renderer) {
        renderer->prepareFrame(camera, *this);
    }
}

void World::render(VkCommandBuffer commandBuffer) {
    if (renderer) {
        renderer->recordCommands(commandBuffer);
    }
}

void World::setDebugVisualization(bool enabled) {
    if (renderer) {
        renderer->setDebugVisualization(enabled);
    }
}

bool World::isDebugVisualizationEnabled() const {
    return renderer ? renderer->isDebugVisualizationEnabled() : false;
}

OctreeNode* World::findNode(const glm::ivec3& position, bool create) {
    if (!root) {
        if (create) {
            root = std::make_unique<OctreeNode>();
            root->position = glm::ivec3(0);
            root->size = 1u << MAX_LEVEL;
            root->level = 0;
            root->isLeaf = true;
            return root.get();
        }
        return nullptr;
    }

    OctreeNode* current = root.get();
    uint32_t size = 1u << MAX_LEVEL;
    uint32_t level = 0;

    while (current && !current->isLeaf && level < MAX_LEVEL) {
        size >>= 1;
        glm::ivec3 localPos = (position - current->position) / static_cast<int>(size);
        uint32_t index = (localPos.x & 1) | ((localPos.y & 1) << 1) | ((localPos.z & 1) << 2);

        if (!(current->childMask & (1 << index))) {
            if (!create) {
                return nullptr;
            }
            
            // Create new child node
            auto& child = current->nodeData.internal.children[index];
            child = std::make_unique<OctreeNode>();
            child->position = current->position + glm::ivec3(
                (index & 1) ? size : 0,
                (index & 2) ? size : 0,
                (index & 4) ? size : 0
            );
            child->size = size;
            child->level = level + 1;
            child->isLeaf = true;
            current->childMask |= (1 << index);
        }

        current = current->nodeData.internal.children[index].get();
        level++;
    }

    return current;
}

const OctreeNode* World::findNode(const glm::ivec3& position) const {
    if (!root) return nullptr;

    const OctreeNode* current = root.get();
    uint32_t size = 1u << MAX_LEVEL;
    uint32_t level = 0;

    while (current && !current->isLeaf && level < MAX_LEVEL) {
        size >>= 1;
        glm::ivec3 localPos = (position - current->position) / static_cast<int>(size);
        uint32_t index = (localPos.x & 1) | ((localPos.y & 1) << 1) | ((localPos.z & 1) << 2);

        if (!(current->childMask & (1 << index))) {
            return nullptr;
        }

        current = current->nodeData.internal.children[index].get();
        level++;
    }

    return current;
}

void World::subdivideNode(OctreeNode* node) {
    if (!node || !node->isLeaf || node->level >= MAX_LEVEL) return;

    // Convert to internal node
    node->isLeaf = false;
    std::vector<uint32_t> leafData;
    if (!node->nodeData.leaf.data.empty()) {
        leafData = node->nodeData.leaf.data;  // Save the data before moving
    }
    node->nodeData.leaf.~LeafData();  // Explicitly destruct leaf data
    new (&node->nodeData.internal) InternalData();

    // Create child nodes
    uint32_t childSize = node->size >> 1;
    for (uint32_t i = 0; i < 8; ++i) {
        auto& child = node->nodeData.internal.children[i];
        child = std::make_unique<OctreeNode>();
        child->position = node->position + glm::ivec3(
            (i & 1) ? childSize : 0,
            (i & 2) ? childSize : 0,
            (i & 4) ? childSize : 0
        );
        child->size = childSize;
        child->level = node->level + 1;
        child->isLeaf = true;
        node->childMask |= (1 << i);
        
        // Copy voxel data if it exists
        if (!leafData.empty()) {
            child->nodeData.leaf.data.resize(8, 0);
            child->nodeData.leaf.data[i] = leafData[i];
        }
    }

    node->needsUpdate = true;
}

void World::optimizeNode(OctreeNode* node) {
    if (!node) return;
    
    if (node->isLeaf) {
        if (!node->nodeData.leaf.data.empty()) {
            // Check if all voxels are the same
            uint32_t firstVoxel = node->nodeData.leaf.data[0];
            bool allSame = true;
            
            for (size_t i = 1; i < node->nodeData.leaf.data.size(); ++i) {
                if (node->nodeData.leaf.data[i] != firstVoxel) {
                    allSame = false;
                    break;
                }
            }
            
            if (allSame) {
                // If all voxels are empty (type == 0)
                if ((firstVoxel & 0xFF) == 0) {
                    node->nodeData.leaf.data.clear();
                    node->nodeData.leaf.runs.clear();
                    node->isOptimized = true;
                    node->optimizedValue = 0;
                } else {
                    // Otherwise, keep one voxel and mark as optimized
                    node->isOptimized = true;
                    node->optimizedValue = firstVoxel;
                }
            }
        }
    }
}

bool World::optimizeNodes() {
    if (!root) return false;

    bool anyOptimized = false;
    std::function<void(OctreeNode*)> optimizeRecursive = [&](OctreeNode* node) {
        if (!node || node->isLeaf) return;

        // First optimize children
        for (uint8_t i = 0; i < 8; ++i) {
            if (node->childMask & (1 << i)) {
                optimizeRecursive(node->nodeData.internal.children[i].get());
            }
        }

        // Then try to optimize this node
        bool wasLeaf = node->isLeaf;
        optimizeNode(node);
        if (node->isLeaf && !wasLeaf) {
            anyOptimized = true;
        }
    };

    optimizeRecursive(root.get());
    return anyOptimized;
}

size_t World::getMemoryUsage() const {
    return calculateMemoryUsage();
}

uint32_t World::getNodeCount() const {
    return static_cast<uint32_t>(countNodes(false));
}

size_t World::calculateMemoryUsage() const {
    size_t total = sizeof(World);
    if (root) {
        std::function<size_t(const OctreeNode*)> calcNodeMemory = 
            [&calcNodeMemory](const OctreeNode* node) -> size_t {
                if (!node) return 0;

                size_t memory = sizeof(OctreeNode);
                
                if (node->isLeaf) {
                    memory += node->nodeData.leaf.data.capacity() * sizeof(uint32_t);
                    memory += node->nodeData.leaf.runs.capacity() * sizeof(VoxelRun);
                } else {
                    for (uint8_t i = 0; i < 8; ++i) {
                        if (node->childMask & (1 << i)) {
                            memory += calcNodeMemory(node->nodeData.internal.children[i].get());
                        }
                    }
                }

                return memory;
            };

        total += calcNodeMemory(root.get());
    }
    return total;
}

size_t World::countNodes(bool activeOnly) const {
    if (!root) return 0;

    std::function<size_t(const OctreeNode*)> countRecursive = 
        [activeOnly, &countRecursive](const OctreeNode* node) -> size_t {
            if (!node) return 0;

            size_t count = 1;
            if (!node->isLeaf) {
                for (uint8_t i = 0; i < 8; ++i) {
                    if (!activeOnly || (node->childMask & (1 << i))) {
                        count += countRecursive(node->nodeData.internal.children[i].get());
                    }
                }
            }
            return count;
        };

    return countRecursive(root.get());
}

size_t World::countNodesByLevel(uint32_t level) const {
    if (!root) return 0;

    std::function<size_t(const OctreeNode*)> countRecursive = 
        [level, &countRecursive](const OctreeNode* node) -> size_t {
            if (!node) return 0;

            size_t count = (node->level == level) ? 1 : 0;
            if (!node->isLeaf) {
                for (uint8_t i = 0; i < 8; ++i) {
                    if (node->childMask & (1 << i)) {
                        count += countRecursive(node->nodeData.internal.children[i].get());
                    }
                }
            }
            return count;
        };

    return countRecursive(root.get());
}

bool World::createComputePipeline() {
    std::cout << "World: Creating compute pipeline..." << std::endl;
    
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[4] = {};
    
    // Binding 0: Input voxel data (storage buffer)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: Vertex buffer (storage buffer)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Index buffer (storage buffer)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Counter buffer (storage buffer)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4; // Updated to match new binding count
    layoutInfo.pBindings = bindings;

    std::cout << "World: Creating descriptor set layout with " << layoutInfo.bindingCount << " bindings" << std::endl;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "World: Failed to create descriptor set layout" << std::endl;
        return false;
    }
    std::cout << "World: Created descriptor set layout: " << descriptorSetLayout << std::endl;

    // Create descriptor pool
    VkDescriptorPoolSize poolSizes[4] = {};
    for (int i = 0; i < 4; i++) {
        poolSizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[i].descriptorCount = 100; // Adjust based on max concurrent nodes
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 100; // Adjust based on max concurrent nodes
    poolInfo.poolSizeCount = 4; // Updated to match new number of bindings
    poolInfo.pPoolSizes = poolSizes;

    std::cout << "World: Creating descriptor pool with " << poolInfo.poolSizeCount << " pool sizes" << std::endl;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        std::cerr << "World: Failed to create descriptor pool" << std::endl;
        return false;
    }
    
    // Create push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::ivec3) + 3 * sizeof(uint32_t); // nodePosition, nodeSize, maxVertices, maxIndices

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "World: Failed to create pipeline layout" << std::endl;
        return false;
    }
    
    // Create shader module
    std::vector<char> shaderCode;
    try {
        std::ifstream file("shaders/mesh_generator.comp.spv", std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "World: Failed to open compute shader file" << std::endl;
            return false;
        }
        
        size_t fileSize = (size_t)file.tellg();
        shaderCode.resize(fileSize);
        file.seekg(0);
        file.read(shaderCode.data(), fileSize);
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "World: Failed to read compute shader file: " << e.what() << std::endl;
        return false;
    }
    
    VkShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = shaderCode.size();
    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "World: Failed to create shader module" << std::endl;
        return false;
    }
    
    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;
    
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        std::cerr << "World: Failed to create compute pipeline" << std::endl;
        vkDestroyShaderModule(device, shaderModule, nullptr);
        return false;
    }
    
    // Clean up shader module
    vkDestroyShaderModule(device, shaderModule, nullptr);

    // Create command pool for compute commands
    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = findComputeQueueFamily(physicalDevice);

    if (vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "World: Failed to create command pool" << std::endl;
        return false;
    }

    // Get compute queue
    vkGetDeviceQueue(device, findComputeQueueFamily(physicalDevice), 0, &computeQueue);

    std::cout << "World: Compute pipeline created successfully" << std::endl;
    return true;
}

uint32_t World::findComputeQueueFamily(VkPhysicalDevice physicalDevice) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find compute queue family");
}

bool World::generateMeshForNode(OctreeNode* node) {
    if (!node || !node->needsUpdate) return false;

    // Create buffers for voxel data
    const uint32_t voxelBufferSize = node->size * node->size * node->size * sizeof(uint32_t);
    VkBuffer voxelBuffer;
    VkDeviceMemory voxelMemory;

    if (!createBuffer(voxelBufferSize, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        voxelBuffer, voxelMemory)) {
        return false;
    }

    // Create staging buffer for voxel data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    if (!createBuffer(voxelBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory)) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelMemory, nullptr);
        return false;
    }

    // Map and fill staging buffer with voxel data
    void* data;
    vkMapMemory(device, stagingMemory, 0, voxelBufferSize, 0, &data);
    uint32_t* voxelData = static_cast<uint32_t*>(data);

    // Fill voxel data from node
    if (node->isLeaf && !node->nodeData.leaf.data.empty()) {
        // For leaf nodes, copy the voxel data directly
        std::memcpy(voxelData, node->nodeData.leaf.data.data(), 
                   node->nodeData.leaf.data.size() * sizeof(uint32_t));
    } else {
        // For internal nodes or empty nodes, fill with air voxels
        std::memset(voxelData, 0, voxelBufferSize);
    }

    vkUnmapMemory(device, stagingMemory);

    // Copy staging buffer to device local buffer
    copyBuffer(stagingBuffer, voxelBuffer, voxelBufferSize);

    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create output mesh buffers
    const uint32_t maxVertices = node->size * node->size * node->size * 24; // 24 vertices per voxel (worst case)
    const uint32_t maxIndices = node->size * node->size * node->size * 36;  // 36 indices per voxel (worst case)
    const uint32_t meshBufferSize = 
        maxVertices * (8 * sizeof(float)) + // pos(3) + normal(3) + uv(2)
        maxIndices * sizeof(uint32_t) +     // indices
        2 * sizeof(uint32_t);               // vertex and index counts

    // Create vertex buffer
    const uint32_t vertexBufferSize = maxVertices * 8 * sizeof(float); // 8 floats per vertex
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexMemory;
    if (!createBuffer(vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer, vertexMemory)) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelMemory, nullptr);
        return false;
    }

    // Create index buffer
    const uint32_t indexBufferSize = maxIndices * sizeof(uint32_t);
    VkBuffer indexBuffer;
    VkDeviceMemory indexMemory;
    if (!createBuffer(indexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexMemory)) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexMemory, nullptr);
        return false;
    }

    // Create counter buffer
    const uint32_t counterBufferSize = 2 * sizeof(uint32_t); // vertexCounter and indexCounter
    VkBuffer counterBuffer;
    VkDeviceMemory counterMemory;
    if (!createBuffer(counterBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        counterBuffer, counterMemory)) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexMemory, nullptr);
        return false;
    }

    // Initialize counter buffer
    uint32_t* counterData;
    vkMapMemory(device, counterMemory, 0, counterBufferSize, 0, (void**)&counterData);
    counterData[0] = 0; // vertexCounter
    counterData[1] = 0; // indexCounter
    vkUnmapMemory(device, counterMemory);

    // Create descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexMemory, nullptr);
        vkDestroyBuffer(device, counterBuffer, nullptr);
        vkFreeMemory(device, counterMemory, nullptr);
        return false;
    }
    
    // Update descriptor set
    VkDescriptorBufferInfo voxelBufferInfo{};
    voxelBufferInfo.buffer = voxelBuffer;
    voxelBufferInfo.offset = 0;
    voxelBufferInfo.range = voxelBufferSize;

    VkDescriptorBufferInfo vertexBufferInfo{};
    vertexBufferInfo.buffer = vertexBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = vertexBufferSize;

    VkDescriptorBufferInfo indexBufferInfo{};
    indexBufferInfo.buffer = indexBuffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range = indexBufferSize;

    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = counterBuffer;
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = counterBufferSize;

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &voxelBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &vertexBufferInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &indexBufferInfo;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &counterBufferInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    // Create command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {
        vkDestroyBuffer(device, voxelBuffer, nullptr);
        vkFreeMemory(device, voxelMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexMemory, nullptr);
        vkDestroyBuffer(device, counterBuffer, nullptr);
        vkFreeMemory(device, counterMemory, nullptr);
        return false;
    }

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Push constants
    struct PushConstants {
        glm::ivec3 nodePosition;
        uint32_t nodeSize;
        uint32_t maxVertices;
        uint32_t maxIndices;
    } pushConstants;

    pushConstants.nodePosition = node->position;
    pushConstants.nodeSize = node->size;
    pushConstants.maxVertices = maxVertices;
    pushConstants.maxIndices = maxIndices;

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Dispatch compute shader
    const uint32_t workGroupSize = 8;
    uint32_t groupCount = (node->size + workGroupSize - 1) / workGroupSize;
    vkCmdDispatch(commandBuffer, groupCount, groupCount, groupCount);

    // Memory barrier to ensure compute shader writes are visible
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_HOST_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_HOST_BIT,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );

    // End command buffer
    vkEndCommandBuffer(commandBuffer);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(computeQueue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    // Clean up command buffer and fence
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

    // Get vertex and index counts from counter buffer
    vkMapMemory(device, counterMemory, 0, counterBufferSize, 0, (void**)&counterData);
    uint32_t vertexCount = counterData[0];
    uint32_t indexCount = counterData[1];
    vkUnmapMemory(device, counterMemory);

    // Clean up counter buffer
    vkDestroyBuffer(device, counterBuffer, nullptr);
    vkFreeMemory(device, counterMemory, nullptr);

    // Store mesh data in node
    auto& meshData = meshes[node];
    if (meshData.vertexBuffer != VK_NULL_HANDLE) {
        cleanupMeshData(meshData);
    }

    meshData.vertexBuffer = vertexBuffer;
    meshData.vertexMemory = vertexMemory;
    meshData.indexBuffer = indexBuffer;
    meshData.indexMemory = indexMemory;
    meshData.vertexCount = vertexCount;
    meshData.indexCount = indexCount;

    // Clean up voxel buffer
    vkDestroyBuffer(device, voxelBuffer, nullptr);
    vkFreeMemory(device, voxelMemory, nullptr);

    return true;
}

void World::cleanupMeshData(MeshData& meshData) {
    if (meshData.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshData.vertexBuffer, nullptr);
        meshData.vertexBuffer = VK_NULL_HANDLE;
    }
    if (meshData.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, meshData.vertexMemory, nullptr);
        meshData.vertexMemory = VK_NULL_HANDLE;
    }
    if (meshData.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, meshData.indexBuffer, nullptr);
        meshData.indexBuffer = VK_NULL_HANDLE;
    }
    if (meshData.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, meshData.indexMemory, nullptr);
        meshData.indexMemory = VK_NULL_HANDLE;
    }
    meshData.vertexCount = 0;
    meshData.indexCount = 0;
}

void World::update() {
    // Update LOD based on camera position
    if (renderer) {
        const Camera* camera = renderer->getCamera();
        if (camera) {
            glm::vec3 viewerPos = camera->getPosition();
            updateLOD(viewerPos);
            generateMeshes(viewerPos);
        }
    }

    // Optimize nodes if needed
    optimizeNodes();
}

bool World::createBuffer(uint64_t size, uint32_t usage, uint32_t properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "World: Failed to create buffer" << std::endl;
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        std::cerr << "World: Failed to allocate buffer memory" << std::endl;
        vkDestroyBuffer(device, buffer, nullptr);
        return false;
    }

    if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        std::cerr << "World: Failed to bind buffer memory" << std::endl;
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, bufferMemory, nullptr);
        return false;
    }

    return true;
}

void World::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint64_t size) {
    // Create command buffer for transfer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        std::cerr << "World: Failed to allocate command buffer for buffer copy" << std::endl;
        return;
    }

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Copy buffer
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Create fence to ensure copy completes before function returns
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    VkFence fence;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        std::cerr << "World: Failed to create fence for buffer copy" << std::endl;
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return;
    }

    // Submit and wait
    if (vkQueueSubmit(computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        std::cerr << "World: Failed to submit buffer copy command" << std::endl;
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return;
    }

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    // Cleanup
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t World::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}
} // namespace voxceleron 

