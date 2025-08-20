#pragma once

#include <coal/fwd.hh>
#include <cassert>
#include <concepts>

#define CANDLEWICK_ASSERT(condition, msg) assert(((condition) && (msg)))

namespace candlewick {
struct Camera;
class CommandBuffer;
struct Device;
class GraphicsPipeline;
class Texture;
class Mesh;
class MeshView;
class MeshLayout;
struct Shader;
struct RenderContext;
struct Window;

struct DirectionalLight;

using coal::AABB;

/// \brief The Scene concept requires that there exist functions to render the
/// scene. Provided a command buffer and Camera, and that there is a function to
/// release the resources of the Scene.
template <typename T>
concept Scene = requires(T t, CommandBuffer &cmdBuf, const Camera &camera) {
  { t.update() } -> std::same_as<void>;
  { t.render(cmdBuf, camera) } -> std::same_as<void>;
  { t.release() } -> std::same_as<void>;
};

} // namespace candlewick
