#pragma once

#include <SDL3/SDL_gpu.h>
#include "math_types.h"
#include <vector>

inline bool operator==(const SDL_GPUVertexBufferDescription &lhs,
                       const SDL_GPUVertexBufferDescription &rhs) {
#define _c(field) lhs.field == rhs.field
  return _c(slot) && _c(pitch) && _c(input_rate) && _c(instance_step_rate);
#undef _c
}

inline bool operator==(const SDL_GPUVertexAttribute &lhs,
                       const SDL_GPUVertexAttribute &rhs) {
#define _c(field) lhs.field == rhs.field
  return _c(location) && _c(buffer_slot) && _c(format) && _c(offset);
#undef _c
}

inline std::strong_ordering
operator<=>(const SDL_GPUVertexAttribute &lhs,
            const SDL_GPUVertexAttribute &rhs) noexcept {
  if (auto cmp = lhs.location <=> rhs.location; cmp != 0)
    return cmp;
  if (auto cmp = lhs.buffer_slot <=> rhs.buffer_slot; cmp != 0)
    return cmp;
  if (auto cmp = lhs.format <=> rhs.format; cmp != 0)
    return cmp;
  return lhs.offset <=> rhs.offset;
}

inline std::strong_ordering
operator<=>(const SDL_GPUVertexBufferDescription &lhs,
            const SDL_GPUVertexBufferDescription &rhs) noexcept {
  if (auto cmp = lhs.slot <=> rhs.slot; cmp != 0)
    return cmp;
  if (auto cmp = lhs.pitch <=> rhs.pitch; cmp != 0)
    return cmp;
  if (auto cmp = lhs.input_rate <=> rhs.input_rate; cmp != 0)
    return cmp;
  return lhs.instance_step_rate <=> rhs.instance_step_rate;
}

namespace candlewick {

constexpr Uint64 vertexElementSize(SDL_GPUVertexElementFormat format) {
  switch (format) {
  case SDL_GPU_VERTEXELEMENTFORMAT_INVALID:
    return 0u;
  case SDL_GPU_VERTEXELEMENTFORMAT_INT:
    return sizeof(Sint32);
  case SDL_GPU_VERTEXELEMENTFORMAT_INT2:
    return sizeof(Sint32[2]);
  case SDL_GPU_VERTEXELEMENTFORMAT_INT3:
    return sizeof(Sint32[3]);
  case SDL_GPU_VERTEXELEMENTFORMAT_INT4:
    return sizeof(Sint32[4]);
  case SDL_GPU_VERTEXELEMENTFORMAT_UINT:
    return sizeof(Uint32);
  case SDL_GPU_VERTEXELEMENTFORMAT_UINT2:
    return sizeof(Uint32[2]);
  case SDL_GPU_VERTEXELEMENTFORMAT_UINT3:
    return sizeof(Uint32[3]);
  case SDL_GPU_VERTEXELEMENTFORMAT_UINT4:
    return sizeof(Uint32[4]);
  case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT:
    return sizeof(float);
  case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2:
    return sizeof(float[2]);
  case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3:
    return sizeof(float[3]);
  case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4:
    return sizeof(float[4]);
  case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2:
  case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2:
  case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM:
  case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM:
    return 16u;
  case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4:
  case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4:
  case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM:
  case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM:
    return 32u;
  case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2:
  case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2:
  case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM:
  case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM:
  case SDL_GPU_VERTEXELEMENTFORMAT_HALF2:
    return 32ul;
  case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4:
  case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4:
  case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM:
  case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM:
  case SDL_GPU_VERTEXELEMENTFORMAT_HALF4:
    return 64ul;
  default:
    return 0l;
  }
}

/// \brief Fixed vertex attributes.
///
/// Each value of this enum maps to a specific input location in the shaders.
/// This defined an implicit contract between the engine and the shaders.
enum class VertexAttrib : Uint16 {
  Position,
  Normal,
  Tangent,
  Bitangent,
  Color0,
  Color1,
  TexCoord0,
  TexCoord1,
};

/// \brief This class defines the layout of a mesh's vertices.
///
/// This is used to build both rasterization pipeline and create Mesh objects.
/// Due to its large size, we store these objects in centralized storage and
/// hand out handles to them.
/// \sa Mesh
/// \sa MeshData
class MeshLayout {
public:
  MeshLayout() : m_bufferDescs{}, m_attrs{}, m_totalVertexSize(0) {}

