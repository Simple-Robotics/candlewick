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

  namespace gui {
    /// \brief Display Pinocchio model and geometry model info in ImGui.
    /// \image html robot-info-panel.png "Robot information panel."
    void addPinocchioModelInfo(entt::registry &reg, const pin::Model &model,
                               const pin::GeometryModel &geom_model,
                               int table_height_lines = 6);
  } // namespace gui

  struct PinGeomObjComponent {
    pin::GeomIndex geom_index;
    operator auto() const { return geom_index; }
  };

  struct PinFrameComponent {
    pin::FrameIndex frame_id;
    operator auto() const { return frame_id; }
  };

  /// Similar to PinFrameComponent, but tags the entity to render the frame
  /// velocity - not its placement as e.g. a triad.
  struct PinFrameVelocityComponent {
    pin::FrameIndex frame_id;
    operator auto() const { return frame_id; }
  };
} // namespace multibody
} // namespace candlewick
