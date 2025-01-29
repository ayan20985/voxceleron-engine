#include "Engine.h"
#include "Window.h"
#include "Camera.h"
#include "InputSystem.h"
#include "../vulkan/core/VulkanContext.h"
#include "../vulkan/core/SwapChain.h"
#include "../vulkan/pipeline/Pipeline.h"
#include "../voxel/World.h"
#include <iostream>

namespace voxceleron {

Engine::Engine()
    : state(State::UNINITIALIZED)
    , deltaTime(0.0f)
    , rightMousePressed(false)
    , leftMousePressed(false) {
    std::cout << "Engine: Creating engine instance" << std::endl;
}

Engine::~Engine() {
    std::cout << "Engine: Destroying engine instance" << std::endl;
    cleanup();
}

void Engine::setError(const char* message) {
    state = State::ERROR;
    lastErrorMessage = message;
    std::cerr << "Engine Error: " << message << std::endl;
}

bool Engine::initialize() {
    std::cout << "Engine: Starting initialization..." << std::endl;

    if (!createWindow()) {
        return false;
    }

    // Initialize Vulkan first
    std::cout << "Engine: Initializing Vulkan..." << std::endl;
    context = std::make_unique<VulkanContext>();
    if (!context->initialize(window.get())) {
        setError("Failed to initialize Vulkan");
        return false;
    }

    if (!createInputSystem()) {
        return false;
    }

    if (!createSwapChain()) {
        return false;
    }

    if (!createPipeline()) {
        return false;
    }

    if (!createCamera()) {
        return false;
    }

    if (!createWorld()) {
        return false;
    }

    setupInputCallbacks();
    setupInputBindings();
    lastFrameTime = std::chrono::high_resolution_clock::now();
    state = State::READY;
    std::cout << "Engine: Initialization complete" << std::endl;
    return true;
}

void Engine::run() {
    std::cout << "Engine: Starting main loop" << std::endl;

    while (state != State::ERROR && !window->shouldClose()) {
        updateDeltaTime();
        window->pollEvents();
        input->update(deltaTime);

        // Handle window resize
        if (window->wasResized()) {
            if (!handleWindowResize()) {
                break;
            }
            window->resetResizeFlag();
        }

        // Check if swap chain needs recreation
        if (!swapChain->isValid()) {
            if (!handleWindowResize()) {
                break;
            }
            continue;
        }

        // Begin frame
        if (!pipeline->beginFrame()) {
            if (pipeline->getState() == Pipeline::State::RECREATING) {
                if (!handleWindowResize()) {
                    break;
                }
                continue;
            }
            break;
        }

        // Update world and camera
        camera->update(deltaTime);
        world->update();

        // Record commands
        world->render(pipeline->getCurrentCommandBuffer());

        // End frame
        if (!pipeline->endFrame()) {
            if (pipeline->getState() == Pipeline::State::RECREATING) {
                if (!handleWindowResize()) {
                    break;
                }
                continue;
            }
            break;
        }
    }

    // Wait for device to finish
    if (context) {
        vkDeviceWaitIdle(context->getDevice());
    }
}

void Engine::cleanup() {
    std::cout << "Engine: Starting cleanup..." << std::endl;

    // First, wait for the device to be idle before cleanup
    if (context) {
        vkDeviceWaitIdle(context->getDevice());
    }

    // 1. Clean up World first (contains compute pipelines and other GPU resources)
    if (world) {
        world.reset();
        std::cout << "Engine: World cleanup complete" << std::endl;
    }

    // 2. Clean up Pipeline (depends on swap chain)
    if (pipeline) {
        pipeline.reset();
        std::cout << "Engine: Pipeline cleanup complete" << std::endl;
    }

    // 3. Clean up SwapChain (contains framebuffers)
    if (swapChain) {
        swapChain.reset();
        std::cout << "Engine: SwapChain cleanup complete" << std::endl;
    }

    // 4. Clean up Camera (no Vulkan dependencies)
    if (camera) {
        camera.reset();
    }

    // 5. Clean up InputSystem (no Vulkan dependencies)
    if (input) {
        input.reset();
    }

    // 6. Clean up Vulkan context (after all Vulkan-dependent resources)
    if (context) {
        context.reset();
        std::cout << "Engine: Vulkan context cleanup complete" << std::endl;
    }

    // 7. Clean up Window last
    if (window) {
        window.reset();
    }

    state = State::UNINITIALIZED;
    std::cout << "Engine: Cleanup complete" << std::endl;
}

bool Engine::createWindow() {
    std::cout << "Engine: Creating window..." << std::endl;
    window = std::make_unique<Window>();
    if (!window->initialize(800, 600, "Voxceleron Engine")) {
        setError("Failed to create window");
        return false;
    }
    return true;
}

bool Engine::createInputSystem() {
    std::cout << "Engine: Creating input system..." << std::endl;
    input = std::make_unique<InputSystem>();
    input->initialize(window.get());
    return true;
}

bool Engine::createSwapChain() {
    std::cout << "Engine: Creating swap chain..." << std::endl;
    swapChain = std::make_unique<SwapChain>(context.get());
    if (!swapChain->initialize(window.get())) {
        setError("Failed to create swap chain");
        return false;
    }
    return true;
}

bool Engine::createPipeline() {
    std::cout << "Engine: Creating pipeline..." << std::endl;
    pipeline = std::make_unique<Pipeline>(context.get(), swapChain.get());
    if (!pipeline->initialize()) {
        setError("Failed to create pipeline");
        return false;
    }
    return true;
}

bool Engine::createCamera() {
    std::cout << "Engine: Creating camera..." << std::endl;
    camera = std::make_unique<Camera>();
    camera->initialize(window.get());

    // Set initial camera position and settings
    Camera::MovementSettings settings;
    settings.moveSpeed = 5.0f;
    settings.mouseSensitivity = 0.1f;
    settings.smoothness = 0.1f;
    camera->setMovementSettings(settings);
    camera->setPosition(glm::vec3(0.0f, 5.0f, 10.0f));
    camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f));

    return true;
}

