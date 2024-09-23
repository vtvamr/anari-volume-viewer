// Copyright 2022 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

// glad
#include <glad/glad.h>
// glm
#include <anari/anari_cpp/ext/glm.h>
// anari
#include "anari_viewer/windows/Window.h"
// std
#include <functional>
#include <string>
#include <vector>

namespace windows {

using ColorPoint = glm::vec4;
using OpacityPoint = glm::vec2;

using TFUpdateCallback =
    std::function<void(const glm::vec2 &, const std::vector<glm::vec4> &)>;

class TransferFunctionEditor : public anari_viewer::windows::Window
{
 public:
  TransferFunctionEditor(const char *name = "TF Editor");
  ~TransferFunctionEditor();

  void buildUI() override;

  void setUpdateCallback(TFUpdateCallback cb);
  void triggerUpdateCallback();

  void setValueRange(const glm::vec2 &vr);

  // getters for current transfer function data
  glm::vec2 getValueRange();
  std::vector<glm::vec4> getSampledColorsAndOpacities(int numSamples = 256);

 private:
  void loadDefaultMaps();
  void setMap(int);

  glm::vec3 interpolateColor(
      const std::vector<ColorPoint> &controlPoints, float x);

  float interpolateOpacity(
      const std::vector<OpacityPoint> &controlPoints, float x);

  void updateTfnPaletteTexture();

  void drawEditor();

  // callback called whenever transfer function is updated
  TFUpdateCallback m_updateCallback;

  // all available transfer functions
  std::vector<std::string> m_tfnsNames;
  std::vector<std::vector<ColorPoint>> m_tfnsColorPoints;
  std::vector<std::vector<OpacityPoint>> m_tfnsOpacityPoints;
  std::vector<bool> m_tfnsEditable;

  // parameters of currently selected transfer function
  int m_currentMap{0};
  std::vector<ColorPoint> *m_tfnColorPoints{nullptr};
  std::vector<OpacityPoint> *m_tfnOpacityPoints{nullptr};
  bool m_tfnEditable{true};

  // flag indicating transfer function has changed in UI
  bool m_tfnChanged{true};

  // scaling factor for generated opacities
  float m_globalOpacityScale{1.f};

  // domain (value range) of transfer function
  glm::vec2 m_valueRange{-1.f, 1.f};
  glm::vec2 m_defaultValueRange{-1.f, 1.f};

  // texture for displaying transfer function color palette
  GLuint tfnPaletteTexture{0};
};

} // namespace windows
