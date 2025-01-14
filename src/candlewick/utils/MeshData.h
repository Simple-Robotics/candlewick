#pragma once

#include "../core/Core.h"
#include "../core/Tags.h"
#include "../core/MeshLayout.h"
#include "VertexDataBlob.h"
#include "MaterialData.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {

struct Mesh;

template <typename Derived> struct MeshDataBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  const Derived &derived() const { return static_cast<const Derived &>(*this); }

  std::size_t numVertices() const { return derived().vertexData.size(); }
  std::size_t numIndices() const { return derived().indexData.size(); }
  bool isIndexed() const { return numIndices() > 0; }
};

struct MeshData : MeshDataBase<MeshData> {
  using IndexType = Uint32;
  SDL_GPUPrimitiveType primitiveType; //< Geometry primitive for the mesh
  VertexDataBlob vertexData;          //< Vertices
  std::vector<IndexType> indexData;   //< Indices for indexed mesh. Optional.
  PbrMaterialData material;

  const MeshLayout &layout() const { return vertexData.layout(); }

  explicit MeshData(NoInitT);
  template <IsVertexType VertexT>
  explicit MeshData(SDL_GPUPrimitiveType primitiveType,
                    std::vector<VertexT> vertexData,
                    std::vector<IndexType> indexData);
  explicit MeshData(SDL_GPUPrimitiveType primitiveType, MeshLayout layout,
                    std::vector<char> vertexData,
                    std::vector<IndexType> indexData);
  MeshData(MeshData &&) noexcept = default;
  MeshData &operator=(MeshData &&) noexcept = default;
  MeshData &operator=(const MeshData &) noexcept = delete;

  /// \brief Explicit copy function, uses private copy ctor.
  static MeshData copy(const MeshData &other) { return MeshData{other}; };

private:
  MeshData(const MeshData &) = default;
};

template <IsVertexType VertexT>
MeshData::MeshData(SDL_GPUPrimitiveType primitiveType,
                   std::vector<VertexT> vertexData,
                   std::vector<IndexType> indexData)
    : primitiveType(primitiveType), vertexData(std::move(vertexData)),
      indexData(std::move(indexData)) {}

/// \brief Convert \c MeshData to a GPU \c Mesh object. This creates the
/// required vertex buffer and index buffer (if required).
/// \warning This does *not* upload the mesh data to the vertex and index
/// buffers.
/// \sa uploadMeshToDevice()
Mesh createMesh(const Device &device, const MeshData &meshData);

/// \brief Create a \c Mesh object from given mesh data, as a view into existing
/// vertex and index buffers.
/// \p vertexOffset Vertex buffer offset (in bytes)
/// \p indexOffset Index buffer offset (in bytes)
/// \overload createMesh()
Mesh createMesh(const MeshData &meshData, SDL_GPUBuffer *vertexBuffer,
                Uint32 vertexOffset, SDL_GPUBuffer *indexBuffer,
                Uint32 indexOffset, bool takeOwnership = false);

/// \brief Upload the contents of a single, individual mesh to the GPU device.
void uploadMeshToDevice(const Device &device, const Mesh &mesh,
                        const MeshData &meshData);
} // namespace candlewick
