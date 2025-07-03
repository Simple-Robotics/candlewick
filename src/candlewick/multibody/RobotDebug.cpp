#include "RobotDebug.h"

#include "../core/Components.h"
#include "../primitives/Arrow.h"

#include <pinocchio/algorithm/frames.hpp>

namespace candlewick::multibody {
entt::entity RobotDebugSystem::addFrameTriad(DebugScene &scene,
                                             pin::FrameIndex frame_id,
                                             const Float3 &scale) {
  entt::registry &reg = scene.registry();
  auto [ent, triad] = scene.addTriad(scale);
  reg.emplace<PinFrameComponent>(ent, frame_id);
  return ent;
}

entt::entity RobotDebugSystem::addFrameVelocityArrow(DebugScene &scene,
                                                     pin::FrameIndex frame_id,
                                                     float scale) {
  entt::registry &reg = scene.registry();
  MeshData arrow_data = loadArrowSolid(false);
  Mesh mesh = createMesh(scene.device(), arrow_data, true);
  Float4 color = 0xFF217Eff_rgbaf;

  auto entity = reg.create();
  auto &dmc = reg.emplace<DebugMeshComponent>(
      entity, DebugPipelines::TRIANGLE_FILL, std::move(mesh),
      std::vector{color}, true);
  dmc.scale << 0.333f, 0.333f, scale;
  reg.emplace<PinFrameVelocityComponent>(entity, frame_id);
  reg.emplace<TransformComponent>(entity, Mat4f::Identity());
  return entity;
}

void RobotDebugSystem::update(DebugScene &scene) {
  auto &reg = scene.registry();
  {
    auto view = reg.view<const PinFrameComponent, const DebugMeshComponent,
                         TransformComponent>();
    for (auto &&[ent, frame_id, dmc, tr] : view.each()) {
      Mat4f pose{m_robotData.oMf[frame_id].cast<float>()};
      auto D = dmc.scale.homogeneous().asDiagonal();
      tr.noalias() = pose * D;
    }
  }

  auto view_vel = reg.view<const PinFrameVelocityComponent,
                           const DebugMeshComponent, TransformComponent>();
  for (auto &&[ent, fvc, dmc, tr] : view_vel.each()) {
    Motionf vel =
        pin::getFrameVelocity(m_robotModel, m_robotData, fvc, pin::LOCAL)
            .cast<float>();

    const SE3f pose = m_robotData.oMf[fvc].cast<float>();
    Eigen::Quaternionf quatf;
    tr = pose.toHomogeneousMatrix();
    Float3 scale = dmc.scale;
    scale.z() *= vel.linear().norm();

    // the arrow mesh is posed z-up by default.
    // we need to rotate towards where the velocity is pointing,
    // then transform to the frame space.
    quatf.setFromTwoVectors(Float3::UnitZ(), vel.linear());
    auto R = tr.topLeftCorner<3, 3>();
    Mat3f R2 = quatf.toRotationMatrix() * scale.asDiagonal();
    R.applyOnTheRight(R2);
  }
}

} // namespace candlewick::multibody
