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
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace windows {

using ISOUpdateCallback = std::function<void(const std::vector<float> &)>;

class ISOSurfaceEditor : public anari_viewer::windows::Window
{
  static constexpr unsigned NumValues = 4;

 public:
  ISOSurfaceEditor(const char *name = "ISO Editor");
  ~ISOSurfaceEditor();

  void buildUI() override;

  void setUpdateCallback(ISOUpdateCallback cb);
  void triggerUpdateCallback();

  void setValueRange(const glm::vec2 &vr);

  // getters for current iso surface data
  glm::vec2 getValueRange();
  constexpr unsigned getNumISOValues() const;
  std::vector<float> getISOValues();

 private:
  void drawEditor();

  // callback called whenever transfer function is updated
  ISOUpdateCallback m_updateCallback;

  // flag indicating transfer function has changed in UI
  bool m_isoSurfaceChanged{true};

  // domain (value range) of transfer function
  glm::vec2 m_valueRange{-1.f, 1.f};
  glm::vec2 m_defaultValueRange{-1.f, 1.f};

  // the iso values
  std::array<float, NumValues> m_isoValues;
  std::array<bool, NumValues> m_enabled;
};

} // namespace windows
