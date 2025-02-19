#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace voxceleron {

class Window;

class Camera {
public:
    enum class Movement {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    enum class State {
        IDLE,
        MOVING,
        ROTATING
    };

    struct MovementSettings {
        float moveSpeed = 5.0f;          // Units per second
        float mouseSensitivity = 0.1f;   // Degrees per pixel
        float smoothness = 0.1f;         // Movement interpolation factor (0-1)
        float minPitch = -89.0f;         // Minimum pitch angle in degrees
        float maxPitch = 89.0f;          // Maximum pitch angle in degrees
        float fov = 45.0f;               // Field of view in degrees
        float nearPlane = 0.1f;          // Near clipping plane
        float farPlane = 1000.0f;        // Far clipping plane
    };

    Camera();
    ~Camera() = default;

    // Initialization
    void initialize(Window* window);
    void setMovementSettings(const MovementSettings& settings) { this->settings = settings; }

    // Update and state
    void update(float deltaTime);
    State getState() const { return state; }

    // Camera control
    void move(Movement direction, float value);
    void handleKeyInput(float deltaTime);
    void handleMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);
    void handleMouseScroll(float yOffset);

    // Position and orientation
    void setPosition(const glm::vec3& position);
    void setRotation(float pitch, float yaw);
    void lookAt(const glm::vec3& target);

    // Getters
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getFront() const { return front; }
    const glm::vec3& getUp() const { return up; }
    const glm::vec3& getRight() const { return right; }
    float getPitch() const { return pitch; }
    float getYaw() const { return yaw; }
    float getFov() const { return settings.fov; }

    // Matrices
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    // Frustum planes for culling
    struct Frustum {
        glm::vec4 planes[6];  // Left, Right, Bottom, Top, Near, Far
    };
    Frustum getFrustum() const;
    
private:
    // Core components
    Window* window;
    State state;
    MovementSettings settings;

    // Position and orientation
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler angles
    float pitch;
    float yaw;

    // Movement
    glm::vec3 targetPosition;
    glm::vec3 velocity;
    bool firstMouse;
    float lastX;
    float lastY;

    // Helper functions
    void updateCameraVectors();
    void smoothMove(const glm::vec3& targetPos, float deltaTime);
    void constrainAngles();

    // Prevent copying
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
};

} // namespace voxceleron 