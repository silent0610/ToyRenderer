module;

#include "Math/Glm.hpp"

export module Engine.Object.Model.Definition;
import std;
import Engine.Rhi.Buffer;
import Engine.Rhi.Texture;
import Engine.Rhi.Descriptor; // 如果材质需要描述符

export namespace Engine
{
    // 定义顶点结构，与 Vulkan 的 InputLayout 对应
    struct Vertex
    {
        glm::vec3 Pos;
        glm::vec3 Normal;
        glm::vec2 UV;
        glm::vec4 Tangent; // w 用于存储切线空间的 handedness
    };

    // 材质参数
    struct Material
    {
        std::string Name;
        
        // 基本 PBR 参数
        glm::vec4 BaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
        float MetallicFactor = 1.0f;
        float RoughnessFactor = 1.0f;

        // 纹理 (使用 std::shared_ptr 以便多个材质共享)
        // 如果纹理不存在，通常使用默认的 1x1 纯色纹理
        std::shared_ptr<Rhi::RhiTexture> BaseColorTexture;
        std::shared_ptr<Rhi::RhiTexture> NormalTexture;
        std::shared_ptr<Rhi::RhiTexture> MetallicRoughnessTexture;
        std::shared_ptr<Rhi::RhiTexture> OcclusionTexture;
        std::shared_ptr<Rhi::RhiTexture> EmissiveTexture;

        // Vulkan 描述符集 (Material Instance)
        // 用于绑定到 Shader
        std::unique_ptr<Rhi::RhiDescriptorSet> DescriptorSet;
    };

    // 子网格 (Primitive in glTF terms)
    struct SubMesh
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        int32_t MaterialIndex = -1; // 索引到 Model::Materials
    };

    // 网格 (Mesh)
    struct Mesh
    {
        std::string Name;
        std::vector<SubMesh> SubMeshes;
        
        // 顶点和索引数据由 Model 统一管理 (通常合并到一个大 Buffer)
        // 这里可以存储局部的 VertexBuffer/IndexBuffer 指针，或者只存储 Offset
    };

    // 场景节点
    struct Node
    {
        std::string Name;
        Node* Parent = nullptr;
        std::vector<Node*> Children;

        // 局部变换
        glm::mat4 LocalTransform = glm::mat4(1.0f);
        
        // 全局变换 (需要 Update)
        glm::mat4 WorldTransform = glm::mat4(1.0f);

        int32_t MeshIndex = -1; // 索引到 Model::Meshes
    };
}
