# Engine Project Context

## Project Overview

This is a **C++ Game Engine** project designed for implementing and testing graphics algorithms. It is built using **xmake** and targets the **Vulkan** API through a custom **RHI (Render Hardware Interface)** layer. The project extensively uses **C++ Modules** (`.ixx` files).

**Key Goals:**
*   Modular and Extensible architecture.
*   Support for **RenderGraph**.
*   Future support for Reflection, Serialization, and ECS.

## Build & Run

The project uses `xmake` as the build system.

**Prerequisites:**
*   xmake
*   Vulkan SDK
*   C++ Compiler supporting C++ Modules (e.g., MSVC 2022/2026 as configured)

**Commands:**
*   **Configure:** `xmake f -p windows -a x64 -m debug` (or release)
*   **Build:** `xmake`
*   **Run:** `xmake run Engine`
*   **Compile Shaders:** See `Engine/Asset/Shader/Compile.py`.

## Directory Structure

*   `xmake.lua`: Main build configuration.
*   `Engine/Source/`: Source code.
    *   `Engine.ixx`: Main engine module definition.
    *   `main.cpp`: Entry point.
    *   `Rhi/`: Render Hardware Interface (Vulkan implementation).
    *   `Renderer/`: High-level rendering logic (RenderGraph, Scene rendering).
    *   `Scene/`: Scene management.
    *   `Math/`: Math utilities (wraps GLM).
*   `Engine/Asset/`: Assets (Shaders, Models).

## Coding Conventions

The project follows specific C++ naming and formatting conventions (Microsoft Style).

**Naming:**
*   **Classes/Structs:** `PascalCase` (e.g., `MyClass`, `FrameResource`)
*   **Functions:** `PascalCase` (e.g., `RenderFrame`, `InitResources`)
*   **Public Members:** `camelCase` (e.g., `cmdList`)
*   **Private Members:** `camelCase_` (trailing underscore) (e.g., `device_`, `window_`)
*   **Private Constants:** `kPascalCase_` (e.g., `kMaxFramesInFlight_`)
*   **Namespaces:** `PascalCase` (e.g., `Engine`, `Rhi`)

**Formatting:**
*   **Indentation:** 4 spaces.
*   **Braces:** Allman style (Always break before braces).
    ```cpp
    void MyFunction()
    {
        if (condition)
        {
            // code
        }
    }
    ```
*   **Modules:** Use `.ixx` for module interfaces and `.cpp` for implementations (or strict module implementation units). `import std;` and `import Engine.*;`.

## Architecture

*   **RHI (Render Hardware Interface):** Abstracts the underlying graphics API (currently Vulkan). Located in `Engine/Source/Rhi`.
*   **Renderer:** Handles the rendering logic. It owns the `RenderGraph` and uses `Rhi` objects to draw the `Scene`.
*   **Scene:** Manages game objects and geometry.
*   **Engine Loop:** `MyEngine` class manages the main loop, windowing (GLFW), and synchronizes frames (`FrameResource`).

## Dependencies (Managed by xmake)
*   `glfw`
*   `spdlog`
*   `vulkansdk`
*   `tinygltf`
*   `imgui`
*   `glm`
*   `nlohmann_json`
