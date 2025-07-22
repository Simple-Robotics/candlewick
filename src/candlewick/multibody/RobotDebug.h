#pragma once

#include "Multibody.h"
#include "../core/DebugScene.h"

namespace candlewick::multibody {

/// \brief A debug system for use with Pinocchio geometries.
///
/// Supports drawing a triad for a frame. Member \ref pinData must
/// refer to an existing pinocchio::Data object at all times.
struct RobotDebugSystem final : IDebugSubSystem {

  RobotDebugSystem(DebugScene &scene, const pin::Model &model,
                   const pin::Data &data)
      : IDebugSubSystem(scene), m_robotModel(&model), m_robotData(&data) {}

  entt::entity addFrameTriad(pin::FrameIndex frame_id,
                             const Float3 &scale = Float3::Constant(0.3333f));

  entt::entity addFrameVelocityArrow(pin::FrameIndex frame_id,
                                     float scale = 0.5f);

  /// \brief Update the transform components for the debug visual entities,
  /// according to their frame placements and velocities.
  ///
  /// \warning We expect pinocchio::updateFramePlacements() or
  /// pinocchio::framesForwardKinematics() to be called first!
  void update() override;

  void destroyEntities();

  ~RobotDebugSystem() {
    this->destroyEntities();
    this->m_robotModel = nullptr;
    this->m_robotData = nullptr;
  }

  void reload(const pin::Model &model, const pin::Data &data) {
    this->destroyEntities();
    this->m_robotModel = &model;
    this->m_robotData = &data;
  }

private:
  pin::Model const *m_robotModel;
  pin::Data const *m_robotData;
};

} // namespace candlewick::multibody
