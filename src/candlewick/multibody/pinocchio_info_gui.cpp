#include "candlewick/multibody/Multibody.h"
#include "candlewick/core/Components.h"
#include "candlewick/core/GuiSystem.h"
#include "candlewick/core/errors.h"

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>
#include <SDL3/SDL_stdinc.h>

#include <pinocchio/multibody/model.hpp>
#include <pinocchio/multibody/geometry.hpp>

namespace candlewick::multibody::gui {

void addPinocchioModelInfo(entt::registry &reg, const pin::Model &model,
                           const pin::GeometryModel &geom_model,
                           int table_height_lines) {
  ImGuiTableFlags flags = 0;
  flags |= ImGuiTableFlags_SizingStretchProp;
  flags |= ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable("pin_info_table", 5, flags)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("No. of joints");
    ImGui::TableSetupColumn("No. of frames");
    ImGui::TableSetupColumn("nq / nv");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", model.name.c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%d", model.njoints);
    ImGui::TableNextColumn();
    ImGui::Text("%d", model.nframes);
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d", model.nq, model.nv);

    ImGui::EndTable();
  }

  flags |= ImGuiTableFlags_ScrollY;
  const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
  ImVec2 outer_size{0.0f, TEXT_BASE_HEIGHT * float(table_height_lines)};

  ImGui::SeparatorText("Frames");
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

  ImGui::SeparatorText("Geometry model");
  ImGui::Spacing();
  ImGui::Text("No. of geometries: %zu", geom_model.ngeoms);

  if (ImGui::BeginTable("pin_geom_table", 6, flags | ImGuiTableFlags_Sortable,
                        outer_size)) {
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            0);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
    ImGui::TableSetupColumn("Object / node type", ImGuiTableColumnFlags_NoSort);
    ImGui::TableSetupColumn("Parent joint", ImGuiTableColumnFlags_NoSort);
    ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_NoSort);
    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_NoSort);
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

      auto &mmc = reg.get<MeshMaterialComponent>(ent);

      const coal::CollisionGeometry &coll = *gobj.geometry;
      coal::OBJECT_TYPE objType = coll.getObjectType();
      coal::NODE_TYPE nodeType = coll.getNodeType();
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%zu", pin::FrameIndex(id));
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
      char label[32];
      bool enabled = !disabled.contains(ent);
      SDL_snprintf(label, 32, "gobj_%zu", pin::GeomIndex(id));
      ImGui::PushID(label);

      ::candlewick::gui::addDisableCheckbox("###enabled", reg, ent, enabled);
      ImGui::TableNextColumn();
      const char *names[] = {"FILL", "LINE"};
      ImGui::Combo("###mode", (int *)&mmc.mode, names, IM_ARRAYSIZE(names));

      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

} // namespace candlewick::multibody::gui
