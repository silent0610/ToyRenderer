module;
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Math/Glm.hpp"
#include "spdlog/spdlog.h"
#include "tiny_gltf.h"

module Engine.Object.Model;
import Engine.Rhi.Definition;
import Engine.Helper;
namespace Engine
{
    Model::Model(Rhi::RhiDevice* device, std::filesystem::path& path, LoadFlag flag) : device_(device), loadFlag_(flag), modelPath_{path}
    {
        
        if (!LoadFromFile(modelPath_,loadFlag_))
        {
            throw std::runtime_error("Failed to load model from file: " + modelPath_.string());
        }
        
    }
    bool Model::LoadFromFile(const std::filesystem::path& filepath,LoadFlag flag)
    {
        tinygltf::Model inputModel;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        modelPath_ = filepath;
        std::string pathStr = filepath.string();

        bool ret = false;
        if (filepath.extension() == ".glb")
        {
            ret = loader.LoadBinaryFromFile(&inputModel, &err, &warn, pathStr);
        }
        else
        {
            ret = loader.LoadASCIIFromFile(&inputModel, &err, &warn, pathStr);
        }

        if (!warn.empty())
        {
            spdlog::warn("glTF Warning: {}", warn);
        }

        if (!err.empty())
        {
            spdlog::error("glTF Error: {}", err);
        }

        if (!ret)
        {
            spdlog::error("Failed to load glTF file: {}", pathStr);
            return false;
        }

        // 1. 加载 Texture
        LoadTextures(inputModel);
        // 2. 加载 Material
        LoadMaterials(inputModel);

        LoadMeshes(inputModel);

        // 3. 预处理 Scene/Node 结构
        const tinygltf::Scene& scene = inputModel.scenes[inputModel.defaultScene > -1 ? inputModel.defaultScene : 0];

        // ... 这里简化实现，假设我们只处理第一个 Scene ...

        // 4. 构建节点树
        nodes_.resize(inputModel.nodes.size()); // 预分配
        for (size_t i = 0; i < inputModel.nodes.size(); i++)
        {
            // 初始化基本信息，LoadNode 会填充层级关系
            nodes_[i].Name = inputModel.nodes[i].name;
        }

        // 从 Root 开始递归
        for (int nodeIdx : scene.nodes)
        {
            rootNodes_.push_back(nodeIdx);
            LoadNode(inputModel.nodes[nodeIdx], inputModel, nullptr, nodeIdx);
        }

        // 5. 上传 GPU Buffer
        if (!allVertices_.empty())
        {
            Rhi::BufferDesc vDesc;
            vDesc.Size = allVertices_.size() * sizeof(Vertex);
            vDesc.Usage = Rhi::BufferUsage::VertexBuffer | Rhi::BufferUsage::TransferDst;
            vDesc.MemoryUsage = Rhi::BufferMemoryUsage::GpuOnly;
            vertexBuffer_ = device_->CreateBuffer(vDesc);

            // 使用临时 Staging Buffer 上传 (简化起见，假设 Device 支持直接上传或者内部处理了 Staging)
            // 实际上 RhiDevice::CreateBuffer(CpuToGpu) + Copy 才是标准做法
            // 这里我们假设 Device 有 UploadData 方法
            device_->UploadBufferData(vertexBuffer_.get(), allVertices_.data(), vDesc.Size);
        }

        if (!allIndices_.empty())
        {
            Rhi::BufferDesc iDesc;
            iDesc.Size = allIndices_.size() * sizeof(uint32_t);
            iDesc.Usage = Rhi::BufferUsage::IndexBuffer | Rhi::BufferUsage::TransferDst;
            iDesc.MemoryUsage = Rhi::BufferMemoryUsage::GpuOnly;
            indexBuffer_ = device_->CreateBuffer(iDesc);

            device_->UploadBufferData(indexBuffer_.get(), allIndices_.data(), iDesc.Size);
        }

        return true;
    }

