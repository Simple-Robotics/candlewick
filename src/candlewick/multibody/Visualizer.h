/// \file Visualizer.h
/// \author ManifoldFR
/// \copyright 2025 INRIA
#pragma once

#include "RobotScene.h"
#include "../core/CameraControls.h"
#include "../core/GuiSystem.h"
#include "../core/DebugScene.h"
#include "../core/RenderContext.h"
#include "../utils/WriteTextureToImage.h"
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
#include "../utils/VideoRecorder.h"
#endif

#include <pinocchio/visualizers/base-visualizer.hpp>
#include <SDL3/SDL_mouse.h>
#include <entt/entity/registry.hpp>

namespace candlewick::multibody {

namespace {
  using pinocchio::visualizers::BaseVisualizer;
  using pinocchio::visualizers::ConstVectorRef;
  using pinocchio::visualizers::Vector3;
  using pinocchio::visualizers::VectorXs;
} // namespace

/// Camera control parameters: senstivities, key bindings, etc...
/// \todo Move to some kind of "runtime" library?
struct CameraControlParams {
  float rotSensitivity = 5e-3f;
  float panSensitivity = 5e-3f;
  float zoomSensitivity = 0.05f;
  float localRotSensitivity = 0.01f;
  bool yInvert = false;

  // Mouse button modifiers
  struct MouseConfig {
    Uint8 rotButton = SDL_BUTTON_LEFT;
    Uint8 panButton = SDL_BUTTON_MIDDLE;
    Uint8 yRotButton = SDL_BUTTON_RIGHT;
  } mouseButtons;
};

void guiAddCameraParams(CylindricalCamera &controller,
                        CameraControlParams &params);

/// \brief A Pinocchio robot visualizer. The display() function will perform the
/// draw calls.
///
/// This visualizer is synchronous. The window is only updated when `display()`
/// is called.
class Visualizer final : public BaseVisualizer {
  bool m_cameraControl = true;
  bool m_showGui = true;
  bool m_shouldExit = false;
  entt::entity m_grid, m_triad;
  std::string m_currentScreenshotFilename{};
  RobotDebugSystem *robotDebug = nullptr;
  std::vector<entt::entity> m_debug_frame_pos;
  std::vector<entt::entity> m_debug_frame_vel;

  void initialize();

  void render();

  void displayPrecall() override {}

  void displayImpl() override;

  GuiSystem::GuiBehavior getDefaultCallback() {
    return [this](auto &) { this->defaultGuiCallback(); };
  }

public:
  static constexpr Radf DEFAULT_FOV = 55.0_degf;

  struct EnvStatus {
    bool show_our_about = false;
    bool show_imgui_about = false;
  };

  using BaseVisualizer::setCameraPose;
  entt::registry registry;
  RenderContext renderer;
  GuiSystem guiSystem;
  RobotScene robotScene;
  DebugScene debugScene;
  CylindricalCamera controller;
  CameraControlParams cameraParams;
  EnvStatus envStatus;
  AABB worldSceneBounds;

  struct Config {
    Uint32 width;
    Uint32 height;
    SDL_GPUTextureFormat depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
  };

  /// \brief Default GUI callback for the Visualizer; provide your own callback
  /// to the Visualizer constructor to change this behaviour.
  void defaultGuiCallback();

  void resetCamera();

  void loadViewerModel() override;

  Visualizer(const Config &config, const pin::Model &model,
             const pin::GeometryModel &visual_model,
             GuiSystem::GuiBehavior gui_callback);

  Visualizer(const Config &config, const pin::Model &model,
             const pin::GeometryModel &visual_model, pin::Data &data,
             pin::GeometryData &visual_data, GuiSystem::GuiBehavior callback);

  Visualizer(const Config &config, const pin::Model &model,
             const pin::GeometryModel &visual_model)
      : Visualizer(config, model, visual_model, getDefaultCallback()) {}

  Visualizer(const Config &config, const pin::Model &model,
             const pin::GeometryModel &visual_model, pin::Data &data,
             pin::GeometryData &visual_data)
      : Visualizer(config, model, visual_model, data, visual_data,
                   getDefaultCallback()) {}

  ~Visualizer() override;

  const Device &device() const { return renderer.device; }

  void setCameraTarget(const Eigen::Ref<const Vector3> &target) override;

  void setCameraPosition(const Eigen::Ref<const Vector3> &position) override;

  void setCameraPose(const Eigen::Ref<const Matrix4> &pose) override;

  void enableCameraControl(bool v) override { m_cameraControl = v; }

  void processEvents();

  [[nodiscard]] bool shouldExit() const noexcept { return m_shouldExit; }

  void takeScreenshot(std::string_view filename);

  void startRecording(std::string_view filename);

  void stopRecording();

#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  auto &videoSettings() { return m_videoSettings; }
#endif

  /// \brief Add visualization for a given frame.
  /// \param id Frame index
  /// \param show_velocity Whether to show frame velocity (as an arrow)
  /// \note For the velocity to show up, first-order
  /// pinocchio::forwardKinematics() must be called with the joint velocity
  /// passed in.
  /// Since display() with the joint configuration argument \c q
  /// calls zeroth-order forward kinematics internally, you should call
  /// first-order forward kinematics, then display() *without*
  /// arguments instead.
  void addFrameViz(pin::FrameIndex id, bool show_velocity = true);

  /// \brief Remove all frame visualizations.
  void removeFramesViz() {
    registry.destroy(m_debug_frame_pos.begin(), m_debug_frame_pos.end());
    registry.destroy(m_debug_frame_vel.begin(), m_debug_frame_vel.end());
    m_debug_frame_pos.clear();
    m_debug_frame_vel.clear();
  }

  /// \brief Clear objects
  void clean() override {
    removeFramesViz();
    robotScene.clearEnvironment();
    robotScene.clearRobotGeometries();
  }

private:
  media::TransferBufferPool m_transferBuffers;
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  std::string m_currentVideoFilename{};
  media::VideoRecorder m_videoRecorder;
  media::VideoRecorder::Settings m_videoSettings;
#endif
};

} // namespace candlewick::multibody
