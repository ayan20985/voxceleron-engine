#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

namespace voxceleron {

class Window {
public:
    // Mouse button states
    enum class MouseButton {
        LEFT,
        RIGHT,
        MIDDLE
    };

    // Input callbacks
    using MouseMoveCallback = std::function<void(double, double)>;
    using MouseButtonCallback = std::function<void(MouseButton, bool)>;
    using MouseScrollCallback = std::function<void(double)>;
    using KeyCallback = std::function<void(int, int)>;  // key, action

    Window();
    ~Window();

    bool initialize(int width, int height, const char* title);
    void cleanup();

    // Window state
    bool shouldClose() const;
    void pollEvents();
    bool wasResized() const { return framebufferResized; }
    void resetResizeFlag() { framebufferResized = false; }

    // Input state
    bool isMouseButtonPressed(MouseButton button) const;
    bool isKeyPressed(int key) const;
    void getCursorPosition(double& x, double& y) const;
    void setCursorMode(bool captured);

    // Input callbacks
    void setMouseMoveCallback(MouseMoveCallback callback) { mouseMoveCallback = callback; }
    void setMouseButtonCallback(MouseButtonCallback callback) { mouseButtonCallback = callback; }
    void setMouseScrollCallback(MouseScrollCallback callback) { mouseScrollCallback = callback; }
    void setKeyCallback(KeyCallback callback) { keyCallback = callback; }

    // Getters
    GLFWwindow* getHandle() const { return window; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    float getAspectRatio() const { return static_cast<float>(width) / static_cast<float>(height); }
    void getFramebufferSize(int* width, int* height) const;
    VkSurfaceKHR getSurface() const;

    // Surface creation
    VkSurfaceKHR createSurface(VkInstance instance);

private:
    GLFWwindow* window;
    int width;
    int height;
    bool framebufferResized;
    VkSurfaceKHR surface;

    // Input callbacks
    MouseMoveCallback mouseMoveCallback;
    MouseButtonCallback mouseButtonCallback;
    MouseScrollCallback mouseScrollCallback;
    KeyCallback keyCallback;

    // Callback setup
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseMoveCallback_internal(GLFWwindow* window, double x, double y);
    static void mouseButtonCallback_internal(GLFWwindow* window, int button, int action, int mods);
    static void mouseScrollCallback_internal(GLFWwindow* window, double xOffset, double yOffset);
    static void keyCallback_internal(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Prevent copying
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

} // namespace voxceleron 