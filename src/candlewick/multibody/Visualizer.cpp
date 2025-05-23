#include "Visualizer.h"
#include "../core/Device.h"
#include "../core/Components.h"
#include "../core/CameraControls.h"
#include "../core/DepthAndShadowPass.h"
#include "../primitives/Plane.h"
#include "RobotDebug.h"

namespace candlewick::multibody {

static Renderer _create_renderer(const Visualizer::Config &config) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    terminate_with_message("Failed to init video: %s", SDL_GetError());
  }

  return Renderer{Device{auto_detect_shader_format_subset()},
                  Window{"Candlewick Pinocchio visualizer", int(config.width),
                         int(config.height), 0},
                  config.depth_stencil_format};
}

Visualizer::Visualizer(const Config &config, const pin::Model &model,
                       const pin::GeometryModel &visual_model,
                       GuiSystem::GuiBehavior gui_callback)
    : BaseVisualizer{model, visual_model}
    , registry{}
    , renderer{_create_renderer(config)}
    , guiSystem{renderer, std::move(gui_callback)}
    , robotScene{registry, renderer}
    , debugScene{registry, renderer}
    , m_transferBuffers{renderer.device}
    , m_videoRecorder{NoInit} {

  RobotScene::Config rconfig;
  rconfig.enable_shadows = true;
  rconfig.shadow_config.numLights = kNumLights;
  robotScene.setConfig(rconfig);
  robotScene.loadModels(visualModel(), visualData());

  debugScene.addSystem<RobotDebugSystem>(this->model(), data());
  std::tie(m_triad, std::ignore) = debugScene.addTriad();
  std::tie(m_grid, std::ignore) = debugScene.addLineGrid();

  robotScene.directionalLight = {
      DirectionalLight{
          .direction = {0., -1., -1.},
          .color = {1.0, 1.0, 1.0},
          .intensity = 8.0,
      },
      DirectionalLight{
          .direction = {0.5, 1., -1.},
          .color = {1.0, 1.0, 1.0},
          .intensity = 8.0,
      },
  };

  robotScene.worldSpaceBounds.update({-1., -1., 0.}, {+1., +1., 1.});

  const Uint32 prepeat = 25;
  m_plane = robotScene.addEnvironmentObject(
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
  if (m_videoRecorder.isRecording())
    this->stopRecording();

  robotScene.release();
  debugScene.release();
  guiSystem.release();
  renderer.destroy();
  SDL_Quit();
}

void Visualizer::displayImpl() {
  processEvents();

  debugScene.update();
  robotScene.updateTransforms();
  render();

  if (!m_currentScreenshotFilename.empty()) {
    takeScreenshot(m_currentScreenshotFilename);
    m_currentScreenshotFilename.clear();
  }

  if (m_videoRecorder.isRecording()) {
    CommandBuffer command_buffer{device()};
    m_videoRecorder.writeTextureToVideoFrame(
        command_buffer, device(), m_transferBuffers, renderer.swapchain,
        renderer.getSwapchainTextureFormat());
  }
}

void Visualizer::render() {

  CommandBuffer command_buffer = renderer.acquireCommandBuffer();
  if (renderer.waitAndAcquireSwapchain(command_buffer)) {
    robotScene.collectOpaqueCastables();
    std::span castables = robotScene.castables();
    renderShadowPassFromAABB(command_buffer, robotScene.shadowPass,
                             robotScene.directionalLight, castables,
                             robotScene.worldSpaceBounds);

    auto &camera = controller.camera;
    robotScene.renderOpaque(command_buffer, camera);
    debugScene.render(command_buffer, camera);
    robotScene.renderTransparent(command_buffer, camera);
    guiSystem.render(command_buffer);
  }

  command_buffer.submit();
}

void Visualizer::takeScreenshot(std::string_view filename) {
  CommandBuffer command_buffer{device()};
  auto [width, height] = renderer.window.sizeInPixels();
  SDL_Log("Saving %dx%d screenshot at: '%s'", width, height, filename.data());
  media::saveTextureToFile(command_buffer, device(), m_transferBuffers,
                           renderer.swapchain,
                           renderer.getSwapchainTextureFormat(), Uint16(width),
                           Uint16(height), filename);
}

void Visualizer::startRecording(std::string_view filename) {
  if (m_videoRecorder.isRecording())
    terminate_with_message("Recording stream was already opened.");

  auto [width, height] = renderer.window.sizeInPixels();
  m_videoRecorder.open(Uint16(width), Uint16(height), filename);
  m_currentVideoFilename = filename;
}

void Visualizer::stopRecording() {
  m_currentVideoFilename.clear();
  SDL_Log("Wrote %d frames.", m_videoRecorder.frameCounter());
  m_videoRecorder.close();
}

} // namespace candlewick::multibody
