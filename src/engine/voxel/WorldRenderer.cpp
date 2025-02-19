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
    , graphicsPipeline(VK_NULL_HANDLE)
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

    // Create pipeline layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);  // Model matrix

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    std::cout << "WorldRenderer: Creating pipeline layout..." << std::endl;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "WorldRenderer: Failed to create pipeline layout" << std::endl;
        return false;
    }

    // Create graphics pipeline
    std::cout << "WorldRenderer: Creating graphics pipeline..." << std::endl;
    VkShaderModule vertShaderModule = createShaderModule("shaders/basic.vert.spv");
    VkShaderModule fragShaderModule = createShaderModule("shaders/basic.frag.spv");
    
    if (!vertShaderModule || !fragShaderModule) {
        std::cerr << "WorldRenderer: Failed to create shader modules" << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    // Vertex input state
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(float) * 8; // pos(3) + normal(3) + uv(2)
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = 0;
    // Normal
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = sizeof(float) * 3;
    // UV
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = sizeof(float) * 6;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth and stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = context->getRenderPass();
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        std::cerr << "WorldRenderer: Failed to create graphics pipeline" << std::endl;
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        return false;
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    if (!createDebugResources()) {
        std::cerr << "WorldRenderer: Failed to create debug resources" << std::endl;
        return false;
    }

    std::cout << "WorldRenderer: Initialization complete" << std::endl;
    return true;
}

VkShaderModule WorldRenderer::createShaderModule(const std::string& filename) {
    std::cout << "WorldRenderer: Loading shader " << filename << std::endl;
    
    // Read shader file
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "WorldRenderer: Failed to open shader file: " << filename << std::endl;
        return VK_NULL_HANDLE;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    // Create shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "WorldRenderer: Failed to create shader module for " << filename << std::endl;
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

void WorldRenderer::cleanup() {
    std::cout << "WorldRenderer: Starting cleanup..." << std::endl;

    cleanupDebugResources();

    if (device != VK_NULL_HANDLE) {
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
    }

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
        std::cout << "WorldRenderer: Skipping invisible or null node" << std::endl;
        return;
    }

    // Validate pipeline layout
    if (pipelineLayout == VK_NULL_HANDLE) {
        std::cerr << "WorldRenderer: Pipeline layout is null" << std::endl;
        return;
    }

    // Try to find mesh data
    const auto& meshes = node.node->getMeshes();
    if (meshes.empty()) {
        std::cout << "WorldRenderer: Node has no meshes" << std::endl;
        return;
    }

    if (!meshes[0].vertexBuffer || !meshes[0].indexBuffer) {
        std::cout << "WorldRenderer: Mesh buffers are null" << std::endl;
        return;
    }

    const auto& mesh = meshes[0];
    if (mesh.vertexCount == 0 || mesh.indexCount == 0) {
        std::cout << "WorldRenderer: Mesh has no vertices or indices" << std::endl;
        return;
    }

    std::cout << "WorldRenderer: Drawing mesh with " << mesh.vertexCount << " vertices and "
              << mesh.indexCount << " indices" << std::endl;

    // Bind pipeline and vertex/index buffers
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkDeviceSize offset = 0;
    VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Calculate and push model matrix
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(node.node->position));
    model = glm::scale(model, glm::vec3(node.node->size));
    
    // Push model matrix as push constant
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                      0, sizeof(glm::mat4), &model);

    // Draw the mesh
    vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
    
    std::cout << "WorldRenderer: Successfully recorded draw commands for node" << std::endl;
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