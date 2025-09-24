module;
#include "vulkan/vulkan.h"

module ForwardPass;
import Core;
import Math;
import RhiTypes;
import RhiPipelineDesc;
import RenderTargetManager;
import RhiGltfModel;
import Logger;
import ToolMod;
import std;

// ForwardPass implementation
ForwardPass::ForwardPass(RhiDevice* device)
    : device_(device), width_(0), height_(0)
{
    // Initialize clear values for forward rendering
    // Color: clear to background color (dark blue)
    clearValues_[0].color = { {0.0f, 0.0f, 0.2f, 1.0f} };
    // Depth: clear to far plane
    clearValues_[1].depthStencil = { 1.0f, 0 };
    
    Log::Debug("ForwardPass created with RHI architecture");
}

bool ForwardPass::Initialize()
{
    Log::Info("Initializing ForwardPass for model rendering");
    
    // Create uniform buffer for MVP matrices
    if (!CreateUniformBuffer()) {
        Log::Error("Failed to create uniform buffer for ForwardPass");
        return false;
    }
    
    // Create descriptor set for MVP uniform buffer
    if (!CreateDescriptorSet()) {
        Log::Error("Failed to create descriptor set for ForwardPass");
        return false;
    }
    
    // Load test model
    if (!LoadTestModel()) {
        Log::Error("Failed to load test model for ForwardPass");
        return false;
    }
    
    Log::Info("ForwardPass initialized successfully");
    return true;
}

bool ForwardPass::CreateUniformBuffer()
{
    // Create MVP uniform buffer
    RhiBufferDesc mvpBufferDesc;
    mvpBufferDesc.size = sizeof(MVPUniformData);
    mvpBufferDesc.usage = RhiBufferUsage::Uniform;
    mvpBufferDesc.memoryUsage = RhiMemoryUsage::CPU_TO_GPU; // Frequently updated
    mvpBufferDesc.debugName = "ForwardPass_MVPUniformBuffer";
    
    mvpBuffer_ = device_->CreateBuffer(mvpBufferDesc);
    if (!mvpBuffer_) {
        Log::Error("Failed to create MVP uniform buffer");
        return false;
    }
    
    Log::Debug("MVP uniform buffer created successfully");
    return true;
}

bool ForwardPass::CreateDescriptorSet()
{
    // Create descriptor set layout for MVP uniform buffer
    RhiDescriptorSetLayoutDesc layoutDesc;
    RhiDescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorType = RhiDescriptorType::UniformBuffer;
    binding.descriptorCount = 1;
    binding.stageFlags = RhiShaderStageFlags::Vertex;
    layoutDesc.bindings = { binding };
    
    descriptorSetLayout_ = device_->CreateDescriptorSetLayout(layoutDesc);
    if (!descriptorSetLayout_) {
        Log::Error("Failed to create descriptor set layout");
        return false;
    }
    
    // Create descriptor pool
    RhiDescriptorPoolDesc poolDesc;
    poolDesc.maxSets = 1;
    poolDesc.poolSizes = {
        {RhiDescriptorType::UniformBuffer, 1}
    };
    
    descriptorPool_ = device_->CreateDescriptorPool(poolDesc);
    if (!descriptorPool_) {
        Log::Error("Failed to create descriptor pool");
        return false;
    }
    
    // Allocate descriptor set
    descriptorSet_ = device_->AllocateDescriptorSet(descriptorPool_.get(), descriptorSetLayout_.get());
    if (!descriptorSet_) {
        Log::Error("Failed to allocate descriptor set");
        return false;
    }
    
    // Update descriptor set with MVP buffer
    device_->UpdateDescriptorSet(descriptorSet_.get(), 0, mvpBuffer_.get());
    
    Log::Debug("Descriptor set created and updated successfully");
    return true;
}

bool ForwardPass::LoadTestModel()
{
    // Create RHI glTF model
    testModel_ = Core::MakeUnique<RhiGltfModel>(device_);
    
    // Try to load a real glTF model first, fall back to test cube
    std::string modelPath = "Models/cat/scene.gltf";
    if (!testModel_->LoadFromFile(modelPath, 1.0f)) {
        Log::Warn(std::format("Failed to load glTF model: {}, using test geometry", modelPath));
        
        // Try alternative model
        modelPath = "Models/Cat/scene.gltf";  
        if (!testModel_->LoadFromFile(modelPath, 1.0f)) {
            Log::Warn(std::format("Failed to load alternative glTF model: {}, using test geometry", modelPath));
        }
    }
    
    if (!testModel_->IsValid()) {
        Log::Error("Failed to create valid test model");
        return false;
    }
    
    Log::Info(std::format("Test model loaded: {} vertices, {} indices", 
                          testModel_->GetVertexCount(), testModel_->GetIndexCount()));
    return true;
}

void ForwardPass::UpdateMVPMatrices()
{
    // Create model matrix (identity for now)
    mvpData_.model = Math::Matrix4(1.0f);
    
    // Create view matrix
    mvpData_.view = Math::CreateLookAt(cameraPos_, cameraTarget_, cameraUp_);
    
    // Create projection matrix
    float aspectRatio = static_cast<float>(width_) / static_cast<float>(height_);
    mvpData_.proj = Math::CreatePerspective(45.0f * 3.14159f / 180.0f, aspectRatio, 0.1f, 100.0f);
    
    // Pre-compute MVP matrix
    mvpData_.mvp = Math::MultiplyMVP(mvpData_.proj, mvpData_.view, mvpData_.model);
    
    // Upload to uniform buffer
    if (mvpBuffer_) {
        device_->UploadBufferData(mvpBuffer_.get(), &mvpData_, sizeof(MVPUniformData));
    }
    
    // MVP matrices updated and uploaded to GPU (reduced logging)
}

