#include "fwd.hpp"
#include "candlewick/utils/MeshData.h"

using namespace candlewick;

void exposeMeshData() {

  bp::class_<MeshLayout>("MeshLayout", bp::no_init);

  bp::class_<MeshData, boost::noncopyable>("MeshData", bp::no_init)
      .def_readonly("layout", &MeshData::layout)
      .add_property("numVertices", &MeshData::numVertices,
                    "Number of vertices.")
      .add_property("vertexSize", &MeshData::vertexSize)
      .add_property("vertexBytes", &MeshData::vertexBytes,
                    "Number of bytes in the vertex data.")
      .add_property("numIndices", &MeshData::numIndices, "Number of indices.")
      .add_property("isIndexed", &MeshData::isIndexed,
                    "Whether the mesh is indexed.")

      ;
}
