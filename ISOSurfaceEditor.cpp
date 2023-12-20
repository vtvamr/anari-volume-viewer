// Copyright 2022 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "ISOSurfaceEditor.h"

namespace windows {

ISOSurfaceEditor::ISOSurfaceEditor(const char *name) : Window(name, true)
{
  for (unsigned i = 0; i < NumValues; ++i) {
    m_isoValues[i] = 0.f;
    m_enabled[i] = false;
  }
}

ISOSurfaceEditor::~ISOSurfaceEditor() {}

void ISOSurfaceEditor::buildUI()
{
  if (m_isoSurfaceChanged) {
    triggerUpdateCallback();
    m_isoSurfaceChanged = false;
  }

  drawEditor();
}

void ISOSurfaceEditor::setUpdateCallback(ISOUpdateCallback cb)
{
  m_updateCallback = cb;
  triggerUpdateCallback();
}

void ISOSurfaceEditor::triggerUpdateCallback()
{
  if (m_updateCallback) {
    m_updateCallback(getISOValues());
  }
}

void ISOSurfaceEditor::setValueRange(const glm::vec2 &vr)
{
  m_valueRange = m_defaultValueRange = vr;
  m_isoSurfaceChanged = true;
}

glm::vec2 ISOSurfaceEditor::getValueRange()
{
  return m_valueRange;
}

std::vector<float> ISOSurfaceEditor::getISOValues()
{
  std::vector<float> result;

  for (unsigned i = 0; i < NumValues; ++i) {
    if (m_enabled[i]) {
      result.push_back(m_isoValues[i]);
    }
  }

  return result;
}

constexpr unsigned ISOSurfaceEditor::getNumISOValues() const
{
  return NumValues;
}

void ISOSurfaceEditor::drawEditor()
{
  const float speed = (m_valueRange.y - m_valueRange.x) / 128.f;

  std::string isoStrings[] = {"ISO 0", "ISO 1", "ISO 2", "ISO 3"};
  std::string enabledStrings[] = {
      "##enable0", "##enable1", "##enable2", "##enable3"};
  for (unsigned i = 0; i < NumValues; ++i) {
    float f = m_isoValues[i];
    if (ImGui::DragFloat(
            isoStrings[i].c_str(), &f, speed, m_valueRange.x, m_valueRange.y)) {
      m_isoValues[i] = f;
      m_isoSurfaceChanged = true;
    }

    ImGui::SameLine();

    bool chk = m_enabled[i];
    if (ImGui::Checkbox(enabledStrings[i].c_str(), &chk)) {
      m_enabled[i] = chk;
      m_isoSurfaceChanged = true;
    }
  }
}

} // namespace windows
