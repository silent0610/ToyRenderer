module;
#include "Math/Glm.hpp"
export module Engine.Camera.Controller;

import Engine.Camera;
import Engine.Input;

export namespace Engine
{
    class FPSCameraController
    {
    public:
        FPSCameraController() = default;

        void SetCamera(Camera* camera)
        {
            camera_ = camera;
        }

        void SetSpeed(float moveSpeed, float sensitivity)
        {
            moveSpeed_ = moveSpeed;
            sensitivity_ = sensitivity;
        }

        void Update(float dt)
        {
            if (!camera_) return;

            // 移动
            float currentSpeed = moveSpeed_ * dt;
            
            // 按住 Shift 加速
            if (Input::IsKeyDown(KeyCode::LeftShift))
            {
                currentSpeed *= 3.0f;
            }

            if (Input::IsKeyDown(KeyCode::W)) camera_->Move(0, 0, currentSpeed);
            if (Input::IsKeyDown(KeyCode::S)) camera_->Move(0, 0, -currentSpeed);
            if (Input::IsKeyDown(KeyCode::A)) camera_->Move(-currentSpeed, 0, 0);
            if (Input::IsKeyDown(KeyCode::D)) camera_->Move(currentSpeed, 0, 0);

            // Roll (歪头) - 使用 Q/E
            float rollSpeed = 1.0f * dt;
            if (Input::IsKeyDown(KeyCode::Q)) camera_->Rotate(0, 0, rollSpeed);
            if (Input::IsKeyDown(KeyCode::E)) camera_->Rotate(0, 0, -rollSpeed);

            // 鼠标旋转
            double dx, dy;
            Input::GetMouseDelta(dx, dy);
            
            // 注意：通常不需要乘 dt，因为鼠标移动已经是“帧间位移”了
            camera_->Rotate(-(float)dx * sensitivity_, -(float)dy * sensitivity_);
        }

    private:
        Camera* camera_ = nullptr;
        float moveSpeed_ = 5.0f;
        float sensitivity_ = 0.002f;
    };
}
