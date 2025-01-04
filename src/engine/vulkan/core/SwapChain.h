#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace voxceleron {

class Window;
class VulkanContext;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain {
public:
    SwapChain(VulkanContext* context);
    ~SwapChain();

    bool initialize(Window* window);
    void cleanup();

    // Getters
    VkSwapchainKHR getHandle() const { return swapChain; }
    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews; }

private:
    VulkanContext* context;
    
    VkSwapchainKHR swapChain;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat imageFormat;
    VkExtent2D extent;

    // Helper functions
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window);
    bool createImageViews();

    // Prevent copying
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
};

} // namespace voxceleron 