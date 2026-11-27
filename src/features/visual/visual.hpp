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

#include <windows.h>
#include <cstdint>
#include <vector>

struct ImDrawList;

namespace Visual
{
  // Category-level orchestration (called from src/core/main.cpp)
  void RegisterSettings();
  bool Init();
  void Shutdown();
  void RenderOverlay(ImDrawList* drawList, float screenWidth, float screenHeight);

  namespace EspShared
  {
    struct PlayerSnapshot
    {
      uintptr_t key{0};
      // Normalized Unity viewport coordinates [0..1], origin bottom-left
      float     minVx{0.0f};
      float     minVy{0.0f};
      float     maxVx{0.0f};
      float     maxVy{0.0f};
      float     feetVx{0.0f};
      float     feetVy{0.0f};
      float     headVx{0.0f};
      float     headVy{0.0f};
      float     healthPct{-1.0f};  // [0..1] when available, < 0 if unavailable
      bool      isEnemy{true};
      bool      headVisible{true};
      bool      bodyVisible{true};
      char      name[64]{};
    };

    using SnapshotConsumerFn = bool (*)();

    bool IsValidUnityObject(const void* obj);
    void RegisterSettings();
    bool Init();
    void Shutdown();
    void SetSnapshotConsumer(SnapshotConsumerFn consumer);
    void GetFrameSnapshots(std::vector<PlayerSnapshot>& outSnapshots, uint64_t* outSeq = nullptr);
  }  // namespace EspShared

  namespace BodyEsp
  {
    void RegisterSettings();
    bool Init();
    void Shutdown();
    bool IsEnabled();
    void OnPlayerUpdate(void* playerVisuals, uintptr_t key, bool shouldShow, bool isEnemy);
    void OnPlayerPruned(uintptr_t key);
  }  // namespace BodyEsp

  namespace BoxEsp
  {
    void RegisterSettings();
    void Shutdown();
    bool IsEnabled();
    void Render(
      ImDrawList* drawList, float screenWidth, float screenHeight, const std::vector<EspShared::PlayerSnapshot>& players
    );
  }  // namespace BoxEsp

  namespace NameEsp
  {
    void RegisterSettings();
    void Shutdown();
    bool IsEnabled();
    void Render(
      ImDrawList* drawList, float screenWidth, float screenHeight, const std::vector<EspShared::PlayerSnapshot>& players
    );
  }  // namespace NameEsp

  namespace HealthEsp
  {
    void RegisterSettings();
    void Shutdown();
    bool IsEnabled();
    void Render(
      ImDrawList* drawList, float screenWidth, float screenHeight, const std::vector<EspShared::PlayerSnapshot>& players
    );
  }  // namespace HealthEsp

  namespace SnaplineEsp
  {
    void RegisterSettings();
    void Shutdown();
    bool IsEnabled();
    void Render(
      ImDrawList* drawList, float screenWidth, float screenHeight, const std::vector<EspShared::PlayerSnapshot>& players
    );
  }  // namespace SnaplineEsp

  namespace HeadDotEsp
  {
    void RegisterSettings();
    void Shutdown();
    bool IsEnabled();
    void Render(
      ImDrawList* drawList, float screenWidth, float screenHeight, const std::vector<EspShared::PlayerSnapshot>& players
    );
  }  // namespace HeadDotEsp
}  // namespace Visual
