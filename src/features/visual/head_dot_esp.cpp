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
#include <algorithm>

namespace Visual::HeadDotEsp
{
  static bool g_enabled = false;  // esp head dot off by default

  void        RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_head_dot", "ESP Head Dot", &g_enabled);
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

    constexpr ImU32 kEnemyColor    = IM_COL32(255, 64, 64, 240);
    constexpr ImU32 kTeammateColor = IM_COL32(64, 180, 255, 240);
    constexpr ImU32 kOutlineColor  = IM_COL32(0, 0, 0, 220);

    for (const auto& p : players) {
      if (p.headVx < 0.0f || p.headVx > 1.0f || p.headVy < 0.0f || p.headVy > 1.0f) {
        continue;
      }

      const float topY      = (1.0f - p.maxVy) * screenHeight;
      const float bottomY   = (1.0f - p.minVy) * screenHeight;
      const float boxHeight = bottomY - topY;
      if (boxHeight < 2.0f) {
        continue;
      }

      const float  radius = std::clamp(boxHeight * 0.07f, 2.5f, 7.0f);
      const ImVec2 center(p.headVx * screenWidth, (1.0f - p.headVy) * screenHeight);
      const ImU32  color = p.isEnemy ? kEnemyColor : kTeammateColor;

      drawList->AddCircleFilled(center, radius + 1.0f, kOutlineColor, 16);
      drawList->AddCircleFilled(center, radius, color, 16);
    }
  }
}  // namespace Visual::HeadDotEsp
