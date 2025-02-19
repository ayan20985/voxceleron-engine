#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "../core/Camera.h"

namespace voxceleron {

class World;
class OctreeNode;

class WorldRenderer {
public:
    // Rendering settings
    struct Settings {
        float lodDistanceFactor = 2.0f;     // Distance multiplier for LOD transitions
        float cullingMargin = 1.1f;         // Margin for frustum culling (1.0 = exact)
        uint32_t maxVisibleNodes = 10000;   // Maximum number of nodes to render
        bool enableFrustumCulling = true;   // Enable/disable frustum culling
        bool enableLOD = true;              // Enable/disable LOD system
        bool enableOcclusion = true;        // Enable/disable occlusion culling
    };

    WorldRenderer();
    ~WorldRenderer();

    // Initialization
    bool initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    void cleanup();

    // Settings
    void setSettings(const Settings& settings) { this->settings = settings; }
    const Settings& getSettings() const { return settings; }

    // Rendering
    void prepareFrame(const Camera& camera, World& world);
    void recordCommands(VkCommandBuffer commandBuffer);

    // Debug visualization
    void setDebugVisualization(bool enabled) { debugVisualization = enabled; }
    bool isDebugVisualizationEnabled() const { return debugVisualization; }

    // Camera access
    const Camera* getCamera() const { return currentCamera; }

private:
    // Core components
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;  // Graphics pipeline for mesh rendering
    Settings settings;
    bool debugVisualization;
    const Camera* currentCamera;  // Current camera being used for rendering

    // Rendering data
    struct RenderNode {
        const OctreeNode* node;
        float distance;    // Distance to camera
        uint32_t lodLevel; // Actual LOD level to use
        bool isVisible;    // Whether node is visible
    };
    std::vector<RenderNode> visibleNodes;

    // Transformation matrices
    glm::mat4 viewProjection;
    glm::vec3 cameraPosition;

    // Culling and LOD
    void updateVisibleNodes(const Camera& camera, World& world);
    void frustumCullNode(const OctreeNode* node, const Camera::Frustum& frustum);
    bool isNodeVisible(const OctreeNode* node, const Camera::Frustum& frustum) const;
    uint32_t calculateLODLevel(const OctreeNode* node, float distance) const;
    float calculateNodePriority(const RenderNode& node) const;

    // Command recording
    void recordNodeCommands(VkCommandBuffer commandBuffer, const RenderNode& node);
    void recordDebugCommands(VkCommandBuffer commandBuffer);

    // Vulkan resources
    struct {
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;
        VkDescriptorSet descriptorSet;
        uint32_t vertexCount;
        uint32_t indexCount;
    } debugMesh;

    // Vulkan helpers
    bool createDebugResources();
    void cleanupDebugResources();
    VkShaderModule createShaderModule(const std::string& filename);

    // Prevent copying
    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;
};

} // namespace voxceleron 