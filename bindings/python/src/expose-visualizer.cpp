#include "fwd.hpp"
#include <eigenpy/optional.hpp>

#include "candlewick/multibody/Visualizer.h"
#include <pinocchio/bindings/python/visualizers/visualizer-visitor.hpp>

#include <pinocchio/config.hpp>
#include <pinocchio/multibody/model.hpp>
#include <pinocchio/multibody/geometry.hpp>

using namespace candlewick::multibody;

#define DEF_PROP_PROXY(name)                                                   \
  add_property(#name, bp::make_function(                                       \
                          +[](Visualizer &v) -> auto & { return v.name(); },   \
                          bp::return_internal_reference<>()))

void exposeVisualizer() {
  using pinocchio::python::VisualizerPythonVisitor;

  eigenpy::OptionalConverter<ConstVectorRef, std::optional>::registration();
  bp::class_<Visualizer::Config>("VisualizerConfig", bp::init<>("self"_a))
      .def_readwrite("width", &Visualizer::Config::width)
      .def_readwrite("height", &Visualizer::Config::height);

  bp::class_<Visualizer, boost::noncopyable>("Visualizer", bp::no_init)
      .def(bp::init<Visualizer::Config, const pin::Model &,
                    const pin::GeometryModel &>(
          ("self"_a, "config", "model", "visual_model")))
      .def(bp::init<Visualizer::Config, const pin::Model &,
                    const pin::GeometryModel &, pin::Data &,
                    pin::GeometryData &>(
          ("self"_a, "config", "model", "visual_model", "data", "visual_data")))
      .def(VisualizerPythonVisitor<Visualizer>{})
      .def_readonly("renderer", &Visualizer::renderer)
      .def_readwrite("worldSceneBounds", &Visualizer::worldSceneBounds)
      .def(
          "takeScreenshot",
          +[](Visualizer &viz, const std::string &filename) {
            viz.takeScreenshot(filename);
          },
          ("self"_a, "filename"), "Save a screenshot to the specified file.")
      .def(
          "startRecording",
          +[](Visualizer &viz, const std::string &filename, int fps) {
            viz.startRecording(filename, fps);
          },
          ("self"_a, "filename"_a, "fps"_a = 30))
      .def("stopRecording", &Visualizer::stopRecording, "self"_a)
      .def("addFrameViz", &Visualizer::addFrameViz, ("self"_a, "frame_id"),
           "Add visualization (triad and frame velocity) for the given frame "
           "by ID.")
      .def("removeFramesViz", &Visualizer::removeFramesViz, ("self"_a),
           "Remove visualization for all frames.")
// fix for Pinocchio 3.5.0
#if PINOCCHIO_VERSION_AT_MOST(3, 5, 0)
      .DEF_PROP_PROXY(model)
      .DEF_PROP_PROXY(visualModel)
      .DEF_PROP_PROXY(collisionModel)
      .DEF_PROP_PROXY(data)
      .DEF_PROP_PROXY(visualData)
      .DEF_PROP_PROXY(collisionData)
#endif
      .add_property("shouldExit", &Visualizer::shouldExit);
}
#undef DEF_PROP_PROXY
