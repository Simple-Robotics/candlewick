#include "Visualizer.h"
#include "Components.h"
#include "../core/CameraControls.h"
#include "../core/Components.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <magic_enum/magic_enum_flags.hpp>

namespace candlewick::multibody {

void guiAddPinocchioModelInfo(entt::registry &reg, const pin::Model &model,
                              const pin::GeometryModel &geom_model,
                              int table_height_lines) {
  ImGuiTableFlags flags = 0;
  flags |= ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("pin_info_table", 4, flags)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("No. of joints / frames");
    ImGui::TableSetupColumn("nq / nv");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", model.name.c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d", model.njoints, model.nframes);
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d", model.nq, model.nv);

    ImGui::EndTable();
  }

  flags |= ImGuiTableFlags_RowBg;
  flags |= ImGuiTableFlags_ScrollY;
  const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
  ImVec2 outer_size{0.0f, TEXT_BASE_HEIGHT * float(table_height_lines)};

  ImGui::Text("Frames");
  ImGui::Spacing();

  if (ImGui::BeginTable("pin_frames_table", 3, flags, outer_size)) {
    ImGui::TableSetupColumn("Index");
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Type");
    ImGui::TableHeadersRow();

    for (pin::FrameIndex i = 0; i < pin::FrameIndex(model.nframes); i++) {
      const pin::Frame &frame = model.frames[i];

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%zu", i);
      ImGui::TableNextColumn();
      ImGui::Text("%s", frame.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%s", magic_enum::enum_name(frame.type).data());
    }
    ImGui::EndTable();
  }

  ImGui::Text("Geometry model");
  ImGui::Spacing();
  ImGui::Text("No. of geometries: %zu", geom_model.ngeoms);

  if (ImGui::BeginTable("pin_geom_table", 5, flags | ImGuiTableFlags_Sortable,
                        outer_size)) {
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            0);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
    ImGui::TableSetupColumn("Object / node type", ImGuiTableColumnFlags_NoSort);
    ImGui::TableSetupColumn("Parent joint", ImGuiTableColumnFlags_NoSort);
    ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_NoSort);
    ImGui::TableHeadersRow();

    if (ImGuiTableSortSpecs *sortSpecs = ImGui::TableGetSortSpecs()) {
      reg.sort<PinGeomObjComponent>(
          [&geom_model, sortSpecs](const PinGeomObjComponent &lhsId,
                                   const PinGeomObjComponent &rhsId) {
            auto &lhs = geom_model.geometryObjects[lhsId];
            auto &rhs = geom_model.geometryObjects[rhsId];
            for (int n = 0; n < sortSpecs->SpecsCount; n++) {
              auto cid = sortSpecs->Specs[n].ColumnIndex;
              auto dir = sortSpecs->Specs[n].SortDirection;
              bool sort;
              switch (cid) {
              case 0: {
                sort = lhsId < rhsId;
                break;
              }
              case 1: {
                sort = lhs.name.compare(rhs.name) <= 0;
                break;
              }
              default:
                return false;
              }
              return (dir == ImGuiSortDirection_Ascending) ? sort : !sort;
            }
            unreachable();
          });
    }

    auto view = reg.view<const PinGeomObjComponent>();
    // grab storage for the Disable component
    // we won't filter on it, but use it to query if entities are disabled
    auto &disabled = reg.storage<Disable>();

    for (auto [ent, id] : view.each()) {
      auto &gobj = geom_model.geometryObjects[id];
      const coal::CollisionGeometry &coll = *gobj.geometry;
      coal::OBJECT_TYPE objType = coll.getObjectType();
      coal::NODE_TYPE nodeType = coll.getNodeType();
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%zu", id.geom_index);
      ImGui::TableNextColumn();
      ImGui::Text("%s", gobj.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%s / %s", magic_enum::enum_name(objType).data(),
                  magic_enum::enum_name(nodeType).data());
      ImGui::TableNextColumn();
      pin::JointIndex parent_joint = gobj.parentJoint;
      const char *parent_joint_name = model.names[parent_joint].c_str();
      ImGui::Text("%zu (%s)", parent_joint, parent_joint_name);

      ImGui::TableNextColumn();
      char chk_label[32];
      bool enabled = !disabled.contains(ent);
      SDL_snprintf(chk_label, 32, "enabled###%zu", id.geom_index);
      guiAddDisableCheckbox(chk_label, reg, ent, enabled);
    }
    ImGui::EndTable();
  }
}

static void camera_params_gui(CylindricalCamera &controller,
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

void Visualizer::default_gui_exec() {

  // Verify ABI compatibility between caller code and compiled version of Dear
  // ImGui. This helps detects some build issues. Check demo code in
  // imgui_demo.cpp.
  IMGUI_CHECKVERSION();

  if (envStatus.show_imgui_about)
    ImGui::ShowAboutWindow(&envStatus.show_imgui_about);
  if (envStatus.show_our_about)
    ::candlewick::showCandlewickAboutWindow(&envStatus.show_our_about);

  auto &light = robotScene.directionalLight;
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

  guiAddLightControls(light);
  camera_params_gui(controller, cameraParams);

  auto add_env_checkbox = [this](const char *title, entt::entity ent) {
    char label[32];
    SDL_snprintf(label, sizeof(label), "hud.%s", title);
    auto &dmc = registry.get<DebugMeshComponent>(ent);
    ImGui::Checkbox(label, &dmc.enable);
  };
  if (ImGui::CollapsingHeader("Settings (HUD and env)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    add_env_checkbox("grid", m_grid);
    ImGui::SameLine();
    {
      auto &dmc = registry.get<DebugMeshComponent>(m_grid);
      ImGui::ColorEdit4("hud.grid.color", dmc.colors[0].data(),
                        ImGuiColorEditFlags_AlphaPreview);
    }
    add_env_checkbox("triad", m_triad);
    guiAddDisableCheckbox("Render plane", registry, m_plane,
                          envStatus.show_plane);
  }

  if (ImGui::CollapsingHeader("Robot model info",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    guiAddPinocchioModelInfo(registry, m_model, visualModel());
  }

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
