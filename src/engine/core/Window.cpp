#include "Window.h"
#include <iostream>
#include <stdexcept>

namespace voxceleron {

Window::Window()
    : window(nullptr)
    , width(0)
    , height(0)
    , framebufferResized(false)
    , surface(VK_NULL_HANDLE)
    , mouseMoveCallback(nullptr)
    , mouseButtonCallback(nullptr)
    , mouseScrollCallback(nullptr)
    , keyCallback(nullptr) {
    std::cout << "Window: Creating window instance" << std::endl;
}

Window::~Window() {
    std::cout << "Window: Destroying window instance" << std::endl;
    cleanup();
}

bool Window::initialize(int width, int height, const char* title) {
    std::cout << "Window: Starting initialization..." << std::endl;

    this->width = width;
    this->height = height;

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Window: Failed to initialize GLFW!" << std::endl;
        return false;
    }

    // Configure GLFW for Vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "Window: Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Set user pointer and callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback_internal);
    glfwSetMouseButtonCallback(window, mouseButtonCallback_internal);
    glfwSetScrollCallback(window, mouseScrollCallback_internal);
    glfwSetKeyCallback(window, keyCallback_internal);

    std::cout << "Window: Initialization complete" << std::endl;
    return true;
}

void Window::cleanup() {
    std::cout << "Window: Starting cleanup..." << std::endl;

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    surface = VK_NULL_HANDLE; // Surface is cleaned up by VulkanContext

    glfwTerminate();
    std::cout << "Window: Cleanup complete" << std::endl;
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

bool Window::isMouseButtonPressed(MouseButton button) const {
    if (!window) return false;

    int glfwButton;
    switch (button) {
        case MouseButton::LEFT: glfwButton = GLFW_MOUSE_BUTTON_LEFT; break;
        case MouseButton::RIGHT: glfwButton = GLFW_MOUSE_BUTTON_RIGHT; break;
        case MouseButton::MIDDLE: glfwButton = GLFW_MOUSE_BUTTON_MIDDLE; break;
        default: return false;
    }

    return glfwGetMouseButton(window, glfwButton) == GLFW_PRESS;
}

bool Window::isKeyPressed(int key) const {
    if (!window) return false;
    return glfwGetKey(window, key) == GLFW_PRESS;
}

void Window::getCursorPosition(double& x, double& y) const {
    if (window) {
        glfwGetCursorPos(window, &x, &y);
    } else {
        x = y = 0.0;
    }
}

void Window::setCursorMode(bool captured) {
    if (window) {
        glfwSetInputMode(window, GLFW_CURSOR, 
            captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

void Window::getFramebufferSize(int* width, int* height) const {
    if (window) {
        glfwGetFramebufferSize(window, width, height);
    } else {
        *width = 0;
        *height = 0;
    }
}

VkSurfaceKHR Window::createSurface(VkInstance instance) {
    if (instance == VK_NULL_HANDLE) {
        std::cerr << "Window: Cannot create surface without valid VkInstance" << std::endl;
        return VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE) {
        std::cout << "Window: Surface already exists, destroying old surface" << std::endl;
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        std::cerr << "Window: Failed to create window surface! Error code: " << result << std::endl;
        return VK_NULL_HANDLE;
    }

    std::cout << "Window: Created surface successfully: " << surface << std::endl;
    return surface;
}

VkSurfaceKHR Window::getSurface() const {
    if (surface == VK_NULL_HANDLE) {
        std::cerr << "Window: Surface has not been created yet" << std::endl;
    }
    return surface;
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->width = width;
    app->height = height;
    app->framebufferResized = true;
    std::cout << "Window: Framebuffer resized to " << width << "x" << height << std::endl;
}

void Window::mouseMoveCallback_internal(GLFWwindow* window, double x, double y) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (app->mouseMoveCallback) {
        app->mouseMoveCallback(x, y);
    }
}

void Window::mouseButtonCallback_internal(GLFWwindow* window, int button, int action, int mods) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (app->mouseButtonCallback) {
        MouseButton mb;
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: mb = MouseButton::LEFT; break;
            case GLFW_MOUSE_BUTTON_RIGHT: mb = MouseButton::RIGHT; break;
            case GLFW_MOUSE_BUTTON_MIDDLE: mb = MouseButton::MIDDLE; break;
            default: return;
        }
        app->mouseButtonCallback(mb, action == GLFW_PRESS);
    }
}

void Window::mouseScrollCallback_internal(GLFWwindow* window, double xOffset, double yOffset) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (app->mouseScrollCallback) {
        app->mouseScrollCallback(yOffset);
    }
}

void Window::keyCallback_internal(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (app->keyCallback) {
        app->keyCallback(key, action);
    }

    // Handle ESC key to close window
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

} // namespace voxceleron 
