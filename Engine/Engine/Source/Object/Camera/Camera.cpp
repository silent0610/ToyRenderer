module;
#include "Math/Glm.hpp"
module Engine.Camera;

// 定义世界坐标系的 Y 轴
static constexpr glm::vec3 kWorldUp = glm::vec3(0.0f, 1.0f, 0.0f);

namespace Engine
{
    Camera::Camera()
    {
        UpdateViewMatrix();
        RecalculateProjection();
    }

    void Camera::SetPerspective(float fovY, float width, float height, float nearZ, float farZ)
    {
        fovY_ = fovY;
        aspectRatio_ = width / height;
        nearZ_ = nearZ;
        farZ_ = farZ;
        RecalculateProjection();
    }

    void Camera::OnResize(float width, float height)
    {
        if (height > 0.0f)
        {
            aspectRatio_ = width / height;
            RecalculateProjection();
        }
    }

    void Camera::SetPosition(const glm::vec3 &position)
    {
        position_ = position;
        viewDirty_ = true;
    }

    void Camera::Move(float dX, float dY, float dZ)
    {
        // 获取当前的基准向量
        // 注意：Vulkan/OpenGL 默认 Forward 是 -Z 方向
        glm::vec3 right = GetRight();
        glm::vec3 up = GetUp();
        glm::vec3 forward = GetForward();

        // 累加位置
        position_ += right * dX;
        position_ += up * dY;
        position_ += forward * dZ;

        viewDirty_ = true;
    }

    void Camera::Rotate(float yawDelta, float pitchDelta, float rollDelta)
    {
        // 1. Pitch (上下看): 绕着相机自己的 Right (本地 X) 轴旋转
        // 这是为了避免万向节死锁，同时符合 FPS 操作习惯
        // 构建绕本地 X 轴的四元数
        glm::quat pitchRotation = glm::angleAxis(pitchDelta, glm::vec3(1.0f, 0.0f, 0.0f));

        // 2. Yaw (左右看): 绕着 世界 UP (全局 Y) 轴旋转
        // FPS 相机一般是绕身体垂直轴转，而不是绕头顶转
        glm::quat yawRotation = glm::angleAxis(yawDelta, kWorldUp);

        glm::quat rollRotation = glm::angleAxis(rollDelta, glm::vec3(0.0f, 0.0f, 1.0f));
        // 3. 更新全局旋转四元数
        // 乘法顺序很重要：
        // 新旋转 = Yaw(全局) * 旧旋转 * Pitch(本地)
        // 这样可以保证 Yaw 始终相对于地面，而 Pitch 始终相对于当前视野
        rotation_ = yawRotation * rotation_ * pitchRotation * rollRotation;

        // 归一化四元数以防止浮点漂移
        rotation_ = glm::normalize(rotation_);

        viewDirty_ = true;
    }

    void Camera::UpdateViewMatrix() const
    {
        if (viewDirty_)
        {
            // View Matrix 是相机变换的逆矩阵
            // V = Inverse(T) * Inverse(R)

            // 1. 获取旋转矩阵 (从四元数)
            glm::mat4 rotateM = glm::mat4_cast(rotation_);

            // 2. 获取平移矩阵
            glm::mat4 translateM = glm::translate(glm::mat4(1.0f), position_);

            // 3. 组合 Camera World Matrix (相机在世界的变换)
            glm::mat4 camWorld = translateM * rotateM;

            // 4. View Matrix 取逆
            // TODO 优化：虽然正交矩阵的逆等于转置，但 glm::inverse 最通用且安全
            viewMatrix_ = glm::inverse(camWorld);

            // 更新 VP 矩阵
            viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;

            viewDirty_ = false;
        }
    }

    void Camera::RecalculateProjection()
    {
        // Vulkan 的裁剪空间 Y 轴是向下的，这是与 OpenGL 的区别
        // GLM 默认是为了 OpenGL 设计的 (Clip Space Y 向上)

        projectionMatrix_ = glm::perspective(fovY_, aspectRatio_, nearZ_, farZ_);

        // 【关键】修复 Vulkan Y 轴方向 (反转 Y)
        // 注意：如果你使用了 VK_KHR_maintenance1 的负视口(Negative Viewport)，
        // 则不需要这一步乘法。这里保留是因为它是最通用的修正方式。
        projectionMatrix_[1][1] *= -1.0f;

        // 更新 VP 矩阵
        viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;
    }

    // --- Getters ---

    const glm::mat4 &Camera::GetViewMatrix() const
    {
        UpdateViewMatrix();
        return viewMatrix_;
    }

    const glm::mat4 &Camera::GetProjectionMatrix() const
    {
        return projectionMatrix_;
    }

    const glm::mat4 &Camera::GetViewProjectionMatrix() const
    {
        UpdateViewMatrix();
        return viewProjectionMatrix_;
    }

    glm::vec3 Camera::GetPosition() const
    {
        return position_;
    }

    glm::quat Camera::GetRotation() const
    {
        return rotation_;
    }

    glm::vec3 Camera::GetForward() const
    {
        // 从四元数提取 Forward (-Z)
        return rotation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 Camera::GetRight() const
    {
        // 从四元数提取 Right (+X)
        return rotation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 Camera::GetUp() const
    {
        // 从四元数提取 Up (+Y)
        return rotation_ * glm::vec3(0.0f, 1.0f, 0.0f);
    }
}