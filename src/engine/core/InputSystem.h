#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <cstring>
#include "Window.h"

namespace voxceleron {

class InputSystem {
public:
    // Input action types
    enum class ActionType {
        PRESS,      // Key/button press
        RELEASE,    // Key/button release
        REPEAT,     // Key/button held down
        CONTINUOUS, // Called every frame while key/button is down
        AXIS       // Continuous value (-1 to 1)
    };

    // Input binding
    struct InputBinding {
        int key;                // GLFW key code
        ActionType type;        // Type of action
        std::string action;     // Action identifier
        float scale;            // Scale factor for axis inputs
        bool active;            // Whether the binding is currently active
    };

    // Action callback
    using ActionCallback = std::function<void(const std::string&, float)>;

    InputSystem();
    ~InputSystem() = default;

    // Initialization
    void initialize(Window* window);

    // Binding management
    void addBinding(const std::string& action, int key, ActionType type, float scale = 1.0f);
    void removeBinding(const std::string& action, int key);
    void clearBindings();

    // Action management
    void addActionCallback(const std::string& action, ActionCallback callback);
    void removeActionCallback(const std::string& action);
    void clearActionCallbacks();

    // State updates
    void update(float deltaTime);
    void handleKeyEvent(int key, int action);
    void handleMouseButton(Window::MouseButton button, bool pressed);
    void handleMouseMove(double x, double y);
    void handleMouseScroll(double offset);

    // State queries
    bool isActionActive(const std::string& action) const;
    float getActionValue(const std::string& action) const;

    // Utility functions
    static const char* getKeyName(int key);
    static int getKeyFromName(const char* name);

private:
    Window* window;
    std::vector<InputBinding> bindings;
    std::unordered_map<std::string, std::vector<ActionCallback>> callbacks;
    std::unordered_map<std::string, float> actionValues;

    // Mouse state
    double lastMouseX;
    double lastMouseY;
    double mouseX;
    double mouseY;

    // Helper functions
    void triggerAction(const std::string& action, float value);
    void updateContinuousActions(float deltaTime);
    void updateAxisValues();

    // Prevent copying
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;
};

} // namespace voxceleron 