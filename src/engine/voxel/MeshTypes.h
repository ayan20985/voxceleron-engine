#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace voxceleron {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    uint32_t material;  // Material ID packed into vertex
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertexMemory;
    VkDeviceMemory indexMemory;
    bool needsUpdate;
    uint32_t lodLevel;

    MeshData() : vertexBuffer(VK_NULL_HANDLE), indexBuffer(VK_NULL_HANDLE),
                 vertexMemory(VK_NULL_HANDLE), indexMemory(VK_NULL_HANDLE),
                 needsUpdate(true), lodLevel(0) {}
};

// For compute shader output
struct GPUMeshData {
    uint32_t vertexCount;
    uint32_t indexCount;
    // Followed by vertex and index data in buffer
};

} // namespace voxceleron 