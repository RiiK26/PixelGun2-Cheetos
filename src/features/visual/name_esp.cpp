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

namespace Visual::NameEsp
{
  static bool g_enabled = false;  // esp name off by default

  void        RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_name", "ESP Name", &g_enabled);
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

    constexpr ImU32 kOutlineColor = IM_COL32(0, 0, 0, 200);
    constexpr ImU32 kTextColor    = IM_COL32(255, 255, 255, 255);

    for (const auto& p : players) {
      if (p.name[0] == '\0' || p.maxVx < 0.0f || p.minVx > 1.0f || p.maxVy < 0.0f || p.minVy > 1.0f) {
        continue;
      }

      const float topY      = (1.0f - p.maxVy) * screenHeight;
      const float bottomY   = (1.0f - p.minVy) * screenHeight;
      const float boxHeight = bottomY - topY;
      if (boxHeight < 2.0f) {
        continue;
      }

      const float  centerX  = ((p.minVx + p.maxVx) * 0.5f) * screenWidth;
      const ImVec2 textSize = ImGui::CalcTextSize(p.name);
      const float  textX    = centerX - textSize.x * 0.5f;
      const float  textY    = topY - textSize.y - 3.0f;

      drawList->AddText(ImVec2(textX + 1.0f, textY + 1.0f), kOutlineColor, p.name);
      drawList->AddText(ImVec2(textX, textY), kTextColor, p.name);
    }
  }
}  // namespace Visual::NameEsp
