// Copyright 2023 Stefan Zellmann and Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

// std
#include <cstdint>
#include <string>
#include <vector>
// ours
#include "FieldTypes.h"

class vtkUnstructuredGrid;
class vtkUnstructuredGridReader;
struct VTKReader
{
  ~VTKReader();

  bool open(const char *fileName);
  UnstructuredField getField(int index, bool indexPrefixed = false);

  std::vector<std::string> fieldNames;
  std::vector<UnstructuredField> fields;
  vtkUnstructuredGrid *ugrid{nullptr};
  vtkUnstructuredGridReader *reader{nullptr};
};
