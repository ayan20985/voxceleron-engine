#include "SwapChain.h"
#include "engine/core/Window.h"
#include "VulkanContext.h"
#include <iostream>
#include <algorithm>

namespace voxceleron {

SwapChain::SwapChain(VulkanContext* context)
    : context(context)
    , window(nullptr)
    , state(SwapChainState::UNINITIALIZED)
    , swapChain(VK_NULL_HANDLE)
    , imageFormat(VK_FORMAT_UNDEFINED)
    , extent{0, 0}
    , renderPass(VK_NULL_HANDLE) {
    std::cout << "SwapChain: Creating swap chain instance" << std::endl;
}

SwapChain::~SwapChain() {
    std::cout << "SwapChain: Destroying swap chain instance" << std::endl;
    cleanup();
}

bool SwapChain::initialize(Window* window) {
    std::cout << "SwapChain: Starting initialization..." << std::endl;
    this->window = window;

    // Verify that we have a valid surface
    VkSurfaceKHR surface = context->getSurface();
    if (surface == VK_NULL_HANDLE) {
        setError("No valid surface available from VulkanContext");
        return false;
    }
    std::cout << "SwapChain: Using surface: " << surface << std::endl;

    if (!checkSurfaceSupport()) {
        setError("Failed to check surface support");
        return false;
    }

    if (!checkSurfaceFormats()) {
        setError("Failed to check surface formats");
        return false;
    }

    if (!checkPresentModes()) {
        setError("Failed to check present modes");
        return false;
    }

    if (!createSwapChain()) {
        setError("Failed to create swap chain");
        return false;
    }

    if (!createImageViews()) {
        setError("Failed to create image views");
        return false;
    }

    if (!createRenderPass()) {
        setError("Failed to create render pass");
        return false;
    }

    if (!createFramebuffers()) {
        setError("Failed to create framebuffers");
        return false;
    }

    state = SwapChainState::READY;
    std::cout << "SwapChain: Initialization complete" << std::endl;
    return true;
}

void SwapChain::cleanup() {
    if (context) {
        VkDevice device = context->getDevice();
        if (device == VK_NULL_HANDLE) {
            return;
        }
        
        // Wait for the device to finish operations
        vkDeviceWaitIdle(device);

        // Wait for all pending operations
        vkDeviceWaitIdle(device);
    
        // First, reset all framebuffer handles to make sure we have the latest state
        std::cout << "SwapChain: Starting framebuffer cleanup, count: " << framebuffers.size() << std::endl;
        std::vector<VkFramebuffer> framebuffersToDestroy = framebuffers;
        framebuffers.clear();
    
        // Now destroy all framebuffers
        for (auto& framebuffer : framebuffersToDestroy) {
            if (framebuffer != VK_NULL_HANDLE) {
                std::cout << "SwapChain: Destroying framebuffer: " << framebuffer << std::endl;
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
        }
        
        // Ensure all framebuffer operations are complete
        vkDeviceWaitIdle(device);
        std::cout << "SwapChain: All framebuffers destroyed" << std::endl;
    
        // Now destroy the render pass
        if (renderPass != VK_NULL_HANDLE) {
            std::cout << "SwapChain: Destroying render pass: " << renderPass << std::endl;
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        // Destroy image views
        for (auto& imageView : imageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }
        imageViews.clear();

        // Destroy swap chain
        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
        }

        // Clear image handles (they are destroyed with the swap chain)
        images.clear();
    }

    state = SwapChainState::UNINITIALIZED;
}

bool SwapChain::recreate(Window* window) {
    std::cout << "SwapChain: Recreating swap chain..." << std::endl;
    
    if (!context) {
        std::cerr << "SwapChain: No valid context for recreation" << std::endl;
        return false;
    }

    VkDevice device = context->getDevice();
    if (device == VK_NULL_HANDLE) {
        std::cerr << "SwapChain: No valid device for recreation" << std::endl;
        return false;
    }
    
    // Store old framebuffers for cleanup
    std::vector<VkFramebuffer> oldFramebuffers = framebuffers;
    framebuffers.clear();

    // Store old render pass
    VkRenderPass oldRenderPass = renderPass;
    renderPass = VK_NULL_HANDLE;

    // Wait for device to be idle before cleanup
    vkDeviceWaitIdle(device);

    // Cleanup old framebuffers
    for (auto& fb : oldFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            std::cout << "SwapChain: Cleaning up old framebuffer: " << fb << std::endl;
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }

    // Cleanup old render pass
    if (oldRenderPass != VK_NULL_HANDLE) {
        std::cout << "SwapChain: Cleaning up old render pass: " << oldRenderPass << std::endl;
        vkDestroyRenderPass(device, oldRenderPass, nullptr);
    }

    // Initialize with new window
    bool result = initialize(window);
    if (result) {
        std::cout << "SwapChain: Successfully recreated with " << framebuffers.size() << " new framebuffers" << std::endl;
    } else {
        std::cerr << "SwapChain: Failed to recreate swap chain" << std::endl;
    }
    return result;
}

bool SwapChain::recreateIfNeeded() {
    if (state == SwapChainState::OUT_OF_DATE || state == SwapChainState::ERROR) {
        return recreate(window);
    }
    return true;
}

void SwapChain::waitIdle() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context->getDevice());
    }
}

