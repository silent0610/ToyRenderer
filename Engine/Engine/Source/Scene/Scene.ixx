module;
#include "Math/Glm.hpp"
#include <vector>
#include <memory>

export module Engine.Scene;

export import Engine.Scene.SceneObject;
export import Engine.Object.Model;
import Engine.Camera;
export namespace Engine
{
    class Scene
    {
    public:
        Scene();
        ~Scene();

        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;

        // MVP 阶段：仅存储背景清除颜色
        glm::vec4 ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};

        void AddObject(std::unique_ptr<SceneObject> object);
        
        const std::vector<std::unique_ptr<SceneObject>>& GetObjects() const { return objects_; }

        void OnResize(float width, float height);
        Camera* GetMainCamera();
        const Camera* GetMainCamera() const;
    private:
        std::vector<std::unique_ptr<SceneObject>> objects_;
        std::unique_ptr<Camera> mainCamera_{};
    };
}
