module;
#include "Math/Glm.hpp"
#include <memory>

export module Engine.Scene.SceneObject;

import Engine.Object.Model; // Import Model definition

export namespace Engine
{
    class SceneObject
    {
    public:
        SceneObject(std::shared_ptr<Model> model) : model_(model) {}
        ~SceneObject() = default;

        void SetTransform(const glm::mat4& transform) { transform_ = transform; }
        const glm::mat4& GetTransform() const { return transform_; }

        Engine::Model* GetModel() const { return model_.get(); }

    private:
        std::shared_ptr<Model> model_;
        glm::mat4 transform_ = glm::mat4(1.0f);
    };
}
