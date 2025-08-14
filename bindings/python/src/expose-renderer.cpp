#include "fwd.hpp"
#include "candlewick/core/RenderContext.h"

using namespace candlewick;

BOOST_PYTHON_FUNCTION_OVERLOADS(adfs_overloads,
                                auto_detect_shader_format_subset, 0, 1)

void exposeRenderer() {
  bp::class_<Device, boost::noncopyable>("Device", bp::no_init)
      .def("driverName", &Device::driverName, ("self"_a))
      .def("shaderFormats", &Device::shaderFormats, ("self"_a));

  bp::class_<Window, boost::noncopyable>("Window", bp::no_init)
      .def("pixelDensity", &Window::pixelDensity, ("self"_a))
      .def("displayScale", &Window::displayScale, ("self"_a))
      .def(
          "title", +[](const Window &w) { return w.title().data(); },
          ("self"_a));

#define _c(name) value(#name, SDL_GPUSampleCount::name)
  bp::enum_<SDL_GPUSampleCount>("SampleCount")
      ._c(SDL_GPU_SAMPLECOUNT_1)
      ._c(SDL_GPU_SAMPLECOUNT_2)
      ._c(SDL_GPU_SAMPLECOUNT_4)
      ._c(SDL_GPU_SAMPLECOUNT_8)
#undef _c
      .export_values();

  bp::def("get_num_gpu_drivers", SDL_GetNumGPUDrivers,
          "Get number of available GPU drivers.");

  bp::def("get_gpu_driver_name", SDL_GetGPUDriver, ("index"_a),
          "Get the name of the GPU driver of the corresponding index.");

  bp::def(
      "get_gpu_drivers",
      +[] {
        int n = SDL_GetNumGPUDrivers();
        bp::list a;
        if (n > 0) {
          for (int i = 0; i < n; i++)
            a.append(SDL_GetGPUDriver(i));
        }
        return a;
      },
      "Get all GPU drivers' names.");

  bp::def("auto_detect_shader_format_subset", auto_detect_shader_format_subset,
          adfs_overloads{
              ("driver_name"_a = NULL),
              "Automatically detect the compatible set of shader formats."});

  bp::class_<RenderContext, boost::noncopyable>("RenderContext", bp::no_init)
      .def_readonly("device", &RenderContext::device)
      .def_readonly("window", &RenderContext::window)
      .add_property("hasDepthTexture", &RenderContext::hasDepthTexture)
      .def("enableMSAA", &RenderContext::enableMSAA, ("self"_a, "samples"))
      .def("disableMSAA", &RenderContext::disableMSAA, ("self"_a));
}
