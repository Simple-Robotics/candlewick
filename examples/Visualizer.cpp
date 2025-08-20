#include "candlewick/multibody/Visualizer.h"
#include "candlewick/multibody/RobotLoader.h"

#include <pinocchio/algorithm/joint-configuration.hpp>
#include <pinocchio/algorithm/geometry.hpp>
#include <chrono>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

#include <spdlog/cfg/env.h>

using namespace candlewick::multibody;
using candlewick::sdlSampleToValue;
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

static void addFloor(pin::GeometryModel &geom_model) {
  auto coll = std::make_shared<coal::Plane>(0., 0., 1., -0.1);
  pin::GeometryObject object{"plane", 0ul, coll, pin::SE3::Identity()};
  object.meshColor << 1.0, 1.0, 1.0, 1.0;
  geom_model.addGeometryObject(object);
}

static void addBall(pin::GeometryModel &geom_model) {
  auto sp = std::make_shared<coal::Sphere>(0.2);
  pin::SE3 M = pin::SE3::Identity();
  M.translation() << 0.4, 0.1, 0.3;
  pin::GeometryObject object{"sphere", 0ul, sp, M};
  object.meshColor << 1.0, 1.0, 0.2, 0.3;
  geom_model.addGeometryObject(object);
}

int main(int argc, char *argv[]) {
  CLI::App app{"Visualizer example"};
  argv = app.ensure_utf8(argv);
  std::array<Uint32, 2> window_dims{1920u, 1080u};
  double fps;
  SDL_GPUSampleCount sample_count{SDL_GPU_SAMPLECOUNT_1};
  const std::map<std::string, SDL_GPUSampleCount> sample_count_map{
      {"1", SDL_GPU_SAMPLECOUNT_1},
      {"2", SDL_GPU_SAMPLECOUNT_2},
      {"4", SDL_GPU_SAMPLECOUNT_4},
      {"8", SDL_GPU_SAMPLECOUNT_8}};
  const auto transform_validator =
      CLI::IsMember(sample_count_map) & CLI::Transformer(sample_count_map);

  app.add_option("--dims", window_dims, "Window dimensions.")
      ->capture_default_str();
  app.add_option<double, unsigned int>("--fps", fps, "Framerate")
      ->default_val(60);
  app.add_option("--msaa", sample_count, "Level of multisample anti-aliasing.")
      ->default_function([&sample_count] {
        return std::to_string(sdlSampleToValue(sample_count));
      })
      ->transform(transform_validator)
      ->capture_default_str();

  CLI11_PARSE(app, argc, argv);

  spdlog::cfg::load_env_levels();
  spdlog::set_pattern(">>> [%T] [%^%l%$] %v");
  spdlog::info("Robot spec:\n{}", ur_robot_spec);
  pin::Model model;
  pin::GeometryModel geom_model;
  loadModels(ur_robot_spec, model, &geom_model, NULL);
  addFloor(geom_model);
  addBall(geom_model);

  Visualizer::Config config{
      window_dims[0],
      window_dims[1],
      sample_count,
  };
  Visualizer visualizer{config, model, geom_model};
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
