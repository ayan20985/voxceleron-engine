#ifndef ENGINE_H
#define ENGINE_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "World.h"
#include "Renderer.h"
#include <atomic>
#include <memory>

class Engine {
public:
    Engine();
    ~Engine();

    void init();
    void mainLoop();
    void cleanup();

    // Mouse callback
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    // Performance metrics getters for ImGui
    float getFPS() const { return currentFPS; }
    float getUPS() const { return currentUPS; }

private:
    bool isRunning;
    GLFWwindow* window;
    std::unique_ptr<World> world;
    Renderer* renderer;
    VkInstance instance;
    VkSurfaceKHR surface;

    // Mouse handling
    bool firstMouse = true;
    bool altWasPressed = false;
    float lastX = 0.0f;
    float lastY = 0.0f;

    // Performance metrics
    std::atomic<float> currentFPS{0.0f};
    std::atomic<float> currentUPS{0.0f};
};

#endif // ENGINE_H

