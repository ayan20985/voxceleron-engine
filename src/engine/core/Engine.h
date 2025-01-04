#pragma once

#include <memory>

namespace voxceleron {

class Window;
class VulkanContext;
class SwapChain;
class Pipeline;

class Engine {
public:
    Engine();
    ~Engine();

    // Initialize the engine
    bool initialize();
    
    // Run the main engine loop
    void run();
    
    // Cleanup resources
    void cleanup();

    // Get engine instance
    static Engine& getInstance() {
        static Engine instance;
        return instance;
    }

private:
    std::unique_ptr<Window> window;
    std::unique_ptr<VulkanContext> vulkanContext;
    std::unique_ptr<SwapChain> swapChain;
    std::unique_ptr<Pipeline> pipeline;
    
    bool isRunning;

    // Prevent copying
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
};

} // namespace voxceleron 