#include "fwd.hpp"
#include <eigenpy/optional.hpp>

#include "candlewick/multibody/Visualizer.h"
#include <pinocchio/bindings/python/visualizers/visualizer-visitor.hpp>

#include <pinocchio/config.hpp>
#include <pinocchio/multibody/model.hpp>
#include <pinocchio/multibody/geometry.hpp>

using namespace candlewick::multibody;

void exposeVisualizer() {
  eigenpy::OptionalConverter<ConstVectorRef, std::optional>::registration();
  bp::class_<Visualizer::Config>("VisualizerConfig", bp::init<>())
      .def_readwrite("width", &Visualizer::Config::width)
      .def_readwrite("height", &Visualizer::Config::height);

  auto cl =
      bp::class_<Visualizer, boost::noncopyable>("Visualizer", bp::no_init)
          .def(bp::init<Visualizer::Config, const pin::Model &,
                        const pin::GeometryModel &>(
              ("self"_a, "config", "model", "geomModel")))
          .def(pinocchio::python::VisualizerPythonVisitor<Visualizer>{})
          .def_readonly("renderer", &Visualizer::renderer)
          .add_property("shouldExit", &Visualizer::shouldExit);

// fix for Pinocchio 3.5.0
#if PINOCCHIO_VERSION_AT_MOST(3, 5, 0)
#define DEF_PROP_PROXY(name)                                                   \
  add_property(#name, bp::make_function(                                       \
                          +[](Visualizer &v) -> auto & { return v.name(); },   \
                          bp::return_internal_reference<>()))
  cl //
      .DEF_PROP_PROXY(model)
      .DEF_PROP_PROXY(visualModel)
      .DEF_PROP_PROXY(collisionModel)
      .DEF_PROP_PROXY(data)
      .DEF_PROP_PROXY(visualData)
      .DEF_PROP_PROXY(collisionData);
#undef DEF_PROP_PROXY
#endif
}
