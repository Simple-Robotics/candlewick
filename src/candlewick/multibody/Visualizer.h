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
#include "../utils/VideoRecorder.h"

#include <pinocchio/visualizers/base-visualizer.hpp>
#include <SDL3/SDL_init.h>
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
    Uint8 panButton = SDL_BUTTON_MIDDLE;
  } mouseButtons;
};

void guiAddCameraParams(CylindricalCamera &controller,
                        CameraControlParams &params);

/// \brief A Pinocchio robot visualizer. The display() function will perform the
/// draw calls.
///
/// This visualizer is asynchronous. It will create its render context
/// (Renderer) in a separate thread along with the GPU device and window, and
/// run until shouldExit() returns true.
class Visualizer final : public BaseVisualizer {
  bool m_cameraControl = true;
  bool m_showGui = true;
  bool m_shouldExit = false;
  entt::entity m_plane, m_grid, m_triad;
  std::string m_currentScreenshotFilename{};
  std::string m_currentVideoFilename{};

  void render();

  void displayPrecall() override {}

  void displayImpl() override;

  const Device &device() const { return renderer.device; }

public:
  static constexpr Radf DEFAULT_FOV = 55.0_degf;

  struct EnvStatus {
    bool show_our_about = false;
    bool show_imgui_about = false;
    bool show_plane = false;
  };

  using BaseVisualizer::setCameraPose;
  entt::registry registry;
  Renderer renderer;
  GuiSystem guiSystem;
  RobotScene robotScene;
  DebugScene debugScene;
  CylindricalCamera controller;
  CameraControlParams cameraParams;
  EnvStatus envStatus;

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
             const pin::GeometryModel &visual_model)
      : Visualizer(config, model, visual_model,
                   [this](auto &) { this->defaultGuiCallback(); }) {}

  ~Visualizer() override;

  void setCameraTarget(const Eigen::Ref<const Vector3> &target) override;

  void setCameraPosition(const Eigen::Ref<const Vector3> &position) override;

  void setCameraPose(const Eigen::Ref<const Matrix4> &pose) override;

  void enableCameraControl(bool v) override { m_cameraControl = v; }

  void processEvents();

  [[nodiscard]] bool shouldExit() const noexcept { return m_shouldExit; }

  void takeScreenshot(std::string_view filename);

  void startRecording(std::string_view filename);

  void stopRecording();

  /// \brief Clear objects
  void clean() override {
    robotScene.clearEnvironment();
    robotScene.clearRobotGeometries();
  }

private:
  media::TransferBufferPool m_transferBuffers;
  media::VideoRecorder m_videoRecorder;
};

} // namespace candlewick::multibody
