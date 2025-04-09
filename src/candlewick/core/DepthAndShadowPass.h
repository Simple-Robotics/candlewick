/// \defgroup depth_pass Depth Pre-Pass and Shadow Mapping Systems
///
/// \{
/// \section requirements Requirements for consistent depth testing between
/// passes.
///
/// When using a depth pre-pass with `EQUAL` depth comparison in the main pass,
/// ensure identical vertex transformations between passes by:
/// 1. Computing the MVP matrix on CPU side
/// 2. Using the same MVP matrix in both pre-pass and main pass shaders
/// 3. Avoiding shader-side matrix multiplication that might cause precision
/// differences.
///
/// Failing to do this can result in z-fighting/Moiré patterns due to
/// floating-point precision differences between CPU and GPU matrix
/// calculations.
///
/// \image html depth-prepass.png "Rendering the depth buffer after early pass"
///
/// \}
#pragma once

#include "Core.h"
#include "Camera.h"
#include "math_types.h"
#include "LightUniforms.h"

#include <SDL3/SDL_gpu.h>
#include <span>

namespace candlewick {

/// \ingroup depth_pass
/// \brief Helper struct for depth or light pre-passes.
/// \warning The depth pre-pass is meant for _opaque_ geometries. Hence, the
/// pipeline created by the create() factory function will have back-face
/// culling enabled.
struct DepthPassInfo {
  enum DepthPassSlots : Uint32 {
    TRANSFORM_SLOT = 0,
  };
  struct Config {
    SDL_GPUCullMode cull_mode;
    float depth_bias_constant_factor;
    float depth_bias_slope_factor;
    bool enable_depth_bias;
    bool enable_depth_clip;
  };
  SDL_GPUTexture *depthTexture = nullptr;
  SDL_GPUGraphicsPipeline *pipeline = nullptr;

  /// \brief Create DepthPass (e.g. for a depth pre-pass) from a Renderer
  /// and specified MeshLayout.
  ///
  /// \param renderer The Renderer object.
  /// \param layout %Mesh layout used for the render pipeline's vertex input
  /// state.
  /// \param depth_texture If null, we assume that the depth texture to use is
  /// the shared depth texture stored in \p renderer.
  /// \sa createShadowPass()
  [[nodiscard]] static DepthPassInfo
  create(const Renderer &renderer, const MeshLayout &layout,
         SDL_GPUTexture *depth_texture = NULL, Config config = {});
  /// Release the pass pipeline.
  /// \warning We do not depth texture here, because it is assumed to be
  /// borrowed.
  void release();

protected:
  SDL_GPUDevice *_device = nullptr;
};

/// \brief Shadow pass configuration, to use in createShadowPass().
struct ShadowPassConfig {
  // default is 2k x 2k texture
  Uint32 width = 2048;
  Uint32 height = 2048;
};

struct ShadowPassInfo : DepthPassInfo {
  /// Sampler to use for main render passes.
  SDL_GPUSampler *sampler;
  Camera cam;

  /// \sa DepthPassInfo::create()
  [[nodiscard]] static ShadowPassInfo create(const Renderer &renderer,
                                             const MeshLayout &layout,
                                             const ShadowPassConfig &config);

  void release();
};

/// \ingroup depth_pass
/// \brief Render a depth-only pass, built from a set of OpaqueCastable.
void renderDepthOnlyPass(CommandBuffer &cmdBuf, const DepthPassInfo &passInfo,
                         const Mat4f &viewProj,
                         std::span<const OpaqueCastable> castables);

/// \addtogroup depth_pass
/// \section depth_testing Depth testing in modern APIs
///
/// SDL GPU is based on modern graphics APIs like Vulkan, which tests depths for
/// objects located in the \f$z \in [-1,0]\f$ half-volume of the NDC cube.
/// This means that to get your geometry rendered to a shadow map, the light
/// space projection matrix needs to map everything to this half-volume.
///
/// \see shadowOrthographicMatrix()

/// \ingroup depth_pass
/// \{
/// \brief Render shadow pass, using provided scene bounds.
///
/// The scene bounds are in world-space.
void renderShadowPassFromAABB(CommandBuffer &cmdBuf, ShadowPassInfo &passInfo,
                              const DirectionalLight &dirLight,
                              std::span<const OpaqueCastable> castables,
                              const AABB &worldSceneBounds);

/// \brief Render shadow pass, using a provided world-space frustum.
///
/// This routine creates a bounding sphere around the frustum, and compute
/// light-space view and projection matrices which will enclose this bounding
/// sphere within the light volume. The frustum can be obtained from the
/// world-space camera.
/// \sa frustumFromCameraProjection()
void renderShadowPassFromFrustum(CommandBuffer &cmdBuf,
                                 ShadowPassInfo &passInfo,
                                 const DirectionalLight &dirLight,
                                 std::span<const OpaqueCastable> castables,
                                 const FrustumCornersType &worldSpaceCorners);

/// \brief Orthographic matrix which maps to the negative-Z half-volume of the
/// NDC cube, for depth-testing/shadow mapping purposes.
///
/// This matrix maps a rectangular cuboid centered on the xy-plane and spanning
/// from \p zMin to \p zMax in Z-direction, to the negative-Z half-volume of the
/// NDC cube. This is useful for mapping an axis-aligned bounding-box (\ref
/// AABB) to an orthographic projection where the entire box is in front of the
/// camera.
///
/// <https://en.wikipedia.org/wiki/Rectangular_cuboid>
///
/// \param sizes xy-plane dimensions of the cuboid/bounding box.
/// \param zMin view-space Z-coordinate the box starts at, equivalent to the \c
/// near argument in other routines.
/// \param zMax view-space Z-coordinate the box ends at, equivalent to \c far in
/// other routines.
inline Mat4f shadowOrthographicMatrix(const Float2 &sizes, float zMin,
                                      float zMax) {
  const float sx = 2.f / sizes.x();
  const float sy = 2.f / sizes.y();
  const float sz = 1.f / (zMin - zMax);
  const float pz = 0.5f * (zMin + zMax) * sz;

  Mat4f proj = Mat4f::Zero();
  proj(0, 0) = sx;
  proj(1, 1) = sy;
  proj(2, 2) = sz;
  proj(2, 3) = -pz;
  proj(3, 3) = 1.f;
  return proj;
}

/// \}

} // namespace candlewick
