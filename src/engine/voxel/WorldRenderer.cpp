#include "WorldRenderer.h"
#include "World.h"
#include "VoxelTypes.h"
#include "../core/Camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <iostream>
#include <algorithm>

namespace voxceleron {

WorldRenderer::WorldRenderer()
    : device(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , debugVisualization(false)
    , currentCamera(nullptr)
    , pipelineLayout(VK_NULL_HANDLE)
    , viewProjection(1.0f)
    , cameraPosition(0.0f) {
    std::cout << "WorldRenderer: Creating world renderer instance" << std::endl;

    // Initialize debug mesh resources
    debugMesh.vertexBuffer = VK_NULL_HANDLE;
    debugMesh.vertexMemory = VK_NULL_HANDLE;
    debugMesh.indexBuffer = VK_NULL_HANDLE;
    debugMesh.indexMemory = VK_NULL_HANDLE;
    debugMesh.descriptorSet = VK_NULL_HANDLE;
    debugMesh.vertexCount = 0;
    debugMesh.indexCount = 0;

    // Initialize default settings
    settings.lodDistanceFactor = 2.0f;
    settings.cullingMargin = 1.1f;
    settings.maxVisibleNodes = 10000;
    settings.enableFrustumCulling = true;
    settings.enableLOD = true;
    settings.enableOcclusion = true;
}

WorldRenderer::~WorldRenderer() {
    std::cout << "WorldRenderer: Destroying world renderer instance" << std::endl;
    cleanup();
}

bool WorldRenderer::initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
    std::cout << "WorldRenderer: Starting initialization..." << std::endl;
    this->device = device;
    this->physicalDevice = physicalDevice;

    if (!createDebugResources()) {
        std::cerr << "WorldRenderer: Failed to create debug resources" << std::endl;
        return false;
    }

    std::cout << "WorldRenderer: Initialization complete" << std::endl;
    return true;
}

void WorldRenderer::cleanup() {
    std::cout << "WorldRenderer: Starting cleanup..." << std::endl;

    cleanupDebugResources();

    device = VK_NULL_HANDLE;
    physicalDevice = VK_NULL_HANDLE;
    std::cout << "WorldRenderer: Cleanup complete" << std::endl;
}

void WorldRenderer::prepareFrame(const Camera& camera, World& world) {
    // Update camera data
    currentCamera = &camera;
    viewProjection = camera.getProjectionMatrix(camera.getFov()) * camera.getViewMatrix();
    cameraPosition = camera.getPosition();

    // Update visible nodes
    updateVisibleNodes(camera, world);
}

void WorldRenderer::recordCommands(VkCommandBuffer commandBuffer) {
    // Sort nodes by distance (back-to-front for transparency)
    std::sort(visibleNodes.begin(), visibleNodes.end(),
        [](const RenderNode& a, const RenderNode& b) {
            return a.distance > b.distance;
        });

    // Record commands for each visible node
    for (const auto& node : visibleNodes) {
        if (node.isVisible) {
            recordNodeCommands(commandBuffer, node);
        }
    }

    // Record debug visualization if enabled
    if (debugVisualization) {
        recordDebugCommands(commandBuffer);
    }
}

void WorldRenderer::updateVisibleNodes(const Camera& camera, World& world) {
    visibleNodes.clear();

    if (!world.getRoot()) {
        return;
    }

    // Get camera frustum for culling
    const auto& frustum = camera.getFrustum();

    // Start with root node
    frustumCullNode(world.getRoot(), frustum);

    // Sort nodes by priority
    std::sort(visibleNodes.begin(), visibleNodes.end(),
        [this](const RenderNode& a, const RenderNode& b) {
            return calculateNodePriority(a) > calculateNodePriority(b);
        });

    // Limit number of visible nodes
    if (visibleNodes.size() > settings.maxVisibleNodes) {
        visibleNodes.resize(settings.maxVisibleNodes);
    }
}

void WorldRenderer::frustumCullNode(const OctreeNode* node, const Camera::Frustum& frustum) {
    if (!node) return;

    // Calculate node bounds
    glm::vec3 center = glm::vec3(node->position) + glm::vec3(node->size / 2.0f);
    float radius = node->size * 0.5f * settings.cullingMargin;

    // Calculate distance to camera
    glm::vec3 toCenter = center - cameraPosition;
    float distance = glm::length(toCenter);

    // Check if node is visible
    bool visible = !settings.enableFrustumCulling || isNodeVisible(node, frustum);

    if (visible) {
        // Calculate appropriate LOD level
        uint32_t lodLevel = settings.enableLOD ? 
            calculateLODLevel(node, distance) : node->level;

        // Add to visible nodes
        visibleNodes.push_back({
            node,
            distance,
            lodLevel,
            true
        });

        // Recursively check children if this isn't a leaf and we need more detail
        if (!node->isLeaf && lodLevel > node->level) {
            for (uint8_t i = 0; i < 8; ++i) {
                if (node->childMask & (1 << i)) {
                    frustumCullNode(node->nodeData.internal.children[i].get(), frustum);
                }
            }
        }
    }
}

bool WorldRenderer::isNodeVisible(const OctreeNode* node, const Camera::Frustum& frustum) const {
    // Calculate node bounds
    glm::vec3 center = glm::vec3(node->position) + glm::vec3(node->size / 2.0f);
    float radius = node->size * 0.5f * settings.cullingMargin;

    // Check against each frustum plane
    for (int i = 0; i < 6; ++i) {
        const auto& plane = frustum.planes[i];
        float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false;
        }
    }