bool SwapChain::checkSurfaceSupport() {
    VkPhysicalDevice physicalDevice = context->getPhysicalDevice();
    VkSurfaceKHR surface = context->getSurface();
    uint32_t queueFamilyIndex = context->getGraphicsQueueFamily();

    if (surface == VK_NULL_HANDLE) {
        setError("Invalid surface handle");
        return false;
    }

    VkBool32 supported = VK_FALSE;
    VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
        physicalDevice, queueFamilyIndex, surface, &supported);

    if (result != VK_SUCCESS || !supported) {
        setError("Physical device does not support surface presentation");
        return false;
    }

    return true;
}

bool SwapChain::checkSurfaceFormats() {
    VkPhysicalDevice physicalDevice = context->getPhysicalDevice();
    VkSurfaceKHR surface = context->getSurface();

    if (surface == VK_NULL_HANDLE) {
        setError("Invalid surface handle");
        return false;
    }

    uint32_t formatCount;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &formatCount, nullptr);

    if (result != VK_SUCCESS || formatCount == 0) {
        setError("Failed to get surface formats");
        return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &formatCount, formats.data());

    if (result != VK_SUCCESS) {
        setError("Failed to get surface formats");
        return false;
    }

    // Choose the first available format for now
    // TODO: Add proper format selection based on preferences
    imageFormat = formats[0].format;
    return true;
}

bool SwapChain::checkPresentModes() {
    VkPhysicalDevice physicalDevice = context->getPhysicalDevice();
    VkSurfaceKHR surface = context->getSurface();

    if (surface == VK_NULL_HANDLE) {
        setError("Invalid surface handle");
        return false;
    }

    uint32_t presentModeCount;
    VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &presentModeCount, nullptr);

    if (result != VK_SUCCESS || presentModeCount == 0) {
        setError("Failed to get present modes");
        return false;
    }

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &presentModeCount, presentModes.data());

    if (result != VK_SUCCESS) {
        setError("Failed to get present modes");
        return false;
    }

    return true;
}

bool SwapChain::createSwapChain() {
    VkPhysicalDevice physicalDevice = context->getPhysicalDevice();
    VkSurfaceKHR surface = context->getSurface();

    if (surface == VK_NULL_HANDLE) {
        setError("Invalid surface handle");
        return false;
    }

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, surface, &capabilities);

    if (result != VK_SUCCESS) {
        setError("Failed to get surface capabilities");
        return false;
    }

    // Choose extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        int width, height;
        window->getFramebufferSize(&width, &height);

        extent.width = std::clamp(static_cast<uint32_t>(width),
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(height),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }

    // Choose image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    // Get queue family indices
    uint32_t graphicsFamily = context->getGraphicsQueueFamily();
    uint32_t presentFamily = context->getQueueFamilyIndices().presentFamily.value();

    // Create swap chain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = imageFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Handle queue families
    if (graphicsFamily != presentFamily) {
        uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = chooseSwapPresentMode(querySwapChainSupport(physicalDevice, surface).presentModes);
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(context->getDevice(), &createInfo, nullptr, &swapChain);
    if (result != VK_SUCCESS) {
        setError("Failed to create swap chain");
        return false;
    }

    // Get swap chain images
    result = vkGetSwapchainImagesKHR(context->getDevice(), swapChain, &imageCount, nullptr);
    if (result != VK_SUCCESS) {
        setError("Failed to get swap chain images");
        return false;
    }

    images.resize(imageCount);
    result = vkGetSwapchainImagesKHR(context->getDevice(), swapChain, &imageCount, images.data());
    if (result != VK_SUCCESS) {
        setError("Failed to get swap chain images");
        return false;
    }

    return true;
}

bool SwapChain::createImageViews() {
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

        VkResult result = vkCreateImageView(context->getDevice(), &createInfo, nullptr, &imageViews[i]);
        if (result != VK_SUCCESS) {
            setError("Failed to create image views");
            return false;
        }
    }

    return true;
}

bool SwapChain::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(context->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }

    return true;
}

bool SwapChain::createFramebuffers() {
    std::cout << "SwapChain: Creating framebuffers for " << imageViews.size() << " image views" << std::endl;

    // First destroy any existing framebuffers
    if (!framebuffers.empty()) {
        std::cout << "SwapChain: Cleaning up " << framebuffers.size() << " existing framebuffers" << std::endl;
        for (auto& framebuffer : framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(context->getDevice(), framebuffer, nullptr);
            }
        }
        framebuffers.clear();
    }

    // Wait for any previous operations to complete
    vkDeviceWaitIdle(context->getDevice());

    // Create new framebuffers
    framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &imageViews[i];
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(context->getDevice(), &framebufferInfo, nullptr, &framebuffers[i]);
        if (result != VK_SUCCESS) {
            std::cerr << "SwapChain: Failed to create framebuffer " << i << ", error: " << result << std::endl;
            return false;
        }
        std::cout << "SwapChain: Created framebuffer " << i << ": " << framebuffers[i] << std::endl;
    }

    std::cout << "SwapChain: Successfully created " << framebuffers.size() << " framebuffers" << std::endl;
    return true;
}

SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // Get surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // Get present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Prefer mailbox mode (triple buffering) if available
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            std::cout << "SwapChain: Using mailbox present mode" << std::endl;
            return availablePresentMode;
        }
    }

    // Fallback to FIFO (vsync) which is guaranteed to be available
    std::cout << "SwapChain: Using FIFO present mode" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
}

} // namespace voxceleron 