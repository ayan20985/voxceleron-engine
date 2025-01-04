#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace voxceleron {

class VulkanContext;
class SwapChain;

class Pipeline {
public:
    Pipeline(VulkanContext* context, SwapChain* swapChain);
    ~Pipeline();

    bool initialize();
    void cleanup();
    
    // Rendering functions
    bool beginFrame();
    void endFrame();
    bool recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Getters
    VkPipeline getHandle() const { return graphicsPipeline; }
    VkPipelineLayout getLayout() const { return pipelineLayout; }
    VkRenderPass getRenderPass() const { return renderPass; }

private:
    VulkanContext* context;
    SwapChain* swapChain;

    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    
    // Command buffers
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    
    // Synchronization objects
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Helper functions
    bool createRenderPass();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // Prevent copying
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
};

} // namespace voxceleron 