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

#include "visual.hpp"

#include "../../utils/settings_registry.h"

#include <imgui.h>

namespace Visual::SnaplineEsp
{
  static bool g_enabled = false;  // esp snapline off by default

  void        RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_snapline", "ESP Snapline", &g_enabled);
  }

  void Shutdown() { g_enabled = false; }

  bool IsEnabled() { return g_enabled; }

  void Render(
    ImDrawList* drawList, float screenWidth, float screenHeight, const std::vector<EspShared::PlayerSnapshot>& players
  )
  {
    if (!g_enabled || !drawList) {
      return;
    }

    constexpr ImU32 kEnemyColor    = IM_COL32(255, 64, 64, 210);
    constexpr ImU32 kTeammateColor = IM_COL32(64, 180, 255, 210);
    constexpr ImU32 kOutlineColor  = IM_COL32(0, 0, 0, 140);

    const ImVec2    origin(screenWidth * 0.5f, screenHeight);

    for (const auto& p : players) {
      if (p.feetVx < 0.0f || p.feetVx > 1.0f || p.feetVy < 0.0f || p.feetVy > 1.0f) {
        continue;
      }

      const ImVec2 target(p.feetVx * screenWidth, (1.0f - p.feetVy) * screenHeight);
      const ImU32  color = p.isEnemy ? kEnemyColor : kTeammateColor;

      drawList->AddLine(origin, target, kOutlineColor, 2.5f);
      drawList->AddLine(origin, target, color, 1.25f);
    }
  }
}  // namespace Visual::SnaplineEsp
