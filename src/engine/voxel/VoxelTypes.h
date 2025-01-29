#pragma once

#include <array>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace voxceleron {

// Forward declarations
struct OctreeNode;

// Basic voxel type
struct Voxel {
    uint32_t type;     // Type of the voxel (air, solid, etc.)
    uint32_t color;    // RGBA color packed into 32 bits
};

// Run-length encoding for voxel compression
struct VoxelRun {
    Voxel voxel;
    uint32_t count;    // Number of consecutive identical voxels
};

// Node data for leaf nodes (contains compressed voxels)
struct LeafData {
    std::vector<VoxelRun> runs;  // Run-length encoded voxels
    std::vector<uint32_t> data;  // Uncompressed voxel data for mesh generation
    size_t totalVoxels;          // Total number of voxels represented
    
    LeafData() : totalVoxels(0) {}
    ~LeafData() = default;
    
    // Helper functions for RLE compression
    void addVoxel(const Voxel& voxel) {
        if (runs.empty() || runs.back().voxel.type != voxel.type || 
            runs.back().voxel.color != voxel.color) {
            runs.push_back({voxel, 1});
        } else {
            runs.back().count++;
        }
        totalVoxels++;
        
        // Update uncompressed data
        uint32_t packedVoxel = (voxel.color & 0xFFFFFF00) | (voxel.type & 0xFF);
        data.push_back(packedVoxel);
    }
    
    Voxel getVoxel(size_t index) const {
        if (index < data.size()) {
            uint32_t packedVoxel = data[index];
            return Voxel{
                packedVoxel & 0xFF,           // type
                packedVoxel & 0xFFFFFF00      // color
            };
        }
        return Voxel{0, 0}; // Default voxel if index out of range
    }
    
    void decompressData() {
        if (data.empty() && !runs.empty()) {
            data.reserve(totalVoxels);
            for (const auto& run : runs) {
                uint32_t packedVoxel = (run.voxel.color & 0xFFFFFF00) | (run.voxel.type & 0xFF);
                for (uint32_t i = 0; i < run.count; ++i) {
                    data.push_back(packedVoxel);
                }
            }
        }
    }
    
    void compressData() {
        if (runs.empty() && !data.empty()) {
            runs.clear();
            totalVoxels = 0;
            for (uint32_t packedVoxel : data) {
                Voxel voxel{
                    packedVoxel & 0xFF,           // type
                    packedVoxel & 0xFFFFFF00      // color
                };
                addVoxel(voxel);
            }
        }
    }
};

// Memory pool for efficient node allocation
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
    struct Block {
        std::array<T, BlockSize> data;
        std::vector<bool> used;
        Block() : used(BlockSize, false) {}
    };
    
    std::vector<std::unique_ptr<Block>> blocks;
    
public:
    T* allocate() {
        for (auto& block : blocks) {
            for (size_t i = 0; i < BlockSize; ++i) {
                if (!block->used[i]) {
                    block->used[i] = true;
                    return &block->data[i];
                }
            }
        }
        
        // No free space found, create new block
        auto newBlock = std::make_unique<Block>();
        newBlock->used[0] = true;
        T* result = &newBlock->data[0];
        blocks.push_back(std::move(newBlock));
        return result;
    }
    
    void deallocate(T* ptr) {
        for (auto& block : blocks) {
            if (ptr >= &block->data[0] && 
                ptr < &block->data[0] + BlockSize) {
                size_t index = ptr - &block->data[0];
                block->used[index] = false;
                return;
            }
        }
    }
};

// Node data for internal nodes (contains children)
struct InternalData {
    std::array<std::unique_ptr<OctreeNode>, 8> children;
    
    InternalData() = default;
    ~InternalData() = default;
};

// Union to store either leaf or internal node data
union NodeData {
    LeafData leaf;
    InternalData internal;
    
    NodeData() {} // Default constructor
    ~NodeData() {} // Destructor (managed manually due to union)
};

// Cache entry for mesh data
struct MeshCacheEntry {
    std::vector<uint32_t> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 cameraPos;    // Camera position when mesh was generated
    float lodLevel;         // LOD level when mesh was generated
    uint64_t lastUsed;      // Timestamp of last use
};

// Now define OctreeNode after all its dependencies
struct OctreeNode {
    uint8_t childMask;      // Bitmask indicating which children exist
    bool isLeaf;           // Whether this is a leaf node
    uint32_t level;        // LOD level (0 = highest detail)
    
    // Node properties
    glm::ivec3 position;
    uint32_t size;
    bool needsUpdate;
    bool isOptimized;
    uint32_t optimizedValue;
    
    // Node data (either children or voxels)
    NodeData nodeData;
    
    // Mesh data
    std::shared_ptr<MeshCacheEntry> meshCache;
    VkBuffer meshBuffer;
    VkDeviceMemory meshMemory;
    uint32_t vertexCount;
    uint32_t indexCount;
    
    OctreeNode() : 
        childMask(0), 
        isLeaf(false), 
        level(0),
        position(0),
        size(0),
        needsUpdate(true),
        isOptimized(false),
        optimizedValue(0),
        meshBuffer(VK_NULL_HANDLE),
        meshMemory(VK_NULL_HANDLE),
        vertexCount(0),
        indexCount(0) {
        // Initialize nodeData based on isLeaf
        if (isLeaf) {
            new (&nodeData.leaf) LeafData();
        } else {
            new (&nodeData.internal) InternalData();
        }
    }
    
    ~OctreeNode() {
        // Cleanup nodeData based on isLeaf
        if (isLeaf) {
            nodeData.leaf.~LeafData();
        } else {
            nodeData.internal.~InternalData();
        }
    }
    
    // Prevent copying
    OctreeNode(const OctreeNode&) = delete;
    OctreeNode& operator=(const OctreeNode&) = delete;
    
    // Allow moving
    OctreeNode(OctreeNode&&) = default;
    OctreeNode& operator=(OctreeNode&&) = default;
};

} // namespace voxceleron