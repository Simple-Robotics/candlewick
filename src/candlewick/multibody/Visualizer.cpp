#include "Visualizer.h"
#include "../core/Device.h"
#include "../core/Components.h"
#include "../core/CameraControls.h"
#include "../core/DepthAndShadowPass.h"
#include "../primitives/Plane.h"
#include "RobotDebug.h"

namespace candlewick::multibody {

Visualizer::Visualizer(const Config &config, const pin::Model &model,
                       const pin::GeometryModel &visual_model,
                       GuiSystem::GuiBehavior gui_callback)
    : BaseVisualizer{model, visual_model}, registry{}, renderer{NoInit},
      guiSystem{NoInit, std::move(gui_callback)} {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(
        std::format("Failed to init video: {}", SDL_GetError()));
  }

  ::new (&renderer) Renderer{Device{auto_detect_shader_format_subset()},
                             Window{"Candlewick Pinocchio visualizer",
                                    int(config.width), int(config.height), 0},
                             config.depth_stencil_format};

  RobotScene::Config rconfig;
  rconfig.enable_shadows = true;
  robotScene.emplace(registry, renderer, visualModel(), visualData(), rconfig);
  debugScene.emplace(registry, renderer);
  debugScene->addSystem<RobotDebugSystem>(this->model(), data());
  std::tie(m_triad, std::ignore) = debugScene->addTriad();
  std::tie(m_grid, std::ignore) = debugScene->addLineGrid();

  robotScene->directionalLight = {
      .direction = {0., -1., -1.},
      .color = {1.0, 1.0, 1.0},
      .intensity = 8.0,
  };
  guiSystem.init(renderer);

  robotScene->worldSpaceBounds.update({-1., -1., 0.}, {+1., +1., 1.});

  const Uint32 prepeat = 25;
  m_plane = robotScene->addEnvironmentObject(
      loadPlaneTiled(0.5f, prepeat, prepeat), Mat4f::Identity());
  if (!envStatus.show_plane) {
    registry.emplace<Disable>(m_plane);
  }

  this->resetCamera();
}

void Visualizer::resetCamera() {
  const float radius = 2.5f;
  const Degf xy_plane_view_angle = 45.0_degf;
  Float3 eye{std::cos(xy_plane_view_angle), std::sin(xy_plane_view_angle),
             0.5f};
  eye *= radius;
  auto [w, h] = renderer.window.size();
  float aspectRatio = float(w) / float(h);
  controller.lookAt(eye, {0., 0., 0.});
  controller.camera.projection =
      perspectiveFromFov(DEFAULT_FOV, aspectRatio, 0.01f, 100.f);
}

void Visualizer::loadViewerModel() {}

void Visualizer::setCameraTarget(const Eigen::Ref<const Vector3> &target) {
  controller.lookAt1(target.cast<float>());
}

void Visualizer::setCameraPosition(const Eigen::Ref<const Vector3> &position) {
  auto &camera = controller.camera;
  camera_util::setWorldPosition(camera, position.cast<float>());
}

void Visualizer::setCameraPose(const Eigen::Ref<const Matrix4> &pose) {
  auto &camera = controller.camera;
  camera.view = pose.cast<float>().inverse();
}

Visualizer::~Visualizer() {
  robotScene->release();
  debugScene->release();
  guiSystem.release();
  renderer.destroy();
  SDL_Quit();
}

void Visualizer::displayImpl() {
  this->processEvents();

  debugScene->update();
  robotScene->updateTransforms();
  render();
}

void Visualizer::render() {

  CommandBuffer cmdBuf = renderer.acquireCommandBuffer();
  if (renderer.waitAndAcquireSwapchain(cmdBuf)) {
    robotScene->collectOpaqueCastables();
    std::span castables = robotScene->castables();
    renderShadowPassFromAABB(cmdBuf, robotScene->shadowPass,
                             robotScene->directionalLight, castables,
                             robotScene->worldSpaceBounds);

    auto &camera = controller.camera;
    robotScene->render(cmdBuf, camera);
    debugScene->render(cmdBuf, camera);
    guiSystem.render(cmdBuf);
  }

  cmdBuf.submit();
}

} // namespace candlewick::multibody
