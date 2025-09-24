module;
#include <vector>
#include <array>
#include <string>

export module SimpleModel;
import Core;
import Math;
import RhiDevice;
import RhiBuffer;
import RhiTypes;
import Logger;
import std;

// Simple vertex structure for basic model rendering
export struct Vertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector2 texCoord;
    Math::Vector3 color;
    
    Vertex() = default;
    Vertex(const Math::Vector3& pos, const Math::Vector3& norm, const Math::Vector2& uv, const Math::Vector3& col = Math::Vector3(1.0f))
        : position(pos), normal(norm), texCoord(uv), color(col) {}
        
    // Get vertex input description for pipeline creation
    static std::vector<RhiVertexInputAttributeDesc> GetAttributeDescriptions() {
        return {
            {0, 0, RhiFormat::R32G32B32_SFLOAT, offsetof(Vertex, position)},    // Position
            {1, 0, RhiFormat::R32G32B32_SFLOAT, offsetof(Vertex, normal)},      // Normal  
            {2, 0, RhiFormat::R32G32_SFLOAT, offsetof(Vertex, texCoord)},       // UV
            {3, 0, RhiFormat::R32G32B32_SFLOAT, offsetof(Vertex, color)}        // Color
        };
    }
    
    static RhiVertexInputBindingDesc GetBindingDescription() {
        return {0, sizeof(Vertex), RhiVertexInputRate::Vertex};
    }
};

// Simple model class for basic geometry rendering
export class SimpleModel {
private:
    RhiDevice* device_;
    Core::UniquePtr<RhiBuffer> vertexBuffer_;
    Core::UniquePtr<RhiBuffer> indexBuffer_;
    
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    
    std::string name_;
    
public:
    SimpleModel(RhiDevice* device, const std::string& name) 
        : device_(device), name_(name) {
        Log::Debug(std::format("SimpleModel '{}' created", name_));
    }
    
    ~SimpleModel() {
        Log::Debug(std::format("SimpleModel '{}' destroyed", name_));
    }
    
    // Create a simple cube model
    void CreateCube(float size = 1.0f) {
        Log::Info(std::format("Creating cube model '{}' with size {}", name_, size));
        
        float s = size * 0.5f; // Half size for centering
        
        // Define cube vertices (24 vertices for proper normals on each face)
        vertices_ = {
            // Front face (Z+)
            {{-s, -s,  s}, {0, 0, 1}, {0, 0}, {1, 0, 0}}, // Bottom-left
            {{ s, -s,  s}, {0, 0, 1}, {1, 0}, {1, 0, 0}}, // Bottom-right
            {{ s,  s,  s}, {0, 0, 1}, {1, 1}, {1, 0, 0}}, // Top-right
            {{-s,  s,  s}, {0, 0, 1}, {0, 1}, {1, 0, 0}}, // Top-left
            
            // Back face (Z-)
            {{ s, -s, -s}, {0, 0, -1}, {0, 0}, {0, 1, 0}}, // Bottom-left
            {{-s, -s, -s}, {0, 0, -1}, {1, 0}, {0, 1, 0}}, // Bottom-right
            {{-s,  s, -s}, {0, 0, -1}, {1, 1}, {0, 1, 0}}, // Top-right
            {{ s,  s, -s}, {0, 0, -1}, {0, 1}, {0, 1, 0}}, // Top-left
            
            // Left face (X-)
            {{-s, -s, -s}, {-1, 0, 0}, {0, 0}, {0, 0, 1}}, // Bottom-left
            {{-s, -s,  s}, {-1, 0, 0}, {1, 0}, {0, 0, 1}}, // Bottom-right
            {{-s,  s,  s}, {-1, 0, 0}, {1, 1}, {0, 0, 1}}, // Top-right
            {{-s,  s, -s}, {-1, 0, 0}, {0, 1}, {0, 0, 1}}, // Top-left
            
            // Right face (X+)
            {{ s, -s,  s}, {1, 0, 0}, {0, 0}, {1, 1, 0}}, // Bottom-left
            {{ s, -s, -s}, {1, 0, 0}, {1, 0}, {1, 1, 0}}, // Bottom-right
            {{ s,  s, -s}, {1, 0, 0}, {1, 1}, {1, 1, 0}}, // Top-right
            {{ s,  s,  s}, {1, 0, 0}, {0, 1}, {1, 1, 0}}, // Top-left
            
            // Top face (Y+)
            {{-s,  s,  s}, {0, 1, 0}, {0, 0}, {1, 0, 1}}, // Bottom-left
            {{ s,  s,  s}, {0, 1, 0}, {1, 0}, {1, 0, 1}}, // Bottom-right
            {{ s,  s, -s}, {0, 1, 0}, {1, 1}, {1, 0, 1}}, // Top-right
            {{-s,  s, -s}, {0, 1, 0}, {0, 1}, {1, 0, 1}}, // Top-left
            
            // Bottom face (Y-)
            {{-s, -s, -s}, {0, -1, 0}, {0, 0}, {0.5f, 0.5f, 0.5f}}, // Bottom-left
            {{ s, -s, -s}, {0, -1, 0}, {1, 0}, {0.5f, 0.5f, 0.5f}}, // Bottom-right
            {{ s, -s,  s}, {0, -1, 0}, {1, 1}, {0.5f, 0.5f, 0.5f}}, // Top-right
            {{-s, -s,  s}, {0, -1, 0}, {0, 1}, {0.5f, 0.5f, 0.5f}}  // Top-left
        };
        
        // Define cube indices (36 indices for 12 triangles)
        indices_ = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face  
            4, 5, 6,  6, 7, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Top face
            16, 17, 18, 18, 19, 16,
            // Bottom face
            20, 21, 22, 22, 23, 20
        };
        
