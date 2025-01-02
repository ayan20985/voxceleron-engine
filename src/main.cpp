#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "engine/Engine.h"
#include "engine/Logger.h"

int main() {
    try {
        LOG_INFO("Starting Voxceleron Engine...");
        Engine engine;
        engine.init();
        engine.mainLoop();
        engine.cleanup();
        LOG_INFO("Engine shutdown complete");
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Fatal error: ") + e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}