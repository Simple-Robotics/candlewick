#pragma once

#include "../Core.h"
#include "../Device.h"
#include "../Collision.h"
#include "../math_types.h"

#include <entt/entity/registry.hpp>
#include <SDL3/SDL_gpu.h>
#include <variant>

namespace candlewick {
namespace frustum_debug {

  SDL_GPUGraphicsPipeline *
  createFrustumDebugPipeline(const RenderContext &renderer);

  void renderFrustum(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                     const Camera &mainCamera, const Camera &otherCam,
                     const Float4 &color = 0x40FF00CC_rgbaf);

  void renderAABB(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                  const Camera &camera, const AABB &aabb,
                  const Float4 &color = 0x00BFFFff_rgbaf);

  void renderOBB(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                 const Camera &camera, const coal::OBB &obb,
                 const Float4 &color = 0x00BFFFff_rgbaf);

  void renderFrustum(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                     const Mat4f &invProj, const Mat4f &mvp,
                     const Float3 eyePos, const Float4 &color);

} // namespace frustum_debug

struct DebugFrustumComponent {
  Camera const *otherCam;
  GpuVec4 color;
};

struct DebugBoundsComponent {
  std::variant<AABB, coal::OBB> bounds;
  GpuVec4 color = 0xA03232FF_rgbaf;
};

class FrustumBoundsDebugSystem final {
  const RenderContext &renderer;
  const Device &device;
  SDL_GPUGraphicsPipeline *pipeline;
  entt::registry &_registry;

public:
  FrustumBoundsDebugSystem(entt::registry &registry,
                           const RenderContext &renderer);

  entt::registry &registry() { return _registry; }
  const entt::registry &registry() const { return _registry; }

  std::tuple<entt::entity, DebugFrustumComponent &>
  addFrustum(const Camera &otherCam, Float4 color = 0x00BFFFff_rgbaf) {
    auto entity = registry().create();
    auto &item =
        registry().emplace<DebugFrustumComponent>(entity, &otherCam, color);
    return {entity, item};
  }

  template <typename BoundsType>
  std::tuple<entt::entity, DebugBoundsComponent &>
  addBounds(const BoundsType &bounds) {
    auto entity = registry().create();
    auto &item = registry().emplace<DebugBoundsComponent>(entity, bounds);
    return {entity, item};
  }

  void render(CommandBuffer &cmdBuf, const Camera &camera);

  void release() noexcept {
    if (pipeline)
      SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
  }

  ~FrustumBoundsDebugSystem() { release(); }
};

} // namespace candlewick
