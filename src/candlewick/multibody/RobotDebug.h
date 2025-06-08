#pragma once

#include "Multibody.h"
#include "../core/DebugScene.h"

namespace candlewick::multibody {

/// \brief A debug system for use with Pinocchio geometries.
///
/// Supports drawing a triad for a frame. Member \ref pinData must
/// refer to an existing pinocchio::Data object at all times.
struct RobotDebugSystem final : IDebugSubSystem {
  inline static const Float3 DEFAULT_TRIAD_SCALE = Float3::Constant(0.3333f);

  RobotDebugSystem(const pin::Model &model, const pin::Data &data)
      : IDebugSubSystem(), m_robotModel(model), m_robotData(data) {}

  entt::entity addFrameTriad(DebugScene &scene, pin::FrameIndex frame_id,
                             const Float3 &scale = DEFAULT_TRIAD_SCALE);

  entt::entity addFrameVelocityArrow(DebugScene &scene,
                                     pin::FrameIndex frame_id);

  /// \brief Update the transform components for the debug visual entities,
  /// according to their frame placements and velocities.
  ///
  /// \warning We expect pinocchio::updateFramePlacements() or
  /// pinocchio::framesForwardKinematics() to be called first!
  void update(DebugScene &scene) {
    updateFrames(scene.registry());
    updateFrameVelocities(scene.registry());
  }

private:
  void updateFrames(entt::registry &reg);
  void updateFrameVelocities(entt::registry &reg);

  const pin::Model &m_robotModel;
  const pin::Data &m_robotData;
};

} // namespace candlewick::multibody
