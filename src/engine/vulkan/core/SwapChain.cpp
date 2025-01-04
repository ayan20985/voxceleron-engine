#include "SwapChain.h"
#include "VulkanContext.h"
#include "../../core/Window.h"
#include <iostream>
#include <algorithm>

namespace voxceleron {

SwapChain::SwapChain(VulkanContext* context) 
    : context(context)
    , swapChain(VK_NULL_HANDLE) {
    std::cout << "SwapChain: Creating swap chain instance" << std::endl;
}

SwapChain::~SwapChain() {
    std::cout << "SwapChain: Destroying swap chain instance" << std::endl;
    cleanup();
}

bool SwapChain::initialize(Window* window) {
    std::cout << "SwapChain: Starting initialization..." << std::endl;

    // Query swap chain support
    std::cout << "SwapChain: Querying swap chain support..." << std::endl;
    auto swapChainSupport = querySwapChainSupport(context->getPhysicalDevice(), context->getSurface());

    // Choose swap surface format
    std::cout << "SwapChain: Choosing surface format..." << std::endl;
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    std::cout << "SwapChain: Selected format: " << surfaceFormat.format << ", colorSpace: " << surfaceFormat.colorSpace << std::endl;

    // Choose present mode
    std::cout << "SwapChain: Choosing present mode..." << std::endl;
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    std::cout << "SwapChain: Selected present mode: " << presentMode << std::endl;

    // Choose swap extent
    std::cout << "SwapChain: Choosing swap extent..." << std::endl;
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);
    std::cout << "SwapChain: Selected extent: " << extent.width << "x" << extent.height << std::endl;

    // Choose image count
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, swapChainSupport.capabilities.maxImageCount);
    }
    std::cout << "SwapChain: Using " << imageCount << " images" << std::endl;

    // Create swap chain
    std::cout << "SwapChain: Creating swap chain..." << std::endl;
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Handle queue families
    auto indices = context->getQueueFamilyIndices();
    if (indices.graphicsFamily != indices.presentFamily) {
        std::cout << "SwapChain: Using concurrent sharing mode" << std::endl;
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        std::cout << "SwapChain: Using exclusive sharing mode" << std::endl;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(context->getDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        std::cerr << "SwapChain: Failed to create swap chain!" << std::endl;
        return false;
    }
    std::cout << "SwapChain: Created successfully" << std::endl;

    // Get swap chain images
    std::cout << "SwapChain: Retrieving swap chain images..." << std::endl;
    vkGetSwapchainImagesKHR(context->getDevice(), swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(context->getDevice(), swapChain, &imageCount, images.data());
    std::cout << "SwapChain: Retrieved " << imageCount << " images" << std::endl;

    // Store format and extent
    imageFormat = surfaceFormat.format;
    this->extent = extent;

    // Create image views
    if (!createImageViews()) {
        std::cerr << "SwapChain: Failed to create image views!" << std::endl;
        return false;
    }

    std::cout << "SwapChain: Initialization complete" << std::endl;
    return true;
}

void SwapChain::cleanup() {
    std::cout << "SwapChain: Starting cleanup..." << std::endl;

    if (!imageViews.empty()) {
        std::cout << "SwapChain: Destroying " << imageViews.size() << " image views..." << std::endl;
        for (auto imageView : imageViews) {
            vkDestroyImageView(context->getDevice(), imageView, nullptr);
        }
        imageViews.clear();
    }

    if (swapChain != VK_NULL_HANDLE) {
        std::cout << "SwapChain: Destroying swap chain..." << std::endl;
        vkDestroySwapchainKHR(context->getDevice(), swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }

    std::cout << "SwapChain: Cleanup complete" << std::endl;
}

SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // Get supported surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // Get supported present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    std::cout << "SwapChain: Found " << formatCount << " surface formats and " 
              << presentModeCount << " present modes" << std::endl;

    return details;
}

VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer SRGB format
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            std::cout << "SwapChain: Found preferred SRGB format" << std::endl;
            return availableFormat;
        }
    }

    std::cout << "SwapChain: Using first available format" << std::endl;
    return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Prefer mailbox mode (triple buffering)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            std::cout << "SwapChain: Using mailbox present mode (triple buffering)" << std::endl;
            return availablePresentMode;
        }
    }

    std::cout << "SwapChain: Using FIFO present mode (vsync)" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    window->getFramebufferSize(&width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actualExtent;
}

bool SwapChain::createImageViews() {
    std::cout << "SwapChain: Creating image views..." << std::endl;
    imageViews.resize(images.size());

    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(context->getDevice(), &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
            std::cerr << "SwapChain: Failed to create image view " << i << std::endl;
            return false;
        }
    }

    std::cout << "SwapChain: Created " << imageViews.size() << " image views" << std::endl;
    return true;
}

} // namespace voxceleron 