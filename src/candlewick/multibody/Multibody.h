#pragma once

#include <pinocchio/multibody/fwd.hpp>
#include <entt/entity/registry.hpp>

namespace candlewick {
/// \brief Support for the Pinocchio rigid-body algorithms library and the Coal
/// collision detection library.
/// \details Loaders for assets defined through Pinocchio, Coal collision
/// geometries, classes for creating a \ref Scene for robots from Pinocchio
/// models,  etc.
namespace multibody {
  namespace pin = pinocchio;
  struct RobotDebugSystem;
  class RobotScene;
  class Visualizer;

  using SE3f = pin::SE3Tpl<float>;
  using Motionf = pin::MotionTpl<float>;
  using Inertiaf = pin::InertiaTpl<float>;
  using Forcef = pin::ForceTpl<float>;

  /// \ingroup gui_util
  /// \brief Display Pinocchio model and geometry model info in ImGui.
  /// \image html robot-info-panel.png "Robot information panel."
  void guiAddPinocchioModelInfo(entt::registry &reg, const pin::Model &model,
                                const pin::GeometryModel &geom_model,
                                int table_height_lines = 6);
} // namespace multibody
} // namespace candlewick
