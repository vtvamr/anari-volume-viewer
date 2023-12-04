// Copyright 2023 Stefan Zellmann and Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

// std
#include <cassert>
// umesh
#include "umesh/UMesh.h"
// ours
#include "readUMesh.h"

UMeshReader::~UMeshReader()
{
}

bool UMeshReader::open(const char *fileName)
{
  std::cout << "#mm: loading umesh from " << fileName << std::endl;
  mesh = umesh::UMesh::loadFrom(fileName);
  if (!mesh)
    return false;
  std::cout << "#mm: got umesh w/ " << mesh->toString() << std::endl;
  fields.resize(1);
  return true;
}

UnstructuredField UMeshReader::getField(int index)
{
  assert(mesh);
  assert(index == 0);

  if (fields.empty()) {
    fields.resize(index+1);
  }

  // vertex.position
  for (size_t i=0; i<mesh->vertices.size(); ++i) {
    const auto V = mesh->vertices[i];
    fields[index].vertexPosition.push_back({V.x, V.y, V.z});
  }

  // vertex.data
  assert(!mesh->perVertex->values.empty());
  fields[index].dataRange.x = FLT_MAX;
  fields[index].dataRange.y = -FLT_MAX;

  for (size_t i=0; i<mesh->vertices.size(); ++i) {
    float value = mesh->perVertex->values[i];
    fields[index].vertexData.push_back(value);
    fields[index].dataRange.x = std::min(fields[index].dataRange.x, value);
    fields[index].dataRange.y = std::max(fields[index].dataRange.y, value);
  }

  // cells
  for (size_t i=0;i<mesh->tets.size(); ++i) {
    fields[index].cellType.push_back(10/*VKL_TETRAHEDRON*/);
    fields[index].cellIndex.push_back(fields[index].index.size());
    for (int j=0; j<mesh->tets[i].numVertices; ++j) {
      fields[index].index.push_back((uint64_t)mesh->tets[i][j]);
    }
  }

  for (size_t i=0;i<mesh->pyrs.size(); ++i) {
    fields[index].cellType.push_back(14/*VKL_PYRAMID*/);
    fields[index].cellIndex.push_back(fields[index].index.size());
    for (int j=0; j<mesh->pyrs[i].numVertices; ++j) {
      fields[index].index.push_back((uint64_t)mesh->pyrs[i][j]);
    }
  }

  for (size_t i=0;i<mesh->wedges.size(); ++i) {
    fields[index].cellType.push_back(13/*VKL_WEDGE*/);
    fields[index].cellIndex.push_back(fields[index].index.size());
    for (int j=0; j<mesh->wedges[i].numVertices; ++j) {
      fields[index].index.push_back((uint64_t)mesh->wedges[i][j]);
    }
  }

  for (size_t i=0;i<mesh->hexes.size(); ++i) {
    fields[index].cellType.push_back(12/*VKL_HEXAHEDRON*/);
    fields[index].cellIndex.push_back(fields[index].index.size());
    for (int j=0; j<mesh->hexes[i].numVertices; ++j) {
      fields[index].index.push_back((uint64_t)mesh->hexes[i][j]);
    }
  }

  return fields[index];
}
