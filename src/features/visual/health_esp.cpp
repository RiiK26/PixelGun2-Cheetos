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

namespace Visual::HealthEsp
{
  static bool  g_enabled = false;  // esp health bar off by default

  static ImU32 HealthColor(float pct)
  {
    const float clamped = std::clamp(pct, 0.0f, 1.0f);
    int         r       = 255;
    int         g       = 255;
    if (clamped >= 0.5f) {
      // Green (40, 224, 80) -> Yellow (255, 224, 48)
      const float t = (clamped - 0.5f) * 2.0f;
      r             = static_cast<int>(255.0f - (215.0f * t));
      g             = 224;
    }
    else {
      // Yellow (255, 224, 48) -> Red (255, 56, 56)
      const float t = clamped * 2.0f;
      r             = 255;
      g             = static_cast<int>(56.0f + (168.0f * t));
    }
    return IM_COL32(r, g, 56, 255);
  }

  void RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_health", "ESP Health", &g_enabled);
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

    constexpr ImU32 kBgColor      = IM_COL32(0, 0, 0, 180);
    constexpr ImU32 kOutlineColor = IM_COL32(0, 0, 0, 220);
    constexpr float kBarWidth     = 3.0f;
    constexpr float kBarGap       = 4.0f;

    for (const auto& p : players) {
      if (p.healthPct < 0.0f || p.maxVx < 0.0f || p.minVx > 1.0f || p.maxVy < 0.0f || p.minVy > 1.0f) {
        continue;
      }

      const float topY      = (1.0f - p.maxVy) * screenHeight;
      const float bottomY   = (1.0f - p.minVy) * screenHeight;
      const float boxHeight = bottomY - topY;
      if (boxHeight < 4.0f) {
        continue;
      }

      const float centerX  = ((p.minVx + p.maxVx) * 0.5f) * screenWidth;
      const float boxWidth = boxHeight * 0.55f;
      const float barRight = centerX - (boxWidth * 0.5f) - kBarGap;
      const float barLeft  = barRight - kBarWidth;

      const float pct      = std::clamp(p.healthPct, 0.0f, 1.0f);
      const float fillTopY = bottomY - (boxHeight * pct);

      drawList->AddRectFilled(ImVec2(barLeft - 1.0f, topY - 1.0f), ImVec2(barRight + 1.0f, bottomY + 1.0f), kBgColor);
      if (pct > 0.0f) {
        drawList->AddRectFilled(ImVec2(barLeft, fillTopY), ImVec2(barRight, bottomY), HealthColor(pct));
      }
      drawList->AddRect(
        ImVec2(barLeft - 1.0f, topY - 1.0f), ImVec2(barRight + 1.0f, bottomY + 1.0f), kOutlineColor, 0.0f, 0, 1.0f
      );
    }
  }
}  // namespace Visual::HealthEsp