bool Engine::createWorld() {
    std::cout << "Engine: Creating world..." << std::endl;
    world = std::make_unique<World>(context.get());
    if (!world->initialize()) {
        setError("Failed to create world");
        return false;
    }
    return true;
}

bool Engine::handleWindowResize() {
    std::cout << "Engine: Handling window resize..." << std::endl;
    
    // Wait for device to be idle
    vkDeviceWaitIdle(context->getDevice());

    // Recreate swap chain
    if (!swapChain->recreate(window.get())) {
        setError("Failed to recreate swap chain");
        return false;
    }

    // Recreate pipeline
    if (!pipeline->recreateIfNeeded()) {
        setError("Failed to recreate pipeline");
        return false;
    }

    return true;
}

void Engine::updateDeltaTime() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;
}

void Engine::setupInputCallbacks() {
    window->setMouseMoveCallback([this](double x, double y) { handleMouseMove(x, y); });
    window->setMouseButtonCallback([this](Window::MouseButton button, bool pressed) { handleMouseButton(button, pressed); });
    window->setMouseScrollCallback([this](double yOffset) { handleMouseScroll(yOffset); });
    window->setKeyCallback([this](int key, int action) { handleKeyEvent(key, action); });
}

void Engine::setupInputBindings() {
    // Movement bindings
    input->addBinding("move_forward", GLFW_KEY_W, InputSystem::ActionType::CONTINUOUS);
    input->addBinding("move_backward", GLFW_KEY_S, InputSystem::ActionType::CONTINUOUS);
    input->addBinding("move_left", GLFW_KEY_A, InputSystem::ActionType::CONTINUOUS);
    input->addBinding("move_right", GLFW_KEY_D, InputSystem::ActionType::CONTINUOUS);
    input->addBinding("move_up", GLFW_KEY_SPACE, InputSystem::ActionType::CONTINUOUS);
    input->addBinding("move_down", GLFW_KEY_LEFT_CONTROL, InputSystem::ActionType::CONTINUOUS);

    // Action bindings
    input->addBinding("interact", GLFW_KEY_E, InputSystem::ActionType::PRESS);
    input->addBinding("toggle_menu", GLFW_KEY_TAB, InputSystem::ActionType::PRESS);
    input->addBinding("sprint", GLFW_KEY_LEFT_SHIFT, InputSystem::ActionType::CONTINUOUS);

    // Register action callbacks
    input->addActionCallback("move_forward", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("move_backward", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("move_left", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("move_right", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("move_up", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("move_down", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("interact", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("toggle_menu", [this](const std::string& action, float value) { handleAction(action, value); });
    input->addActionCallback("sprint", [this](const std::string& action, float value) { handleAction(action, value); });
}

void Engine::handleMouseMove(double x, double y) {
    if (rightMousePressed) {
        camera->handleMouseMovement(static_cast<float>(x), static_cast<float>(y));
    }
}

void Engine::handleMouseButton(Window::MouseButton button, bool pressed) {
    switch (button) {
        case Window::MouseButton::RIGHT:
            rightMousePressed = pressed;
            window->setCursorMode(rightMousePressed);
            if (pressed) {
                double x, y;
                window->getCursorPosition(x, y);
                camera->handleMouseMovement(static_cast<float>(x), static_cast<float>(y));
            }
            break;
        case Window::MouseButton::LEFT:
            leftMousePressed = pressed;
            break;
        default:
            break;
    }
}

void Engine::handleMouseScroll(double yOffset) {
    camera->handleMouseScroll(static_cast<float>(yOffset));
}

void Engine::handleKeyEvent(int key, int action) {
    input->handleKeyEvent(key, action);
}

void Engine::handleAction(const std::string& action, float value) {
    // Handle movement actions
    if (action == "move_forward") {
        // Forward movement logic
    } else if (action == "move_backward") {
        // Backward movement logic
    } else if (action == "move_left") {
        // Left movement logic
    } else if (action == "move_right") {
        // Right movement logic
    } else if (action == "move_up") {
        // Up movement logic
    } else if (action == "move_down") {
        // Down movement logic
    }
    
    // Handle other actions
    else if (action == "interact") {
        // Interaction logic
    } else if (action == "toggle_menu") {
        // Menu toggle logic
    } else if (action == "sprint") {
        // Sprint logic
    }
}

} // namespace voxceleron 