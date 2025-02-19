#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <array>

namespace voxceleron {

struct MeshData {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Forward declaration
class OctreeNode;

struct LeafData {
    std::vector<uint32_t> data;  // Packed voxel data
    std::vector<uint8_t> runs;   // RLE compressed runs
};

struct InternalData {
    std::array<std::unique_ptr<OctreeNode>, 8> children;
};

class OctreeNode {
public:
    glm::ivec3 position{0};      // Position in world space
    uint32_t size{0};            // Size of this node (power of 2)
    uint32_t level{0};           // Level in octree (0 = root)
    bool isLeaf{true};           // Is this a leaf node?
    bool needsUpdate{false};     // Does this node need mesh update?
    bool isOptimized{false};     // Has this node been optimized?
    uint32_t optimizedValue{0};  // Value for optimized nodes
    uint8_t childMask{0};        // Bitmask of active children

    // Mesh data
    std::vector<MeshData> meshes;

    // Node data (union to save memory)
    union NodeData {
        LeafData leaf;
        InternalData internal;

        NodeData() : leaf() {}  // Initialize as leaf data by default
        ~NodeData() {}         // Empty destructor
    } nodeData;

    const std::vector<MeshData>& getMeshes() const { return meshes; }
};

} // namespace voxceleron