  /// \brief Add a binding (i.e. a vertex binding) for the mesh.
  ///
  /// Calling this multiple times allows to describe a mesh layout where e.g.
  /// positions and normals are non-interleaved and should be uploaded to
  /// different vertex buffers.
  /// \param slot Index for vertex buffer
  /// \param size Equivalent of pitch in SDL_gpu, size of consecutive elements
  /// of the vertex buffer, i.e. sizeof(Vertex) if your vertices are of some
  /// type Vertex.
  MeshLayout &addBinding(Uint32 slot, Uint32 pitch) {
    m_bufferDescs.emplace_back(slot, pitch, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0u);
    return *this;
  }

  /// \brief Add a vertex attribute.
  /// \param name The vertex attribute name.
  /// \param loc Location index in the vertex shader.
  /// \param binding Binding slot of the corresponding vertex buffer.
  /// \param format Format of the vertex attribute.
  /// \param offset Byte offset of the attribute relative to the start of the
  /// vertex buffer element.
  constexpr MeshLayout &addAttribute(VertexAttrib loc, Uint32 binding,
                                     SDL_GPUVertexElementFormat format,
                                     Uint32 offset) {
    const Uint16 _loc = static_cast<Uint16>(loc);
    m_attrs.emplace_back(_loc, binding, format, offset);
    const Uint32 attrSize =
        math::roundUpTo16(Uint32(vertexElementSize(format)));
    m_totalVertexSize = std::max(m_totalVertexSize, offset + attrSize);
    return *this;
  }

  const SDL_GPUVertexAttribute *getAttribute(VertexAttrib loc) const noexcept {
    for (Uint32 i = 0; i < numAttributes(); i++) {
      if (m_attrs[i].location == static_cast<Uint16>(loc)) {
        return &m_attrs[i];
      }
    }
    return nullptr;
  }

  /// \brief Cast to the SDL_GPU vertex input state struct, used to create
  /// pipelines.
  /// \warning The data here only references data internal to MeshLayout and
  /// *not* copied. It must stay in scope until the pipeline is created.
  SDL_GPUVertexInputState toVertexInputState() const noexcept {
    return {
        m_bufferDescs.data(),
        numBuffers(),
        m_attrs.data(),
        numAttributes(),
    };
  }

  operator SDL_GPUVertexInputState() const noexcept {
    return toVertexInputState();
  }

  std::strong_ordering operator<=>(const MeshLayout &other) const noexcept;

  bool operator==(const MeshLayout &other) const noexcept = default;

  /// \brief Number of vertex buffers.
  Uint32 numBuffers() const { return Uint32(m_bufferDescs.size()); }
  /// \brief Number of vertex attributes.
  Uint32 numAttributes() const { return Uint32(m_attrs.size()); }
  /// \brief Total size of a vertex (in bytes).
  /// \todo Make this compatible with multiple vertex bindings.
  Uint32 vertexSize() const { return m_totalVertexSize; }
  /// \brief Size of mesh indices (in bytes).
  Uint32 indexSize() const { return sizeof(Uint32); }

  std::vector<SDL_GPUVertexBufferDescription> m_bufferDescs;
  std::vector<SDL_GPUVertexAttribute> m_attrs;

private:
  Uint32 m_totalVertexSize;
};

inline std::strong_ordering
MeshLayout::operator<=>(const MeshLayout &other) const noexcept {
  if (auto cmp = numBuffers() <=> other.numBuffers(); cmp != 0)
    return cmp;
  if (auto cmp = numAttributes() <=> other.numAttributes(); cmp != 0)
    return cmp;
  if (auto cmp = m_bufferDescs <=> other.m_bufferDescs; cmp != 0)
    return cmp;
  return m_attrs <=> other.m_attrs;
}

/// \brief Validation function. Checks if a MeshLayout produces invalid data for
/// a Mesh.
inline bool validateMeshLayout(const MeshLayout &layout) {
  return (layout.numBuffers() > 0) && (layout.numAttributes() > 0);
}

/// \brief Basic concept checking if type V has the correct layout and alignment
/// requirements to be a vertex element.
template <typename V>
concept IsVertexType = std::is_standard_layout_v<V> && (alignof(V) == 16);

template <IsVertexType V> struct VertexTraits;

/// \brief Shortcut for extracting layout from compile-time struct.
template <IsVertexType V> MeshLayout meshLayoutFor() {
  return VertexTraits<V>::layout();
}

} // namespace candlewick
