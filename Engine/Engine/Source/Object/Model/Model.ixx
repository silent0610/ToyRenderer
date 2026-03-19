module;

#include "tiny_gltf.h" // 假设 tinygltf 已经包含在项目中

export module Engine.Object.Model;

export import Engine.Object.Model.Definition;
import Engine.Rhi.Device;
import Engine.Rhi.Buffer;
import Engine.Rhi.Texture;
import Engine.Rhi.CommandList;
import Engine.Rhi.Descriptor;
import Engine.Rhi.Sampler;
import std;

export namespace Engine
{
    struct LoadFlag
    {
        bool FlipY = true; // 是否垂直翻转纹理坐标
    };

    class Model
    {
    public:
        Model() = delete;
        Model(Rhi::RhiDevice* device, std::filesystem::path& path, LoadFlag flag);
        ~Model() = default;

        
        
        // 资源访问
        const std::vector<Mesh>& GetMeshes() const { return meshes_; }
        const std::vector<Material>& GetMaterials() const { return materials_; }
        const std::vector<Node>& GetNodes() const { return nodes_; } // 线性存储所有节点
        const std::vector<int>& GetRootNodes() const { return rootNodes_; } // 根节点索引

        // 统一的顶点/索引 Buffer (为了性能，通常合并所有 Mesh 到一个大 Buffer)
        Rhi::RhiBuffer* GetVertexBuffer() const { return vertexBuffer_.get(); }
        Rhi::RhiBuffer* GetIndexBuffer() const { return indexBuffer_.get(); }


        void BakeMaterials(Rhi::DescriptorSetLayout* layout, Rhi::Sampler* sampler, Rhi::RhiTexture* defaultWhite);

    private:
        bool LoadFromFile(const std::filesystem::path& filepath, LoadFlag flag);
        void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& inputModel, Node* parent, uint32_t nodeIndex);
        void LoadMaterials(const tinygltf::Model& inputModel);
        void LoadTextures(const tinygltf::Model& inputModel);
        void LoadMeshes(const tinygltf::Model& inputModel);
        
        // 辅助函数
        std::shared_ptr<Rhi::RhiTexture> LoadTexture(const tinygltf::Image& image, const std::string& debugName);
        
        Rhi::RhiDevice* device_;
        std::filesystem::path modelPath_;

        std::vector<Mesh> meshes_;
        std::vector<Material> materials_;
        Rhi::DescriptorSetLayout* setLayout_;
        std::vector<std::shared_ptr<Rhi::RhiTexture>> textures_;
        std::vector<Node> nodes_; // 所有节点线性数组
        std::vector<int> rootNodes_;

        std::unique_ptr<Rhi::RhiBuffer> vertexBuffer_;
        std::unique_ptr<Rhi::RhiBuffer> indexBuffer_;
        
        // 临时存储，用于合并 Buffer
        std::vector<Vertex> allVertices_;
        std::vector<uint32_t> allIndices_;

        LoadFlag loadFlag_{};
    };
}
