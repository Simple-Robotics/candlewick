#pragma once

#include <span>
#include "MeshData.h"

namespace candlewick {
struct MeshDataView : MeshDataBase<MeshDataView> {
  using IndexType = MeshData::IndexType;
  SDL_GPUPrimitiveType primitiveType;
  MeshLayout layout;
  std::span<const char> vertexData;
  std::span<const IndexType> indexData;

  explicit MeshDataView(const MeshData &meshData);

  template <IsVertexType V>
  MeshDataView(SDL_GPUPrimitiveType primitiveType, std::span<const V> vertices,
               std::span<const IndexType> indices = {});

  template <IsVertexType V, size_t N, size_t M>
  MeshDataView(SDL_GPUPrimitiveType primitiveType, const V (&vertices)[N],
               const IndexType (&indices)[M]);

  MeshDataView(SDL_GPUPrimitiveType primitiveType, const MeshLayout &layout,
               std::span<const char> vertices,
               std::span<const IndexType> indices = {});

  MeshData toOwned() const;
};

template <IsVertexType V>
MeshDataView::MeshDataView(SDL_GPUPrimitiveType primitiveType,
                           std::span<const V> vertices,
                           std::span<const IndexType> indices)
    : primitiveType(primitiveType)
    , layout(meshLayoutFor<V>())
    , vertexData(reinterpret_cast<const char *>(vertices.data()),
                 vertices.size() * sizeof(V))
    , indexData(indices) {}

template <IsVertexType V, size_t N, size_t M>
MeshDataView::MeshDataView(SDL_GPUPrimitiveType primitiveType,
                           const V (&vertices)[N],
                           const IndexType (&indices)[M])
    : MeshDataView(primitiveType, std::span<const V>{vertices},
                   std::span<const IndexType>{indices}) {}

} // namespace candlewick
