/*  Copyright 2026 - RiiK
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <string>
#include <variant>
#include <vector>

namespace SettingsRegistry
{
  using ActionFn = void (*)();

  enum class SettingType
  {
    Bool,
    Float,
    Int,
    Choice,
    Action,
  };

  using SettingTarget = std::variant<bool*, float*, int*, ActionFn>;

  struct SettingEntry
  {
    SettingType              type{SettingType::Bool};
    std::string              iniKey{};
    std::string              menuLabel{};
    SettingTarget            target{static_cast<bool*>(nullptr)};
    float                    fMin{0.0f};
    float                    fMax{0.0f};
    float                    fStep{0.0f};
    int                      iMin{0};
    int                      iMax{0};
    int                      iStep{0};
    std::vector<std::string> choiceLabels{};
  };

  struct SettingGroup
  {
    std::string               iniSection{};
    std::string               menuHeader{};
    bool                      defaultExpanded{true};
    std::vector<SettingEntry> entries{};

    SettingGroup&             AddBool(const std::string& iniKey, const std::string& menuLabel, bool* value);
    SettingGroup&
    AddFloat(const std::string& iniKey, const std::string& menuLabel, float* value, float min, float max, float step);
    SettingGroup&
    AddInt(const std::string& iniKey, const std::string& menuLabel, int* value, int min, int max, int step);
    SettingGroup&
    AddChoice(const std::string& iniKey, const std::string& menuLabel, int* value, std::vector<std::string> labels);
    SettingGroup& AddAction(const std::string& menuLabel, ActionFn action);
  };

  // Clears all registered groups before startup population.
  void          Clear();

  // Returns the existing group for a non-empty iniSection, or appends a new group in call order.
  SettingGroup& AddGroup(const std::string& iniSection, const std::string& menuHeader, bool defaultExpanded = true);

  // Returns all registered setting groups in deterministic registration order.
  const std::vector<SettingGroup>& GetGroups();
}  // namespace SettingsRegistry
