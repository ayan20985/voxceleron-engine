#include "VulkanContext.h"
#include "../../core/Window.h"
#include <iostream>
#include <set>

namespace voxceleron {

VulkanContext::VulkanContext()
    : instance(VK_NULL_HANDLE)
    , debugMessenger(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , device(VK_NULL_HANDLE)
    , graphicsQueue(VK_NULL_HANDLE)
    , presentQueue(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , commandPool(VK_NULL_HANDLE)
    , enableValidationLayers(true) {
    std::cout << "Vulkan: Creating Vulkan context" << std::endl;
}

VulkanContext::~VulkanContext() {
    std::cout << "Vulkan: Destroying Vulkan context" << std::endl;
    cleanup();
}

bool VulkanContext::initialize(Window* window) {
    std::cout << "VulkanContext: Starting initialization..." << std::endl;

    if (!createInstance()) {
        std::cerr << "VulkanContext: Failed to create instance!" << std::endl;
        return false;
    }
    std::cout << "VulkanContext: Created instance: " << instance << std::endl;

    if (enableValidationLayers && !setupDebugMessenger()) {
        std::cerr << "VulkanContext: Failed to setup debug messenger!" << std::endl;
        return false;
    }
    std::cout << "VulkanContext: Setup debug messenger" << std::endl;

    // Create surface after instance is created
    if (!createSurface(window)) {
        std::cerr << "VulkanContext: Failed to create surface!" << std::endl;
        return false;
    }
    std::cout << "VulkanContext: Created surface: " << surface << std::endl;

    if (!pickPhysicalDevice()) {
        std::cerr << "VulkanContext: Failed to find a suitable GPU!" << std::endl;
        return false;
    }
    std::cout << "VulkanContext: Selected physical device" << std::endl;

    if (!createLogicalDevice()) {
        std::cerr << "VulkanContext: Failed to create logical device!" << std::endl;
        return false;
    }
    std::cout << "VulkanContext: Created logical device" << std::endl;

    if (!createCommandPool()) {
        std::cerr << "VulkanContext: Failed to create command pool!" << std::endl;
        return false;
    }
    std::cout << "VulkanContext: Created command pool" << std::endl;

    std::cout << "VulkanContext: Initialization complete" << std::endl;
    return true;
}

void VulkanContext::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        // Destroy command pool first
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        // Destroy device
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    // Destroy surface after device
    if (instance != VK_NULL_HANDLE && surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    if (instance != VK_NULL_HANDLE && debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(instance, debugMessenger, nullptr);
        }
        debugMessenger = VK_NULL_HANDLE;
    }

    // Destroy instance last
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

bool VulkanContext::createInstance() {
    std::cout << "VulkanContext: Creating instance..." << std::endl;

    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Voxceleron Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Voxceleron";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get required extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    std::cout << "VulkanContext: Required extensions:" << std::endl;
    for (const auto& extension : extensions) {
        std::cout << "  - " << extension << std::endl;
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validation layers
    if (enableValidationLayers) {
        std::cout << "VulkanContext: Enabling validation layers:" << std::endl;
        for (const auto& layer : validationLayers) {
            std::cout << "  - " << layer << std::endl;
        }
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    // Create instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to create instance! Error code: " << result << std::endl;
        return false;
    }

    std::cout << "VulkanContext: Created instance successfully: " << instance << std::endl;
    return true;
}

bool VulkanContext::setupDebugMessenger() {
    if (!enableValidationLayers) return true;

    std::cout << "Vulkan: Setting up debug messenger..." << std::endl;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                   void* pUserData) -> VKAPI_ATTR VkBool32 {
        std::cerr << "Vulkan Validation: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    };

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr || func(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        std::cerr << "Vulkan: Failed to set up debug messenger!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanContext::pickPhysicalDevice() {
    std::cout << "Vulkan: Picking physical device..." << std::endl;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        std::cerr << "Vulkan: Failed to find GPUs with Vulkan support!" << std::endl;
        return false;
    }

    std::cout << "Vulkan: Found " << deviceCount << " device(s) with Vulkan support" << std::endl;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Pick first suitable device
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // Check for required extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        std::cout << "Vulkan: Checking device: " << deviceProperties.deviceName << std::endl;
        if (!requiredExtensions.empty()) {
            std::cout << "Vulkan: Device missing required extensions" << std::endl;
            continue;
        }

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            std::cout << "Vulkan: Selected discrete GPU: " << deviceProperties.deviceName << std::endl;
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::cout << "Vulkan: No discrete GPU found, using first available device" << std::endl;
        physicalDevice = devices[0];
        
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        std::cout << "Vulkan: Selected device: " << deviceProperties.deviceName << std::endl;
    }

    return physicalDevice != VK_NULL_HANDLE;
}

bool VulkanContext::createLogicalDevice() {
    std::cout << "Vulkan: Creating logical device..." << std::endl;

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    std::cout << "Vulkan: Found " << queueFamilyCount << " queue families" << std::endl;

    // Find graphics queue family
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueFamilyIndices.graphicsFamily = i;
            std::cout << "Vulkan: Graphics queue family found at index " << i << std::endl;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            queueFamilyIndices.presentFamily = i;
            std::cout << "Vulkan: Present queue family found at index " << i << std::endl;
        }

        if (queueFamilyIndices.isComplete()) {
            break;
        }
    }

    // Create logical device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Device features
    VkPhysicalDeviceFeatures deviceFeatures{};

    // Create info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    // Enable device extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    std::cout << "Vulkan: Enabling device extensions:" << std::endl;
    for (const auto& extension : deviceExtensions) {
        std::cout << "  - " << extension << std::endl;
    }

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    // Create device
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "Vulkan: Failed to create logical device!" << std::endl;
        return false;
    }

    // Get queue handles
    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
    std::cout << "Vulkan: Retrieved queue handles" << std::endl;

    return true;
}

bool VulkanContext::createSurface(Window* window) {
    std::cout << "VulkanContext: Creating surface..." << std::endl;
    
    if (glfwCreateWindowSurface(instance, window->getHandle(), nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to create window surface!" << std::endl;
        return false;
    }

    std::cout << "VulkanContext: Created surface successfully: " << surface << std::endl;
    return true;
}

bool VulkanContext::createCommandPool() {
    std::cout << "VulkanContext: Creating command pool..." << std::endl;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to create command pool!" << std::endl;
        return false;
    }

    std::cout << "VulkanContext: Created command pool successfully" << std::endl;
    return true;
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    std::cout << "VulkanContext: Beginning single time commands..." << std::endl;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to allocate command buffer!" << std::endl;
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to begin command buffer!" << std::endl;
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return VK_NULL_HANDLE;
    }

    std::cout << "VulkanContext: Command buffer ready for recording" << std::endl;
    return commandBuffer;
}

bool VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    std::cout << "VulkanContext: Ending single time commands..." << std::endl;

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to end command buffer!" << std::endl;
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to create fence!" << std::endl;
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to submit command buffer!" << std::endl;
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }

    if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        std::cerr << "VulkanContext: Failed to wait for fence!" << std::endl;
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return false;
    }

    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

    std::cout << "VulkanContext: Command buffer executed successfully" << std::endl;
    return true;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("VulkanContext: Failed to find suitable memory type!");
}

} // namespace voxceleron 