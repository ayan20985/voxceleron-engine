#include "Window.h"
#include <iostream>

namespace voxceleron {

Window::Window(int width, int height, const std::string& title)
    : window(nullptr)
    , width(width)
    , height(height)
    , title(title) {
    std::cout << "Window: Creating window " << width << "x" << height << " with title '" << title << "'" << std::endl;
}

Window::~Window() {
    std::cout << "Window: Destroying window" << std::endl;
    cleanup();
}

bool Window::initialize() {
    std::cout << "Window: Initializing GLFW..." << std::endl;
    
    if (!glfwInit()) {
        std::cerr << "Window: Failed to initialize GLFW!" << std::endl;
        return false;
    }
    std::cout << "Window: GLFW initialized successfully" << std::endl;

    std::cout << "Window: Configuring GLFW for Vulkan..." << std::endl;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    std::cout << "Window: Creating GLFW window..." << std::endl;
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "Window: Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Center window on screen
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int xpos = (mode->width - width) / 4;
    int ypos = (mode->height - height) / 4;
    glfwSetWindowPos(window, xpos, ypos);
    std::cout << "Window: Centered window on screen at " << xpos << "," << ypos << std::endl;

    std::cout << "Window: Initialization complete" << std::endl;
    return true;
}

void Window::cleanup() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
    std::cout << "Window: Terminating GLFW" << std::endl;
}

} // namespace voxceleron 