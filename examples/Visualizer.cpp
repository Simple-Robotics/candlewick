#include "candlewick/multibody/Visualizer.h"
#include "candlewick/multibody/RobotLoader.h"

#include <pinocchio/algorithm/joint-configuration.hpp>
#include <pinocchio/algorithm/geometry.hpp>
#include <chrono>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

using namespace candlewick::multibody;
using pinocchio::visualizers::Vector3;
using std::chrono::steady_clock;
namespace fs = std::filesystem;

static const RobotSpec ur_robot_spec =
    RobotSpec{
        "urdf/ur5_gripper.urdf",
        "srdf/ur5_gripper.srdf",
        fs::path(EXAMPLE_ROBOT_DATA_MODEL_DIR).parent_path(),
        "robots/ur_description",
    }
        .ensure_absolute_filepaths();

int main(int argc, char **argv) {
  CLI::App app{"Visualizer example"};
  argv = app.ensure_utf8(argv);
  std::array<Uint32, 2> window_dims{1920u, 1080u};
  double fps;

  app.add_option("--dims", window_dims, "Window dimensions.")
      ->capture_default_str();
  app.add_option<double, unsigned int>("--fps", fps, "Framerate")
      ->default_val(60);

  CLI11_PARSE(app, argc, argv);

  pin::Model model;
  pin::GeometryModel geom_model;
  loadModels(ur_robot_spec, model, &geom_model, NULL);

  Visualizer visualizer{{window_dims[0], window_dims[1]}, model, geom_model};
  assert(!visualizer.hasExternalData());
  visualizer.addFrameViz(model.getFrameId("world"), false, Vector3::Ones());
  visualizer.addFrameViz(model.getFrameId("elbow_joint"));
  visualizer.addFrameViz(model.getFrameId("ee_link"));
  pin::Data &data = visualizer.data();

  Eigen::VectorXd q0 = pin::neutral(model);
  Eigen::VectorXd q1 = pin::randomConfiguration(model);

  double dt = 1. / static_cast<double>(fps);
  using duration_t = std::chrono::duration<double>;
  Eigen::VectorXd q = q0;
  Eigen::VectorXd qn = q;
  Eigen::VectorXd v = Eigen::VectorXd::Zero(model.nv);

  double t = 0.;

  while (!visualizer.shouldExit()) {
    const auto now = steady_clock::now();

    double alpha = std::sin(t);
    pin::interpolate(model, q0, q1, alpha, q);
    pin::difference(model, qn, q, v);
    v /= dt;
    pin::forwardKinematics(model, data, q, v);

    visualizer.display();
    std::this_thread::sleep_until(now + duration_t(dt));

    t += dt;
    qn = q;
  }
  return 0;
}
