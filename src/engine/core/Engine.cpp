#include "Engine.h"
#include "Window.h"
#include "../vulkan/core/VulkanContext.h"
#include "../vulkan/core/SwapChain.h"
#include "../vulkan/pipeline/Pipeline.h"
#include <iostream>

namespace voxceleron {

Engine::Engine() : isRunning(false) {
    std::cout << "Engine: Creating engine instance" << std::endl;
}

Engine::~Engine() {
    std::cout << "Engine: Destroying engine instance" << std::endl;
    cleanup();
}

bool Engine::initialize() {
    std::cout << "Engine: Initializing..." << std::endl;

    // Create window
    std::cout << "Engine: Creating window..." << std::endl;
    window = std::make_unique<Window>();
    if (!window->initialize()) {
        std::cerr << "Engine: Failed to create window!" << std::endl;
        return false;
    }
    std::cout << "Engine: Window created successfully" << std::endl;

    // Initialize Vulkan
    std::cout << "Engine: Initializing Vulkan..." << std::endl;
    vulkanContext = std::make_unique<VulkanContext>();
    if (!vulkanContext->initialize(window.get())) {
        std::cerr << "Engine: Failed to initialize Vulkan!" << std::endl;
        return false;
    }
    std::cout << "Engine: Vulkan initialized successfully" << std::endl;

    // Create swap chain
    std::cout << "Engine: Creating swap chain..." << std::endl;
    swapChain = std::make_unique<SwapChain>(vulkanContext.get());
    if (!swapChain->initialize(window.get())) {
        std::cerr << "Engine: Failed to create swap chain!" << std::endl;
        return false;
    }
    std::cout << "Engine: Swap chain created successfully" << std::endl;

    // Create graphics pipeline
    std::cout << "Engine: Creating graphics pipeline..." << std::endl;
    pipeline = std::make_unique<Pipeline>(vulkanContext.get(), swapChain.get());
    if (!pipeline->initialize()) {
        std::cerr << "Engine: Failed to create graphics pipeline!" << std::endl;
        return false;
    }
    std::cout << "Engine: Graphics pipeline created successfully" << std::endl;

    isRunning = true;
    std::cout << "Engine: Initialization complete" << std::endl;
    return true;
}

void Engine::run() {
    std::cout << "Engine: Starting main loop" << std::endl;
    
    while (isRunning && !window->shouldClose()) {
        window->pollEvents();
        
        // Begin frame
        if (!pipeline->beginFrame()) {
            std::cout << "Engine: Frame begin failed, recreating swap chain..." << std::endl;
            // TODO: Handle swap chain recreation
            continue;
        }

        // Update game state
        // TODO: Add game state update logic

        // End frame
        pipeline->endFrame();
    }

    // Wait for the device to finish operations before cleanup
    vkDeviceWaitIdle(vulkanContext->getDevice());
    std::cout << "Engine: Main loop ended" << std::endl;
}

void Engine::cleanup() {
    std::cout << "Engine: Starting cleanup..." << std::endl;
    
    if (pipeline) {
        std::cout << "Engine: Cleaning up graphics pipeline..." << std::endl;
        pipeline->cleanup();
    }

    if (swapChain) {
        std::cout << "Engine: Cleaning up swap chain..." << std::endl;
        swapChain->cleanup();
    }

    if (vulkanContext) {
        std::cout << "Engine: Cleaning up Vulkan context..." << std::endl;
        vulkanContext->cleanup();
    }
    
    if (window) {
        std::cout << "Engine: Cleaning up window..." << std::endl;
    }
    
    pipeline.reset();
    swapChain.reset();
    window.reset();
    vulkanContext.reset();
    isRunning = false;

    std::cout << "Engine: Cleanup complete" << std::endl;
}

} // namespace voxceleron 