void ForwardPass::SetDimensions(uint32_t width, uint32_t height)
{
    width_ = width;
    height_ = height;
    Log::Debug(std::format("ForwardPass dimensions set: {}x{}", width, height));
}

void ForwardPass::SetCameraPosition(const Math::Vector3& pos, const Math::Vector3& target, const Math::Vector3& up)
{
    cameraPos_ = pos;
    cameraTarget_ = target;
    cameraUp_ = up;
    Log::Debug(std::format("Camera position updated: pos({:.2f}, {:.2f}, {:.2f})", pos.x, pos.y, pos.z));
}

void ForwardPass::SetModelTransform(const Math::Matrix4& transform)
{
    mvpData_.model = transform;
    Log::Debug("Model transform updated");
}

void ForwardPass::Execute(RhiCommandBuffer* cmd, PipelineManager* pipelineManager, RenderTargetManager* renderTargetManager)
{
    if (!cmd || !pipelineManager || !renderTargetManager) {
        Log::Error("ForwardPass::Execute - Invalid parameters");
        return;
    }
    
    // Executing ForwardPass with model rendering (reduced logging)
    
    // Update MVP matrices before rendering
    UpdateMVPMatrices();
    
    // Request swapchain render target from manager
    auto* renderTarget = renderTargetManager->GetSwapchainRenderTarget();
    if (!renderTarget || !renderTarget->IsValid()) {
        Log::Error("Failed to get swapchain render target for ForwardPass");
        return;
    }
    
    // Create pipeline descriptor for model rendering
    RhiGraphicsPipelineDesc pipelineDesc;
    
    // Setup shaders using Tool::GetShadersPath() for correct path resolution
    std::string shadersPath = Tool::GetShadersPath();
    
    RhiShaderDesc vertexShader;
    vertexShader.stage = RhiShaderStage::Vertex;
    vertexShader.filePath = shadersPath + "Model.Vert.spv"; // Model vertex shader with MVP support
    vertexShader.entryPoint = "main";
    
    RhiShaderDesc fragmentShader;
    fragmentShader.stage = RhiShaderStage::Fragment;
    fragmentShader.filePath = shadersPath + "Model.Frag.spv"; // Model fragment shader with lighting
    fragmentShader.entryPoint = "main";
    
    pipelineDesc.shaders = { vertexShader, fragmentShader };
    
    // Setup descriptor set layouts for pipeline
    if (descriptorSetLayout_) {
        pipelineDesc.descriptorSetLayouts = { descriptorSetLayout_.get() };
    }
    
    // Setup vertex input for glTF model
    if (testModel_ && testModel_->IsValid()) {
        pipelineDesc.vertexInput.bindings = { GltfVertex::GetBindingDescription() };
        pipelineDesc.vertexInput.attributes = GltfVertex::GetAttributeDescriptions();
    }
    
    // Setup input assembly for triangle list
    pipelineDesc.inputAssembly.topology = RhiPrimitiveTopology::TriangleList;
    pipelineDesc.inputAssembly.primitiveRestartEnable = false;
    
    // Setup rasterization state - Back-face culling enabled
    pipelineDesc.rasterization.cullMode = RhiCullMode::Back; // Cull back faces for proper rendering
    pipelineDesc.rasterization.frontFace = RhiFrontFace::Clockwise; // Try clockwise = front face
    pipelineDesc.rasterization.polygonMode = RhiPolygonMode::Fill;
    
    // Setup depth stencil state
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;
    pipelineDesc.depthStencil.depthCompareOp = RhiCompareOp::Less;
    
    // Setup color blend state
    RhiColorBlendAttachmentDesc colorBlendAttachment;
    colorBlendAttachment.blendEnable = false;
    pipelineDesc.colorBlend.attachments = { colorBlendAttachment };
    
    // Request graphics pipeline from manager
    auto* pipeline = pipelineManager->GetGraphicsPipeline(pipelineDesc);
    if (!pipeline) {
        Log::Error("Failed to get graphics pipeline for ForwardPass");
        return;
    }
    
    // Set viewport and scissor
    RhiViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(renderTarget->desc.width);
    viewport.height = static_cast<float>(renderTarget->desc.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    RhiRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {renderTarget->desc.width, renderTarget->desc.height};
    
    // Record commands
    cmd->SetViewport(viewport);
    cmd->SetScissor(scissor);
    
    // Begin render pass
    cmd->BeginRenderPass(renderTarget->renderPass.get());
    
    // Bind pipeline
    cmd->BindPipeline(pipeline);
    
    // Bind MVP uniform buffer descriptor set
    if (descriptorSet_) {
        cmd->BindDescriptorSet(descriptorSet_.get(), 0);
    }
    
    // Render the model if available
    if (testModel_ && testModel_->IsValid()) {
        testModel_->Render(cmd);
    } else {
        // Fallback to simple triangle
        cmd->Draw(3, 1, 0, 0);
    }
    
    // End render pass
    cmd->EndRenderPass();
}