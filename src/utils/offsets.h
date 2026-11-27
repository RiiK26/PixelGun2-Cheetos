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
  inline constexpr const char* kPlayerVisuals_Match              = "_match";
  inline constexpr const char* kPlayerVisuals_HealthComponent    = "_healthComponent";
  inline constexpr const char* kPlayerVisuals_CurrentHitboxSet   = "_currentHitboxSet";
  inline constexpr const char* kPlayerVisuals_RenderedAliveState = "_renderedAliveState";
  inline constexpr const char* kPlayerStanding_Profile           = "<Profile>k__BackingField";
  inline constexpr const char* kParticipantProfile_Name          = "<Name>k__BackingField";
}  // namespace fields

namespace offsets
{
  // Unity / IL2CPP runtime object layout
  inline constexpr int32_t kUnityObject_CachedPtr            = 0x10;
  inline constexpr int32_t kIl2CppString_Length              = 0x10;
  inline constexpr int32_t kIl2CppString_Chars               = 0x14;
  inline constexpr int32_t kIl2CppArray_Length               = 0x18;
  inline constexpr int32_t kIl2CppArray_Items                = 0x20;
  inline constexpr int32_t kNetworkBehaviour_Ptr             = 0x48;

  // Visual / Player fields
  inline constexpr int32_t kPlayerVisuals_CharacterView      = 0xC8;
  inline constexpr int32_t kPlayerVisuals_Match              = 0x118;
  inline constexpr int32_t kPlayerVisuals_HealthComponent    = 0x160;
  inline constexpr int32_t kPlayerVisuals_CurrentHitboxSet   = 0x168;
  inline constexpr int32_t kPlayerVisuals_RenderedAliveState = 0x25C;
  inline constexpr int32_t kHitboxSet_AimAssistHeadHitbox    = 0x38;
  inline constexpr int32_t kPlayerStanding_Profile           = 0x20;
  inline constexpr int32_t kParticipantProfile_Name          = 0x10;
  inline constexpr int32_t kHealthState_Health               = 0x4;
  inline constexpr int32_t kHealthState_MaxHealth            = 0x30;
}  // namespace offsets

namespace RVA
{
  // Visual / Player
  inline constexpr uintptr_t kPlayerVisuals_Present                      = 0x1223510;
  inline constexpr uintptr_t kPlayerVisuals_GetIsFpsCharacter            = 0x122B350;
  inline constexpr uintptr_t kPlayerVisuals_GetPlayerTeamRelation        = 0x122B400;
  inline constexpr uintptr_t kPlayerVisuals_ApplyVisualEffect            = 0x121E8B0;
  inline constexpr uintptr_t kPlayerVisuals_RemoveVisualEffect           = 0x1224AD0;
  inline constexpr uintptr_t kPlayerVisualLatePresent_LatePresent        = 0x121CFC0;

  // Health
  inline constexpr uintptr_t kHealthComponent_GetState                   = 0x130BE00;
  inline constexpr uintptr_t kHealthState_GetPercentage                  = 0x1310F10;

  // ECS / GameMode
  inline constexpr uintptr_t kEcsComponentBeh_GetId                      = 0x1425EF0;
  inline constexpr uintptr_t kMatchPresentation_TryGetGameModePlayerData = 0x10335A0;
  inline constexpr uintptr_t kPlayerStanding_GetName                     = 0x105A350;

  // UnityEngine
  inline constexpr uintptr_t kCamera_GetMain                             = 0x921AE40;
  inline constexpr uintptr_t kCamera_GetAllCameras                       = 0x92194B0;
  inline constexpr uintptr_t kCamera_GetOrthographic                     = 0x921B210;
  inline constexpr uintptr_t kCamera_GetDepth                            = 0x921A520;
  inline constexpr uintptr_t kCamera_WorldToViewportPoint                = 0x9219170;
  inline constexpr uintptr_t kComponent_GetTransform                     = 0x92BC0F0;
  inline constexpr uintptr_t kTransform_GetPosition                      = 0x9311150;
  inline constexpr uintptr_t kTransform_GetRoot                          = 0x930B5C0;
  inline constexpr uintptr_t kPhysics_Linecast                           = 0x93B4C30;
  inline constexpr uintptr_t kRaycastHit_GetCollider                     = 0x93B8F60;
}  // namespace RVA

