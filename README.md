[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/Simple-Robotics/candlewick/main.svg)](https://results.pre-commit.ci/latest/github/Simple-Robotics/candlewick/main)
[![Conda package](https://img.shields.io/conda/vn/conda-forge/candlewick.svg)](https://anaconda.org/conda-forge/candlewick)


# Candlewick: a WIP renderer based on SDL_gpu

> [!WARNING]
> Candlewick is still under **active** development. Support will be limited, and the API might break unexpectedly and repeatedly.


Candlewick is a WIP library for a renderer based on SDL3's new [GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU).

## Features

Candlewick comes with a set of graphical, interaction, and utility features.

### Graphical features

* Shadow mapping using directional shadow maps
* Screen-space ambient occlusion (SSAO)
* **WIP:** Screen-space shadows (SSS)

### Interactivitity

* Integration with ImGui
* Take screenshots of the main window
* **(optional)** Record videos from the main window using FFmpeg

### Pinocchio visualizer

Candlewick visualization utilities for robotics based on Pinocchio.

You can load a Pinocchio model, its geometry model, and create a visualizer that can be used similar to the other visualizers included in `pinocchio.visualize`.
Here's a Python example:

```python
import example_robot_data as erd
import pinocchio as pin
import numpy as np
from candlewick.multibody import Visualizer, VisualizerConfig

robot = erd.load("ur10")
model: pin.Model = robot.model
data: pin.Data = robot.data
visual_model = robot.visual_model

config = VisualizerConfig()
config.width = 1280
config.height = 720
viz = Visualizer(config, model, visual_model)

q0 = pin.neutral(model)
viz.setCameraPose(pin.SE3.Identity())
viz.display(q0)
```


## Installation

### Dependencies

Candlewick depends mainly on:

* [SDL3](https://github.com/libsdl-org/SDL/) for windowing, processing input events, and of course SDL GPU.
* [EnTT](https://github.com/skypjack/entt/) for the entity-component system (ECS)
* [nlohmann/json](https://github.com/nlohmann/json) for processing JSON files
* the [Eigen](https://gitlab.com/libeigen/eigen/) linear algebra template library
* the [Coal](https://github.com/coal-library/coal) collision algorithms library
* [magic_enum](https://github.com/Neargye/magic_enum) (enum reflection library)
* [Open Asset Importer Library (assimp)](https://assimp-docs.readthedocs.io/en/latest/) for loading meshes

These dependencies can be installed from Conda as follows:
```bash
conda install -c conda-forge sdl3 eigen coal magic_enum assimp entt nlohmann_json
```

#### Optional dependencies

* [eigenpy](https://github.com/stack-of-tasks/eigenpy) for Python bindings.
* [FFmpeg](https://ffmpeg.org/) for support for recording videos from the rendered graphics. The CMake finder module also requires pkg-config.
  ```bash
  conda install ffmpeg pkg-config
  ```
* [GoogleTest](https://github.com/google/googletest) for the tests | `conda install gtest`
* [CLI11](https://github.com/CLIUtils/CLI11) for the examples and tests | `conda install cli11`
* The [Pinocchio](https://github.com/stack-of-tasks/pinocchio) rigid-body dynamics library (required for the `candlewick::multibody` classes and functions). Pinocchio must be built with collision support. | `conda install -c conda-forge pinocchio`
  * With Pinocchio support activated, building the tests requires [example-robot-data](https://github.com/Gepetto/example-robot-data) | `conda install -c conda-forge example-robot-data`

The optional dependencies can be installed from conda:
```bash
conda install -c conda-forge
  gtest \  # for testing
  ffmpeg pkg-config \  # for FFmpeg support
  cli11 \  # for the examples
  pinocchio \  # for multibody/Pinocchio support
  example-robot-data # for the examples, with the above
```

### Building

For building the library, you will need [CMake](https://cmake.org/) (version at least 3.20) and a C++20-compliant compiler. These can also be obtained through Conda.

In the directory where you have checked out the code, perform the following steps:
```bash
# 1. Create a CMake build directory
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PINOCCHIO_VISUALIZER:BOOL=ON \  # For Pinocchio support
  -DBUILD_PYTHON_BINDINGS:BOOL=ON \ # For Python bindings
  -GNinja \ # or -G"Unix Makefiles" to use Make
  -DBUILD_TESTING=OFF \  # or ON not build the tests
  -DCMAKE_INSTALL_PREFIX=<your-install-prefix> # e.g. ~/.local/, or $CONDA_PREFIX
# 2. Move into it and build (generator-independent)
cd build/ && cmake --build . -j<num-parallel-jobs>
# 3. Install
cmake --install .
```

## Credits

Many of the design choices of this library are heavily inspired by other, more mature work in the open-source 3D graphics middleware space.

Here are some of the resources I've looked at:

* the [Magnum](https://magnum.graphics/) graphics middleware (the design ideas around mesh layouts, how to load geometry primitives, and the type-erased `MeshData` type)
* [bgfx](https://github.com/bkaradzic/bgfx/)
* Sascha Willems' Vulkan examples: https://github.com/SaschaWillems/Vulkan/
