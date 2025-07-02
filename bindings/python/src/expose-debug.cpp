#include "fwd.hpp"
#include "candlewick/core/DebugScene.h"

using namespace candlewick;

void exposeDebug() {

  bp::class_<DebugMeshComponent, boost::noncopyable>("DebugMeshComponent",
                                                     bp::no_init)
      .def_readonly("colors", &DebugMeshComponent::colors)
      .def_readonly("enable", &DebugMeshComponent::enable)
      .def_readwrite("scale", &DebugMeshComponent::scale, "Debug model scale.");
}
