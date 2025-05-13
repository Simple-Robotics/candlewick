#pragma once

#include "Utils.h"
#include <Eigen/Geometry>
#include <SDL3/SDL_stdinc.h>
#include <span>

namespace candlewick {

/// \brief Apply an \c Eigen::Affine3f 3D transform to a mesh in-place,
/// transforming its vertices.
void apply3DTransformInPlace(MeshData &meshData, const Eigen::Affine3f &tr);

/// \brief Generate indices for a triangle strip geometry, given the vertex
/// count.
void triangleStripGenerateIndices(Uint32 vertexCount,
                                  std::vector<Uint32> &indices);

/// \brief Convert \c MeshData object to an indexed mesh.
/// \param meshData the input un-indexed \c MeshData object to be transformed.
/// \warning For now, only supports \ref SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP.
/// \sa triangleStripGenerateIndices
MeshData generateIndices(const MeshData &meshData);

namespace detail {
  /// \brief Number of vertices and indices in the merged mesh.
  std::pair<Uint32, Uint32>
  mergeCalcIndexVertexCount(std::span<const MeshData> meshes);
} // namespace detail

/// \brief Merge meshes down to a single mesh with consistent indexing.
MeshData mergeMeshes(std::span<const MeshData> meshes);

/// \copybrief mergeMeshes().
MeshData mergeMeshes(std::vector<MeshData> &&meshes);

} // namespace candlewick
