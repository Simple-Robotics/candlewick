#pragma once

#include <coal/fwd.hh>
#include <cassert>

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

} // namespace candlewick
