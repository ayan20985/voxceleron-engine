#pragma once

#include <memory>
#include <string>
#include <chrono>
#include "Window.h"

namespace voxceleron {

class VulkanContext;
class SwapChain;
class Pipeline;
class Camera;
class World;
class InputSystem;

class Engine {
public:
    enum class State {
        UNINITIALIZED,
        READY,
        ERROR,
        RECREATING
    };

    // Singleton pattern
    static Engine& getInstance() {
        static Engine instance;
        return instance;
    }

    ~Engine();

    bool initialize();
    void run();
    void cleanup();

    // State management
    State getState() const { return state; }
    bool isValid() const { return state == State::READY; }
    const char* getLastErrorMessage() const { return lastErrorMessage.c_str(); }

private:
    // Private constructor for singleton
    Engine();

    // Core components
    std::unique_ptr<Window> window;
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<SwapChain> swapChain;
    std::unique_ptr<Pipeline> pipeline;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<World> world;
    std::unique_ptr<InputSystem> input;

    // State tracking
    State state;
    std::string lastErrorMessage;

    // Timing
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    float deltaTime;

    // Input state
    bool rightMousePressed;
    bool leftMousePressed;

    // Helper functions
    bool initializeVulkan();
    bool createWindow();
    bool createSwapChain();
    bool createPipeline();
    bool createWorld();
    bool createCamera();
    bool createInputSystem();
    void setError(const char* message);
    bool handleWindowResize();
    void updateDeltaTime();

    // Input handling
    void setupInputCallbacks();
    void setupInputBindings();
    void handleMouseMove(double x, double y);
    void handleMouseButton(Window::MouseButton button, bool pressed);
    void handleMouseScroll(double yOffset);
    void handleKeyEvent(int key, int action);
    void handleAction(const std::string& action, float value);

    // Prevent copying
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
};

} // namespace voxceleron 