#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

namespace voxceleron {

class Window;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    // Initialize Vulkan
    bool initialize(Window* window);
    
    // Cleanup Vulkan resources
    void cleanup();

    // Command buffer management
    VkCommandBuffer beginSingleTimeCommands();
    bool endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Memory management
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Getters
    VkInstance getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    VkSurfaceKHR getSurface() const { return surface; }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return queueFamilyIndices; }
    uint32_t getGraphicsQueueFamily() const { return queueFamilyIndices.graphicsFamily.value(); }

private:
    // Vulkan instance and debug
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    
    // Physical device (GPU)
    VkPhysicalDevice physicalDevice;
    QueueFamilyIndices queueFamilyIndices;
    
    // Logical device and queues
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    
    // Surface
    VkSurfaceKHR surface;

    // Command pool
    VkCommandPool commandPool;

    // Helper functions
    bool createInstance();
    bool setupDebugMessenger();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface(Window* window);
    bool createCommandPool();
    
    // Validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // Prevent copying
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
};

} // namespace voxceleron 