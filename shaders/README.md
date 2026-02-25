# Shaders

Candlewick's shaders are written using the [Slang](https://shader-slang.org/) [shading language](https://github.com/shader-slang/slang), a new, scalable shading language to manage a set of shaders in a modular fashion.

The shader workflow is as follows:
- edit the `.slang` shader source file
- run `slangc` to output SPIR-V and MSL, using the `process_shaders.py` script at the root of this repository.


Before moving to Slang, our shaders were written in classic GLSL with the required `layout` and other keywords to fit with both the Vulkan GLSL spec and SDLGPU's own idiosyncracies. The shaders were then compiled to SPIR-V using Google's `glslc` and this bytecode was then transpiled to Metal along with an emitted JSON metadata file using [shadercross](https://github.com/libsdl-org/SDL_shadercross).
