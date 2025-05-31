#pragma once

#include <coal/fwd.hh>
#include <cassert>

#define CDW_ASSERT(condition, msg) assert(((condition) && (msg)))

namespace candlewick {
struct Camera;
class CommandBuffer;
struct Device;
class Texture;
class Mesh;
class MeshView;
class MeshLayout;
struct Shader;
struct RenderContext;
struct Window;

using coal::AABB;

} // namespace candlewick
