#include "Heightfield.h"

#include "Internal.h"

namespace candlewick {

MeshData loadHeightfield(const Eigen::Ref<const Eigen::MatrixXf> &heights,
                         const Eigen::Ref<const Eigen::VectorXf> &xgrid,
                         const Eigen::Ref<const Eigen::VectorXf> &ygrid) {
  CANDLEWICK_ASSERT(
      heights.rows() == xgrid.size(),
      "Incompatible dimensions between x-grid and 'heights' matrix.");
  CANDLEWICK_ASSERT(
      heights.cols() == ygrid.size(),
      "Incompatible dimensions between y-grid and 'heights' matrix.");
  const auto nx = (Sint32)heights.rows();
  const auto ny = (Sint32)heights.cols();
  Uint32 vertexCount = Uint32(nx * ny);
  std::vector<PosOnlyVertex> vertexData;
  std::vector<MeshData::IndexType> indexData;

  vertexData.reserve(vertexCount);
  indexData.resize(Uint32(4 * nx * (ny - 1)));
  size_t idx = 0;
  Sint32 ih, jh;
  // y-dir
  for (jh = 0; jh < ny; jh++) {
    // x-dir
    for (ih = 0; ih < nx; ih++) {
      Float3 pos{xgrid[ih], ygrid[jh], heights(ih, jh)};
      vertexData.emplace_back(pos);
      if (ih != nx - 1) {
        // connect x to x+dx
        indexData[idx++] = Uint32(jh * nx + ih);
        indexData[idx++] = Uint32(jh * nx + ih + 1);
      }
      if (jh != ny - 1) {
        // connect y to y+dy
        indexData[idx++] = Uint32(jh * nx + ih);
        indexData[idx++] = Uint32((jh + 1) * nx + ih);
      }
    }
  }
  assert(idx == indexData.size());

  return MeshData{SDL_GPU_PRIMITIVETYPE_LINELIST, std::move(vertexData),
                  std::move(indexData)};
}

} // namespace candlewick
