#include <nanobind/nanobind.h>

#include "candlewick/core/RenderContext.h"

namespace nb = nanobind;
using namespace nb::literals;
using namespace candlewick;

#define _c(name) name = SDL_GPU_SHADERFORMAT_##name
enum class ShaderFormat_wrap : SDL_GPUShaderFormat {
  _c(INVALID),
  _c(PRIVATE),
  _c(SPIRV),
  _c(DXBC),
  _c(DXIL),
  _c(MSL),
  _c(METALLIB)
};
#undef _c

void exposeCore(nb::module_ &m) {
  nb::class_<Device>(m, "Device").def("driverName", &Device::driverName);

  m.def("get_num_gpu_drivers", SDL_GetNumGPUDrivers,
        "Get number of available GPU drivers.");

  m.def("get_gpu_driver_name", SDL_GetGPUDriver, ("index"_a),
        "Get the name of the GPU driver of the corresponding index.");

#define _c(name) value(#name, ShaderFormat_wrap::name)
  nb::enum_<ShaderFormat_wrap>(m, "ShaderFormat")
      ._c(INVALID)
      ._c(PRIVATE)
      ._c(SPIRV)
      ._c(DXBC)
      ._c(DXIL)
      ._c(MSL)
      ._c(METALLIB);
#undef _c

  m.def(
      "auto_detect_shader_format_subset",
      [](nb::str name) {
        return ShaderFormat_wrap(
            auto_detect_shader_format_subset(name.c_str()));
      },
      "name"_a);

  nb::class_<Renderer>(m, "Renderer").def_ro("device", &Renderer::device);
}

NB_MODULE(pycandlewick_nb, m) { exposeCore(m); }
