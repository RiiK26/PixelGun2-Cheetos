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

#include "../../utils/logger.h"
#include "../../utils/offsets.h"
#include "../../utils/resolver.h"
#include "../../utils/settings_registry.h"

#include <MinHook.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Visual::EspShared
{
  struct Vector3
  {
    float x;
    float y;
    float z;
  };

  struct NetworkId
  {
    uint32_t raw;
  };

  enum class TeamRelation : int32_t
  {
    Unknown  = 0,
    Friendly = 1,
    Enemy    = 2,
  };

  struct TrackedPlayer
  {
    ULONGLONG lastSeenMs{0};
    ULONGLONG lastNameQueryMs{0};
    bool      hasName{false};
    char      cachedName[64]{};
  };

  static bool g_teamCheck = true;  // Team Check for all visual are ON which mean teammate didnt included when its on

  using PlayerVisuals_Present_t               = void (*)(void* self);
  using PlayerVisualLatePresent_LatePresent_t = void (*)(void* self, const void* runnerState, void* bus);
  using PlayerVisuals_GetIsFpsCharacter_t     = bool (*)(void* self);
  using PlayerVisuals_GetPlayerTeamRelation_t = TeamRelation (*)(void* self);
  using EcsComponentBeh_GetId_t               = NetworkId (*)(void* self);
  using GameMode_TryGetGameModePlayerData_t   = bool (*)(void* self, NetworkId entityId, void** outPlayerData);
  using Camera_GetMain_t                      = void* (*)();
  using Camera_GetAllCameras_t                = void* (*)();
  using Camera_GetOrthographic_t              = bool (*)(void* camera);
  using Camera_GetDepth_t                     = float (*)(void* camera);
  using Camera_WorldToViewportPoint_t         = Vector3 (*)(void* camera, Vector3 position);
  using Component_GetTransform_t              = void* (*)(void* component);
  using Transform_GetPosition_t               = Vector3 (*)(void* transform);

  static PlayerVisuals_Present_t                      g_origPresent             = nullptr;
  static PlayerVisualLatePresent_LatePresent_t        g_origLatePresent         = nullptr;
  static PlayerVisuals_GetIsFpsCharacter_t            g_fnGetIsFpsCharacter     = nullptr;
  static PlayerVisuals_GetPlayerTeamRelation_t        g_fnGetPlayerTeamRelation = nullptr;
  static EcsComponentBeh_GetId_t                      g_fnGetId                 = nullptr;
  static GameMode_TryGetGameModePlayerData_t          g_fnTryGetPlayerData      = nullptr;
  static Camera_GetMain_t                             g_fnCameraGetMain         = nullptr;
  static Camera_GetAllCameras_t                       g_fnCameraGetAll          = nullptr;
  static Camera_GetOrthographic_t                     g_fnCameraGetOrthographic = nullptr;
  static Camera_GetDepth_t                            g_fnCameraGetDepth        = nullptr;
  static Camera_WorldToViewportPoint_t                g_fnWorldToViewportPoint  = nullptr;
  static Component_GetTransform_t                     g_fnComponentGetTransform = nullptr;
  static Transform_GetPosition_t                      g_fnTransformGetPosition  = nullptr;

  static int32_t                                      g_offCharacterView        = offsets::kPlayerVisuals_CharacterView;
  static int32_t                                      g_offGameMode             = offsets::kPlayerVisuals_GameMode;
  static int32_t                                      g_offCurrentHitboxSet = offsets::kPlayerVisuals_CurrentHitboxSet;
  static int32_t                                      g_offRenderedAlive = offsets::kPlayerVisuals_RenderedAliveState;
  static int32_t                                      g_offHeadHitbox    = offsets::kHitboxSet_AimAssistHeadHitbox;
  static int32_t                                      g_offPlayerName    = offsets::kGameModePlayer_Name;

  // Double-buffered snapshots: LatePresent builds into g_stagingSnapshots (lock-free),
  // swaps once into g_publishedSnapshots; RenderOverlay copies once into g_renderSnapshots.
  static std::mutex                                   g_snapshotMutex;
  static std::vector<PlayerSnapshot>                  g_stagingSnapshots;
  static std::vector<PlayerSnapshot>                  g_publishedSnapshots;
  static std::vector<PlayerSnapshot>                  g_renderSnapshots;
  static ULONGLONG                                    g_publishedMs = 0;

  static std::unordered_map<uintptr_t, TrackedPlayer> g_trackedVisuals;
  static void*                                        g_cachedGameplayCam = nullptr;
  static ULONGLONG                                    g_lastCamRefreshMs  = 0;

  bool                                                IsValidUnityObject(const void* obj)
  {
    if (!obj) {
      return false;
    }
    const void* cachedPtr =
      *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(obj) + offsets::kUnityObject_CachedPtr);
    return cachedPtr != nullptr;
  }

  bool         IsTeamCheckEnabled() { return g_teamCheck; }

  static void* FindGameplayCamera(ULONGLONG nowMs)
  {
    if (IsValidUnityObject(g_cachedGameplayCam) && (nowMs - g_lastCamRefreshMs) < 1000) {
      return g_cachedGameplayCam;
    }

    g_lastCamRefreshMs  = nowMs;
    g_cachedGameplayCam = nullptr;

    if (g_fnCameraGetAll && g_fnCameraGetOrthographic) {
      void* camArray = g_fnCameraGetAll();
      if (camArray) {
        const auto*   base  = static_cast<const uint8_t*>(camArray);
        const int64_t count = *reinterpret_cast<const int64_t*>(base + offsets::kIl2CppArray_Length);
        if (count > 0 && count <= 64) {
          void* const* items     = reinterpret_cast<void* const*>(base + offsets::kIl2CppArray_Items);
          void*        bestCam   = nullptr;
          float        bestDepth = 1e9f;

          for (int64_t i = 0; i < count; ++i) {
            void* cam = items[i];
            if (!IsValidUnityObject(cam) || g_fnCameraGetOrthographic(cam)) {
              continue;
            }
            const float depth = g_fnCameraGetDepth ? g_fnCameraGetDepth(cam) : 0.0f;
            if (!bestCam || depth < bestDepth) {
              bestCam   = cam;
              bestDepth = depth;
            }
          }

          if (bestCam) {
            g_cachedGameplayCam = bestCam;
            return bestCam;
          }
        }
      }
    }

    if (g_fnCameraGetMain) {
      void* mainCam = g_fnCameraGetMain();
      if (IsValidUnityObject(mainCam)) {
        g_cachedGameplayCam = mainCam;
        return mainCam;
      }
    }

    return nullptr;
  }

  static bool ReadIl2CppStringUtf8(const void* il2cppString, char* outBuf, size_t outBufSize)
  {
    if (!il2cppString || !outBuf || outBufSize == 0) {
      return false;
    }

    const auto* base = static_cast<const uint8_t*>(il2cppString);
    int32_t     len  = *reinterpret_cast<const int32_t*>(base + offsets::kIl2CppString_Length);
    if (len <= 0 || len > 128) {
      return false;
    }

    const auto* utf16 = reinterpret_cast<const wchar_t*>(base + offsets::kIl2CppString_Chars);
    int         written =
      WideCharToMultiByte(CP_UTF8, 0, utf16, len, outBuf, static_cast<int>(outBufSize - 1), nullptr, nullptr);
    if (written <= 0) {
      outBuf[0] = '\0';
      return false;
    }

    outBuf[written] = '\0';
    return true;
  }

  static void UpdatePlayerEsp(
    void*          self,
    uintptr_t      key,
    TrackedPlayer& tracked,
    void*          camera,
    bool           bodyOn,
    bool           boxOn,
    bool           nameOn,
    ULONGLONG      nowMs
  )
  {
    if (g_fnGetIsFpsCharacter && g_fnGetIsFpsCharacter(self)) {
      BodyEsp::OnPlayerUpdate(self, key, false, false);
      return;
    }

    bool isAlive = true;
    if (g_offRenderedAlive > 0) {
      isAlive = *reinterpret_cast<const bool*>(static_cast<const uint8_t*>(self) + g_offRenderedAlive);
    }

    if ((!bodyOn && !boxOn && !nameOn) || !isAlive) {
      BodyEsp::OnPlayerUpdate(self, key, false, false);
      return;
    }

    TeamRelation relation = TeamRelation::Enemy;
    if (g_fnGetPlayerTeamRelation) {
      relation = g_fnGetPlayerTeamRelation(self);
    }

    const bool isEnemy = (relation != TeamRelation::Friendly);
    if (g_teamCheck && !isEnemy) {
      BodyEsp::OnPlayerUpdate(self, key, false, false);
      return;
    }

    BodyEsp::OnPlayerUpdate(self, key, bodyOn, isEnemy);

    if (
      (!boxOn && !nameOn) || !camera || !g_fnWorldToViewportPoint || !g_fnComponentGetTransform
      || !g_fnTransformGetPosition
    ) {
      return;
    }

    void* bodyComponent = self;
    if (g_offCharacterView > 0) {
      void* charView = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(self) + g_offCharacterView);
      if (IsValidUnityObject(charView)) {
        bodyComponent = charView;
      }
    }

    if (!IsValidUnityObject(bodyComponent)) {
      return;
    }

    void* feetTransform = g_fnComponentGetTransform(bodyComponent);
    if (!IsValidUnityObject(feetTransform)) {
      return;
    }

    Vector3 feetWorld = g_fnTransformGetPosition(feetTransform);
    Vector3 headWorld = {feetWorld.x, feetWorld.y + 2.1f, feetWorld.z};

    if (g_offCurrentHitboxSet > 0 && g_offHeadHitbox > 0) {
      void* hitboxSet = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(self) + g_offCurrentHitboxSet);
      if (IsValidUnityObject(hitboxSet)) {
        void* headHitbox = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(hitboxSet) + g_offHeadHitbox);
        if (IsValidUnityObject(headHitbox)) {
          void* headTransform = g_fnComponentGetTransform(headHitbox);
          if (IsValidUnityObject(headTransform)) {
            Vector3 h = g_fnTransformGetPosition(headTransform);
            if (h.y > feetWorld.y + 1.2f) {
              headWorld = {h.x, h.y + 0.35f, h.z};
            }
          }
        }
      }
    }

    Vector3 feetVp = g_fnWorldToViewportPoint(camera, feetWorld);
    Vector3 headVp = g_fnWorldToViewportPoint(camera, headWorld);

    if (feetVp.z <= 0.01f || headVp.z <= 0.01f) {
      return;
    }

    const float boxH = std::abs(headVp.y - feetVp.y);
    if (boxH < 0.002f) {
      return;
    }

    PlayerSnapshot snap{};
    snap.minVx   = (std::min) (feetVp.x, headVp.x);
    snap.maxVx   = (std::max) (feetVp.x, headVp.x);
    snap.minVy   = (std::min) (feetVp.y, headVp.y);
    snap.maxVy   = (std::max) (feetVp.y, headVp.y);
    snap.isEnemy = isEnemy;

    if (nameOn) {
      if (
        !tracked.hasName && (nowMs - tracked.lastNameQueryMs) >= 1000 && g_fnGetId && g_fnTryGetPlayerData
        && g_offGameMode > 0 && g_offPlayerName > 0
      ) {
        tracked.lastNameQueryMs = nowMs;
        void* gameMode          = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(self) + g_offGameMode);
        if (gameMode) {
          NetworkId id         = g_fnGetId(self);
          void*     playerData = nullptr;
          if (g_fnTryGetPlayerData(gameMode, id, &playerData) && playerData) {
            const void* nameStr =
              *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(playerData) + g_offPlayerName);
            tracked.hasName = ReadIl2CppStringUtf8(nameStr, tracked.cachedName, sizeof(tracked.cachedName));
          }
        }
      }

      if (tracked.hasName) {
        std::memcpy(snap.name, tracked.cachedName, sizeof(snap.name));
      }
    }

    g_stagingSnapshots.push_back(snap);
  }

  static void HookedPlayerVisualsPresent(void* self)
  {
    if (g_origPresent) {
      g_origPresent(self);
    }

    if (!IsValidUnityObject(self)) {
      return;
    }

    const ULONGLONG nowMs = GetTickCount64();
    const auto      key   = reinterpret_cast<uintptr_t>(self);
    auto&           entry = g_trackedVisuals[key];
    entry.lastSeenMs      = nowMs;
  }

  static void HookedLatePresent(void* self, const void* runnerState, void* bus)
  {
    if (g_origLatePresent) {
      g_origLatePresent(self, runnerState, bus);
    }

    if (g_trackedVisuals.empty()) {
      return;
    }

    const bool      bodyOn = BodyEsp::IsEnabled();
    const bool      boxOn  = BoxEsp::IsEnabled();
    const bool      nameOn = NameEsp::IsEnabled();

    const ULONGLONG nowMs  = GetTickCount64();
    void*           camera = (boxOn || nameOn) ? FindGameplayCamera(nowMs) : nullptr;

    g_stagingSnapshots.clear();

    for (auto it = g_trackedVisuals.begin(); it != g_trackedVisuals.end();) {
      auto* pv = reinterpret_cast<void*>(it->first);
      if ((nowMs - it->second.lastSeenMs) > 500 || !IsValidUnityObject(pv)) {
        BodyEsp::OnPlayerPruned(it->first);
        it = g_trackedVisuals.erase(it);
        continue;
      }

      UpdatePlayerEsp(pv, it->first, it->second, camera, bodyOn, boxOn, nameOn, nowMs);
      ++it;
    }

    // Single lock per frame: O(1) swap into published buffer
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    g_publishedSnapshots.swap(g_stagingSnapshots);
    g_publishedMs = nowMs;
  }

  void RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_team_check", "Team Check", &g_teamCheck);
  }

  bool Init()
  {
    g_stagingSnapshots.reserve(32);
    g_publishedSnapshots.reserve(32);
    g_renderSnapshots.reserve(32);

    constexpr const char* kPlayerNs   = "Psa.Core.Modules.Player.Components.Implementation";
    constexpr const char* kGameModeNs = "Psa.Core.Modules.RoundFlow.GameModes";
    constexpr const char* kUnityNs    = "UnityEngine";

    auto                  presentRes  = Resolver::ResolveMethod({
      .name       = "PlayerVisuals::Present",
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "Present",
      .argCount   = 0,
      .staticRva  = RVA::kPlayerVisuals_Present,
    });
    if (!presentRes) {
      Logger::Log("[ESP] Failed to resolve PlayerVisuals::Present.");
      return false;
    }

    auto resolveFn = [](const Resolver::MethodSpec& spec) -> uintptr_t {
      auto res = Resolver::ResolveMethod(spec);
      if (!res) {
        Logger::Log("[ESP] Warning: failed to resolve %s.", spec.name ? spec.name : "method");
      }
      return res.address;
    };

    g_fnGetIsFpsCharacter     = reinterpret_cast<PlayerVisuals_GetIsFpsCharacter_t>(resolveFn({
      .name       = "PlayerVisuals::get_IsFpsCharacter",
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "get_IsFpsCharacter",
      .argCount   = 0,
      .staticRva  = RVA::kPlayerVisuals_GetIsFpsCharacter,
    }));

    g_fnGetPlayerTeamRelation = reinterpret_cast<PlayerVisuals_GetPlayerTeamRelation_t>(resolveFn({
      .name       = "PlayerVisuals::get_PlayerTeamRelation",
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "get_PlayerTeamRelation",
      .argCount   = 0,
      .staticRva  = RVA::kPlayerVisuals_GetPlayerTeamRelation,
    }));

    g_fnGetId                 = reinterpret_cast<EcsComponentBeh_GetId_t>(resolveFn({
      .name       = "EcsComponentBeh::get_Id",
      .namespaze  = "Psa.Core.Modules.ECS.Components",
      .className  = "EcsComponentBeh",
      .methodName = "get_Id",
      .argCount   = 0,
      .staticRva  = RVA::kEcsComponentBeh_GetId,
    }));

    g_fnTryGetPlayerData      = reinterpret_cast<GameMode_TryGetGameModePlayerData_t>(resolveFn({
      .name       = "GameMode::TryGetGameModePlayerData",
      .namespaze  = kGameModeNs,
      .className  = "GameMode",
      .methodName = "TryGetGameModePlayerData",
      .argCount   = 2,
      .staticRva  = RVA::kGameMode_TryGetGameModePlayerData,
    }));

    g_fnCameraGetMain         = reinterpret_cast<Camera_GetMain_t>(resolveFn({
      .name       = "Camera::get_main",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_main",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetMain,
    }));

    g_fnCameraGetAll          = reinterpret_cast<Camera_GetAllCameras_t>(resolveFn({
      .name       = "Camera::get_allCameras",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_allCameras",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetAllCameras,
    }));

    g_fnCameraGetOrthographic = reinterpret_cast<Camera_GetOrthographic_t>(resolveFn({
      .name       = "Camera::get_orthographic",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_orthographic",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetOrthographic,
    }));

    g_fnCameraGetDepth        = reinterpret_cast<Camera_GetDepth_t>(resolveFn({
      .name       = "Camera::get_depth",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_depth",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetDepth,
    }));

    g_fnWorldToViewportPoint  = reinterpret_cast<Camera_WorldToViewportPoint_t>(resolveFn({
      .name       = "Camera::WorldToViewportPoint",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "WorldToViewportPoint",
      .argCount   = 1,
      .staticRva  = RVA::kCamera_WorldToViewportPoint,
    }));

    g_fnComponentGetTransform = reinterpret_cast<Component_GetTransform_t>(resolveFn({
      .name       = "Component::get_transform",
      .namespaze  = kUnityNs,
      .className  = "Component",
      .methodName = "get_transform",
      .argCount   = 0,
      .staticRva  = RVA::kComponent_GetTransform,
    }));

    g_fnTransformGetPosition  = reinterpret_cast<Transform_GetPosition_t>(resolveFn({
      .name       = "Transform::get_position",
      .namespaze  = kUnityNs,
      .className  = "Transform",
      .methodName = "get_position",
      .argCount   = 0,
      .staticRva  = RVA::kTransform_GetPosition,
    }));

    g_offCharacterView        = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_characterView",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_CharacterView,
      .staticOffset = offsets::kPlayerVisuals_CharacterView,
    });

    g_offGameMode             = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_gameMode",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_GameMode,
      .staticOffset = offsets::kPlayerVisuals_GameMode,
    });

    g_offCurrentHitboxSet     = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_currentHitboxSet",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_CurrentHitboxSet,
      .staticOffset = offsets::kPlayerVisuals_CurrentHitboxSet,
    });

    g_offRenderedAlive        = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_renderedAliveState",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_RenderedAliveState,
      .staticOffset = offsets::kPlayerVisuals_RenderedAliveState,
    });

    g_offPlayerName           = Resolver::ResolveFieldOffset({
      .name         = "GameModePlayer::<Name>k__BackingField",
      .namespaze    = kGameModeNs,
      .className    = "GameModePlayer",
      .fieldName    = fields::kGameModePlayer_Name,
      .staticOffset = offsets::kGameModePlayer_Name,
    });

    MH_STATUS status          = MH_CreateHook(
      reinterpret_cast<LPVOID>(presentRes.address), reinterpret_cast<LPVOID>(&HookedPlayerVisualsPresent),
      reinterpret_cast<LPVOID*>(&g_origPresent)
    );
    if (status != MH_OK) {
      Logger::Log("[ESP] MH_CreateHook(PlayerVisuals::Present) failed: %d", static_cast<int>(status));
      return false;
    }

    auto latePresentRes = Resolver::ResolveMethod({
      .name       = "PlayerVisualLatePresentSystem::LatePresent",
      .namespaze  = "Psa.Core.Modules.Player",
      .className  = "PlayerVisualLatePresentSystem",
      .methodName = "LatePresent",
      .argCount   = 2,
      .staticRva  = RVA::kPlayerVisualLatePresent_LatePresent,
    });
    if (latePresentRes) {
      MH_STATUS lpStatus = MH_CreateHook(
        reinterpret_cast<LPVOID>(latePresentRes.address), reinterpret_cast<LPVOID>(&HookedLatePresent),
        reinterpret_cast<LPVOID*>(&g_origLatePresent)
      );
      if (lpStatus != MH_OK) {
        Logger::Log("[ESP] Warning: MH_CreateHook(LatePresent) failed: %d", static_cast<int>(lpStatus));
      }
    }

    Logger::Log("[ESP] Shared visual hooks initialized.");
    return true;
  }

  void Shutdown()
  {
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    g_stagingSnapshots.clear();
    g_publishedSnapshots.clear();
    g_renderSnapshots.clear();
    g_trackedVisuals.clear();
    g_cachedGameplayCam = nullptr;
  }

  void GetFrameSnapshots(std::vector<PlayerSnapshot>& outSnapshots)
  {
    const ULONGLONG             nowMs = GetTickCount64();
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    if (g_publishedSnapshots.empty() || (nowMs - g_publishedMs) > 150) {
      outSnapshots.clear();
      return;
    }
    outSnapshots = g_publishedSnapshots;
  }

  void RenderOverlay(ImDrawList* drawList, float screenWidth, float screenHeight)
  {
    if (!drawList || (!BoxEsp::IsEnabled() && !NameEsp::IsEnabled())) {
      return;
    }

    GetFrameSnapshots(g_renderSnapshots);
    if (g_renderSnapshots.empty()) {
      return;
    }

    BoxEsp::Render(drawList, screenWidth, screenHeight, g_renderSnapshots);
    NameEsp::Render(drawList, screenWidth, screenHeight, g_renderSnapshots);
  }
}  // namespace Visual::EspShared

namespace Visual
{
  void RegisterSettings()
  {
    BodyEsp::RegisterSettings();
    BoxEsp::RegisterSettings();
    NameEsp::RegisterSettings();
    EspShared::RegisterSettings();
  }

  bool Init()
  {
    BodyEsp::Init();
    return EspShared::Init();
  }

  void Shutdown()
  {
    EspShared::Shutdown();
    BodyEsp::Shutdown();
    BoxEsp::Shutdown();
    NameEsp::Shutdown();
  }

  void RenderOverlay(ImDrawList* drawList, float screenWidth, float screenHeight)
  {
    EspShared::RenderOverlay(drawList, screenWidth, screenHeight);
  }
}  // namespace Visual
