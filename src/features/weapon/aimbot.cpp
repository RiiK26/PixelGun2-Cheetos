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

#include "weapon.hpp"

#include "../../ui/menu.h"
#include "../../utils/settings_registry.h"
#include "../visual/visual.hpp"

#include <imgui.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Weapon::Aimbot
{
  enum class TargetBone : int
  {
    Head = 0,
    Neck = 1,
    Body = 2,
  };

  struct SelectedTarget
  {
    float x{0.0f};
    float y{0.0f};
    float distSq{0.0f};
  };

  static bool                                           g_enabled         = false;
  static bool                                           g_showFov         = false;
  static bool                                           g_visCheck        = true;
  static float                                          g_fov             = 120.0f;
  static float                                          g_smoothness      = 3.0f;
  static int                                            g_targetBone      = static_cast<int>(TargetBone::Head);

  static uint64_t                                       g_lastSnapshotSeq = 0;
  static uintptr_t                                      g_lockedTargetKey = 0;
  static int                                            g_losMissedTicks  = 0;
  static std::vector<Visual::EspShared::PlayerSnapshot> g_snapshots;

  static void                                           ResetTrackingState()
  {
    g_lockedTargetKey = 0;
    g_losMissedTicks  = 0;
  }

  static bool IsGameWindowForeground()
  {
    HWND fg = GetForegroundWindow();
    if (!fg) {
      return false;
    }
    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    return fgPid == GetCurrentProcessId();
  }

  static bool IsTargetBoneVisible(const Visual::EspShared::PlayerSnapshot& p, TargetBone bone)
  {
    if (!g_visCheck) {
      return true;
    }
    return (bone == TargetBone::Body) ? p.bodyVisible : p.headVisible;
  }

  static bool ComputeTargetScreenPoint(
    const Visual::EspShared::PlayerSnapshot& p,
    TargetBone                               bone,
    float                                    screenWidth,
    float                                    screenHeight,
    float&                                   outX,
    float&                                   outY
  )
  {
    const float topY      = (1.0f - p.maxVy) * screenHeight;
    const float bottomY   = (1.0f - p.minVy) * screenHeight;
    const float boxHeight = bottomY - topY;
    if (boxHeight < 2.0f) {
      return false;
    }

    const bool  hasValidHead = p.headVx >= 0.0f && p.headVx <= 1.0f && p.headVy >= 0.0f && p.headVy <= 1.0f;
    const float boxCenterX   = 0.5f * (p.minVx + p.maxVx) * screenWidth;

    switch (bone) {
    case TargetBone::Head :
      if (hasValidHead) {
        outX = p.headVx * screenWidth;
        outY = (1.0f - p.headVy) * screenHeight;
      }
      else {
        outX = boxCenterX;
        outY = topY + boxHeight * 0.12f;
      }
      break;
    case TargetBone::Neck :
      if (hasValidHead) {
        outX = p.headVx * screenWidth;
        outY = (1.0f - p.headVy) * screenHeight + boxHeight * 0.10f;
      }
      else {
        outX = boxCenterX;
        outY = topY + boxHeight * 0.22f;
      }
      break;
    case TargetBone::Body :
      outX = boxCenterX;
      outY = topY + boxHeight * 0.42f;
      break;
    }

    return outX >= 0.0f && outX <= screenWidth && outY >= 0.0f && outY <= screenHeight;
  }

  static void EmitRelativeMouseMove(LONG dx, LONG dy)
  {
    if (dx == 0 && dy == 0) {
      return;
    }

    INPUT input{};
    input.type       = INPUT_MOUSE;
    input.mi.dx      = dx;
    input.mi.dy      = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
  }

  static bool SelectTarget(
    float screenWidth, float screenHeight, float centerX, float centerY, float fov, SelectedTarget& outTarget
  )
  {
    constexpr int  kMaxLosGraceTicks = 6;
    const auto     bone              = static_cast<TargetBone>(std::clamp(g_targetBone, 0, 2));
    const float    fovSq             = fov * fov;
    const float    stickySq          = (fov * 1.35f) * (fov * 1.35f);

    bool           foundLocked       = false;
    bool           lockedVisibleNow  = false;
    SelectedTarget locked{centerX, centerY, stickySq};

    bool           foundBest = false;
    uintptr_t      bestKey   = 0;
    SelectedTarget best{centerX, centerY, fovSq};

    for (const auto& p : g_snapshots) {
      if (!p.isEnemy) {
        continue;
      }

      float tx = 0.0f;
      float ty = 0.0f;
      if (!ComputeTargetScreenPoint(p, bone, screenWidth, screenHeight, tx, ty)) {
        continue;
      }

      const bool  visible = IsTargetBoneVisible(p, bone);
      const float dx      = tx - centerX;
      const float dy      = ty - centerY;
      const float distSq  = dx * dx + dy * dy;

      if (g_lockedTargetKey != 0 && p.key == g_lockedTargetKey && distSq < stickySq) {
        if (visible || g_losMissedTicks < kMaxLosGraceTicks) {
          foundLocked      = true;
          lockedVisibleNow = visible;
          locked           = {tx, ty, distSq};
        }
      }

      if (visible && distSq < best.distSq) {
        bestKey   = p.key;
        best      = {tx, ty, distSq};
        foundBest = true;
      }
    }

    if (foundLocked) {
      g_losMissedTicks = lockedVisibleNow ? 0 : (g_losMissedTicks + 1);
      outTarget        = locked;
      return true;
    }

    if (foundBest) {
      g_lockedTargetKey = bestKey;
      g_losMissedTicks  = 0;
      outTarget         = best;
      return true;
    }

    return false;
  }

  static void StepAimTowardTarget(const SelectedTarget& target, float centerX, float centerY)
  {
    constexpr float kDeadzonePx  = 5.0f;
    constexpr float kSensDivisor = 6.0f;  // Keeps 1-tick step below overshoot at smoothness = 1
    constexpr float kMaxStep     = 15.0f;

    if (target.distSq < (kDeadzonePx * kDeadzonePx)) {
      return;
    }

    const float denom = std::max(1.0f, g_smoothness) * kSensDivisor;
    const float stepX = std::clamp((target.x - centerX) / denom, -kMaxStep, kMaxStep);
    const float stepY = std::clamp((target.y - centerY) / denom, -kMaxStep, kMaxStep);

    EmitRelativeMouseMove(static_cast<LONG>(std::lround(stepX)), static_cast<LONG>(std::lround(stepY)));
  }

  void RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("weapon", "-- WEAPON --", true);
    group.AddBool("aimbot_enabled", "Aimbot", &g_enabled);
    group.AddBool("aimbot_vis_check", "Aimbot Visibility Check", &g_visCheck);
    group.AddBool("aimbot_fov_circle", "Aimbot FOV Circle", &g_showFov);
    group.AddFloat("aimbot_fov", "Aimbot FOV", &g_fov, 20.0f, 400.0f, 10.0f);
    group.AddFloat("aimbot_smoothness", "Aimbot Smoothness", &g_smoothness, 1.0f, 20.0f, 0.5f);
    group.AddChoice("aimbot_target", "Aimbot Target", &g_targetBone, {"Head", "Neck", "Body"});
  }

  bool Init()
  {
    g_snapshots.reserve(32);
    g_lastSnapshotSeq = 0;
    ResetTrackingState();
    return true;
  }

  void Shutdown()
  {
    g_enabled         = false;
    g_showFov         = false;
    g_lastSnapshotSeq = 0;
    ResetTrackingState();
    g_snapshots.clear();
  }

  bool IsEnabled() { return g_enabled; }

  void UpdateAndRender(ImDrawList* drawList, float screenWidth, float screenHeight)
  {
    if (screenWidth <= 1.0f || screenHeight <= 1.0f) {
      return;
    }

    const float centerX = screenWidth * 0.5f;
    const float centerY = screenHeight * 0.5f;
    const float fov     = std::clamp(g_fov, 20.0f, 400.0f);

    if (g_showFov && drawList) {
      const ImVec2 center(centerX, centerY);
      drawList->AddCircle(center, fov + 1.0f, IM_COL32(0, 0, 0, 140), 64, 1.5f);
      drawList->AddCircle(center, fov, IM_COL32(255, 255, 255, 190), 64, 1.25f);
    }

    if (!g_enabled || Menu::IsOpen() || !IsGameWindowForeground()) {
      g_lastSnapshotSeq = 0;
      ResetTrackingState();
      return;
    }

    uint64_t seq = 0;
    Visual::EspShared::GetFrameSnapshots(g_snapshots, &seq);
    if (g_snapshots.empty()) {
      ResetTrackingState();
      return;
    }

    if (seq == g_lastSnapshotSeq) {
      return;
    }
    g_lastSnapshotSeq = seq;

    SelectedTarget target{};
    if (!SelectTarget(screenWidth, screenHeight, centerX, centerY, fov, target)) {
      ResetTrackingState();
      return;
    }

    StepAimTowardTarget(target, centerX, centerY);
  }
}  // namespace Weapon::Aimbot

namespace Weapon
{
  void RegisterSettings() { Aimbot::RegisterSettings(); }

  bool Init() { return Aimbot::Init(); }

  void Shutdown() { Aimbot::Shutdown(); }

  void RenderOverlay(ImDrawList* drawList, float screenWidth, float screenHeight)
  {
    Aimbot::UpdateAndRender(drawList, screenWidth, screenHeight);
  }
}  // namespace Weapon
