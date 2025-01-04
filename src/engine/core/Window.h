#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace voxceleron {

class Window {
public:
    Window(int width = 1280, int height = 720, const std::string& title = "Voxceleron Engine");
    ~Window();

    bool initialize();
    void cleanup();
    void pollEvents() { glfwPollEvents(); }
    bool shouldClose() const { return glfwWindowShouldClose(window); }
    void getFramebufferSize(int* width, int* height) const { glfwGetFramebufferSize(window, width, height); }
    
    // Getters
    GLFWwindow* getHandle() const { return window; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    GLFWwindow* window;
    int width;
    int height;
    std::string title;

    // Prevent copying
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

} // namespace voxceleron 