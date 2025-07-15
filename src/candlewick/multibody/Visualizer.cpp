#include "Visualizer.h"
#include "../core/Device.h"
#include "../core/CameraControls.h"
#include "../core/DepthAndShadowPass.h"
#include "RobotDebug.h"

#include <pinocchio/algorithm/frames.hpp>
#include <SDL3/SDL_init.h>

namespace candlewick {
const char *sdlMouseButtonToString(Uint8 button) {
  switch (button) {
  case SDL_BUTTON_LEFT:
    return "LMB";
  case SDL_BUTTON_MIDDLE:
    return "MMB";
  case SDL_BUTTON_RIGHT:
    return "RMB";
  case SDL_BUTTON_X1:
    return "X1";
  case SDL_BUTTON_X2:
    return "X2";
  default:
    terminate_with_message("Unsupported button value (%d)", button);
  }
}
} // namespace candlewick

namespace candlewick::multibody {

static RenderContext _create_renderer(const Visualizer::Config &config) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    terminate_with_message("Failed to init video: {:s}", SDL_GetError());
  }
  SDL_Log("Video driver: %s", SDL_GetCurrentVideoDriver());

  return RenderContext{Device{auto_detect_shader_format_subset()},
                       Window{"Candlewick Pinocchio visualizer",
                              int(config.width), int(config.height), 0},
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
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
    , m_videoRecorder{NoInit}
#endif
{
  this->initialize();
}

Visualizer::Visualizer(const Config &config, const pin::Model &model,
                       const pin::GeometryModel &visual_model, pin::Data &data,
                       pin::GeometryData &visual_data,
                       GuiSystem::GuiBehavior gui_callback)
    : BaseVisualizer{model, visual_model, nullptr, data, visual_data, nullptr}
    , registry{}
    , renderer{_create_renderer(config)}
    , guiSystem{renderer, std::move(gui_callback)}
    , robotScene{registry, renderer}
    , debugScene{registry, renderer}
    , m_transferBuffers{renderer.device}
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
    , m_videoRecorder{NoInit}
#endif
{
  this->initialize();
}

void Visualizer::initialize() {
  RobotScene::Config rconfig;
  rconfig.enable_shadows = true;
  rconfig.enable_normal_target = true;
  robotScene.setConfig(rconfig);
  robotScene.loadModels(visualModel(), visualData());

  m_robotDebug = &debugScene.addSystem<RobotDebugSystem>(this->model(), data());
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

  worldSceneBounds.update({-1., -1., 0.}, {+1., +1., 1.});

  this->resetCamera();

  SDL_Log("┌───────Controls────────");
  SDL_Log("│ Toggle GUI:      [%s]", "H");
  SDL_Log("│ Move camera:     [%s]",
          sdlMouseButtonToString(cameraParams.mouseButtons.rotButton));
  SDL_Log("│ Pan camera:      [%s]",
          sdlMouseButtonToString(cameraParams.mouseButtons.panButton));
  SDL_Log("│ Y-rotate camera: [%s]",
          sdlMouseButtonToString(cameraParams.mouseButtons.yRotButton));
  SDL_Log("└───────────────────────");
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
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  this->stopRecording();
#endif
  m_transferBuffers.release();

  robotScene.release();
  debugScene.release();
  guiSystem.release();
  renderer.destroy();
  SDL_Quit();
}

void Visualizer::displayImpl() {
  // update frames. needed for frame debug viz
  pin::updateFramePlacements(model(), data());

  this->processEvents();

  debugScene.update();
  robotScene.updateTransforms();
  this->render();

  if (m_shouldScreenshot) {
    this->takeScreenshot(m_currentScreenshotFilename);
    m_currentScreenshotFilename.clear();
    m_shouldScreenshot = false;
  }

#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  if (m_videoRecorder.isRecording()) {
    CommandBuffer command_buffer{device()};
    m_videoRecorder.writeTextureToVideoFrame(
        command_buffer, device(), m_transferBuffers, renderer.swapchain,
        renderer.getSwapchainTextureFormat());
  }
#endif
}

void Visualizer::render() {

  CommandBuffer command_buffer = renderer.acquireCommandBuffer();
  if (renderer.waitAndAcquireSwapchain(command_buffer)) {
    robotScene.collectOpaqueCastables();
    std::span castables = robotScene.castables();
    renderShadowPassFromAABB(command_buffer, robotScene.shadowPass,
                             robotScene.directionalLight, castables,
                             worldSceneBounds);

    auto &camera = controller.camera;
    robotScene.renderOpaque(command_buffer, camera);
    debugScene.render(command_buffer, camera);
    robotScene.renderTransparent(command_buffer, camera);
    if (m_showGui)
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

void Visualizer::startRecording([[maybe_unused]] std::string_view filename) {
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  if (m_videoRecorder.isRecording())
    terminate_with_message("Recording stream was already opened.");

  auto [width, height] = renderer.window.sizeInPixels();
  m_videoRecorder.open(Uint32(width), Uint32(height), filename,
                       m_videoSettings);
  m_currentVideoFilename = filename;
#else
  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
              "Visualizer::%s() does nothing here, since Candlewick was "
              "compiled without FFmpeg support.",
              __FUNCTION__);
#endif
}

void Visualizer::stopRecording() {
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  if (!m_videoRecorder.isRecording())
    return;
  m_currentVideoFilename.clear();
  SDL_Log("Wrote %d frames.", m_videoRecorder.frameCounter());
  m_videoRecorder.close();
#endif
}

void Visualizer::addFrameViz(pin::FrameIndex id, bool show_velocity) {
  assert(m_robotDebug);
  m_debug_frame_pos.push_back(m_robotDebug->addFrameTriad(debugScene, id));
  if (show_velocity)
    m_debug_frame_vel.push_back(
        m_robotDebug->addFrameVelocityArrow(debugScene, id));
}

void Visualizer::removeFramesViz() {
  registry.destroy(m_debug_frame_pos.begin(), m_debug_frame_pos.end());
  registry.destroy(m_debug_frame_vel.begin(), m_debug_frame_vel.end());
  m_debug_frame_pos.clear();
  m_debug_frame_vel.clear();
}

} // namespace candlewick::multibody
