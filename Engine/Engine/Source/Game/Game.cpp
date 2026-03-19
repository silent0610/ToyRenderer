module;
#include "Math/Glm.hpp"
#include <GLFW/glfw3.h>

module MyApp;
import std;
namespace Engine
{
    void MyApp::OnInit()
    {
        auto scene = engine_.GetScene();
        auto device = engine_.GetDevice();
        auto renderer = engine_.GetRenderer();

        // 1. 加载模型 (Game Layer 职责)
        std::filesystem::path modelPath = R"(E:\All\Projects\Engine\Engine\Asset\Model\cat\scene.gltf)";
        ::Engine::LoadFlag flag{false};
        auto model = std::make_shared<Engine::Model>(device, modelPath, flag);

        // 2. 准备材质
        model->BakeMaterials(
            renderer->GetStandardMaterialLayout(),
            renderer->GetStandardSampler(),
            renderer->GetDefaultWhiteTexture());

        // 3. 创建并添加场景对象
        auto object = std::make_unique<Engine::SceneObject>(model);
        object->SetTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)));
        scene->AddObject(std::move(object));

        // 4. 设置相机
        auto camera = scene->GetMainCamera();
        camera->SetPosition({0.0f, 2.0f, 5.0f});

        // 初始化控制器
        controller_.SetCamera(camera);

        // 5. 锁定鼠标
        Input::SetCursorMode(true);
    }

    void MyApp::OnUpdate(float dt)
    {
        auto window = engine_.GetWindow();

        // 更新输入状态
        Input::Update();

        // 处理退出
        if (Input::IsKeyDown(KeyCode::Esc))
        {
            glfwSetWindowShouldClose(static_cast<GLFWwindow *>(window->GetNativeHandle()), true);
        }

        // 使用控制器更新相机
        controller_.Update(dt);
    }
}