    void Model::LoadTextures(const tinygltf::Model& inputModel)
    {
        textures_.resize(inputModel.textures.size());
        for (size_t i = 0; i < inputModel.textures.size(); i++)
        {
            const auto& texInfo = inputModel.textures[i];
            if (texInfo.source > -1)
            {
                const auto& image = inputModel.images[texInfo.source];
                textures_[i] = LoadTexture(image, image.name);
            }
        }
    }

    std::shared_ptr<Rhi::RhiTexture> Model::LoadTexture(const tinygltf::Image& image, const std::string& debugName)
    {
        Rhi::TextureDesc desc;
        desc.Width = image.width;
        desc.Height = image.height;
        desc.Name = debugName;
        desc.Usage = Rhi::TextureUsage::Sampled | Rhi::TextureUsage::TransferDst;
        std::vector<unsigned char> convertedBuffer;

        const unsigned char* bufferData = nullptr;
        size_t bufferSize = 0;

        // 判断格式
        // 如果是 RGB 则转换为 RGBA
        if (image.component == 3)
        {
            desc.Format = Rhi::PixelFormat::R8G8B8A8UNORM; // glTF image data is usually packed
            convertedBuffer = Engine::Helper::ConvertRgbToRgba(image.image.data(), image.width, image.height);

            bufferData = convertedBuffer.data();
            bufferSize = convertedBuffer.size();
        }
        if (image.component == 4)
        {
            desc.Format = Rhi::PixelFormat::R8G8B8A8UNORM; // glTF image data is usually packed
            bufferData = image.image.data();
            bufferSize = image.image.size();
        }
        else
        {
            // 处理其他情况，通常需要转换
            desc.Format = Rhi::PixelFormat::R8G8B8A8UNORM;
        }

        auto texture = device_->CreateTexture(desc);

        // 上传数据
        device_->UploadTextureData(texture.get(), bufferData, bufferSize);

        return texture;
    }

    void Model::LoadMaterials(const tinygltf::Model& inputModel)
    {
        materials_.resize(inputModel.materials.size());
        for (size_t i = 0; i < inputModel.materials.size(); i++)
        {
            const auto& matIn = inputModel.materials[i];
            auto& matOut = materials_[i];
            matOut.Name = matIn.name;

            // PBR
            if (matIn.values.find("baseColorFactor") != matIn.values.end())
            {
                const auto& factor = matIn.values.at("baseColorFactor").ColorFactor();
                matOut.BaseColorFactor = glm::vec4(factor[0], factor[1], factor[2], factor[3]);
            }
            else
            {
                matOut.BaseColorFactor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            }

            if (matIn.values.find("baseColorTexture") != matIn.values.end())
            {
                int texIndex = matIn.values.at("baseColorTexture").TextureIndex();
                if (texIndex >= 0 && texIndex < textures_.size())
                {
                    matOut.BaseColorTexture = textures_[texIndex];
                }
            }
            if (matIn.values.find("metallicFactor") != matIn.values.end())
            {
                matOut.MetallicFactor = static_cast<float>(matIn.values.at("metallicFactor").Factor());
            }
            else
            {
                matOut.MetallicFactor = 1.0f; // 默认值
            }
            if (matIn.values.find("roughnessFactor") != matIn.values.end())
            {
                matOut.RoughnessFactor = static_cast<float>(matIn.values.at("roughnessFactor").Factor());
            }
            else
            {
                matOut.RoughnessFactor = 1.0f;
            }

            if (matIn.values.find("metallicRoughnessTexture") != matIn.values.end())
            {
                int texIndex = matIn.values.at("metallicRoughnessTexture").TextureIndex();
                if (texIndex >= 0 && texIndex < textures_.size())
                {
                    matOut.MetallicRoughnessTexture = textures_[texIndex];
                }
            }

            // other maps;
            if (matIn.additionalValues.find("normalTexture") != matIn.additionalValues.end())
            {
                int texIndex = matIn.additionalValues.at("normalTexture").TextureIndex();
                if (texIndex >= 0 && texIndex < textures_.size())
                {
                    matOut.NormalTexture = textures_[texIndex];
                }
            }
            if (matIn.additionalValues.find("emissiveTexture") != matIn.additionalValues.end())
            {
                int texIndex = matIn.additionalValues.at("emissiveTexture").TextureIndex();
                if (texIndex >= 0 && texIndex < textures_.size())
                {
                    matOut.EmissiveTexture = textures_[texIndex];
                }
            }
            if (matIn.additionalValues.find("occlusionTexture") != matIn.additionalValues.end())
            {
                int texIndex = matIn.additionalValues.at("occlusionTexture").TextureIndex();
                if (texIndex >= 0 && texIndex < textures_.size())
                {
                    matOut.OcclusionTexture = textures_[texIndex];
                }
            }
        }
    }

