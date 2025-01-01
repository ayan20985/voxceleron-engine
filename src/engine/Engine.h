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

    // Mouse input handling
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    // Performance metrics getters for ImGui
    float getFPS() const { return currentFPS.load(); }
    float getUPS() const { return currentUPS.load(); }

private:
    bool isRunning;
    GLFWwindow* window;
    World* world;
    Renderer* renderer;
    VkInstance instance;
    VkSurfaceKHR surface;

    // Mouse input state
    bool firstMouse;
    float lastX;
    float lastY;
    bool altWasPressed = false;  // Track Alt key state

    // Performance metrics
    std::atomic<float> currentFPS{0.0f};
    std::atomic<float> currentUPS{0.0f};
};

#endif // ENGINE_H

