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

namespace Visual::BoxEsp
{
  static bool g_enabled = false;  // esp box off by default

  void        RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_box", "ESP Box", &g_enabled);
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

    constexpr ImU32 kEnemyColor    = IM_COL32(255, 64, 64, 255);
    constexpr ImU32 kTeammateColor = IM_COL32(64, 180, 255, 255);
    constexpr ImU32 kOutlineColor  = IM_COL32(0, 0, 0, 200);

    for (const auto& p : players) {
      if (p.maxVx < 0.0f || p.minVx > 1.0f || p.maxVy < 0.0f || p.minVy > 1.0f) {
        continue;
      }

      const float topY      = (1.0f - p.maxVy) * screenHeight;
      const float bottomY   = (1.0f - p.minVy) * screenHeight;
      const float boxHeight = bottomY - topY;
      if (boxHeight < 2.0f) {
        continue;
      }

      const float  centerX  = ((p.minVx + p.maxVx) * 0.5f) * screenWidth;
      const float  boxWidth = boxHeight * 0.55f;
      const ImVec2 topLeft(centerX - boxWidth * 0.5f, topY);
      const ImVec2 bottomRight(centerX + boxWidth * 0.5f, bottomY);
      const ImU32  color = p.isEnemy ? kEnemyColor : kTeammateColor;

      drawList->AddRect(
        ImVec2(topLeft.x - 1.0f, topLeft.y - 1.0f), ImVec2(bottomRight.x + 1.0f, bottomRight.y + 1.0f), kOutlineColor,
        0.0f, 0, 1.5f
      );
      drawList->AddRect(topLeft, bottomRight, color, 0.0f, 0, 1.5f);
    }
  }
}  // namespace Visual::BoxEsp
