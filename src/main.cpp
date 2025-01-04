#include "engine/core/Engine.h"
#include <iostream>

int main() {
    try {
        auto& engine = voxceleron::Engine::getInstance();
        
        if (!engine.initialize()) {
            std::cerr << "Failed to initialize engine!" << std::endl;
            return -1;
        }

        engine.run();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
} 