        CreateBuffers();
        Log::Info(std::format("Cube model '{}' created: {} vertices, {} indices", 
                              name_, vertices_.size(), indices_.size()));
    }
    
    // Create a simple triangle (for testing)
    void CreateTriangle() {
        Log::Info(std::format("Creating triangle model '{}'", name_));
        
        vertices_ = {
            {{ 0.0f, -0.5f, 0.0f}, {0, 0, 1}, {0.5f, 0.0f}, {1, 0, 0}}, // Bottom center - Red
            {{ 0.5f,  0.5f, 0.0f}, {0, 0, 1}, {1.0f, 1.0f}, {0, 1, 0}}, // Top right - Green
            {{-0.5f,  0.5f, 0.0f}, {0, 0, 1}, {0.0f, 1.0f}, {0, 0, 1}}  // Top left - Blue
        };
        
        indices_ = {0, 1, 2};
        
        CreateBuffers();
        Log::Info(std::format("Triangle model '{}' created: {} vertices, {} indices", 
                              name_, vertices_.size(), indices_.size()));
    }
    
    // Create GPU buffers
    void CreateBuffers() {
        if (vertices_.empty() || indices_.empty()) {
            Log::Error("Cannot create buffers: vertices or indices are empty");
            return;
        }
        
        // Create vertex buffer
        RhiBufferDesc vertexBufferDesc;
        vertexBufferDesc.size = vertices_.size() * sizeof(Vertex);
        vertexBufferDesc.usage = RhiBufferUsage::VertexBuffer;
        vertexBufferDesc.memoryUsage = RhiMemoryUsage::GPU_Only;
        vertexBufferDesc.debugName = name_ + "_VertexBuffer";
        
        vertexBuffer_ = device_->CreateBuffer(vertexBufferDesc);
        if (!vertexBuffer_) {
            Log::Error("Failed to create vertex buffer");
            return;
        }
        
        // Upload vertex data
        device_->UploadBufferData(vertexBuffer_.get(), vertices_.data(), vertexBufferDesc.size);
        
        // Create index buffer
        RhiBufferDesc indexBufferDesc;
        indexBufferDesc.size = indices_.size() * sizeof(uint32_t);
        indexBufferDesc.usage = RhiBufferUsage::IndexBuffer;
        indexBufferDesc.memoryUsage = RhiMemoryUsage::GPU_Only;
        indexBufferDesc.debugName = name_ + "_IndexBuffer";
        
        indexBuffer_ = device_->CreateBuffer(indexBufferDesc);
        if (!indexBuffer_) {
            Log::Error("Failed to create index buffer");
            return;
        }
        
        // Upload index data
        device_->UploadBufferData(indexBuffer_.get(), indices_.data(), indexBufferDesc.size);
        
        Log::Debug(std::format("Model '{}' buffers created successfully", name_));
    }
    
    // Render the model
    void Render(RhiCommandBuffer* commandBuffer) {
        if (!vertexBuffer_ || !indexBuffer_ || indices_.empty()) {
            Log::Warn(std::format("Model '{}' cannot be rendered: missing buffers or indices", name_));
            return;
        }
        
        // Bind vertex buffer
        commandBuffer->BindVertexBuffer(vertexBuffer_.get(), 0);
        
        // Bind index buffer
        commandBuffer->BindIndexBuffer(indexBuffer_.get());
        
        // Draw indexed
        commandBuffer->DrawIndexed(static_cast<uint32_t>(indices_.size()), 1, 0, 0, 0);
        
        Log::Debug(std::format("Model '{}' rendered: {} indices", name_, indices_.size()));
    }
    
    // Getters
    size_t GetVertexCount() const { return vertices_.size(); }
    size_t GetIndexCount() const { return indices_.size(); }
    const std::string& GetName() const { return name_; }
    
    // Check if model is ready for rendering
    bool IsValid() const {
        return vertexBuffer_ && indexBuffer_ && !vertices_.empty() && !indices_.empty();
    }
};