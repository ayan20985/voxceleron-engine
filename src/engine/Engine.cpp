#include "Engine.h"
#include "Logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>

Engine::Engine() : 
    isRunning(false), 
    window(nullptr), 
    world(nullptr), 
    renderer(nullptr), 
    instance(VK_NULL_HANDLE), 
    surface(VK_NULL_HANDLE),
    firstMouse(true),
    lastX(400.0f),
    lastY(300.0f),
    altWasPressed(false) {
}

Engine::~Engine() {
    cleanup();
}

void Engine::init() {
    try {
        LOG_INFO("Starting engine initialization...");
        
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        LOG_INFO("GLFW initialized successfully");
        
        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(1280, 720, "Voxceleron Engine v2.76", nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create window");
        }
        LOG_INFO("Window created successfully");
        
        // Center window on screen
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode) {
            int xpos = (mode->width - 1280) / 2;
            int ypos = (mode->height - 720) / 2;
            glfwSetWindowPos(window, xpos, ypos);
            LOG_INFO("Window centered on screen");
        }
        
        // Create Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Voxceleron Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(2, 76, 0);
        appInfo.pEngineName = "Voxceleron";
        appInfo.engineVersion = VK_MAKE_VERSION(2, 76, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;
        
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
        LOG_INFO("Vulkan instance created successfully");
        
        // Create surface
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
        LOG_INFO("Window surface created successfully");
        
        // Set window user pointer and callbacks
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
        LOG_INFO("Input callbacks set up successfully");
        
        // Initialize renderer with camera
        renderer = new Renderer();
        renderer->setEngine(this);
        renderer->init(instance, surface, window);
        renderer->initCamera();  // Initialize the camera
        LOG_RENDER("Renderer initialized successfully");
        
        // Initialize world
        world = std::make_unique<World>();
        world->setCamera(renderer->getCamera());
        world->generateTestWorld();
        renderer->setWorld(world.get());  // Pass the raw pointer
        renderer->updateWorldMesh();
        LOG_WORLDGEN("World initialized successfully");
        
        LOG_INFO("Engine initialization complete");
    } catch (const std::exception& e) {
        LOG_ERROR("Error during engine initialization: " + std::string(e.what()));
        throw;
    }
}

void Engine::mainLoop() {
    try {
        LOG_INFO("Entering main loop...");
        isRunning = true;
        
        // Set up timing variables
        float lastFPSUpdate = static_cast<float>(glfwGetTime());
        float lastFrame = static_cast<float>(glfwGetTime());
        int frameCount = 0;
        std::atomic<int> updateCount{0};
        
        LOG_INFO("Starting update thread...");
        // Start update thread
        std::thread updateThread([this, &updateCount]() {
            const float targetUPS = 60.0f;
            const std::chrono::duration<float> updateInterval(1.0f / targetUPS);
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
        
        LOG_INFO("Starting main render loop...");
        
        // Main loop (render thread)
        while (isRunning && !glfwWindowShouldClose(window)) {
            float currentFrame = static_cast<float>(glfwGetTime());
            float deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            
            glfwPollEvents();
            
            // Handle escape key to exit
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                LOG_INFO("Escape key pressed, exiting...");
                glfwSetWindowShouldClose(window, true);
            }

            // Handle Alt key state
            bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || 
                             glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
            
            // Toggle cursor and input mode when Alt state changes
            if (altPressed != altWasPressed) {
                altWasPressed = altPressed;
                if (altPressed) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    LOG_DEBUG("Cursor mode set to normal (Alt pressed)");
                } else {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    // Recenter cursor when returning to game mode
                    int width, height;
                    glfwGetWindowSize(window, &width, &height);
                    glfwSetCursorPos(window, static_cast<float>(width) / 2.0f, static_cast<float>(height) / 2.0f);
                    lastX = static_cast<float>(width) / 2.0f;
                    lastY = static_cast<float>(height) / 2.0f;
                    LOG_DEBUG("Cursor mode set to disabled (Alt released)");
                }
            }
            
            // Update camera and render
            if (renderer) {
                renderer->updateCamera(deltaTime);
                renderer->updateWorldMesh();
                renderer->draw();
                frameCount++;
            }
            
            // Update FPS counter every second
            float currentTime = static_cast<float>(glfwGetTime());
            if (currentTime - lastFPSUpdate >= 1.0f) {
                float fps = static_cast<float>(frameCount) / (currentTime - lastFPSUpdate);
                int currentUpdateCount = updateCount.load();
                float ups = static_cast<float>(currentUpdateCount) / (currentTime - lastFPSUpdate);
                
                LOG_PERF("FPS: " + std::to_string(fps) + ", UPS: " + std::to_string(ups));
                
                // Store metrics for ImGui display
                currentFPS.store(fps);
                currentUPS.store(ups);
                
                // Reset counters
                frameCount = 0;
                updateCount.store(0);
                lastFPSUpdate = currentTime;
            }
        }
        
        LOG_INFO("Main loop ended, cleaning up...");
        // Cleanup
        isRunning = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error in main loop: " + std::string(e.what()));
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
    
    world.reset(); // unique_ptr will handle deletion
    
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

void Engine::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine || !engine->renderer) return;

    if (engine->altWasPressed) {
        // When Alt is pressed, just update the position without affecting the view
        engine->lastX = static_cast<float>(xpos);
        engine->lastY = static_cast<float>(ypos);
        return;
    }

    // When Alt is not pressed, calculate the offset and immediately recenter
    float xoffset = static_cast<float>(xpos - engine->lastX);
    float yoffset = static_cast<float>(engine->lastY - ypos);  // Reversed since y-coordinates range from bottom to top

    // Get window size for centering
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    float centerX = static_cast<float>(width / 2);
    float centerY = static_cast<float>(height / 2);

    // Update the view with the offset
    engine->renderer->handleMouseMovement(xoffset, yoffset);

    // Always recenter when not in Alt mode
    glfwSetCursorPos(window, centerX, centerY);
    engine->lastX = centerX;
    engine->lastY = centerY;
}