namespace signatures
{
  // Tier 1 AOB byte patterns (verified unique in release GameAssembly.dll build 26920624)
  inline constexpr const char* kPlayerVisuals_Present =
    "48 89 5C 24 10 48 89 4C 24 08 56 57 41 56 48 81 EC E0 00 00 00 0F 29 B4 24 D0 00 00 00 48 8B F9 80 3D ? ? ? ? 00 "
    "75 ? 48 8D 0D ? ? ? ? E8 ? ? ? ? C6";
  inline constexpr const char* kPlayerVisuals_GetIsFpsCharacter = "0F B6 81 B8 01 00 00 C3";
  inline constexpr const char* kPlayerVisuals_GetPlayerTeamRelation =
    "48 89 5C 24 08 57 48 83 EC 20 80 3D ? ? ? ? 00 48 8B D9 75 ? 48 8D 0D ? ? ? ? E8 ? ? ? ? C6 05 ? ? ? ? 01 48 8B "
    "BB 20 01 00 00 33 D2 48 8B CB E8 ? ? ? ? 33 D2 8B C8 E8 ? ? ? ? 8B D8 48 85 FF 0F 84 ? ? ? ? 4C 8B 17 33 C9 4C "
    "8B 0D ? ? ? ? 41 0F B7 92 2E 01 00 00 66 3B CA 73 ? 4D 8B 82 B0 00 00 00 66 90 0F B7 C1 48 03 C0 4D 39 0C C0 74 "
    "? 66 FF C1 66 3B CA 72 ? 41 B8 07 00";
  inline constexpr const char* kPlayerVisuals_ApplyVisualEffect =
    "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 8B FA 48 8B 89";
  inline constexpr const char* kPlayerVisualLatePresent_LatePresent = "48 89 5C 24 18 48 89 74 24 20 57 41 55 41 56 48";
  inline constexpr const char* kMatchPresentation_TryGetGameModePlayerData =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 80 3D ? ? ? ? 00 49 8B F0 8B DA 48 8B F9 75 ? 48 8D 0D ? ? ? ? E8 ? "
    "? ? ? C6 05 ? ? ? ? 01 48 8B 7F 10 48 85 FF 0F 84 ? ? ? ? 4C 8B 17 33 C9 4C 8B 0D ? ? ? ? 41 0F B7 92 2E 01 00 00 "
    "66 3B CA 73 ? 4D 8B 82 B0 00 00 00 0F B7 C1 48 03 C0 4D 39 0C C0 74 ? 66 FF C1 66 3B CA 72 ? 41 B8 04 00";
  inline constexpr const char* kPlayerStanding_GetName    = "48 83 EC 38 48 8B 41 30 48 83 C4 38 C3";
  inline constexpr const char* kHealthState_GetPercentage = "40 53 48 83 EC 20 8B 59 04 33 D2 E8";
  inline constexpr const char* kCamera_GetAllCameras =
    "40 53 48 83 EC 20 80 3D ? ? ? ? 00 75 ? 48 8D 0D ? ? ? ? E8 ? ? ? ? C6 05 ? ? ? ? 01 48 8B 05 ? ? ? ? 48 85 C0";
  inline constexpr const char* kPhysics_Linecast =
    "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 70 80 3D ? ? ? ? 00 41 8B F1 0F 29 74 24 60 49";
  inline constexpr const char* kRaycastHit_GetCollider =
    "40 53 48 83 EC 20 80 3D ? ? ? ? 00 48 8B D9 75 ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? C6 05 ? "
    "? ? ? 01 48 8B 0D ? ? ? ? 8B 5B 28 83";
}  // namespace signatures
