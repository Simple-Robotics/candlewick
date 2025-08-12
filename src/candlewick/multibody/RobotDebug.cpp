#include "RobotDebug.h"

#include "../core/Components.h"

#include <pinocchio/algorithm/frames.hpp>

namespace candlewick::multibody {
entt::entity RobotDebugSystem::addFrameTriad(pin::FrameIndex frame_id,
                                             const Float3 &scale) {
  entt::registry &reg = m_scene.registry();
  auto [ent, dmc] = m_scene.addTriad(scale);
  reg.emplace<PinFrameComponent>(ent, frame_id);
  return ent;
}

entt::entity RobotDebugSystem::addFrameVelocityArrow(pin::FrameIndex frame_id,
                                                     float scale) {
  entt::registry &reg = m_scene.registry();
  Float4 color = 0xFF217Eff_rgbaf;

  auto entity = reg.create();
  auto &dmc = reg.emplace<DebugMeshComponent>(
      entity, DebugPipelines::TRIANGLE_FILL, DebugMeshType::ARROW,
      std::vector{color}, true);
  dmc.scale << 0.333f, 0.333f, scale;
  reg.emplace<PinFrameVelocityComponent>(entity, frame_id);
  reg.emplace<TransformComponent>(entity, Mat4f::Identity());
  return entity;
}

void RobotDebugSystem::update() {
  using Eigen::Quaternionf;
  auto &reg = m_scene.registry();
  {
    auto view = reg.view<const PinFrameComponent, const DebugMeshComponent,
                         TransformComponent>();
    for (auto &&[ent, frame_id, dmc, tr] : view.each()) {
      const SE3f pose = m_robotData->oMf[frame_id].cast<float>();
      auto D = dmc.scale.homogeneous().asDiagonal();
      tr.noalias() = pose.toHomogeneousMatrix() * D;
    }
  }

  {
    auto view = reg.view<const PinFrameVelocityComponent,
                         const DebugMeshComponent, TransformComponent>();
    for (auto &&[ent, fvc, dmc, tr] : view.each()) {
      Motionf vel =
          pin::getFrameVelocity(*m_robotModel, *m_robotData, fvc, pin::LOCAL)
              .cast<float>();

      const SE3f pose = m_robotData->oMf[fvc].cast<float>();
      tr = pose.toHomogeneousMatrix();
      Float3 scale = dmc.scale;
      scale.z() *= vel.linear().norm();

      // the arrow mesh is posed z-up by default.
      // we need to rotate towards where the velocity is pointing,
      // then transform to the frame space.
      auto quat = Quaternionf::FromTwoVectors(Float3::UnitZ(), vel.linear());
      Eigen::Ref<Mat3f> R = tr.topLeftCorner<3, 3>();
      Mat3f R2 = quat.toRotationMatrix() * scale.asDiagonal();
      R.applyOnTheRight(R2);
    }
  }

  {
    auto view = reg.view<const ExternalForceComponent, const DebugMeshComponent,
                         TransformComponent>();
    for (auto &&[ent, arrow, dmc, tr] : view.each()) {
      pin::FrameIndex frame_id = arrow.frame_id;
      const SE3f pose = m_robotData->oMf[frame_id].cast<float>();
      tr = pose.toHomogeneousMatrix();

      const auto &f = arrow.force;

      // base scale
      Float3 scale = dmc.scale;
      scale.z() *= std::tanh(f.linear().norm());

      auto quat = Quaternionf::FromTwoVectors(Float3::UnitZ(), f.linear());
      Eigen::Ref<Mat3f> R = tr.topLeftCorner<3, 3>();
      Mat3f R2 = quat.toRotationMatrix() * scale.asDiagonal();
      R.applyOnTheRight(R2);
    }
  }

  // cleanup expired force arrows
  {
    auto view = reg.group<ExternalForceComponent>();
    for (auto [ent, arrow] : view.each()) {
      if (--arrow.lifetime <= 0) {
#ifndef NDEBUG
        SDL_Log("Force arrow for frame %zu has expired... destroy.",
                arrow.frame_id);
#endif
        reg.destroy(ent);
      }
    }
  }
}

template <class... Ts>
void destroy_debug_entities_from_types(entt::registry &reg) {
  auto f = []<typename U>(entt::registry &reg, U *) {
    auto view = reg.view<const DebugMeshComponent, const U>();
    reg.destroy(view.begin(), view.end());
  };
  (f(reg, (Ts *)nullptr), ...);
}

void RobotDebugSystem::destroyEntities() {
  destroy_debug_entities_from_types<const PinFrameComponent,
                                    const PinFrameVelocityComponent,
                                    const ExternalForceComponent>(
      m_scene.registry());
}

} // namespace candlewick::multibody
