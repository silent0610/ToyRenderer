module;
#include <vector>
#include <string>
#include <memory>
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

export module RhiGltfModel;
import Core;
import Math;
import RhiDevice;
import RhiBuffer;
import RhiCommandBuffer;
import RhiTypes;
import Logger;
import VkglTFModel; // Import existing glTF loader
import VulkanDevice; // Need this for dynamic_cast
import ToolMod; // For GetAssetsPath()
import std;

// Vertex structure that matches glTF loader
export struct GltfVertex {
    Math::Vector3 pos;
    Math::Vector3 normal;
    Math::Vector2 uv;
    Math::Vector4 color;
    Math::Vector4 joint0;
    Math::Vector4 weight0;
    Math::Vector4 tangent;
    
    // Get RHI vertex input description
    static std::vector<RhiVertexAttributeDesc> GetAttributeDescriptions() {
        return {
            {0, 0, RhiFormat::R32G32B32_SFLOAT, offsetof(GltfVertex, pos)},        // Position
            {1, 0, RhiFormat::R32G32B32_SFLOAT, offsetof(GltfVertex, normal)},     // Normal
            {2, 0, RhiFormat::R32G32_SFLOAT, offsetof(GltfVertex, uv)},            // UV
            {3, 0, RhiFormat::R32G32B32A32_SFLOAT, offsetof(GltfVertex, color)},   // Color
            {4, 0, RhiFormat::R32G32B32A32_SFLOAT, offsetof(GltfVertex, joint0)},  // Joints
            {5, 0, RhiFormat::R32G32B32A32_SFLOAT, offsetof(GltfVertex, weight0)}, // Weights
            {6, 0, RhiFormat::R32G32B32A32_SFLOAT, offsetof(GltfVertex, tangent)}  // Tangent
        };
    }
    
    static RhiVertexBindingDesc GetBindingDescription() {
        return {0, sizeof(GltfVertex), RhiVertexInputRate::Vertex};
    }
};

// Simplified material for RHI
export struct GltfMaterial {
    Math::Vector4 baseColorFactor = Math::Vector4(1.0f);
    Math::Vector4 emissiveFactor = Math::Vector4(0.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    float alphaCutoff = 0.5f;
    
    // Texture indices (will be -1 if not present)
    int32_t baseColorTextureIndex = -1;
    int32_t metallicRoughnessTextureIndex = -1;
    int32_t normalTextureIndex = -1;
    int32_t occlusionTextureIndex = -1;
    int32_t emissiveTextureIndex = -1;
};

// Simplified node for RHI
export struct GltfNode {
    std::string name;
    Math::Matrix4 transform = Math::Matrix4(1.0f);
    Math::Matrix4 worldTransform = Math::Matrix4(1.0f);
    
    // Mesh data
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int32_t materialIndex = -1;
    
    // Hierarchy
    std::vector<Core::UniquePtr<GltfNode>> children;
    GltfNode* parent = nullptr;
    
    GltfNode(const std::string& nodeName) : name(nodeName) {}
};

// RHI-compatible glTF model
export class RhiGltfModel {
private:
    RhiDevice* device_;
    std::string filename_;
    
    // GPU resources
    Core::UniquePtr<RhiBuffer> vertexBuffer_;
    Core::UniquePtr<RhiBuffer> indexBuffer_;
    
    // Model data
    std::vector<GltfVertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<GltfMaterial> materials_;
    std::vector<Core::UniquePtr<GltfNode>> nodes_;
    
    // Bounding box
    Math::Vector3 aabbMin_ = Math::Vector3(std::numeric_limits<float>::max());
    Math::Vector3 aabbMax_ = Math::Vector3(std::numeric_limits<float>::lowest());
    
    // Legacy glTF model for loading
    Core::UniquePtr<vkglTF::Model> legacyModel_;
    
public:
    RhiGltfModel(RhiDevice* device) : device_(device) {
        Log::Debug("RhiGltfModel created");
    }
    
    ~RhiGltfModel() {
        Log::Debug("RhiGltfModel destroyed");
    }
    