    void Model::LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& inputModel, Node* parent, uint32_t nodeIndex)
    {
        Node& node = nodes_[nodeIndex];
        node.Parent = parent;

        node.LocalTransform = glm::mat4(1.0f);
        // Transform
        if (inputNode.matrix.size() == 16)
        {
            node.LocalTransform = glm::make_mat4(inputNode.matrix.data());
        }
        else
        {
            glm::mat4 translation = glm::mat4(1.0f);
            glm::mat4 rotation = glm::mat4(1.0f);
            glm::mat4 scale = glm::mat4(1.0f);

            if (inputNode.translation.size() == 3)
            {
                translation = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(static_cast<float>(inputNode.translation[0]), static_cast<float>(inputNode.translation[1]),
                                                       static_cast<float>(inputNode.translation[2])));
            }

            if (inputNode.rotation.size() == 4)
            {
                // glTF rotation is quaternion [x, y, z, w]
                // 需要确保包含 glm/gtc/quaternion.hpp
                glm::quat q = glm::make_quat(inputNode.rotation.data());
                rotation = glm::mat4_cast(q);
            }

            if (inputNode.scale.size() == 3)
            {
                scale = glm::scale(glm::mat4(1.0f), glm::vec3(static_cast<float>(inputNode.scale[0]), static_cast<float>(inputNode.scale[1]),
                                                              static_cast<float>(inputNode.scale[2])));
            }
            node.LocalTransform = translation * rotation * scale;
        }

        // Mesh
        if (inputNode.mesh > -1)
        {
            // 我们不再解析顶点，而是直接指向已经加载好的 Mesh
            node.MeshIndex = inputNode.mesh;

            // 注意：这里没有任何 Vertices 的 push_back 操作！
            // 100 个 Node 指向同一个 inputNode.mesh，内存里只有一份数据。
        }

        for (int childIdx : inputNode.children)
        {
            node.Children.push_back(&nodes_[childIdx]);
            LoadNode(inputModel.nodes[childIdx], inputModel, &node, childIdx);
        }
    }

    void Model::LoadMeshes(const tinygltf::Model& inputModel)
    {
        meshes_.resize(inputModel.meshes.size());

        // 遍历每个 Mesh

        for (size_t i = 0; i < inputModel.meshes.size(); ++i)
        {
            const auto& meshIn = inputModel.meshes[i];
            Mesh& meshOut = meshes_[i];
            meshOut.Name = meshIn.name;

            for (const auto& primitive : meshIn.primitives)
            {
                SubMesh subMesh;
                // 记录当前已经有的索引数量作为 FirstIndex
                subMesh.FirstIndex = static_cast<uint32_t>(allIndices_.size());
                subMesh.MaterialIndex = primitive.material;

                // 记录当前的顶点数量，作为 BaseVertex (用于索引偏移)
                uint32_t vertexStart = static_cast<uint32_t>(allVertices_.size());

                // 1. 读取顶点属性 (Attributes)
                const float* positionBuffer = nullptr;
                const float* normalsBuffer = nullptr;
                const float* texCoordsBuffer = nullptr;
                const float* tangentsBuffer = nullptr;

                size_t vertexCount = 0;

                // Position
                if (primitive.attributes.find("POSITION") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = inputModel.accessors[primitive.attributes.at("POSITION")];
                    const tinygltf::BufferView& view = inputModel.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }

                // Normal
                if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = inputModel.accessors[primitive.attributes.at("NORMAL")];
                    const tinygltf::BufferView& view = inputModel.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // UV
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = inputModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                    const tinygltf::BufferView& view = inputModel.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // Tangent
                if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = inputModel.accessors[primitive.attributes.at("TANGENT")];
                    const tinygltf::BufferView& view = inputModel.bufferViews[accessor.bufferView];
                    tangentsBuffer = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // 组装 Vertex 数据
                for (size_t v = 0; v < vertexCount; v++)
                {
                    Vertex vert{};

                    vert.Pos = glm::vec3(positionBuffer[v * 3], positionBuffer[v * 3 + 1], positionBuffer[v * 3 + 2]);

                    if (normalsBuffer)
                    {
                        vert.Normal = glm::vec3(normalsBuffer[v * 3], normalsBuffer[v * 3 + 1], normalsBuffer[v * 3 + 2]);
                    }
                    else
                    {
                        vert.Normal = glm::vec3(0.0f); // Fallback
                    }

                    if (texCoordsBuffer)
                    {
                        vert.UV = glm::vec2(texCoordsBuffer[v * 2], texCoordsBuffer[v * 2 + 1]);
                    }
                    else
                    {
                        vert.UV = glm::vec2(0.0f);
                    }

                    if (tangentsBuffer)
                    {
                        vert.Tangent =
                            glm::vec4(tangentsBuffer[v * 4], tangentsBuffer[v * 4 + 1], tangentsBuffer[v * 4 + 2], tangentsBuffer[v * 4 + 3]);
                    }
                    else
                    {
                        vert.Tangent = glm::vec4(0.0f);
                    }

                    allVertices_.push_back(vert);
                }

                // 2. 读取索引 (Indices)
                if (primitive.indices > -1)
                {
                    const tinygltf::Accessor& accessor = inputModel.accessors[primitive.indices];
                    const tinygltf::BufferView& bufferView = inputModel.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = inputModel.buffers[bufferView.buffer];

                    const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                    switch (accessor.componentType)
                    {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    {
                        const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            // 加上 vertexStart 偏移，因为我们把所有 Mesh 的顶点都合并到了一个 Buffer
                            allIndices_.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            allIndices_.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_BYTE:
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                        const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            allIndices_.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    default:
                        spdlog::error("Index component type {} not supported!", accessor.componentType);
                        return;
                    }
                    subMesh.IndexCount = static_cast<uint32_t>(accessor.count);
                }

                meshOut.SubMeshes.push_back(subMesh);
            }
        }
    }
    void Model::BakeMaterials(Rhi::DescriptorSetLayout* layout, Rhi::Sampler* sampler, Rhi::RhiTexture* defaultWhite)
    {
        for (auto& mat : materials_)
        {
            mat.DescriptorSet = device_->CreateDescriptorSet(layout,false);

            mat.DescriptorSet->UpdateTexture(0, mat.BaseColorTexture ? mat.BaseColorTexture.get() : defaultWhite, sampler);
            mat.DescriptorSet->UpdateTexture(1, mat.NormalTexture ? mat.NormalTexture.get() : defaultWhite, sampler);
            mat.DescriptorSet->UpdateTexture(2, mat.MetallicRoughnessTexture ? mat.MetallicRoughnessTexture.get() : defaultWhite, sampler);
        }
    }
} // namespace Engine
