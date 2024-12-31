#ifndef ENGINE_H
#define ENGINE_H

#include <vulkan/vulkan.h>

class Engine {
public:
    Engine();
    ~Engine();

    void init();
    void run();
    void cleanup();

private:
    VkInstance instance;
    // Other Vulkan objects (device, swapchain, etc.)
};

#endif // ENGINE_H