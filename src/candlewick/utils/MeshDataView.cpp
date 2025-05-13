#include "MeshDataView.h"

namespace candlewick {

MeshDataView::MeshDataView(const MeshData &meshData)
    : primitiveType{meshData.primitiveType}
    , layout{meshData.layout}
    , vertexData{meshData.vertexData().data(),
                 meshData.numVertices() * layout.vertexSize()}
    , indexData{meshData.indexData} {}

MeshDataView::MeshDataView(SDL_GPUPrimitiveType primitiveType,
                           const MeshLayout &layout,
                           std::span<const char> vertices,
                           std::span<const IndexType> indices)
    : primitiveType{primitiveType}
    , layout{layout}
    , vertexData{vertices}
    , indexData{indices} {}

MeshData MeshDataView::toOwned() const {
  std::vector<char> vtxOut{vertexData.begin(), vertexData.end()};
  std::vector<IndexType> idxOut{indexData.begin(), indexData.end()};
  return MeshData{primitiveType, layout, std::move(vtxOut), std::move(idxOut)};
}

} // namespace candlewick
