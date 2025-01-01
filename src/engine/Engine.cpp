#include "Engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>

Engine::Engine() : 
    isRunning(false), 
    window(nullptr), 
    world(nullptr), 
    renderer(nullptr), 
    instance(VK_NULL_HANDLE), 
    surface(VK_NULL_HANDLE),
    firstMouse(true),
    lastX(400.0f),
    lastY(300.0f) {
}

Engine::~Engine() {
    cleanup();
}

void Engine::init() {
    try {
        // Create logs directory if it doesn't exist
        std::filesystem::create_directory("logs");
        static std::ofstream logFile("logs/engine.log", std::ios::app);
        
        logFile << "Starting engine initialization..." << std::endl;
        
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        logFile << "GLFW initialized successfully" << std::endl;
        
        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(1280, 720, "Voxceleron Engine v2.75", nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create window");
        }
        logFile << "Window created successfully" << std::endl;
        
        // Center window on screen
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode) {
            int xpos = (mode->width - 1280) / 2;
            int ypos = (mode->height - 720) / 2;
            glfwSetWindowPos(window, xpos, ypos);
            logFile << "Window centered on screen" << std::endl;
        }
        
        // Create Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Voxceleron Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(2, 75, 0);
        appInfo.pEngineName = "Voxceleron";
        appInfo.engineVersion = VK_MAKE_VERSION(2, 75, 0);
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
        logFile << "Vulkan instance created successfully" << std::endl;
        
        // Create surface
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
        logFile << "Window surface created successfully" << std::endl;
        
        // Set window user pointer and callbacks
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
        logFile << "Input callbacks set up successfully" << std::endl;
        
        // Initialize renderer with camera
        renderer = new Renderer();
        renderer->setEngine(this);
        renderer->init(instance, surface, window);
        renderer->initCamera();  // Initialize the camera
        logFile << "Renderer initialized successfully" << std::endl;
        
        // Initialize world
        world = std::make_unique<World>();
        world->setCamera(renderer->getCamera());
        world->generateTestWorld();
        renderer->setWorld(world.get());  // Pass the raw pointer
        renderer->updateWorldMesh();
        logFile << "World initialized successfully" << std::endl;
        
        logFile << "Engine initialization complete" << std::endl;
        logFile.flush();
    } catch (const std::exception& e) {
        std::ofstream logFile("logs/error.log", std::ios::app);
        logFile << "Error during engine initialization: " << e.what() << std::endl;
        logFile.flush();
        throw;
    }
}

void Engine::mainLoop() {
    try {
        static std::ofstream logFile("logs/engine.log", std::ios::app);
        logFile << "Entering main loop..." << std::endl;
        isRunning = true;
        
        // Set up timing variables
        double lastFPSUpdate = glfwGetTime();
        double lastFrame = glfwGetTime();
        int frameCount = 0;
        std::atomic<int> updateCount{0};
        
        logFile << "Starting update thread..." << std::endl;
        // Start update thread
        std::thread updateThread([this, &updateCount]() {
            const double targetUPS = 60.0;
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
        
        logFile << "Starting main render loop..." << std::endl;
        logFile.flush();
        
        // Main loop (render thread)
        while (isRunning && !glfwWindowShouldClose(window)) {
            double currentFrame = glfwGetTime();
            float deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            
            glfwPollEvents();
            
            // Handle escape key to exit
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                logFile << "Escape key pressed, exiting..." << std::endl;
                glfwSetWindowShouldClose(window, true);
            }
            
            // Update camera and render
            if (renderer) {
                renderer->updateCamera(deltaTime);
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
                
                logFile << "FPS: " << fps << ", UPS: " << ups << std::endl;
                logFile.flush();
                
                // Store metrics for ImGui display
                currentFPS.store(fps);
                currentUPS.store(ups);
                
                // Reset counters
                frameCount = 0;
                updateCount.store(0);
                lastFPSUpdate = currentTime;
            }
        }
        
        logFile << "Main loop ended, cleaning up..." << std::endl;
        // Cleanup
        isRunning = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
        
    } catch (const std::exception& e) {
        std::ofstream logFile("logs/error.log", std::ios::app);
        logFile << "Fatal error in main loop: " << e.what() << std::endl;
        logFile.flush();
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
        engine->lastX = xpos;
        engine->lastY = ypos;
        return;
    }

    // When Alt is not pressed, calculate the offset and immediately recenter
    float xoffset = xpos - engine->lastX;
    float yoffset = engine->lastY - ypos;  // Reversed since y-coordinates range from bottom to top

    // Get window size for centering
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    int centerX = width / 2;
    int centerY = height / 2;

    // Update the view with the offset
    engine->renderer->handleMouseMovement(xoffset, yoffset);

    // Always recenter when not in Alt mode
    glfwSetCursorPos(window, centerX, centerY);
    engine->lastX = centerX;
    engine->lastY = centerY;
}