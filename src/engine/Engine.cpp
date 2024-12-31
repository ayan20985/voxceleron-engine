#include "Engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

Engine::Engine() : isRunning(false), window(nullptr), world(nullptr), renderer(nullptr), instance(VK_NULL_HANDLE), surface(VK_NULL_HANDLE) {
}

Engine::~Engine() {
    cleanup();
}

void Engine::init() {
    try {
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(800, 600, "Voxceron Engine", nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create window");
        }

        // Setup Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Voxceron";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Voxceron Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Get required extensions
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        // Create surface
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }

        // Initialize world
        world = new World();
        world->generateTestWorld();

        // Initialize renderer
        renderer = new Renderer();
        renderer->setEngine(this);  // Set engine reference
        renderer->init(instance, surface, window);
        renderer->setWorld(world);
        renderer->updateWorldMesh();

        std::cout << "Engine initialization complete" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during engine initialization: " << e.what() << std::endl;
        throw;
    }
}

void Engine::mainLoop() {
    try {
        std::cout << "Entering main loop..." << std::endl;
        isRunning = true;
        
        // Set up timing variables
        double lastFPSUpdate = glfwGetTime();
        int frameCount = 0;
        std::atomic<int> updateCount{0};  // Make updateCount thread-safe
        
        // Start update thread
        std::thread updateThread([this, &updateCount]() {
            const double targetUPS = 60.0;  // Target updates per second
            const std::chrono::duration<double> updateInterval(1.0 / targetUPS);
            auto lastUpdateTime = std::chrono::steady_clock::now();
            
            while (isRunning) {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsedTime = currentTime - lastUpdateTime;
                
                if (elapsedTime >= updateInterval) {
                    if (world) {
                        world->update(updateInterval.count());
                        updateCount++;
                        lastUpdateTime = currentTime;
                    }
                }
            }
        });
        
        // Main loop (render thread)
        while (isRunning && !glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            // Handle escape key to exit
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, true);
            }
            
            // Render frame
            if (renderer) {
                renderer->updateWorldMesh();
                renderer->draw();
                frameCount++;
            }
            
            // Update FPS counter every second
            double currentTime = glfwGetTime();
            if (currentTime - lastFPSUpdate >= 1.0) {
                float fps = frameCount / (currentTime - lastFPSUpdate);
                int currentUpdateCount = updateCount.load();
                float ups = currentUpdateCount / (currentTime - lastFPSUpdate);
                
                // Store metrics for ImGui display
                currentFPS.store(fps);
                currentUPS.store(ups);
                
                // Reset counters
                frameCount = 0;
                updateCount.store(0);
                lastFPSUpdate = currentTime;
            }
        }
        
        // Cleanup
        isRunning = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error in main loop: " << e.what() << std::endl;
        isRunning = false;
        throw;
    }
}

void Engine::cleanup() {
    isRunning = false;
    
    if (renderer) {
        delete renderer;
        renderer = nullptr;
    }
    
    if (world) {
        delete world;
        world = nullptr;
    }
    
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
    
    glfwTerminate();
}