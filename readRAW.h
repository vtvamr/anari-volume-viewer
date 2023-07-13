// Copyright 2023 Stefan Zellmann and Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdio.h>

struct StructuredField
{
  std::vector<uint8_t> dataUI8;
  std::vector<uint16_t> dataUI16;
  std::vector<float> dataF32;
  int dimX{0};
  int dimY{0};
  int dimZ{0};
  unsigned bytesPerCell{0};
  struct
  {
    float x, y;
  } dataRange;

  bool empty() const
  {
    if (bytesPerCell == 1 && dataUI8.empty())
      return true;
    if (bytesPerCell == 2 && dataUI16.empty())
      return true;
    if (bytesPerCell == 4 && dataF32.empty())
      return true;
    return false;
  }
};

struct RAWReader
{
  ~RAWReader()
  {
    if (file)
      fclose(file);
  }

  bool open(
      const char *fileName, int dimX, int dimY, int dimZ, unsigned bytesPerCell)
  {
    file = fopen(fileName, "rb");
    if (!file) {
      std::cerr << "cannot open file: " << fileName << '\n';
      return false;
    }

    field.dimX = dimX;
    field.dimY = dimY;
    field.dimZ = dimZ;
    field.bytesPerCell = bytesPerCell;

    return true;
  }

  const StructuredField &getField(int index = 0)
  {
    if (field.empty()) {
      auto readData =
          [this](
              auto &data, int dimX, int dimY, int dimZ, unsigned bytesPerCell) {
            size_t size = dimX * size_t(dimY) * dimZ * bytesPerCell;
            data.resize(size);
            fread((char *)data.data(), size, 1, file);
          };

      if (field.bytesPerCell == 1) {
        readData(field.dataUI8,
            field.dimX,
            field.dimY,
            field.dimZ,
            field.bytesPerCell);
      } else if (field.bytesPerCell == 2) {
        readData(field.dataUI16,
            field.dimX,
            field.dimY,
            field.dimZ,
            field.bytesPerCell);
      } else if (field.bytesPerCell == 4) {
        readData(field.dataF32,
            field.dimX,
            field.dimY,
            field.dimZ,
            field.bytesPerCell);
      }

      field.dataRange = {0.f, 1.f};
    }

    return field;
  }

  FILE *file{nullptr};
  StructuredField field;
};
