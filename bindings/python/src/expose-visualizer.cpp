#include "fwd.hpp"
#include <eigenpy/optional.hpp>

#include "candlewick/multibody/Visualizer.h"
#include <pinocchio/bindings/python/visualizers/visualizer-visitor.hpp>

#include <pinocchio/config.hpp>
#include <pinocchio/multibody/model.hpp>
#include <pinocchio/multibody/geometry.hpp>

using namespace candlewick;
using namespace candlewick::multibody;

#define DEF_PROP_PROXY(name)                                                   \
  add_property(#name, bp::make_function(                                       \
                          +[](Visualizer &v) -> auto & { return v.name(); },   \
                          bp::return_internal_reference<>()))

static auto visualizer_get_frame_debugs(Visualizer &viz) {
  auto view = viz.registry.view<DebugMeshComponent, const PinFrameComponent>();
  bp::list out;
  view.each([&](auto &dmc, auto) { out.append(boost::ref(dmc)); });
  return out;
}

void exposeVisualizer() {
  using pinocchio::python::VisualizerPythonVisitor;
  using pinocchio::visualizers::Vector3;

  eigenpy::OptionalConverter<ConstVectorRef, std::optional>::registration();
  eigenpy::OptionalConverter<Vector3, std::optional>::registration();
  eigenpy::detail::NoneToPython<std::nullopt_t>::registration();

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
      .def("resetCamera", &Visualizer::resetCamera, ("self"_a))
      .DEF_PROP_PROXY(device)
      .def(
          "takeScreenshot",
          +[](Visualizer &viz, const std::string &filename) {
            viz.takeScreenshot(filename);
          },
          ("self"_a, "filename"), "Save a screenshot to the specified file.")
      .def(
          "startRecording",
          +[]([[maybe_unused]] Visualizer &viz,
              [[maybe_unused]] const std::string &filename) {
#ifndef CANDLEWICK_WITH_FFMPEG_SUPPORT
            PyErr_WarnEx(PyExc_UserWarning,
                         "Recording videos is not available because Candlewick "
                         "was built without FFmpeg support.",
                         1);
            bp::throw_error_already_set();
#else
            viz.startRecording(filename);
#endif
          },
          ("self"_a, "filename"_a))
      .def("stopRecording", &Visualizer::stopRecording, "self"_a)
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
      .def("videoSettings", &Visualizer::videoSettings, "self"_a,
           bp::return_internal_reference<>())
#endif
      .def("addFrameViz", &Visualizer::addFrameViz,
           ("self"_a, "frame_id", "show_velocity"_a = true,
            "scale"_a = std::nullopt, "vel_scale"_a = std::nullopt),
           "Add visualization (triad and frame velocity) for the given frame "
           "by ID.")
      .def("removeFramesViz", &Visualizer::removeFramesViz, ("self"_a),
           "Remove visualization for all frames.")
      .def("getDebugFrames", &visualizer_get_frame_debugs, ("self"_a),
           "Get the DebugMeshComponent objects associated with the current "
           "debug frames.")
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
