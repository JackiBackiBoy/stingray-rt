<h1 align="center">stingray</h1>
<p align="center">GPU-accelerated real-time path tracer powered by Vulkan® and C++.</p>
<h1 align="center"></h1>

Stingray is a real-time path tracer leveraging the Vulkan ray tracing pipeline. Delivering high-fidelity graphics with a minimalistic and efficient design philosophy in mind.

# Features
- **Fully bindless resources:** Buffers, images, and acceleration structures are efficiently managed without the constraints of fixed bindings, enabling maximum flexibility in rendering pipelines.
- **Fully path traced:** Includes physically accurate shadows, reflections, refraction, and global illumination.
- **Lean and mean:** Very minimal and clean abstraction layer over both Vulkan and the rendering hardware interface (RHI).
  Stingray makes very little use of OOP design patterns, opting instead for a data-driven approach with POD types.
- **Automatic GPU synchronization:** Stingray utilizes a render graph to manage resource barriers between passes, ensuring efficient execution.

# Gallery
![Sponza](https://github.com/JackiBackiBoy/stingray-rt/blob/main/Gallery/sponza.png)
![Sponza balcony](https://github.com/JackiBackiBoy/stingray-rt/blob/main/Gallery/sponza_balcony.png)
![Cornell box with Fresnel](https://github.com/JackiBackiBoy/stingray-rt/blob/main/Gallery/cornell_box_fresnel.png)
![Cornell box with Fresnel 2](https://github.com/JackiBackiBoy/stingray-rt/blob/main/Gallery/cornell_box_fresnel_2.png)
![Simple Cornell box](https://github.com/JackiBackiBoy/stingray-rt/blob/main/Gallery/simple_cornell_box.png)

# Building
## Prerequisites
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) version 1.3 or higher.
- [CMake](https://cmake.org/) version 3.6 or higher.
- Windows 10 or higher.
- Microsoft Visual Studio.
- GPU with hardware ray tracing support.

## Build Instructions
In order for all dependencies to be downloaded with the project, you must recursively clone this repo via:
```
git clone --recursive https://github.com/JackiBackiBoy/stingray-rt 
```

Once downloaded, run the `build.bat` script which will automatically create a `build` directory. Or alternatively run:
```
mkdir build
cd build
cmake -S ../ -B .
cd ..
```

You will also need to compile the Vulkan shaders, which are not automatically compiled. Run the `compile_vk_shaders.bat` script.
You can now open the `.sln` project file located in the `build` directory with Microsoft Visual Studio. Also remember to set
`stingray` as the "Startup Project" in Visual Studio. This can be done by right-clicking the
stingray project and select the option "Set as Startup Project".

You should now be able to run the program!

# Controls
- W/A/S/D - Move forward, left, back and right.
- Space - Move upwards.
- Left Control - Move downwards.
- Middle Mouse Button (Mouse3) - Hold down and move the mouse to pan the camera.
- F11 - Enable/Disable the UI overlay.

# Dependencies
- [glm](https://github.com/g-truc/glm) - Math operations.
- [freetype](https://github.com/freetype/freetype) - Font loading and font atlas creation.
- [tinygltf](https://github.com/syoyo/tinygltf) - GLTF model loader.
- [volk](https://github.com/zeux/volk) - Meta-loader for Vulkan.

# Roadmap
- Support for real-time denoising.
- Dynamic scene support with rebuildable TLAS/BLAS.
- Additional primitive support for path-traced scenes.
- Extended rendering effects like caustics and volumetrics.

# Acknowledgements
- Pharr, Jakob, and Humphreys, *Physically Based Rendering - From Theory to Implementation*, 4th edn.
- Peter Shirley, Trevor David Black, and Steve Hollasch, *Ray Tracing in One Weekend*.
