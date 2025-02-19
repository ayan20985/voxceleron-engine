#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>

namespace voxceleron {

class Window;
class VulkanContext;

enum class SwapChainState {
    UNINITIALIZED,
    READY,
    ERROR,
    OUT_OF_DATE
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain {
public:
    SwapChain(VulkanContext* context, VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE);
    ~SwapChain();

    bool initialize(Window* window);
    void cleanup();
    bool recreate(Window* window);
    bool isValid() const { return swapChain != VK_NULL_HANDLE; }
    bool recreateIfNeeded();
    void waitIdle();

    // Getters
    VkSwapchainKHR getHandle() const { return swapChain; }
    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews; }
    const std::vector<VkImage>& getImages() const { return images; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }
    SwapChainState getState() const { return state; }

    // Error handling
    void setError(const std::string& message) { lastErrorMessage = message; state = SwapChainState::ERROR; }
    const std::string& getLastError() const { return lastErrorMessage; }

private:
    VulkanContext* context;
    Window* window;
    SwapChainState state;
    VkSwapchainKHR oldSwapChain;
    std::string lastErrorMessage;
    
    VkSwapchainKHR swapChain;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkFormat imageFormat;
    VkExtent2D extent;
    VkRenderPass renderPass;

    // Helper functions
    bool checkSurfaceSupport();
    bool checkSurfaceFormats();
    bool checkPresentModes();
    bool createSwapChain();
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window);
    bool createImageViews();
    bool createFramebuffers();
    bool createRenderPass();

    // Prevent copying
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
};

} // namespace voxceleron 