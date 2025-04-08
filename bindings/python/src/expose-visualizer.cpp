#include "fwd.hpp"
#include <eigenpy/optional.hpp>

#include "candlewick/multibody/Visualizer.h"
#include <pinocchio/bindings/python/visualizers/visualizer-visitor.hpp>

#include <pinocchio/multibody/model.hpp>
#include <pinocchio/multibody/geometry.hpp>

using namespace candlewick::multibody;

void exposeVisualizer() {
  eigenpy::OptionalConverter<ConstVectorRef, std::optional>::registration();
  bp::class_<Visualizer::Config>("VisualizerConfig", bp::init<>())
      .def_readwrite("width", &Visualizer::Config::width)
      .def_readwrite("height", &Visualizer::Config::height);
  bp::class_<Visualizer, boost::noncopyable>("Visualizer", bp::no_init)
      .def(bp::init<Visualizer::Config, const pin::Model &,
                    const pin::GeometryModel &>(
          ("self"_a, "config", "model", "geomModel")))
      .def(pinocchio::python::VisualizerPythonVisitor<Visualizer>{})
      .def_readonly("renderer", &Visualizer::renderer)
      .add_property("shouldExit", &Visualizer::shouldExit);
}
