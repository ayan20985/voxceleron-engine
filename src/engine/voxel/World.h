#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "VoxelTypes.h"
#include "../vulkan/core/Vertex.h"

namespace voxceleron {

class Camera;
class WorldRenderer;
class VulkanContext;

// Maximum level of detail for the octree
static constexpr uint32_t MAX_LEVEL = 16;

// LOD constants
struct LODParameters {
    float baseDistance = 100.0f;     // Distance for LOD level 0
    float lodFactor = 2.0f;          // Geometric progression factor
    float transitionRange = 32.0f;   // Blend range between LODs
    float directionBias = 0.5f;      // View direction influence
};

class World {
public:
    World(VulkanContext* context);
    ~World();
    
    // Core world manipulation
    void setVoxel(const glm::ivec3& pos, const Voxel& voxel);
    Voxel getVoxel(const glm::ivec3& pos) const;
    
    // LOD and mesh generation
    void updateLOD(const glm::vec3& viewerPos);
    void generateMeshes(const glm::vec3& viewerPos);
    bool generateMeshForNode(OctreeNode* node);
    
    // Node management
    bool optimizeNodes();
    void subdivideNode(OctreeNode* node);
    void optimizeNode(OctreeNode* node);
    
    // Statistics and memory
    size_t getMemoryUsage() const;
    uint32_t getNodeCount() const;
    size_t calculateMemoryUsage() const;
    size_t countNodes(bool activeOnly = false) const;
    size_t countNodesByLevel(uint32_t level) const;
    
    // Vulkan initialization
    bool initialize();
    void cleanup();

    // Main update function
    void update();

    // Rendering
    void prepareFrame(const Camera& camera);
    void render(VkCommandBuffer commandBuffer);
    
    // Debug visualization
    void setDebugVisualization(bool enabled);
    bool isDebugVisualizationEnabled() const;

    // LOD parameters
    void setLODParameters(const LODParameters& params) { lodParams = params; }
    const LODParameters& getLODParameters() const { return lodParams; }

    // Getters
    const OctreeNode* getRoot() const { return root.get(); }
    OctreeNode* getRoot() { return root.get(); }
    
private:
    // Octree management
    std::unique_ptr<OctreeNode> root;
    const OctreeNode* findNode(const glm::ivec3& pos) const;
    OctreeNode* findNode(const glm::ivec3& pos, bool create = false);

    // Memory management
    MemoryPool<OctreeNode> nodePool;
    std::unordered_map<OctreeNode*, std::unique_ptr<MeshCacheEntry>> meshCache;
    void cleanupOldCacheEntries();
    
    // LOD management
    LODParameters lodParams;
    float calculateNodeLOD(const glm::vec3& nodePos, float nodeSize, const glm::vec3& viewerPos);
    bool shouldGenerateMesh(OctreeNode* node, const glm::vec3& viewerPos);
    
    // Vulkan resources
    VulkanContext* context;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline computePipeline;
    VkQueue computeQueue;
    VkCommandPool commandPool;
    
    // Mesh data
    struct MeshData {
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;
        uint32_t vertexCount;
        uint32_t indexCount;
    };
    std::unordered_map<OctreeNode*, MeshData> meshes;

    // Mesh generation
    void addCubeToMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, const Voxel& voxel);
    bool createMeshBuffers(OctreeNode* node, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    // Rendering
    std::unique_ptr<WorldRenderer> renderer;
    
    // Vulkan helpers
    bool createComputePipeline();
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                     VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void cleanupMeshData(MeshData& meshData);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    uint32_t findComputeQueueFamily(VkPhysicalDevice physicalDevice);

    // Debug
    bool debugVisualization;
};

} // namespace voxceleron