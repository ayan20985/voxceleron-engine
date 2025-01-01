#ifndef RENDERER_H
#define RENDERER_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <optional>
#include <array>
#include <deque>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include "World.h"
#include <chrono>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "Camera.h"
#include <mutex>

// Forward declaration
class Engine;

// Performance history constants
const size_t ONE_MINUTE_SAMPLES = 60;  // 1 sample per second for 1 minute
const size_t FIVE_MINUTE_SAMPLES = 300;  // 1 sample per second for 5 minutes

struct PerformanceMetrics {
    std::deque<float> fpsHistory1Min;
    std::deque<float> upsHistory1Min;
    std::deque<float> fpsHistory5Min;
    std::deque<float> upsHistory5Min;
    float minFPS = std::numeric_limits<float>::max();
    float maxFPS = 0.0f;
    float avgFPS = 0.0f;
    float minUPS = std::numeric_limits<float>::max();
    float maxUPS = 0.0f;
    float avgUPS = 0.0f;
    uint32_t totalFaces = 0;
    uint32_t totalVoxels = 0;
    
    // Memory usage metrics
    size_t ramUsageMB = 0;
    size_t vramUsageMB = 0;
    std::deque<size_t> ramHistory1Min;
    std::deque<size_t> vramHistory1Min;
    size_t peakRAM = 0;
    size_t peakVRAM = 0;
    
    void updateMemoryUsage(size_t ram, size_t vram) {
        ramUsageMB = ram;
        vramUsageMB = vram;
        
        // Update history
        ramHistory1Min.push_back(ram);
        vramHistory1Min.push_back(vram);
        if (ramHistory1Min.size() > ONE_MINUTE_SAMPLES) {
            ramHistory1Min.pop_front();
        }
        if (vramHistory1Min.size() > ONE_MINUTE_SAMPLES) {
            vramHistory1Min.pop_front();
        }
        
        // Update peaks
        peakRAM = std::max(peakRAM, ram);
        peakVRAM = std::max(peakVRAM, vram);
    }
    
    void updateFPS(float fps) {
        // Update 1-minute history
        fpsHistory1Min.push_back(fps);
        if (fpsHistory1Min.size() > ONE_MINUTE_SAMPLES) {
            fpsHistory1Min.pop_front();
        }
        
        // Update 5-minute history
        fpsHistory5Min.push_back(fps);
        if (fpsHistory5Min.size() > FIVE_MINUTE_SAMPLES) {
            fpsHistory5Min.pop_front();
        }
        
        // Update min/max/avg
        minFPS = std::min(minFPS, fps);
        maxFPS = std::max(maxFPS, fps);
        avgFPS = 0.0f;
        for (float f : fpsHistory1Min) {
            avgFPS += f;
        }
        avgFPS /= fpsHistory1Min.size();
    }
    
    void updateUPS(float ups) {
        // Update 1-minute history
        upsHistory1Min.push_back(ups);
        if (upsHistory1Min.size() > ONE_MINUTE_SAMPLES) {
            upsHistory1Min.pop_front();
        }
        
        // Update 5-minute history
        upsHistory5Min.push_back(ups);
        if (upsHistory5Min.size() > FIVE_MINUTE_SAMPLES) {
            upsHistory5Min.pop_front();
        }
        
        // Update min/max/avg
        minUPS = std::min(minUPS, ups);
        maxUPS = std::max(maxUPS, ups);
        avgUPS = 0.0f;
        for (float u : upsHistory1Min) {
            avgUPS += u;
        }
        avgUPS /= upsHistory1Min.size();
    }

    // Helper functions to convert deque to vector for plotting
    std::vector<float> getFPSHistory1MinVector() const {
        return std::vector<float>(fpsHistory1Min.begin(), fpsHistory1Min.end());
    }

    std::vector<float> getUPSHistory1MinVector() const {
        return std::vector<float>(upsHistory1Min.begin(), upsHistory1Min.end());
    }

    std::vector<float> getFPSHistory5MinVector() const {
        return std::vector<float>(fpsHistory5Min.begin(), fpsHistory5Min.end());
    }

    std::vector<float> getUPSHistory5MinVector() const {
        return std::vector<float>(upsHistory5Min.begin(), upsHistory5Min.end());
    }
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Normal
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        return attributeDescriptions;
    }
};

// Push constants for MVP matrix and lighting
struct PushConstants {
    glm::mat4 mvp;  // Model-View-Projection matrix
    glm::mat4 model; // Model matrix for world space transforms
    glm::vec3 lightDir; // Directional light vector
    float padding; // Padding for alignment
};

class Renderer {
public:
    static const int MAX_FRAMES_IN_FLIGHT = 2;  // Number of frames to process concurrently

    Renderer();
    ~Renderer();

    void init(VkInstance instance, VkSurfaceKHR surface, GLFWwindow* window);
    void draw();
    void cleanup();
    
    // World management
    void setWorld(World* world) { this->world = world; }
    void updateWorldMesh();

    // Set engine reference
    void setEngine(Engine* eng) { engine = eng; }

    // Camera management
    void initCamera() { camera = std::make_unique<Camera>(window); }
    void updateCamera(float deltaTime) { if (camera) camera->update(deltaTime); }
    void handleMouseMovement(float xoffset, float yoffset) { if (camera) camera->handleMouseMovement(xoffset, yoffset); }

    // Statistics getters
    float getFPS() const { return fps; }
    uint32_t getTotalFaces() const { return totalFaces; }
    uint32_t getCulledFaces() const { return culledFaces; }
    uint32_t getTotalVoxels() const { return totalVoxels; }

    // ImGui access functions
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
    VkRenderPass getRenderPass() const { return renderPass; }

    // Camera access
    Camera* getCamera() const { return camera.get(); }

private:
    // Engine reference
    Engine* engine;

    // Vulkan objects
    VkInstance instance;
    VkSurfaceKHR surface;
    GLFWwindow* window;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    
    // Vertex buffer
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    std::vector<Vertex> vertices;
    
    // World reference
    World* world;

    // Camera
    std::unique_ptr<Camera> camera;

    // Depth buffer resources
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // Helper functions
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createVertexBuffer();
    void updateVertexBuffer();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void createDescriptorPool();
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    
    // Memory allocation helper
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    // Utility functions
    std::vector<char> readFile(const std::string& filename);

    // Performance metrics
    std::chrono::steady_clock::time_point lastFrameTime;
    float fps;
    uint32_t frameCount;
    uint32_t totalFaces;
    uint32_t culledFaces;
    uint32_t totalVoxels;
    void updatePerformanceMetrics();
    void calculateStatistics();
    PerformanceMetrics metrics;  // New performance metrics tracking

    // ImGui resources
    VkDescriptorPool descriptorPool;

    // Command buffer helpers
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Depth buffer related functions
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    void createDepthResources();
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    // Multiple frames in flight resources
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    // Pipeline cache
    VkPipelineCache pipelineCache;

    std::mutex meshUpdateMutex;  // Mutex for synchronizing mesh updates
};

#endif // RENDERER_H

