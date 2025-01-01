#include "Engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

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
        std::cout << "Starting engine initialization..." << std::endl;
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        std::cout << "GLFW initialized successfully" << std::endl;

        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
        window = glfwCreateWindow(1280, 720, "Voxceron Engine", nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create window");
        }
        std::cout << "Window created successfully" << std::endl;

        // Center the window on the primary monitor
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        int xpos = (mode->width - 1280) / 2;
        int ypos = (mode->height - 720) / 2;
        glfwSetWindowPos(window, xpos, ypos);
        std::cout << "Window centered on screen" << std::endl;

        // Setup Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Voxceron";
        appInfo.applicationVersion = VK_MAKE_VERSION(2, 0, 0);
        appInfo.pEngineName = "Voxceron Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(2, 0, 0);
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
        std::cout << "Vulkan instance created successfully" << std::endl;

        // Create surface
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
        std::cout << "Window surface created successfully" << std::endl;

        // Initialize world
        world = new World();
        world->generateTestWorld();
        std::cout << "World initialized successfully" << std::endl;

        // Set window user pointer and callbacks
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        std::cout << "Input callbacks set up successfully" << std::endl;

        // Initialize renderer with camera
        renderer = new Renderer();
        renderer->setEngine(this);
        renderer->init(instance, surface, window);
        renderer->initCamera();  // Initialize the camera
        renderer->setWorld(world);
        renderer->updateWorldMesh();
        std::cout << "Renderer initialized successfully" << std::endl;

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
        double lastFrame = glfwGetTime();
        int frameCount = 0;
        std::atomic<int> updateCount{0};
        
        std::cout << "Starting update thread..." << std::endl;
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
        
        std::cout << "Starting main render loop..." << std::endl;
        // Main loop (render thread)
        while (isRunning && !glfwWindowShouldClose(window)) {
            double currentFrame = glfwGetTime();
            float deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            glfwPollEvents();
            
            // Handle escape key to exit
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                std::cout << "Escape key pressed, exiting..." << std::endl;
                glfwSetWindowShouldClose(window, true);
            }

            // Handle Alt key for mouse cursor toggle
            if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
                if (!altWasPressed) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    altWasPressed = true;
                    // Store current mouse position when Alt is first pressed
                    double currentX, currentY;
                    glfwGetCursorPos(window, &currentX, &currentY);
                    lastX = currentX;
                    lastY = currentY;
                }
            } else {
                if (altWasPressed) {
                    // When Alt is released, force cursor to center and reset mouse state
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    altWasPressed = false;
                    // Get window size for proper centering
                    int width, height;
                    glfwGetWindowSize(window, &width, &height);
                    int centerX = width / 2;
                    int centerY = height / 2;
                    // Center cursor and update last positions
                    glfwSetCursorPos(window, centerX, centerY);
                    lastX = centerX;
                    lastY = centerY;
                    firstMouse = true;  // Reset first mouse to prevent jump
                }
            }
            
            // Update camera and render
            if (renderer) {
                renderer->updateCamera(deltaTime);  // Always update camera, but mouse movement is controlled by altWasPressed flag
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
                
                std::cout << "FPS: " << fps << ", UPS: " << ups << std::endl;
                
                // Store metrics for ImGui display
                currentFPS.store(fps);
                currentUPS.store(ups);
                
                // Reset counters
                frameCount = 0;
                updateCount.store(0);
                lastFPSUpdate = currentTime;
            }
        }
        
        std::cout << "Main loop ended, cleaning up..." << std::endl;
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