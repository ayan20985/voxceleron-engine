#include "InputSystem.h"
#include "Window.h"
#include <iostream>
#include <algorithm>
#include <cstring>

namespace voxceleron {

InputSystem::InputSystem() : window(nullptr), lastMouseX(0), lastMouseY(0), mouseX(0), mouseY(0) {
    std::cout << "InputSystem: Creating input system instance" << std::endl;
}

void InputSystem::initialize(Window* window) {
    std::cout << "InputSystem: Starting initialization..." << std::endl;
    this->window = window;

    window->setKeyCallback([this](int key, int action) {
        handleKeyEvent(key, action);
    });

    window->setMouseButtonCallback([this](Window::MouseButton button, bool pressed) {
        handleMouseButton(button, pressed);
    });

    window->setMouseMoveCallback([this](double x, double y) {
        handleMouseMove(x, y);
    });

    window->setMouseScrollCallback([this](double offset) {
        handleMouseScroll(offset);
    });

    std::cout << "InputSystem: Initialization complete" << std::endl;
}

void InputSystem::addBinding(const std::string& action, int key, ActionType type, float scale) {
    InputBinding binding;
    binding.key = key;
    binding.type = type;
    binding.action = action;
    binding.scale = scale;
    binding.active = false;
    bindings.push_back(binding);
}

void InputSystem::removeBinding(const std::string& action, int key) {
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(),
            [&](const InputBinding& binding) {
                return binding.action == action && binding.key == key;
            }),
        bindings.end()
    );
}

void InputSystem::clearBindings() {
    bindings.clear();
    actionValues.clear();
}

void InputSystem::addActionCallback(const std::string& action, ActionCallback callback) {
    callbacks[action].push_back(callback);
}

void InputSystem::removeActionCallback(const std::string& action) {
    callbacks.erase(action);
}

void InputSystem::clearActionCallbacks() {
    callbacks.clear();
}

void InputSystem::update(float deltaTime) {
    updateContinuousActions(deltaTime);
    updateAxisValues();
}

void InputSystem::handleKeyEvent(int key, int action) {
    for (auto& binding : bindings) {
        if (binding.key == key) {
            switch (action) {
                case GLFW_PRESS:
                    binding.active = true;
                    if (binding.type == ActionType::PRESS) {
                        triggerAction(binding.action, 1.0f);
                    }
                    break;
                case GLFW_RELEASE:
                    binding.active = false;
                    if (binding.type == ActionType::RELEASE) {
                        triggerAction(binding.action, 1.0f);
                    }
                        break;
                case GLFW_REPEAT:
                    if (binding.type == ActionType::REPEAT) {
                        triggerAction(binding.action, 1.0f);
                    }
                        break;
                }
            }
        }
}

bool InputSystem::isActionActive(const std::string& action) const {
    auto it = actionValues.find(action);
    return it != actionValues.end() && it->second != 0.0f;
}

float InputSystem::getActionValue(const std::string& action) const {
    auto it = actionValues.find(action);
    return it != actionValues.end() ? it->second : 0.0f;
}

const char* InputSystem::getKeyName(int key) {
    return glfwGetKeyName(key, 0);
}

int InputSystem::getKeyFromName(const char* name) {
    if (!name) return GLFW_KEY_UNKNOWN;
    
    if (strcmp(name, "space") == 0) return GLFW_KEY_SPACE;
    if (strcmp(name, "escape") == 0) return GLFW_KEY_ESCAPE;
    if (strcmp(name, "enter") == 0) return GLFW_KEY_ENTER;
    
    if (strlen(name) == 1) {
        return static_cast<int>(toupper(name[0]));
    }
    
    return GLFW_KEY_UNKNOWN;
}

void InputSystem::triggerAction(const std::string& action, float value) {
    actionValues[action] = value;
            auto it = callbacks.find(action);
            if (it != callbacks.end()) {
        for (const auto& callback : it->second) {
            callback(action, value);
        }
    }
}

void InputSystem::updateContinuousActions(float deltaTime) {
    for (const auto& binding : bindings) {
        if (binding.type == ActionType::CONTINUOUS && binding.active) {
            triggerAction(binding.action, deltaTime * binding.scale);
        }
    }
}

void InputSystem::updateAxisValues() {
    for (const auto& binding : bindings) {
        if (binding.type == ActionType::AXIS) {
            float value = binding.active ? binding.scale : 0.0f;
            actionValues[binding.action] = value;
        }
    }
}

void InputSystem::handleMouseButton(Window::MouseButton button, bool pressed) {
    int key;
    switch (button) {
        case Window::MouseButton::LEFT:   key = GLFW_MOUSE_BUTTON_LEFT; break;
        case Window::MouseButton::RIGHT:  key = GLFW_MOUSE_BUTTON_RIGHT; break;
        case Window::MouseButton::MIDDLE: key = GLFW_MOUSE_BUTTON_MIDDLE; break;
        default: return;
    }
    handleKeyEvent(key, pressed ? GLFW_PRESS : GLFW_RELEASE);
}

void InputSystem::handleMouseMove(double x, double y) {
    mouseX = x;
    mouseY = y;

    float deltaX = static_cast<float>(x - lastMouseX);
    float deltaY = static_cast<float>(y - lastMouseY);

    lastMouseX = x;
    lastMouseY = y;

    for (const auto& binding : bindings) {
        if (binding.type == ActionType::AXIS) {
            if (binding.key == GLFW_MOUSE_BUTTON_LEFT) {
                triggerAction(binding.action, deltaX * binding.scale);
            } else if (binding.key == GLFW_MOUSE_BUTTON_RIGHT) {
                triggerAction(binding.action, deltaY * binding.scale);
            }
        }
    }
}

void InputSystem::handleMouseScroll(double offset) {
    for (const auto& binding : bindings) {
        if (binding.type == ActionType::AXIS && binding.key == GLFW_MOUSE_BUTTON_MIDDLE) {
            triggerAction(binding.action, static_cast<float>(offset) * binding.scale);
        }
    }
}

} // namespace voxceleron 