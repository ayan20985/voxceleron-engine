#ifndef ENGINE_H
#define ENGINE_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "World.h"
#include "Renderer.h"
#include <atomic>

class Engine {
public:
    Engine();
    ~Engine();

    void init();
    void mainLoop();
    void cleanup();

    // Performance metrics getters for ImGui
    float getFPS() const { return currentFPS; }
    float getUPS() const { return currentUPS; }

private:
    bool isRunning;
    GLFWwindow* window;
    World* world;
    Renderer* renderer;
    VkInstance instance;
    VkSurfaceKHR surface;

    // Performance metrics
    std::atomic<float> currentFPS{0.0f};
    std::atomic<float> currentUPS{0.0f};
};

#endif // ENGINE_H

