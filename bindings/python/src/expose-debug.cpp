#include "fwd.hpp"
#include "candlewick/core/DebugScene.h"

#include <eigenpy/std-vector.hpp>

using namespace candlewick;

void exposeDebug() {
#define _c(name) value(#name, DebugPipelines::name)
  bp::enum_<DebugPipelines>("DebugPipelines")
      ._c(TRIANGLE_FILL)
      ._c(TRIANGLE_LINE);
#undef _c

  bp::class_<DebugMeshComponent, boost::noncopyable>("DebugMeshComponent",
                                                     bp::no_init)
      .def_readwrite("pipeline_type", &DebugMeshComponent::pipeline_type)
      .def_readonly("colors", &DebugMeshComponent::colors)
      .def_readonly("enable", &DebugMeshComponent::enable)
      .def_readwrite("scale", &DebugMeshComponent::scale, "Debug model scale.");

  eigenpy::StdVectorPythonVisitor<std::vector<Float4>>::expose("StdVec_Vec4");
}
