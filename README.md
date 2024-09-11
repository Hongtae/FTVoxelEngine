# VoxelEngine / Volume Renderer & Dynamics [WIP]

### C++ Lightweight Voxel Engine
This engine is a volumetric visualization and dynamics simulation engine, not a full-featured game engine.

## Features:
- Volume Rendering
  - Raycasting SVO
  - Vulkan Renderer
- Concurrency
  - GPGPU
  - DispatchQueue
- 3D Audio with OpenAL
- Volume Dynamics **(WIP)**

## Modules
- Core
  - Core-Engine
- Editor
  - Scene Editor (WIP)
  - Currently, there are only volume data conversion and visualization capabilities.
- Game
  - Game for functional testing
- RenderTest
  - Vulkan rendering test

# Build
- Visual Studio 2022
  - Desktop development with C++
- Vulkan SDK 1.3.243 or later (requires Validation-Layer for debugging)
  - [Download Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
    > **Note**  
    > to disable Validation-Layer
    > 1. set `enableValidation = false` at `GraphicsDeviceContext::makeDefault()`
    > 2. build Release
- GIT with LFS


# Test
1. Build & Run Editor
1. open glTF then convert to VXM
1. Build & Run Game
1. Load VXM


# Screenshots
> [!NOTE]  
This repository does not contain any graphical resources.  
Please use your own glTF or [download glTF samples here](https://github.com/KhronosGroup/glTF-Sample-Assets).

![image-2024-2-23_13-27-46](https://github.com/user-attachments/assets/959f76a3-ed0d-4e30-b1e0-4d62c4501752)
![image-2024-2-23_13-26-51](https://github.com/user-attachments/assets/b693d625-8894-4326-942b-89a39005f526)
![image-2024-2-23_13-26-18](https://github.com/user-attachments/assets/f3eec5a4-98de-48e1-acf1-866449b8046d)
![image-2024-2-23_13-25-34](https://github.com/user-attachments/assets/fbb63a61-6498-4af4-b60a-4ffc45f242ce)
![image-2024-2-23_13-39-33](https://github.com/user-attachments/assets/41842b12-195f-49d3-8d8a-1d6505595907)
![image-2024-2-23_13-42-46](https://github.com/user-attachments/assets/4ea331a3-fc56-4dc2-ad88-bef1ac0f79e3)
![image-2024-2-23_13-49-52](https://github.com/user-attachments/assets/af79aad3-98e9-45b0-8aa2-14e6a1eab7ed)
![image-2024-2-23_13-51-25](https://github.com/user-attachments/assets/7bd62e13-8adb-4b64-b03a-c4915e230b3c)

# Video Clips
![duck](https://github.com/user-attachments/assets/48a07d3e-9d3b-47bf-a6c6-31ad3273bf21)
![cartoonFarm_LOD](https://github.com/user-attachments/assets/1e70c38b-9b57-4a23-8085-fb4dc8832ec1)

https://github.com/user-attachments/assets/954d9ce9-a651-463b-8234-2c956f62a571
