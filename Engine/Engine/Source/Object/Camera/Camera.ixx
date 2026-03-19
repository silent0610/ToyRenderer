module;
#include "Math/Glm.hpp"
export module Engine.Camera;

export namespace Engine
{
    class Camera
    {
    public:
        Camera();
        ~Camera() = default;

        // --- 配置接口 ---

        // 设置透视投影
        // fovY: 垂直视场角 (Radians)
        // width, height: 视口宽高
        // nearZ, farZ: 裁剪平面
        void SetPerspective(float fovY, float width, float height, float nearZ, float farZ);

        // 设置视口尺寸 (用于窗口大小改变时)
        void OnResize(float width, float height);

        // --- 控制接口 (FPS 风格) ---

        // 绝对设置位置
        void SetPosition(const glm::vec3 &position);

        // 沿当前朝向移动 (local space)
        // dX: Right方向, dY: Up方向, dZ: Forward方向
        void Move(float dX, float dY, float dZ);

        // 旋转相机 (基于四元数)
        // yaw: 绕世界 Y 轴旋转 (左右看)
        // pitch: 绕本地 X 轴旋转 (上下看)
        void Rotate(float yawDelta, float pitchDelta, float rollDelta=0);

        // 强制更新视图矩阵 (通常在每帧渲染前调用)
        void UpdateViewMatrix() const;

        // --- 查询接口 (供渲染器使用) ---

        const glm::mat4 &GetViewMatrix() const;
        const glm::mat4 &GetProjectionMatrix() const;
        const glm::mat4 &GetViewProjectionMatrix() const; // VP 矩阵

        glm::vec3 GetPosition() const;
        glm::quat GetRotation() const;

        // 获取方向向量
        glm::vec3 GetForward() const;
        glm::vec3 GetRight() const;
        glm::vec3 GetUp() const;

    private:
        // 重新计算投影矩阵
        void RecalculateProjection();

    private:
        // --- 变换数据 ---
        glm::vec3 position_{0.0f, 0.0f, 0.0f};
        glm::quat rotation_{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion

        // --- 投影参数 ---
        float fovY_ = glm::radians(45.0f);
        float aspectRatio_ = 1.777f;
        float nearZ_ = 0.1f;
        float farZ_ = 1000.0f;

        // --- 矩阵缓存 ---
        mutable glm::mat4 viewMatrix_{1.0f};
        glm::mat4 projectionMatrix_{1.0f};
        mutable glm::mat4 viewProjectionMatrix_{1.0f};

        // 标记矩阵是否需要更新
        mutable bool viewDirty_ = true;
    };
}