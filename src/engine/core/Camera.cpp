#include "Camera.h"
#include "Window.h"
#include <algorithm>
#include <iostream>

namespace voxceleron {

Camera::Camera()
    : window(nullptr)
    , state(State::IDLE)
    , position(0.0f, 0.0f, 0.0f)
    , front(0.0f, 0.0f, -1.0f)
    , up(0.0f, 1.0f, 0.0f)
    , right(1.0f, 0.0f, 0.0f)
    , worldUp(0.0f, 1.0f, 0.0f)
    , pitch(0.0f)
    , yaw(-90.0f)
    , targetPosition(0.0f, 0.0f, 0.0f)
    , velocity(0.0f)
    , firstMouse(true)
    , lastX(0.0f)
    , lastY(0.0f) {
    std::cout << "Camera: Creating camera instance" << std::endl;
    updateCameraVectors();
}

void Camera::initialize(Window* window) {
    std::cout << "Camera: Starting initialization..." << std::endl;
    this->window = window;
    lastX = static_cast<float>(window->getWidth() / 2);
    lastY = static_cast<float>(window->getHeight() / 2);
    targetPosition = position;
    std::cout << "Camera: Initialization complete" << std::endl;
}

void Camera::update(float deltaTime) {
    handleKeyInput(deltaTime);
    smoothMove(targetPosition, deltaTime);
    updateCameraVectors();
}

void Camera::handleKeyInput(float deltaTime) {
    if (!window) return;

    GLFWwindow* handle = window->getHandle();
    bool moving = false;
    glm::vec3 movement(0.0f);

    // Forward/Backward
    if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS) {
        movement += front;
        moving = true;
    }
    if (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS) {
        movement -= front;
        moving = true;
    }

    // Left/Right
    if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS) {
        movement -= right;
        moving = true;
    }
    if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS) {
        movement += right;
        moving = true;
    }

    // Up/Down
    if (glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS) {
        movement += worldUp;
        moving = true;
    }
    if (glfwGetKey(handle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        movement -= worldUp;
        moving = true;
    }

    if (moving) {
        state = State::MOVING;
        movement = glm::normalize(movement);
        targetPosition += movement * settings.moveSpeed * deltaTime;
    } else {
        state = State::IDLE;
    }
}

void Camera::handleMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
    if (firstMouse) {
        lastX = xOffset;
        lastY = yOffset;
        firstMouse = false;
        return;
    }

    float xDiff = xOffset - lastX;
    float yDiff = lastY - yOffset;
    lastX = xOffset;
    lastY = yOffset;

    xDiff *= settings.mouseSensitivity;
    yDiff *= settings.mouseSensitivity;

    yaw += xDiff;
    pitch += yDiff;

    if (constrainPitch) {
        constrainAngles();
    }

    state = State::ROTATING;
    updateCameraVectors();
}

void Camera::handleMouseScroll(float yOffset) {
    settings.fov -= yOffset;
    settings.fov = std::clamp(settings.fov, 1.0f, 90.0f);
}

void Camera::setPosition(const glm::vec3& position) {
    this->position = position;
    this->targetPosition = position;
    updateCameraVectors();
}

void Camera::setRotation(float pitch, float yaw) {
    this->pitch = pitch;
    this->yaw = yaw;
    constrainAngles();
    updateCameraVectors();
}

void Camera::lookAt(const glm::vec3& target) {
    glm::vec3 direction = glm::normalize(target - position);
    pitch = glm::degrees(asin(direction.y));
    yaw = glm::degrees(atan2(direction.z, direction.x));
    constrainAngles();
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(settings.fov), aspectRatio, settings.nearPlane, settings.farPlane);
}

Camera::Frustum Camera::getFrustum() const {
    Frustum frustum;
    glm::mat4 viewProj = getProjectionMatrix(window->getAspectRatio()) * getViewMatrix();

    // Extract frustum planes from view-projection matrix
    // Left plane
    frustum.planes[0].x = viewProj[0][3] + viewProj[0][0];
    frustum.planes[0].y = viewProj[1][3] + viewProj[1][0];
    frustum.planes[0].z = viewProj[2][3] + viewProj[2][0];
    frustum.planes[0].w = viewProj[3][3] + viewProj[3][0];

    // Right plane
    frustum.planes[1].x = viewProj[0][3] - viewProj[0][0];
    frustum.planes[1].y = viewProj[1][3] - viewProj[1][0];
    frustum.planes[1].z = viewProj[2][3] - viewProj[2][0];
    frustum.planes[1].w = viewProj[3][3] - viewProj[3][0];

    // Bottom plane
    frustum.planes[2].x = viewProj[0][3] + viewProj[0][1];
    frustum.planes[2].y = viewProj[1][3] + viewProj[1][1];
    frustum.planes[2].z = viewProj[2][3] + viewProj[2][1];
    frustum.planes[2].w = viewProj[3][3] + viewProj[3][1];

    // Top plane
    frustum.planes[3].x = viewProj[0][3] - viewProj[0][1];
    frustum.planes[3].y = viewProj[1][3] - viewProj[1][1];
    frustum.planes[3].z = viewProj[2][3] - viewProj[2][1];
    frustum.planes[3].w = viewProj[3][3] - viewProj[3][1];

    // Near plane
    frustum.planes[4].x = viewProj[0][3] + viewProj[0][2];
    frustum.planes[4].y = viewProj[1][3] + viewProj[1][2];
    frustum.planes[4].z = viewProj[2][3] + viewProj[2][2];
    frustum.planes[4].w = viewProj[3][3] + viewProj[3][2];

    // Far plane
    frustum.planes[5].x = viewProj[0][3] - viewProj[0][2];
    frustum.planes[5].y = viewProj[1][3] - viewProj[1][2];
    frustum.planes[5].z = viewProj[2][3] - viewProj[2][2];
    frustum.planes[5].w = viewProj[3][3] - viewProj[3][2];

    // Normalize all planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(frustum.planes[i]));
        frustum.planes[i] /= length;
    }

    return frustum;
}

void Camera::updateCameraVectors() {
    // Calculate new front vector
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    // Recalculate right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::smoothMove(const glm::vec3& targetPos, float deltaTime) {
    if (position != targetPos) {
        position = glm::mix(position, targetPos, settings.smoothness);
        if (glm::distance(position, targetPos) < 0.01f) {
            position = targetPos;
        }
    }
}

void Camera::constrainAngles() {
    pitch = std::clamp(pitch, settings.minPitch, settings.maxPitch);
    yaw = fmod(yaw, 360.0f);
}

} // namespace voxceleron 