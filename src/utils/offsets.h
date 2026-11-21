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

// This file contain centralized fields, offsets, RVA's, and also signatures, respecting to their own namespaces

#pragma once
#include <cstdint>

namespace fields
{
  inline constexpr const char* kPlayerVisuals_CharacterView      = "_characterView";
  inline constexpr const char* kPlayerVisuals_GameMode           = "_gameMode";
  inline constexpr const char* kPlayerVisuals_CurrentHitboxSet   = "_currentHitboxSet";
  inline constexpr const char* kPlayerVisuals_RenderedAliveState = "_renderedAliveState";
  inline constexpr const char* kGameModePlayer_Name              = "<Name>k__BackingField";
}  // namespace fields

namespace offsets
{
  // Unity / IL2CPP runtime object layout
  inline constexpr int32_t kUnityObject_CachedPtr            = 0x10;
  inline constexpr int32_t kIl2CppString_Length              = 0x10;
  inline constexpr int32_t kIl2CppString_Chars               = 0x14;
  inline constexpr int32_t kIl2CppArray_Length               = 0x18;
  inline constexpr int32_t kIl2CppArray_Items                = 0x20;

  // Visual / Player fields
  inline constexpr int32_t kPlayerVisuals_CharacterView      = 0x88;
  inline constexpr int32_t kPlayerVisuals_GameMode           = 0x120;
  inline constexpr int32_t kPlayerVisuals_CurrentHitboxSet   = 0x150;
  inline constexpr int32_t kPlayerVisuals_RenderedAliveState = 0x1D0;
  inline constexpr int32_t kHitboxSet_AimAssistHeadHitbox    = 0x38;
  inline constexpr int32_t kGameModePlayer_Name              = 0x30;
}  // namespace offsets

namespace RVA
{
  // Visual / Player
  inline constexpr uintptr_t kPlayerVisuals_Present               = 0x10EAAC0;
  inline constexpr uintptr_t kPlayerVisuals_GetIsFpsCharacter     = 0x10EDD00;
  inline constexpr uintptr_t kPlayerVisuals_GetPlayerTeamRelation = 0x10EDD50;
  inline constexpr uintptr_t kPlayerVisuals_ApplyVisualEffect     = 0x10E85A0;
  inline constexpr uintptr_t kPlayerVisuals_RemoveVisualEffect    = 0x10EBCB0;
  inline constexpr uintptr_t kPlayerVisualLatePresent_LatePresent = 0x10E7570;

  // ECS / GameMode
  inline constexpr uintptr_t kEcsComponentBeh_GetId               = 0x11F7BC0;
  inline constexpr uintptr_t kGameMode_TryGetGameModePlayerData   = 0x10676E0;

  // UnityEngine
  inline constexpr uintptr_t kCamera_GetMain                      = 0x703FE80;
  inline constexpr uintptr_t kCamera_GetAllCameras                = 0x703E9A0;
  inline constexpr uintptr_t kCamera_GetOrthographic              = 0x7040250;
  inline constexpr uintptr_t kCamera_GetDepth                     = 0x703F6E0;
  inline constexpr uintptr_t kCamera_WorldToViewportPoint         = 0x703E750;
  inline constexpr uintptr_t kComponent_GetTransform              = 0x70C0FC0;
  inline constexpr uintptr_t kTransform_GetPosition               = 0x70F62D0;
}  // namespace RVA

namespace signatures
{
  // Tier 1 AOB byte patterns (populated as signatures are verified)
}  // namespace signatures
