module;
#include <GLFW/glfw3.h>
export module Application;
import Engine.Window;
import std;
import Engine;

export class Application
{
public:
    virtual ~Application() = default;

    void Run()
    {
        engine_.Init();
        OnInit();
        const float kFixedTimeStep = 1.0f / 60.0f; // TODO 物理模拟固定时间步长
        auto window = engine_.GetWindow();
        float lastTime = (float)glfwGetTime();

        while (!window->ShouldClose())
        {
            window->PollEvents();

            float currentTime = (float)glfwGetTime();
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;

            OnUpdate(deltaTime);
            engine_.Tick();
        }

        OnShutdown();
        engine_.Shutdown();
    }

protected:
    Engine::MyEngine engine_;

    virtual void OnInit() {}
    virtual void OnUpdate(float dt) {}
    virtual void OnShutdown() {}
};