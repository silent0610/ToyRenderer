module;
#include "spdlog/spdlog.h"
module Engine.Scene;

import std;

namespace Engine
{
    Scene::Scene()
    {
        spdlog::info("Scene Initialized");
        mainCamera_ = std::make_unique<Camera>();
        mainCamera_->SetPosition({0.0f, 0.0f, 5.0f});
    }

    Scene::~Scene()
    {
        spdlog::info("Scene Destroyed");
    }

    void Scene::AddObject(std::unique_ptr<SceneObject> object)
    {
        objects_.push_back(std::move(object));
    }


    Camera* Scene::GetMainCamera()
    {
        return mainCamera_.get();
    }

    const Camera* Scene::GetMainCamera() const
    {
        return mainCamera_.get();
    }

    void Scene::OnResize(float width, float height)
    {
        mainCamera_->OnResize(width, height);
    }
}