    return true;
}

uint32_t WorldRenderer::calculateLODLevel(const OctreeNode* node, float distance) const {
    // Base LOD on distance and node size
    float factor = distance / (node->size * settings.lodDistanceFactor);
    uint32_t level = static_cast<uint32_t>(glm::log2(factor));
    return glm::clamp(level, 0u, 8u); // Using 8 as MAX_LEVEL
}

float WorldRenderer::calculateNodePriority(const RenderNode& node) const {
    // Priority based on distance and size
    float sizeFactor = node.node->size / static_cast<float>(1 << 8); // Using 8 as MAX_LEVEL
    return sizeFactor / (node.distance + 1.0f);
}

void WorldRenderer::recordNodeCommands(VkCommandBuffer commandBuffer, const RenderNode& node) {
    // Skip if node has no mesh data
    if (!node.node || !node.isVisible) {
        return;
    }

    // Get mesh data from the node
    if (node.node->vertexCount == 0 || node.node->indexCount == 0) {
        return;
    }

    // Bind vertex and index buffers
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &node.node->meshBuffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, node.node->meshBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Calculate model matrix
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(node.node->position));
    model = glm::scale(model, glm::vec3(node.node->size));

    // Update push constants with transform
    vkCmdPushConstants(commandBuffer, pipelineLayout, 
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

    // Draw the node
    vkCmdDrawIndexed(commandBuffer, node.node->indexCount, 1, 0, 0, 0);
}

void WorldRenderer::recordDebugCommands(VkCommandBuffer commandBuffer) {
    if (!debugMesh.vertexBuffer || !debugMesh.indexBuffer) {
        return;
    }

    // Bind debug mesh buffers
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &debugMesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, debugMesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Draw debug visualization for each visible node
    for (const auto& node : visibleNodes) {
        if (node.isVisible && node.node) {
            // Update push constants with node transform
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(node.node->position));
            model = glm::scale(model, glm::vec3(node.node->size));
            vkCmdPushConstants(commandBuffer, pipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

            // Draw debug mesh
            vkCmdDrawIndexed(commandBuffer, debugMesh.indexCount, 1, 0, 0, 0);
        }
    }
}

bool WorldRenderer::createDebugResources() {
    // Create cube mesh for debug visualization
    std::vector<float> vertices = {
        // Front face
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        // Back face
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
    };

    std::vector<uint32_t> indices = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        4, 0, 3, 3, 7, 4,
        // Right face
        1, 5, 6, 6, 2, 1,
        // Top face
        3, 2, 6, 6, 7, 3,
        // Bottom face
        4, 5, 1, 1, 0, 4
    };

    debugMesh.vertexCount = static_cast<uint32_t>(vertices.size()) / 3;
    debugMesh.indexCount = static_cast<uint32_t>(indices.size());

    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = vertices.size() * sizeof(float);
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &vertexBufferInfo, nullptr, &debugMesh.vertexBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create debug mesh vertex buffer" << std::endl;
        return false;
    }

    // Create index buffer
    VkBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = indices.size() * sizeof(uint32_t);
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &indexBufferInfo, nullptr, &debugMesh.indexBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create debug mesh index buffer" << std::endl;
        return false;
    }

    return true;
}

void WorldRenderer::cleanupDebugResources() {
    if (debugMesh.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, debugMesh.vertexBuffer, nullptr);
        debugMesh.vertexBuffer = VK_NULL_HANDLE;
    }

    if (debugMesh.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, debugMesh.vertexMemory, nullptr);
        debugMesh.vertexMemory = VK_NULL_HANDLE;
    }

    if (debugMesh.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, debugMesh.indexBuffer, nullptr);
        debugMesh.indexBuffer = VK_NULL_HANDLE;
    }

    if (debugMesh.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, debugMesh.indexMemory, nullptr);
        debugMesh.indexMemory = VK_NULL_HANDLE;
    }
}

} // namespace voxceleron 