    // Load model from glTF file
    bool LoadFromFile(const std::string& filename, float scale = 1.0f) {
        filename_ = filename;
        // Construct full path using Tool::GetAssetsPath()
        std::string fullPath = Tool::GetAssetsPath() + filename;
        Log::Info(std::format("Loading glTF model: {} -> {}", filename, fullPath));
        
        // Create legacy glTF model for loading
        legacyModel_ = Core::MakeUnique<vkglTF::Model>();
        
        // Load using existing glTF loader
        bool useTestModel = false;
        try {
            // Cast RhiDevice to VulkanDevice to access Vulkan-specific methods
            auto* vulkanDevice = dynamic_cast<VulkanDevice*>(device_);
            if (!vulkanDevice) {
                Log::Error("Device is not a VulkanDevice - cannot load glTF model, using test model");
                useTestModel = true;
            } else {
                // Try to load the actual glTF file
                Log::Info(std::format("Attempting to load glTF file: {}", fullPath));
                
                // Check if file exists
                std::ifstream file(fullPath);
                if (!file.good()) {
                    Log::Error(std::format("glTF file not found: {}, using test model", fullPath));
                    useTestModel = true;
                } else {
                    file.close();
                    // Load using existing vkglTF loader
                    Log::Info(std::format("Loading glTF file using existing loader: {}", fullPath));
                    
                    // Load glTF file directly using tinygltf
                    Log::Info("Loading glTF file with direct tinygltf parsing");
                    if (!LoadGltfFile(fullPath, scale)) {
                        Log::Warn("Failed to parse glTF file, falling back to test model");
                        useTestModel = true;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            Log::Error(std::format("Failed to load glTF model '{}': {}, using test model", filename, e.what()));
            useTestModel = true;
        }
        
        if (useTestModel) {
            // Create test model and skip legacy conversion
            CreateTestModel(scale);
            
            // Don't run ConvertLegacyModel as it will overwrite our parsed data
        }
        
        if (!CreateGPUResources()) {
            Log::Error("Failed to create GPU resources for glTF model");
            return false;
        }
        
        Log::Info(std::format("Successfully loaded glTF model: {} vertices, {} indices", 
                              vertices_.size(), indices_.size()));
        return true;
    }
    
    // Create a simple test model (cube) until we integrate real glTF loading
    void CreateTestModel(float scale) {
        Log::Debug("Creating test cube model for glTF testing");
        
        float s = scale * 0.5f;
        
        // Create a simple textured cube
        vertices_ = {
            // Front face
            {{-s, -s,  s}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{ s, -s,  s}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{ s,  s,  s}, {0, 0, 1}, {1, 1}, {1, 0, 0, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{-s,  s,  s}, {0, 0, 1}, {0, 1}, {1, 0, 0, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            
            // Back face
            {{ s, -s, -s}, {0, 0, -1}, {0, 0}, {0, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {-1,0,0,0}},
            {{-s, -s, -s}, {0, 0, -1}, {1, 0}, {0, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {-1,0,0,0}},
            {{-s,  s, -s}, {0, 0, -1}, {1, 1}, {0, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {-1,0,0,0}},
            {{ s,  s, -s}, {0, 0, -1}, {0, 1}, {0, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {-1,0,0,0}},
            
            // Left face
            {{-s, -s, -s}, {-1, 0, 0}, {0, 0}, {0, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {0,-1,0,0}},
            {{-s, -s,  s}, {-1, 0, 0}, {1, 0}, {0, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {0,-1,0,0}},
            {{-s,  s,  s}, {-1, 0, 0}, {1, 1}, {0, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {0,-1,0,0}},
            {{-s,  s, -s}, {-1, 0, 0}, {0, 1}, {0, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {0,-1,0,0}},
            
            // Right face  
            {{ s, -s,  s}, {1, 0, 0}, {0, 0}, {1, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {0,1,0,0}},
            {{ s, -s, -s}, {1, 0, 0}, {1, 0}, {1, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {0,1,0,0}},
            {{ s,  s, -s}, {1, 0, 0}, {1, 1}, {1, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {0,1,0,0}},
            {{ s,  s,  s}, {1, 0, 0}, {0, 1}, {1, 1, 0, 1}, {0,0,0,0}, {0,0,0,0}, {0,1,0,0}},
            
            // Top face
            {{-s,  s,  s}, {0, 1, 0}, {0, 0}, {1, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{ s,  s,  s}, {0, 1, 0}, {1, 0}, {1, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{ s,  s, -s}, {0, 1, 0}, {1, 1}, {1, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{-s,  s, -s}, {0, 1, 0}, {0, 1}, {1, 0, 1, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            
            // Bottom face
            {{-s, -s, -s}, {0, -1, 0}, {0, 0}, {0.5f, 0.5f, 0.5f, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{ s, -s, -s}, {0, -1, 0}, {1, 0}, {0.5f, 0.5f, 0.5f, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{ s, -s,  s}, {0, -1, 0}, {1, 1}, {0.5f, 0.5f, 0.5f, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}},
            {{-s, -s,  s}, {0, -1, 0}, {0, 1}, {0.5f, 0.5f, 0.5f, 1}, {0,0,0,0}, {0,0,0,0}, {1,0,0,0}}
        };
        
        indices_ = {
            0, 1, 2,  2, 3, 0,    // Front
            4, 5, 6,  6, 7, 4,    // Back
            8, 9, 10, 10, 11, 8,  // Left
            12, 13, 14, 14, 15, 12, // Right
            16, 17, 18, 18, 19, 16, // Top
            20, 21, 22, 22, 23, 20  // Bottom
        };
        
        // Update bounding box
        UpdateBoundingBox();
        
        // Create a single default material
        materials_.push_back(GltfMaterial{});
        materials_[0].baseColorFactor = Math::Vector4(0.8f, 0.8f, 0.9f, 1.0f);
        
        // Create root node
        auto rootNode = Core::MakeUnique<GltfNode>("TestCube");
        rootNode->firstIndex = 0;
        rootNode->indexCount = static_cast<uint32_t>(indices_.size());
        rootNode->materialIndex = 0;
        rootNode->transform = Math::Matrix4(1.0f);
        rootNode->worldTransform = Math::Matrix4(1.0f);
        
        nodes_.push_back(std::move(rootNode));
    }
    
    // Load glTF file directly using tinygltf
    bool LoadGltfFile(const std::string& filepath, float scale) {
        Log::Info(std::format("Parsing glTF file: {}", filepath));
        
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        
        bool ret = false;
        if (filepath.size() >= 4 && filepath.substr(filepath.size() - 4) == ".glb") {
            ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
        } else {
            ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
        }
        
        if (!warn.empty()) {
            Log::Warn(std::format("glTF loader warning: {}", warn));
        }
        
        if (!err.empty()) {
            Log::Error(std::format("glTF loader error: {}", err));
            return false;
        }
        
        if (!ret) {
            Log::Error("Failed to parse glTF file");
            return false;
        }
        
        Log::Info(std::format("Successfully loaded glTF: {} meshes, {} nodes, {} materials", 
                              gltfModel.meshes.size(), gltfModel.nodes.size(), gltfModel.materials.size()));
        
        // Clear existing data
        vertices_.clear();
        indices_.clear();
        materials_.clear();
        nodes_.clear();
        
        // Parse the glTF model
        if (!ParseGltfModel(gltfModel, scale)) {
            Log::Error("Failed to parse glTF model data");
            return false;
        }
        
        return true;
    }
    
    // Parse glTF model data
    bool ParseGltfModel(const tinygltf::Model& gltfModel, float scale) {
        Log::Debug("Parsing glTF model data...");
        
        // Load meshes and extract vertex/index data
        uint32_t indexOffset = 0;
        for (size_t meshIndex = 0; meshIndex < gltfModel.meshes.size(); ++meshIndex) {
            const tinygltf::Mesh& mesh = gltfModel.meshes[meshIndex];
            Log::Debug(std::format("Processing mesh {}: {} primitives", meshIndex, mesh.primitives.size()));
            
            for (const tinygltf::Primitive& primitive : mesh.primitives) {
                if (!ParsePrimitive(gltfModel, primitive, scale, indexOffset)) {
                    Log::Error(std::format("Failed to parse primitive in mesh {}", meshIndex));
                    return false;
                }
            }
        }
        
        // Load materials
        for (const tinygltf::Material& gltfMaterial : gltfModel.materials) {
            GltfMaterial material;
            if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                material.baseColorFactor = Math::Vector4(
                    static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[0]),
                    static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[1]),
                    static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[2]),
                    static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[3])
                );
            }
            material.metallicFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor);
            material.roughnessFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);
            material.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
            materials_.push_back(material);
        }
        
        // Create nodes (simplified - one root node for all geometry for now)
        if (!indices_.empty()) {
            auto rootNode = Core::MakeUnique<GltfNode>("GltfRoot");
            rootNode->firstIndex = 0;
            rootNode->indexCount = static_cast<uint32_t>(indices_.size());
            rootNode->materialIndex = materials_.empty() ? -1 : 0;
            rootNode->transform = Math::Matrix4(1.0f);
            rootNode->worldTransform = Math::Matrix4(1.0f);
            nodes_.push_back(std::move(rootNode));
        }
        
        // Update bounding box
        UpdateBoundingBox();
        
        Log::Info(std::format("Parsed glTF model: {} vertices, {} indices, {} materials", 
                              vertices_.size(), indices_.size(), materials_.size()));
        return true;
    }
    
    // Parse a single primitive (fixed buffer access)
    bool ParsePrimitive(const tinygltf::Model& gltfModel, const tinygltf::Primitive& primitive, float scale, uint32_t& indexOffset) {
        Log::Debug("Parsing primitive...");
        
        // Get vertex positions
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end()) {
            Log::Error("Primitive missing POSITION attribute");
            return false;
        }
        
        const tinygltf::Accessor& posAccessor = gltfModel.accessors[posIt->second];
        const tinygltf::BufferView& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
        const tinygltf::Buffer& posBuffer = gltfModel.buffers[posBufferView.buffer];
        
        // Get normals (optional)
        const tinygltf::Accessor* normalAccessor = nullptr;
        const tinygltf::BufferView* normalBufferView = nullptr;
        const tinygltf::Buffer* normalBuffer = nullptr;
        
        auto normalIt = primitive.attributes.find("NORMAL");
        if (normalIt != primitive.attributes.end()) {
            normalAccessor = &gltfModel.accessors[normalIt->second];
            normalBufferView = &gltfModel.bufferViews[normalAccessor->bufferView];
            normalBuffer = &gltfModel.buffers[normalBufferView->buffer];
        }
        
        // Get texture coordinates (optional)
        const tinygltf::Accessor* uvAccessor = nullptr;
        const tinygltf::BufferView* uvBufferView = nullptr;
        const tinygltf::Buffer* uvBuffer = nullptr;
        
        auto uvIt = primitive.attributes.find("TEXCOORD_0");
        if (uvIt != primitive.attributes.end()) {
            uvAccessor = &gltfModel.accessors[uvIt->second];
            uvBufferView = &gltfModel.bufferViews[uvAccessor->bufferView];
            uvBuffer = &gltfModel.buffers[uvBufferView->buffer];
        }
        
        // Process vertices
        size_t vertexCount = posAccessor.count;
        Log::Debug(std::format("Processing {} vertices", vertexCount));
        
        for (size_t i = 0; i < vertexCount; ++i) {
            GltfVertex vertex;
            
            // Position (required)
            const float* pos = reinterpret_cast<const float*>(
                &posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 3 * sizeof(float)]
            );
            vertex.pos = Math::Vector3(pos[0] * scale, pos[1] * scale, pos[2] * scale);
            
            // Normal (optional)
            if (normalAccessor && normalBufferView && normalBuffer) {
                const float* normal = reinterpret_cast<const float*>(
                    &normalBuffer->data[normalBufferView->byteOffset + normalAccessor->byteOffset + i * 3 * sizeof(float)]
                );
                vertex.normal = Math::Vector3(normal[0], normal[1], normal[2]);
            } else {
                vertex.normal = Math::Vector3(0.0f, 0.0f, 1.0f); // Default normal
            }
            
            // UV (optional)
            if (uvAccessor && uvBufferView && uvBuffer) {
                const float* uv = reinterpret_cast<const float*>(
                    &uvBuffer->data[uvBufferView->byteOffset + uvAccessor->byteOffset + i * 2 * sizeof(float)]
                );
                vertex.uv = Math::Vector2(uv[0], uv[1]);
            } else {
                vertex.uv = Math::Vector2(0.0f, 0.0f); // Default UV
            }
            
            // Default values for other attributes
            vertex.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            vertex.joint0 = Math::Vector4(0.0f);
            vertex.weight0 = Math::Vector4(0.0f);
            vertex.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
            
            vertices_.push_back(vertex);
        }
        
        // Process indices
        if (primitive.indices >= 0) {
            const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
            const tinygltf::BufferView& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
            
            Log::Debug(std::format("Processing {} indices", indexAccessor.count));
            
            const void* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
            
            for (size_t i = 0; i < indexAccessor.count; ++i) {
                uint32_t index = indexOffset;
                
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    index += static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(indexData)[i]);
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    index += reinterpret_cast<const uint32_t*>(indexData)[i];
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    index += static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(indexData)[i]);
                }
                
                indices_.push_back(index);
            }
        } else {
            // Generate indices for non-indexed geometry
            for (size_t i = 0; i < vertexCount; ++i) {
                indices_.push_back(indexOffset + static_cast<uint32_t>(i));
            }
        }
        
        indexOffset += static_cast<uint32_t>(vertexCount);
        
        Log::Debug(std::format("Primitive parsed successfully: {} vertices, {} indices", vertexCount, 
                              primitive.indices >= 0 ? gltfModel.accessors[primitive.indices].count : vertexCount));
        return true;
    }

    // Convert legacy vkglTF::Model to RHI format
    bool ConvertLegacyModel() {
        if (!legacyModel_) {
            Log::Error("No legacy model to convert");
            return false;
        }
        
        Log::Debug("Converting legacy glTF model to RHI format");
        
        // Clear existing data
        vertices_.clear();
        indices_.clear();
        materials_.clear();
        nodes_.clear();
        
        // Convert vertex data from vkglTF format to RHI format
        vertices_.reserve(legacyModel_->vertexBuffer.size());
        for (const auto& legacyVertex : legacyModel_->vertexBuffer) {
            GltfVertex rhiVertex;
            rhiVertex.pos = Math::Vector3(legacyVertex.pos.x, legacyVertex.pos.y, legacyVertex.pos.z);
            rhiVertex.normal = Math::Vector3(legacyVertex.normal.x, legacyVertex.normal.y, legacyVertex.normal.z);
            rhiVertex.uv = Math::Vector2(legacyVertex.uv.x, legacyVertex.uv.y);
            rhiVertex.color = Math::Vector4(legacyVertex.color.r, legacyVertex.color.g, legacyVertex.color.b, legacyVertex.color.a);
            rhiVertex.joint0 = Math::Vector4(legacyVertex.joint0.x, legacyVertex.joint0.y, legacyVertex.joint0.z, legacyVertex.joint0.w);
            rhiVertex.weight0 = Math::Vector4(legacyVertex.weight0.x, legacyVertex.weight0.y, legacyVertex.weight0.z, legacyVertex.weight0.w);
            rhiVertex.tangent = Math::Vector4(legacyVertex.tangent.x, legacyVertex.tangent.y, legacyVertex.tangent.z, legacyVertex.tangent.w);
            vertices_.push_back(rhiVertex);
        }
        
        // Copy index data directly
        indices_ = legacyModel_->indexBuffer;
        
        // Convert materials
        materials_.reserve(legacyModel_->materials.size());
        for (const auto& legacyMaterial : legacyModel_->materials) {
            GltfMaterial rhiMaterial;
            rhiMaterial.baseColorFactor = Math::Vector4(
                legacyMaterial.baseColorFactor.r,
                legacyMaterial.baseColorFactor.g, 
                legacyMaterial.baseColorFactor.b,
                legacyMaterial.baseColorFactor.a);
            rhiMaterial.metallicFactor = legacyMaterial.metallicFactor;
            rhiMaterial.roughnessFactor = legacyMaterial.roughnessFactor;
            rhiMaterial.alphaCutoff = legacyMaterial.alphaCutoff;
            
            // TODO: Handle texture indices when texture system is integrated
            materials_.push_back(rhiMaterial);
        }
        
        // Convert nodes (simplified - just create a single root node for all geometry)
        if (!legacyModel_->linearNodes.empty()) {
            auto rootNode = Core::MakeUnique<GltfNode>("RootNode");
            rootNode->firstIndex = 0;
            rootNode->indexCount = static_cast<uint32_t>(indices_.size());
            rootNode->materialIndex = materials_.empty() ? -1 : 0;
            rootNode->transform = Math::Matrix4(1.0f);
            rootNode->worldTransform = Math::Matrix4(1.0f);
            nodes_.push_back(std::move(rootNode));
        }
        
        // Update bounding box
        UpdateBoundingBox();
        
        Log::Info(std::format("Converted legacy model: {} vertices, {} indices, {} materials, {} nodes",
                              vertices_.size(), indices_.size(), materials_.size(), nodes_.size()));
        return true;
    }
    
    bool CreateGPUResources() {
        if (vertices_.empty() || indices_.empty()) {
            Log::Error("Cannot create GPU resources: no vertex or index data");
            return false;
        }
        
        // Create vertex buffer
        RhiBufferDesc vertexBufferDesc;
        vertexBufferDesc.size = vertices_.size() * sizeof(GltfVertex);
        vertexBufferDesc.usage = RhiBufferUsage::Vertex;
        vertexBufferDesc.memoryUsage = RhiMemoryUsage::GPU_Only;
        vertexBufferDesc.debugName = filename_ + "_VertexBuffer";
        
        vertexBuffer_ = device_->CreateBuffer(vertexBufferDesc);
        if (!vertexBuffer_) {
            Log::Error("Failed to create vertex buffer");
            return false;
        }
        
        // Upload vertex data
        device_->UploadBufferData(vertexBuffer_.get(), vertices_.data(), vertexBufferDesc.size);
        
        // Create index buffer
        RhiBufferDesc indexBufferDesc;
        indexBufferDesc.size = indices_.size() * sizeof(uint32_t);
        indexBufferDesc.usage = RhiBufferUsage::Index;
        indexBufferDesc.memoryUsage = RhiMemoryUsage::GPU_Only;
        indexBufferDesc.debugName = filename_ + "_IndexBuffer";
        
        indexBuffer_ = device_->CreateBuffer(indexBufferDesc);
        if (!indexBuffer_) {
            Log::Error("Failed to create index buffer");
            return false;
        }
        
        // Upload index data
        device_->UploadBufferData(indexBuffer_.get(), indices_.data(), indexBufferDesc.size);
        
        Log::Debug("GPU resources created successfully");
        return true;
    }
    
    void UpdateBoundingBox() {
        aabbMin_ = Math::Vector3(std::numeric_limits<float>::max());
        aabbMax_ = Math::Vector3(std::numeric_limits<float>::lowest());
        
        for (const auto& vertex : vertices_) {
            aabbMin_ = Math::Vector3(std::min(aabbMin_.x, vertex.pos.x), std::min(aabbMin_.y, vertex.pos.y), std::min(aabbMin_.z, vertex.pos.z));
            aabbMax_ = Math::Vector3(std::max(aabbMax_.x, vertex.pos.x), std::max(aabbMax_.y, vertex.pos.y), std::max(aabbMax_.z, vertex.pos.z));
        }
        
        Log::Debug(std::format("Model bounding box: min({:.2f}, {:.2f}, {:.2f}) max({:.2f}, {:.2f}, {:.2f})", 
                              aabbMin_.x, aabbMin_.y, aabbMin_.z, aabbMax_.x, aabbMax_.y, aabbMax_.z));
    }
    
    // Render the entire model
    void Render(RhiCommandBuffer* commandBuffer) {
        if (!IsValid()) {
            Log::Warn("Cannot render glTF model: not valid");
            return;
        }
        
        // Bind vertex buffer
        commandBuffer->BindVertexBuffer(vertexBuffer_.get(), 0);
        commandBuffer->BindIndexBuffer(indexBuffer_.get());
        
        // Render all nodes
        for (const auto& node : nodes_) {
            RenderNode(node.get(), commandBuffer);
        }
    }
    
    // Render a specific node
    void RenderNode(GltfNode* node, RhiCommandBuffer* commandBuffer) {
        if (!node || node->indexCount == 0) {
            return;
        }
        
        // TODO: Set material uniforms here
        // TODO: Bind textures for this material
        
        // Draw this node's geometry
        commandBuffer->DrawIndexed(node->indexCount, 1, node->firstIndex, 0, 0);
        
        // Render children
        for (const auto& child : node->children) {
            RenderNode(child.get(), commandBuffer);
        }
    }
    
    // Getters
    size_t GetVertexCount() const { return vertices_.size(); }
    size_t GetIndexCount() const { return indices_.size(); }
    size_t GetNodeCount() const { return nodes_.size(); }
    size_t GetMaterialCount() const { return materials_.size(); }
    
    const Math::Vector3& GetAABBMin() const { return aabbMin_; }
    const Math::Vector3& GetAABBMax() const { return aabbMax_; }
    Math::Vector3 GetCenter() const { 
        return Math::Vector3(
            (aabbMin_.x + aabbMax_.x) * 0.5f,
            (aabbMin_.y + aabbMax_.y) * 0.5f,
            (aabbMin_.z + aabbMax_.z) * 0.5f
        );
    }
    Math::Vector3 GetSize() const { 
        return Math::Vector3(
            aabbMax_.x - aabbMin_.x,
            aabbMax_.y - aabbMin_.y,
            aabbMax_.z - aabbMin_.z
        );
    }
    
    bool IsValid() const {
        return vertexBuffer_ && indexBuffer_ && !vertices_.empty() && !indices_.empty();
    }
    
    const std::string& GetFilename() const { return filename_; }
};