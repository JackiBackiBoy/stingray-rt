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
![Simple Cornell box](https://github.com/JackiBackiBoy/stingray-rt/blob/main/gallery/simple_cornell_box.png)

# Dependencies
- [glfw](https://github.com/glfw/glfw) - Window management and input handling.
- [glm](https://github.com/g-truc/glm) - Math operations.
- [freetype](https://github.com/freetype/freetype) - Font loading and font atlas creation.
- [tinygltf](https://github.com/syoyo/tinygltf) - GLTF model loader.
- [volk](https://github.com/zeux/volk) - Meta-loader for Vulkan.

# Roadmap
- Improve material system.
- Support for real-time denoising.
- Dynamic scene support with rebuildable TLAS/BLAS.
- Additional primitive support for path-traced scenes.
- Extended rendering effects like caustics and volumetrics.

# Acknowledgements
- Pharr, Jakob, and Humphreys, *Physically Based Rendering - From Theory to Implementation*, 4th edn.
- Peter Shirley, Trevor David Black, and Steve Hollasch, *Ray Tracing in One Weekend*

