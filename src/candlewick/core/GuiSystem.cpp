#include "GuiSystem.h"
#include "RenderContext.h"
#include "Components.h"
#include "LightUniforms.h"
#include "candlewick/config.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include "../../fonts/inter-medium.cpp"

namespace candlewick {

GuiSystem::GuiSystem(const RenderContext &renderer, GuiBehavior behav)
    : m_renderer(&renderer), _callbacks{behav} {
  if (!init(renderer)) {
    terminate_with_message("Failed to initialize ImGui for SDLGPU3.");
  }
}

bool GuiSystem::init(const RenderContext &renderer) {
  m_renderer = &renderer;
  assert(!m_initialized); // can't initialize twice
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  ImFont *inter = io.Fonts->AddFontFromMemoryCompressedTTF(
      InterMedium_compressed_data, InterMedium_compressed_size);
  SDL_assert(inter);

  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.IniFilename = nullptr;

  ImGuiStyle &style = ImGui::GetStyle();
  ImGui::StyleColorsDark(&style);
  style.WindowBorderSize = 0.5f;
  style.WindowRounding = 6;
  if (!ImGui_ImplSDL3_InitForSDLGPU(renderer.window)) {
    return false;
  }
  ImGui_ImplSDLGPU3_InitInfo imguiInfo{
      .Device = renderer.device,
      .ColorTargetFormat = renderer.getSwapchainTextureFormat(),
      .MSAASamples = SDL_GPU_SAMPLECOUNT_1,
  };
  m_initialized = ImGui_ImplSDLGPU3_Init(&imguiInfo);
  return m_initialized;
}

void GuiSystem::render(CommandBuffer &cmdBuf) {
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  for (auto &cb : _callbacks) {
    cb(*m_renderer);
  }

  ImGui::Render();
  ImDrawData *draw_data = ImGui::GetDrawData();
  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdBuf);

  SDL_GPUColorTargetInfo info{
      .texture = m_renderer->swapchain,
      .clear_color{},
      .load_op = SDL_GPU_LOADOP_LOAD,
      .store_op = SDL_GPU_STOREOP_STORE,
  };
  auto render_pass = SDL_BeginGPURenderPass(cmdBuf, &info, 1, NULL);
  ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdBuf, render_pass);

  SDL_EndGPURenderPass(render_pass);
}

void GuiSystem::release() {
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
  m_renderer = nullptr;
}

void showCandlewickAboutWindow(bool *p_open, float wrap_width) {
  if (!ImGui::Begin("About Candlewick", p_open,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Candlewick v%s", CANDLEWICK_VERSION);
  ImGui::Spacing();

  ImGui::TextLinkOpenURL("Homepage",
                         "https://github.com/Simple-Robotics/candlewick/");
  ImGui::SameLine();
  ImGui::TextLinkOpenURL(
      "Releases", "https://github.com/Simple-Robotics/candlewick/releases");

  ImGui::Separator();
  ImGui::Text("Copyright (c) 2024-2025 Inria");
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
  ImGui::Text("Candlewick is licensed under the BSD 2-Clause License, see "
              "LICENSE file for more information.");
  ImGui::PopTextWrapPos();

  ImGui::End();
}

void guiAddLightControls(DirectionalLight &light) {
  ImGui::SliderFloat("intensity", &light.intensity, 0.1f, 10.f);
  ImGui::DragFloat3("direction", light.direction.data(), 0.0f, -1.f, 1.f);
  light.direction.stableNormalize();
  ImGui::ColorEdit3("color", light.color.data());
}

void guiAddLightControls(std::span<DirectionalLight> lights) {
  Uint16 i = 0;
  char label[32];
  for (auto &light : lights) {
    SDL_snprintf(label, 32, "light###%d", i);
    ImGui::PushID(label);
    ImGui::Bullet();
    ImGui::Indent();
    guiAddLightControls(light);
    ImGui::Unindent();
    ImGui::PopID();
    i++;
  }
}

void guiAddLightControls(std::span<DirectionalLight> lights, Uint32 numLights,
                         Uint32 start) {
  guiAddLightControls(lights.subspan(start, numLights));
}

void guiAddDisableCheckbox(const char *label, entt::registry &reg,
                           entt::entity id, bool &flag) {
  if (ImGui::Checkbox(label, &flag)) {
    toggleDisable(reg, id, flag);
  }
}

} // namespace candlewick
