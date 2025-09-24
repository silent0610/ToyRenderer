# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MyToyRenderer is a Vulkan-based 3D renderer written in modern C++ (C++latest) using C++20 modules (.ixx files). The project focuses on advanced rendering techniques including physically based rendering (PBR), shadow mapping, ambient occlusion (SSAO/HBAO/GTAO), voxelization, and octree-based spatial data structures.

## Build System and Commands

The project uses **xmake** as its build system with MSVC compiler (v14.44).

### Essential Build Commands
- `xmake` - Build the project
- `xmake project -k vsxmake build/` - Generate Visual Studio solution (Windows only)
- `xmake run` - Build and run the executable
- `xmake clean` - Clean build artifacts

### Shader Compilation
- Navigate to `Renderer/Shader/` directory
- Run `python Compile.py` - Compiles all HLSL shaders to SPIR-V (.spv files)
- The script only recompiles shaders modified within the last hour
- Supports vertex (.Vert), fragment (.Frag), geometry (.Geom), and compute (.Comp) shaders

## RHI (Render Hardware Interface) Architecture

The project implements a modern RHI system with clean separation between API-agnostic interfaces and Vulkan-specific implementations.

### Core Architecture (Three-Layer Design)

**1. RHI Core Layer** (`Renderer/Source/Rhi/Core/`)
- `RhiTypes.ixx` - Fundamental enums and data types (RhiFormat, usage flags, etc.)
- `RhiDevice.ixx` - Main device abstraction (CreateBuffer, CreateTexture, Present, etc.)
- Resource interfaces: Buffer, Texture, Pipeline, RenderPass, CommandBuffer
- **PipelineManager** - Hash-based pipeline caching and creation
- **RenderTargetManager** - Render target pooling (swapchain + offscreen)
- **CommandBufferPool** - Efficient CommandBuffer reuse with RAII management

**2. Vulkan Implementation Layer** (`Renderer/Source/Rhi/Vulkan/`)
- `VulkanDevice.cpp/.ixx` - Core implementation (instance, device, swapchain, 64MB staging buffer)
- Resource implementations: VulkanBuffer, VulkanTexture, VulkanRenderPass, etc.
- `VulkanUtils` - Format conversions and helper functions

**3. Application Renderer Layer** (`Application/Renderer/`)
- **NewRenderer** - Main renderer class, initialization and render loop
- **RenderGraph** - Frame graph system with task-based pass execution
- **Rendering Passes** - ForwardPass, DeferredPass, ShadowPass, etc.

### Key Data Flows

**Initialization:**
```
Main() → NewRenderer() → VulkanDevice() → PipelineManager + RenderTargetManager + CommandBufferPool → RenderGraph()
```

**Frame Rendering (Optimized with CommandBuffer pooling):**
```
NewRenderer::Run()
├── VulkanDevice::AcquireNextImage() [semaphore sync]
├── CommandBufferPool::AcquirePooledBuffer() [reuse existing buffer]
├── RenderGraph::Execute()
│   └── ForwardPass execution:
│       ├── RenderTargetManager::GetSwapchainRenderTarget() [cached]
│       ├── PipelineManager::GetGraphicsPipeline() [cached]
│       ├── CommandBuffer::Reset() → Begin() → BeginRenderPass()
│       ├── CommandBuffer::BindPipeline() → Draw() → EndRenderPass()
│       └── CommandBuffer::End()
├── VulkanDevice::Submit() [semaphore sync]
├── VulkanDevice::Present()
└── PooledCommandBuffer destructor → automatic return to pool
```

**Pipeline Creation:**
```
ForwardPass → PipelineManager → VulkanDevice::CreateGraphicsPipeline()
├── Load shaders from Tool::GetShadersPath()
├── Create VkShaderModules  
├── Setup pipeline state
└── Create VkPipeline with defaultRenderPass_
```

## Key Implementation Details

**Resource Management:**
- **RAII**: All Vulkan objects wrapped in RAII classes with automatic cleanup
- **Smart Pointers**: Core::UniquePtr<T> for automatic resource management
- **Caching**: Hash-based pipeline cache, render target pooling
- **Staging Buffer**: 64MB staging buffer for efficient GPU memory transfers

**Synchronization:**
- **Frame Pipelining**: MAX_FRAMES_IN_FLIGHT (2-3) with per-frame resources
- **Vulkan Sync**: Proper semaphore/fence usage (imageAvailable, renderFinished, inFlight)
- **Single-threaded**: Current architecture assumes single render thread

**Error Handling:**
- **Validation Layers**: Comprehensive Vulkan validation in debug builds
- **Logging**: spdlog-based system with Debug/Info/Warn/Error levels
- **RhiResult**: Standardized error reporting across RHI layer

## Known Current Issues

**Render Pass Compatibility Problem:**
- **Issue**: Pipelines created with VulkanDevice's `defaultRenderPass_` (color-only)
- **Runtime**: Uses RenderTargetManager's render pass (color + depth)
- **Impact**: Vulkan validation error VUID-vkCmdDraw-renderPass-02684
- **Workaround**: Depth testing disabled in ForwardPass temporarily

**Architecture Status:**
- ✅ All major components implemented and working
- ✅ Pipeline caching, render target management, frame graph execution
- ✅ Shader loading, resource creation, proper synchronization
- ⚠️ Only remaining issue: render pass compatibility for depth buffers

## Development Guidelines

**Path Resolution:**
- Use `Tool::GetShadersPath()` for shader file paths
- Use `Tool::GetProjectPath()` and `Tool::GetAssetsPath()` for resources

**Memory Management:**
- Follow RAII principles strictly
- Use Core::UniquePtr<T> for all RHI resources
- Rely on automatic caching in PipelineManager and RenderTargetManager

**Module Dependencies:**
- Import ToolMod for path utilities
- Import Logger for logging
- Import appropriate RHI modules (RhiTypes, RhiDevice, etc.)

**Debugging:**
- Vulkan validation layers enabled in debug builds
- Comprehensive logging throughout RHI layer
- Resource creation and destruction tracked

## Dependencies
- **Vulkan SDK** - Graphics API
- **GLFW** - Window and input management  
- **GLM** - Mathematics library
- **spdlog** - Logging system
- **nlohmann_json** - JSON configuration parsing

**Platform Support:** Windows only with MSVC toolchain and Vulkan 1.0+ hardware