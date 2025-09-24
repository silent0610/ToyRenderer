## Project Overview

This is a modern C++20 real-time rendering project that uses the Vulkan API. It's built with `xmake` and appears to be primarily for Windows, generating a Visual Studio solution. The project implements a deferred rendering pipeline with a rich set of features, including:

*   Physically-Based Rendering (PBR) with Image-Based Lighting (IBL)
*   Advanced shadow mapping techniques (Cascaded Shadow Maps and omni-directional shadows)
*   Multiple Ambient Occlusion methods (SSAO, HBAO, GTAO)
*   Post-processing effects like FXAA and Bloom
*   An immediate-mode GUI using ImGui for tweaking rendering parameters.
*   Experimental support for advanced rendering techniques like voxelization and GPU-accelerated Sparse Voxel Octrees (SVOs).

The project is currently undergoing a significant refactoring to introduce a new Render Hardware Interface (RHI) abstraction layer. This is intended to decouple the core rendering logic from the specific Vulkan implementation, making the engine more modular and potentially portable to other graphics APIs in the future.

## Building and Running

1.  **Prerequisites:**
    *   [xmake](https://xmake.io/)
    *   A Vulkan-compatible GPU and drivers.
    *   Visual Studio (for the generated solution).

2.  **Generate Visual Studio Solution:**
    ```bash
    xmake project -k vsxmake build/
    ```

3.  **Build and Run:**
    *   Open the generated solution file in the `build` directory with Visual Studio.
    *   Set the `MyToyRenderer` project as the startup project.
    *   Build and run the project.

## Development Conventions

*   **Language:** C++20 with modules.
*   **Build System:** `xmake`.
*   **Architecture:** The project is structured into several modules, including `Application`, `Renderer`, and `Rhi`. The `Renderer` class is the core of the application, managing the main loop and rendering passes. The new `Rhi` layer is being introduced to abstract away the Vulkan-specific code.
*   **Coding Style:**
    *   PascalCase for classes and structs.
    *   camelCase for member variables.
    *   The code makes extensive use of modern C++ features.
*   **Comments:** The code contains a significant number of comments written in Chinese.

