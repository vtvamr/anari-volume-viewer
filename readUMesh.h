// Copyright 2023 Stefan Zellmann and Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

// std
#include <vector>
// ours
#include "FieldTypes.h"

namespace umesh {
class UMesh;
} // namespace umesh

struct UMeshReader
{
  ~UMeshReader();

  bool open(const char *fileName);
  UnstructuredField getField(int index);

  std::vector<UnstructuredField> fields;
  std::shared_ptr<umesh::UMesh> mesh{nullptr};
};
