#include "LoadMesh.h"

#include "MeshData.h"
#include "LoadMaterial.h"
#include "../core/DefaultVertex.h"

#include <source_location>
#include <SDL3/SDL_log.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

namespace candlewick {

MeshData loadAiMesh(const aiMesh *inMesh, const aiMatrix4x4 transform) {
  using IndexType = MeshData::IndexType;
  const Uint32 expectedFaceSize = 3;

  std::vector<DefaultVertex> vertexData;
  std::vector<IndexType> indexData;
  vertexData.resize(inMesh->mNumVertices);
  indexData.resize(inMesh->mNumFaces * expectedFaceSize);

  for (Uint32 vertex_id = 0; vertex_id < inMesh->mNumVertices; vertex_id++) {
    aiVector3D pos = inMesh->mVertices[vertex_id];
    pos = transform * pos;
    DefaultVertex &vertex = vertexData[vertex_id];
    vertex.pos = Float3::Map(&pos.x);

    aiMatrix3x3 normMatrix(transform);
    if (inMesh->HasNormals()) {
      aiVector3D n_ = inMesh->mNormals[vertex_id];
      n_ = normMatrix * n_;
      vertex.normal = Float3::Map(&n_.x);
    }
    if (inMesh->HasTangentsAndBitangents()) {
      aiVector3D t = inMesh->mTangents[vertex_id];
      t = normMatrix * t;
      vertex.tangent = Float3::Map(&t.x);
    }
  }

  for (Uint32 face_id = 0; face_id < inMesh->mNumFaces; face_id++) {
    const aiFace &f = inMesh->mFaces[face_id];
    SDL_assert(f.mNumIndices == expectedFaceSize);
    for (Uint32 ii = 0; ii < f.mNumIndices; ii++) {
      indexData[face_id * expectedFaceSize + ii] = f.mIndices[ii];
    }
  }
  return MeshData{SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, std::move(vertexData),
                  std::move(indexData)};
}

static void log_resource_failure(
    const char *err_message,
    std::source_location loc = std::source_location::current()) {
  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s: Failed to load resource [%s]",
              loc.function_name(), err_message);
}

mesh_load_retc loadSceneMeshes(const char *path,
                               std::vector<MeshData> &meshData) {

  ::Assimp::Importer import;
  // remove point primitives
  import.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                            aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  Uint32 pFlags = aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                  aiProcess_GenSmoothNormals | aiProcess_SortByPType |
                  aiProcess_JoinIdenticalVertices | aiProcess_GenUVCoords |
                  aiProcess_RemoveComponent | aiProcess_FindDegenerates |
                  aiProcess_PreTransformVertices |
                  aiProcess_ImproveCacheLocality;
  import.SetPropertyBool(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION, true);
  const aiScene *scene = import.ReadFile(path, pFlags);
  if (!scene) {
    log_resource_failure(import.GetErrorString());
    return mesh_load_retc::FAILED_TO_LOAD;
  }

  if (!scene->HasMeshes())
    return mesh_load_retc::NO_MESHES;

  aiMatrix4x4 transform = scene->mRootNode->mTransformation;
  for (std::size_t i = 0; i < scene->mNumMeshes; i++) {
    aiMesh *inMesh = scene->mMeshes[i];
    MeshData &md = meshData.emplace_back(loadAiMesh(inMesh, transform));
    Uint32 materialId = inMesh->mMaterialIndex;
    if (scene->HasMaterials()) {
      aiMaterial *material = scene->mMaterials[materialId];
      md.material = loadFromAssimpMaterial(material);
    }
  }

  return mesh_load_retc::OK;
}

} // namespace candlewick
