# Sidreal Engine 2026

Sidreal Engine 2026 is a small rendering engine project intended for learning modern Vulkan rendering and C++ game engine concepts. It is an educational codebase and playground for experimenting with pipelines, resource management, model loading, and other rendering systems.

## Features
- **Vertex/Index Buffer Pulling** — Efficient GPU-driven buffer management for fast vertex and index data access
- **Automatic Model Instancing** — Zero-setup instancing pipeline for rendering repeated geometry with minimal draw calls
- **Slang Shader Compilation** — Live-compiles Slang shaders directly to SPIR-V at load time
- **JSON Scene Composition** — Declarative scene files with transform data (position, rotation, scale) for easy level design
- **GLTF 3.0 Model Loading** — Uses tinyGLTF v3 for loading modern glTF assets
- **Modern Vulkan API**
  - **Dynamic Rendering** — Vulkan 1.3 dynamic rendering with no legacy render pass objects
  - **Timeline Semaphores** — Clean GPU synchronization for frame-in-flight management

## Building
### Requirements
- Windows 10/11 or later
- Visual Studio 2022/2026
- Vulkan SDK installed (LunarG Vulkan SDK) and up-to-date GPU drivers

### Instructions
1. Install the Vulkan SDK and ensure VK_SDK_PATH is available in your environment.
2. Run the according build script for Windows in the Scripts folder to generate project files.
2. Open the solution file in Visual Studio
3. Build the project and run through VS or the App executable.

## Included Third-party Libraries & Tools
- [tinyGLTF](https://github.com/syoyo/tinygltf)
- [GLM](https://github.com/g-truc/glm)
- [SDL2](https://github.com/libsdl-org/SDL)
- [Vulkan Memory Allocator](https://gpuopen.com/vulkan-memory-allocator/)
- [Premake](https://premake.github.io/)
- [JSON for Modern C++](https://github.com/nlohmann/json)
