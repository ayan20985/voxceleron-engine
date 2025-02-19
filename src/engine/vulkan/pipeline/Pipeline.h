#pragma once

#include <vulkan/vulkan.h>
#include <vector>   
#include <memory>
#include <string>
#include <fstream>
#include <array>
#include "../core/Vertex.h"
#include "../core/VulkanContext.h"
#include "../core/SwapChain.h"

namespace voxceleron {

class VulkanContext;
class SwapChain;
class Window;

class Pipeline {
public:
    enum class State {
        UNINITIALIZED,
        READY,
        ERROR,
        RECREATING
    };

    Pipeline(VulkanContext* context, SwapChain* swapChain);
    ~Pipeline();

    bool initialize();
    void cleanup();

    // Frame management
    bool beginFrame();
    bool endFrame();
    bool recreateIfNeeded();
    void waitIdle();

    // Getters
    VkCommandBuffer getCurrentCommandBuffer() const;
    uint32_t getCurrentImageIndex() const { return currentImageIndex; }
    State getState() const { return state; }
    bool isValid() const { return state == State::READY; }
    const std::string& getLastErrorMessage() const { return lastErrorMessage; }

private:
    VulkanContext* context;
    SwapChain* swapChain;
    Window* window;

    // Pipeline resources
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    // Command buffers and synchronization
    std::vector<VkCommandPool> commandPools;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkPipelineStageFlags waitStageFlags;
    
    // Frame state
    uint32_t currentFrame;
    uint32_t currentImageIndex;
    State state;
    std::string lastErrorMessage;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Buffer resources
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    // Uniform buffer for camera matrices
    struct UniformBufferObject {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    // Descriptor set resources
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    // Helper functions
    bool createUniformBuffers();
    bool createDescriptorSetLayout();
    bool createDescriptorPool();
    bool createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);
    bool createRenderPass();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createCommandPools();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createVertexBuffer();
    void setError(const std::string& message) { lastErrorMessage = message; state = State::ERROR; }

    // Helper functions for shader handling
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);

    // Prevent copying
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
};

} // namespace voxceleron 