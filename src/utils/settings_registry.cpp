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

#include "settings_registry.h"

namespace SettingsRegistry
{
  static std::vector<SettingGroup> s_groups;

  SettingGroup& SettingGroup::AddBool(const std::string& iniKey, const std::string& menuLabel, bool* value)
  {
    if (!value)
      return *this;

    SettingEntry entry{};
    entry.type      = SettingType::Bool;
    entry.iniKey    = iniKey;
    entry.menuLabel = menuLabel;
    entry.target    = value;
    entries.push_back(std::move(entry));
    return *this;
  }

  SettingGroup& SettingGroup::AddFloat(
    const std::string& iniKey, const std::string& menuLabel, float* value, float min, float max, float step
  )
  {
    if (!value)
      return *this;

    SettingEntry entry{};
    entry.type      = SettingType::Float;
    entry.iniKey    = iniKey;
    entry.menuLabel = menuLabel;
    entry.target    = value;
    entry.fMin      = min;
    entry.fMax      = max;
    entry.fStep     = step;
    entries.push_back(std::move(entry));
    return *this;
  }

  SettingGroup&
  SettingGroup::AddInt(const std::string& iniKey, const std::string& menuLabel, int* value, int min, int max, int step)
  {
    if (!value)
      return *this;

    SettingEntry entry{};
    entry.type      = SettingType::Int;
    entry.iniKey    = iniKey;
    entry.menuLabel = menuLabel;
    entry.target    = value;
    entry.iMin      = min;
    entry.iMax      = max;
    entry.iStep     = step;
    entries.push_back(std::move(entry));
    return *this;
  }

  SettingGroup& SettingGroup::AddChoice(
    const std::string& iniKey, const std::string& menuLabel, int* value, std::vector<std::string> labels
  )
  {
    if (!value || labels.empty())
      return *this;

    SettingEntry entry{};
    entry.type         = SettingType::Choice;
    entry.iniKey       = iniKey;
    entry.menuLabel    = menuLabel;
    entry.target       = value;
    entry.iMin         = 0;
    entry.iMax         = static_cast<int>(labels.size()) - 1;
    entry.iStep        = 1;
    entry.choiceLabels = std::move(labels);
    entries.push_back(std::move(entry));
    return *this;
  }

  SettingGroup& SettingGroup::AddAction(const std::string& menuLabel, ActionFn action)
  {
    if (!action)
      return *this;

    SettingEntry entry{};
    entry.type      = SettingType::Action;
    entry.menuLabel = menuLabel;
    entry.target    = action;
    entries.push_back(std::move(entry));
    return *this;
  }

  void          Clear() { s_groups.clear(); }

  SettingGroup& AddGroup(const std::string& iniSection, const std::string& menuHeader, bool defaultExpanded)
  {
    if (!iniSection.empty()) {
      for (auto& existing : s_groups) {
        if (existing.iniSection == iniSection) {
          return existing;
        }
      }
    }

    SettingGroup group{};
    group.iniSection      = iniSection;
    group.menuHeader      = menuHeader;
    group.defaultExpanded = defaultExpanded;
    s_groups.push_back(std::move(group));
    return s_groups.back();
  }

  const std::vector<SettingGroup>& GetGroups() { return s_groups; }
}  // namespace SettingsRegistry
