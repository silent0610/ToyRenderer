module;
#include "GLFW/glfw3.h"
#include <chrono>
#include <cmath>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module Camera;
import Math;
import Logger;

// First-person camera with WASD + mouse controls
export class Camera {
private:
    // Camera vectors
    Math::Vector3 position_;
    Math::Vector3 front_;
    Math::Vector3 up_;
    Math::Vector3 right_;
    Math::Vector3 worldUp_;
    
    // Camera angles (FPS style)
    float yaw_;   // Y-axis rotation (left/right)
    float pitch_; // X-axis rotation (up/down)
    // Note: No roll for FPS camera
    
    // Camera settings
    float movementSpeed_;
    float mouseSensitivity_;
    float zoom_;
    
    // Mouse input
    float lastX_, lastY_;
    bool firstMouse_;
    
    // Timing
    std::chrono::steady_clock::time_point lastTime_;

public:
    Camera(Math::Vector3 position = Math::Vector3(0.0f, 0.0f, 3.0f),
           Math::Vector3 up = Math::Vector3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f,
           float pitch = 0.0f)
        : position_(position), worldUp_(up), yaw_(yaw), pitch_(pitch),
          movementSpeed_(5.0f), mouseSensitivity_(0.05f), zoom_(45.0f),
          lastX_(400.0f), lastY_(300.0f), firstMouse_(true),
          lastTime_(std::chrono::steady_clock::now())
    {
        UpdateCameraVectors();
    }
    
    // Get view matrix using LookAt
    Math::Matrix4 GetViewMatrix()const {
        return Math::CreateLookAt(position_, Math::Add(position_, front_), up_);
    }
    
    // Process keyboard input
    void ProcessKeyboard(GLFWwindow* window) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime_).count();
        lastTime_ = currentTime;
        
        // Calculate movement speed (with sprint modifier)
        float currentSpeed = movementSpeed_;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            currentSpeed *= 2.5f; // Sprint speed multiplier
        }
        float velocity = currentSpeed * deltaTime;
        
        // Standard FPS movement controls (WASD)
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position_ = Math::Add(position_, Math::Mul(front_, velocity));
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position_ = Math::Sub(position_, Math::Mul(front_, velocity));
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position_ = Math::Sub(position_, Math::Mul(right_, velocity));
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position_ = Math::Add(position_, Math::Mul(right_, velocity));
        
        // Vertical movement (Space to fly up, Shift to fly down)
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            position_ = Math::Add(position_, Math::Mul(worldUp_, velocity));
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            position_ = Math::Sub(position_, Math::Mul(worldUp_, velocity));
    }
    
    // Process mouse movement
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= mouseSensitivity_;
        yoffset *= mouseSensitivity_;
        
        yaw_ += xoffset;
        pitch_ += yoffset;
        
        // Constrain pitch to avoid screen flip
        if (constrainPitch) {
            if (pitch_ > 89.0f)
                pitch_ = 89.0f;
            if (pitch_ < -89.0f)
                pitch_ = -89.0f;
        }
        
        UpdateCameraVectors();
    }
    
    // Handle mouse callback
    void MouseCallback(GLFWwindow* window, double xpos, double ypos) {
        if (firstMouse_) {
            lastX_ = static_cast<float>(xpos);
            lastY_ = static_cast<float>(ypos);
            firstMouse_ = false;
        }
        
        float xoffset = static_cast<float>(xpos) - lastX_;
        float yoffset = lastY_ - static_cast<float>(ypos); // Reversed since y-coordinates go from bottom to top
        
        lastX_ = static_cast<float>(xpos);
        lastY_ = static_cast<float>(ypos);
        
        ProcessMouseMovement(xoffset, yoffset);
    }
    
    // Getters
    Math::Vector3 GetPosition() const { return position_; }
    Math::Vector3 GetFront() const { return front_; }
    Math::Vector3 GetUp() const { return up_; }
    Math::Vector3 GetRight() const { return right_; }
    float GetZoom() const { return zoom_; }
    
    // Setters
    void SetPosition(const Math::Vector3& position) { 
        position_ = position; 
        Log::Debug(std::format("Camera position set to ({:.2f}, {:.2f}, {:.2f})", position.x, position.y, position.z));
    }
    void SetMovementSpeed(float speed) { movementSpeed_ = speed; }
    void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

private:
    // Update camera vectors based on yaw and pitch (standard FPS camera)
    void UpdateCameraVectors() {
        // Calculate front vector from yaw and pitch
        Math::Vector3 front;
        front.x = std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
        front.y = std::sin(glm::radians(pitch_));
        front.z = std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
        front_ = Math::normalize(front);
        
        // Calculate right and up vectors
        right_ = Math::normalize(Math::cross(front_, worldUp_));
        up_ = Math::normalize(Math::cross(right_, front_));
    }
};