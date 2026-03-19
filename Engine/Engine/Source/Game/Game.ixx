module;
#include "Math/Glm.hpp"
export module MyApp;

import Application;
import Engine;
import Engine.Scene;
import Engine.Camera;
import Engine.Input;
import Engine.Renderer;
import Engine.Object.Model;
import Engine.Camera.Controller;
import std;

export namespace Engine
{
    class MyApp : public ::Application
    {
    public:
        MyApp() = default;

    protected:
        void OnInit() override;
        void OnUpdate(float dt) override;

    private:
        FPSCameraController controller_;
    };
}