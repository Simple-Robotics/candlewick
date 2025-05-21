#include "candlewick/multibody/Visualizer.h"
#include "candlewick/core/CameraControls.h"

#include <string>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <magic_enum/magic_enum_flags.hpp>

namespace candlewick::multibody {

void guiAddCameraParams(CylindricalCamera &controller,
                        CameraControlParams &params) {
  if (ImGui::TreeNode("Camera controls")) {
    ImGui::SliderFloat("Rot. sensitivity", &params.rotSensitivity, 0.001f,
                       0.01f);
    ImGui::SliderFloat("Zoom sensitivity", &params.zoomSensitivity, 0.001f,
                       0.1f);
    ImGui::SliderFloat("Pan sensitivity", &params.panSensitivity, 0.001f,
                       0.01f);
    ImGui::SliderFloat("Local rot. sensitivity", &params.localRotSensitivity,
                       0.001f, 0.04f);
    ImGui::Checkbox("Invert Y", &params.yInvert);
    if (ImGui::Button("Reset target")) {
      controller.lookAt1(Float3::Zero());
    }
    ImGui::TreePop();
  }
}

static void screenshot_taker_gui(SDL_Window *window, std::string &filename) {
  static std::string out;
  out.reserve(64ul);

  ImGui::BeginChild("screenshot_taker", {0, 0},
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
  guiAddFileDialog(window, DialogFileType::IMAGES, out);
  if (ImGui::Button("Take screenshot"))
    filename = out.empty() ? generateMediaFilenameFromTimestamp() : out;

  ImGui::EndChild();
}

void Visualizer::defaultGuiCallback() {

  // Verify ABI compatibility between caller code and compiled version of Dear
  // ImGui. This helps detects some build issues. Check demo code in
  // imgui_demo.cpp.
  IMGUI_CHECKVERSION();

  if (envStatus.show_imgui_about)
    ImGui::ShowAboutWindow(&envStatus.show_imgui_about);
  if (envStatus.show_our_about)
    ::candlewick::showCandlewickAboutWindow(&envStatus.show_our_about);

  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
  window_flags |= ImGuiWindowFlags_MenuBar;
  ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Renderer info & controls", nullptr, window_flags);

  if (ImGui::BeginMenuBar()) {
    ImGui::MenuItem("About Dear ImGui", NULL, &envStatus.show_imgui_about);
    ImGui::MenuItem("About Candlewick", NULL, &envStatus.show_our_about);
    ImGui::EndMenuBar();
  }

  ImGui::Text("Video driver: %s", SDL_GetCurrentVideoDriver());
  ImGui::Text("Display pixel density: %.2f / scale: %.2f",
              renderer.window.pixelDensity(), renderer.window.displayScale());
  ImGui::Text("Device driver: %s", renderer.device.driverName());

  ImGui::SeparatorText("Lights and camera controls");

  guiAddLightControls(robotScene.directionalLight);
  guiAddCameraParams(controller, cameraParams);

  auto addDebugCheckbox = [this](const char *title,
                                 entt::entity ent) -> auto & {
    char label[32];
    SDL_snprintf(label, sizeof(label), "hud.%s", title);
    auto &dmc = registry.get<DebugMeshComponent>(ent);
    ImGui::Checkbox(label, &dmc.enable);
    return dmc;
  };
  if (ImGui::CollapsingHeader("Settings (HUD and env)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    {
      auto &dmc = addDebugCheckbox("grid", m_grid);
      ImGui::SameLine();
      ImGui::ColorEdit4("hud.grid.color", dmc.colors[0].data(),
                        ImGuiColorEditFlags_AlphaPreview);
    }
    addDebugCheckbox("triad", m_triad);
    ImGui::SameLine();
    guiAddDisableCheckbox("Render plane", registry, m_plane,
                          envStatus.show_plane);
  }

  if (ImGui::CollapsingHeader("Robot model info",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    guiAddPinocchioModelInfo(registry, m_model, visualModel());
  }

  if (ImGui::CollapsingHeader("Screenshot and video recording")) {
    screenshot_taker_gui(renderer.window, m_currentScreenshotFilename);

    ImGui::BeginChild("video_record", {0, 0},
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    guiAddFileDialog(renderer.window, DialogFileType::VIDEOS,
                     m_currentVideoFilename);

    if (!m_videoRecorder.isRecording()) {
      if (ImGui::Button("Start recording")) {
        if (m_currentVideoFilename.empty()) {
          ImGui::OpenPopup("record_no_filename");
        } else {
          auto [width, height] = renderer.window.sizeInPixels();
          m_videoRecorder.settings.bit_rate = 4'000'000u;
          m_videoRecorder.open(Uint16(width), Uint16(height),
                               m_currentVideoFilename);
        }
      }
      if (ImGui::BeginPopup("record_no_filename")) {
        ImGui::TextColored({0.95f, 0.27f, 0., 1.f},
                           "You must specify a filename.");
        ImGui::EndPopup();
      }
    } else {
      if (ImGui::Button("End recording")) {
        m_currentVideoFilename.clear();
        SDL_Log("Wrote %d frames.", m_videoRecorder.frameCounter());
        m_videoRecorder.close();
      }
    }
    ImGui::EndChild();
  };

  ImGui::End();
}

static void mouse_wheel_handler(CylindricalCamera &controller,
                                const CameraControlParams &params,
                                SDL_MouseWheelEvent event) {
  controller.moveInOut(1.f - params.zoomSensitivity, event.y);
}

static void mouse_motion_handler(CylindricalCamera &controller,
                                 const CameraControlParams &params,
                                 const SDL_MouseMotionEvent &event) {
  Float2 mvt{event.xrel, event.yrel};
  SDL_MouseButtonFlags mb = event.state;
  // check if left mouse pressed
  if (mb & SDL_BUTTON_LMASK) {
    controller.viewportDrag(mvt, params.rotSensitivity, params.panSensitivity,
                            params.yInvert);
  }
  if (mb & params.mouseButtons.panButton) {
    controller.pan(mvt, params.panSensitivity);
  }
  if (mb & SDL_BUTTON_RMASK) {
    Radf rot_angle = params.localRotSensitivity * mvt.y();
    camera_util::localRotateXAroundOrigin(controller.camera, rot_angle);
  }
}

void Visualizer::processEvents() {
  ImGuiIO &io = ImGui::GetIO();

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);

    if (event.type == SDL_EVENT_QUIT) {
      SDL_Log("Exiting application...");
      m_shouldExit = true;
    }

    if (io.WantCaptureMouse | io.WantCaptureKeyboard)
      continue;

    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      // camera mouse control
      if (m_cameraControl)
        mouse_motion_handler(this->controller, cameraParams, event.motion);
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      if (m_cameraControl)
        mouse_wheel_handler(this->controller, cameraParams, event.wheel);
      break;
    }
  }
}

} // namespace candlewick::multibody
