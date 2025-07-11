#include "Visualizer.h"

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

void guiAddDebugMesh(DebugMeshComponent &dmc,
                     bool enable_pipeline_switch = true) {
  ImGui::Checkbox("##enabled", &dmc.enable);
  Uint32 col_id = 0;
  ImGuiColorEditFlags color_flags = ImGuiColorEditFlags_NoAlpha |
                                    ImGuiColorEditFlags_NoSidePreview |
                                    ImGuiColorEditFlags_NoInputs;
  char label[32];
  for (auto &col : dmc.colors) {
    SDL_snprintf(label, sizeof(label), "##color##%u", col_id);
    ImGui::SameLine();
    ImGui::ColorEdit4(label, col.data(), color_flags);
    col_id++;
  }
  if (enable_pipeline_switch) {
    const char *names[] = {"FILL", "LINE"};
    static_assert(IM_ARRAYSIZE(names) ==
                  magic_enum::enum_count<DebugPipelines>());
    ImGui::SameLine();
    ImGui::Combo("Mode##pipeline", (int *)&dmc.pipeline_type, names,
                 IM_ARRAYSIZE(names));
  }
}

void Visualizer::guiCallbackImpl() {

  // Verify ABI compatibility between caller code and compiled version of Dear
  // ImGui. This helps detects some build issues. Check demo code in
  // imgui_demo.cpp.
  IMGUI_CHECKVERSION();

  if (show_imgui_about)
    ImGui::ShowAboutWindow(&show_imgui_about);
  if (show_our_about)
    ::candlewick::showCandlewickAboutWindow(&show_our_about);

  ImGuiWindowFlags window_flags = 0;
  window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
  window_flags |= ImGuiWindowFlags_MenuBar;
  ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Renderer info & controls", nullptr, window_flags);

  if (ImGui::BeginMenuBar()) {
    ImGui::MenuItem("About Dear ImGui", NULL, &show_imgui_about);
    ImGui::MenuItem("About Candlewick", NULL, &show_our_about);
    ImGui::EndMenuBar();
  }

  ImGui::Text("Video driver: %s", SDL_GetCurrentVideoDriver());
  ImGui::Text("Display pixel density: %.2f / scale: %.2f",
              renderer.window.pixelDensity(), renderer.window.displayScale());
  ImGui::Text("Device driver: %s", renderer.device.driverName());

  if (ImGui::CollapsingHeader("Lights and camera controls")) {
    guiAddLightControls(robotScene.directionalLight, robotScene.numLights());
    guiAddCameraParams(controller, cameraParams);
  }

  if (ImGui::CollapsingHeader("Settings (HUD and env)")) {
    {
      const char *name = "hud.grid";
      ImGui::PushID(name);
      ImGui::Text("%s", name);
      auto &dmc = registry.get<DebugMeshComponent>(m_grid);
      ImGui::SameLine();
      guiAddDebugMesh(dmc, false);
      ImGui::PopID();
    }
    {
      const char *name = "hud.triad";
      ImGui::PushID(name);
      ImGui::Text("%s", name);
      auto &dmc = registry.get<DebugMeshComponent>(m_triad);
      ImGui::SameLine();
      guiAddDebugMesh(dmc);
      ImGui::PopID();
    }
    ImGui::Checkbox("Ambient occlusion (SSAO)",
                    &robotScene.config().enable_ssao);
  }

  if (ImGui::CollapsingHeader("Robot model info",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    guiAddPinocchioModelInfo(registry, m_model, visualModel());
  }

  if (ImGui::CollapsingHeader(
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
          "Screenshots/Video recording"
#else
          "Screenshots"
#endif
          )) {
    ImGui::BeginChild("screenshot_taker", {0, 0},
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    guiAddFileDialog(renderer.window, DialogFileType::IMAGES,
                     m_currentScreenshotFilename);
    if (ImGui::Button("Take screenshot")) {
      m_shouldScreenshot = true;
      if (m_currentScreenshotFilename.empty()) {
        generateMediaFilenameFromTimestamp("cdw_screenshot",
                                           m_currentScreenshotFilename);
      }
    }

    ImGui::EndChild();

#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
    ImGui::BeginChild("video_record", {0, 0},
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    guiAddFileDialog(renderer.window, DialogFileType::VIDEOS,
                     m_currentVideoFilename);

    ImGui::BeginDisabled(m_videoRecorder.isRecording());
    ImGui::SliderInt("bitrate", &m_videoSettings.bitRate, 2'000'000, 6'000'000);
    ImGui::SliderInt("framerate", &m_videoSettings.fps, 10, 60);
    ImGui::EndDisabled();

    if (!m_videoRecorder.isRecording()) {
      if (ImGui::Button("Start recording")) {
        if (m_currentVideoFilename.empty()) {
          ImGui::OpenPopup("record_no_filename");
        } else {
          this->startRecording(m_currentVideoFilename);
        }
      }
      if (ImGui::BeginPopup("record_no_filename")) {
        ImGui::TextColored({0.95f, 0.27f, 0., 1.f},
                           "You must specify a filename.");
        ImGui::EndPopup();
      }
    } else {
      if (ImGui::Button("End recording")) {
        this->stopRecording();
      }
    }
    ImGui::EndChild();
#endif
  };

  if (ImGui::CollapsingHeader("Robot debug")) {
    auto view = registry.view<DebugMeshComponent, const PinFrameComponent>();
    for (auto [ent, dmc, fc] : view.each()) {
      auto frame_name = model().frames[fc].name.c_str();
      char label[64];
      SDL_snprintf(label, 64, "frame_%d", int(fc));
      ImGui::PushID(label);
      ImGui::Text("%s", frame_name);
      ImGui::SameLine();
      guiAddDebugMesh(dmc);
      ImGui::PopID();
    }
  }

  ImGui::End();
}

static void mouseWheelHandler(CylindricalCamera &controller,
                              const CameraControlParams &params,
                              SDL_MouseWheelEvent event) {
  controller.moveInOut(1.f - params.zoomSensitivity, event.y);
}

static void mouseMotionHandler(CylindricalCamera &controller,
                               const CameraControlParams &params,
                               const SDL_MouseMotionEvent &event) {
  Float2 mvt{event.xrel, event.yrel};
  SDL_MouseButtonFlags mb = event.state;
  // check if left mouse pressed
  if (mb & SDL_BUTTON_MASK(params.mouseButtons.rotButton)) {
    controller.viewportDrag(mvt, params.rotSensitivity, params.panSensitivity,
                            params.yInvert);
  }
  if (mb & SDL_BUTTON_MASK(params.mouseButtons.panButton)) {
    controller.pan(mvt, params.panSensitivity);
  }
  if (mb & SDL_BUTTON_MASK(params.mouseButtons.yRotButton)) {
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
        mouseMotionHandler(this->controller, cameraParams, event.motion);
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      if (m_cameraControl)
        mouseWheelHandler(this->controller, cameraParams, event.wheel);
      break;
    case SDL_EVENT_KEY_DOWN:
      auto keyEvent = event.key;
      if (keyEvent.key == SDLK_H) {
        // toggle GUI on and off
        m_showGui = !m_showGui;
      }
      break;
    }
  }
}

} // namespace candlewick::multibody
