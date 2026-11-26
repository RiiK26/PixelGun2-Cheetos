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
  using MatchPresentation_TryGetGameModePlayerData_t =
    bool (*)(void* self, NetworkId entityRef, void** outPlayerStanding);
  using PlayerStanding_GetName_t                                        = const void* (*)(void* self);
  using HealthComponent_GetState_t                                      = void* (*)(void* self);
  using HealthState_GetPercentage_t                                     = float (*)(void* healthState);
  using Camera_GetMain_t                                                = void* (*)();
  using Camera_GetAllCameras_t                                          = void* (*)();
  using Camera_GetOrthographic_t                                        = bool (*)(void* camera);
  using Camera_GetDepth_t                                               = float (*)(void* camera);
  using Camera_WorldToViewportPoint_t                                   = Vector3 (*)(void* camera, Vector3 position);
  using Component_GetTransform_t                                        = void* (*)(void* component);
  using Transform_GetPosition_t                                         = Vector3 (*)(void* transform);

  static PlayerVisuals_Present_t                      g_origPresent     = nullptr;
  static PlayerVisualLatePresent_LatePresent_t        g_origLatePresent = nullptr;
  static PlayerVisuals_GetIsFpsCharacter_t            g_fnGetIsFpsCharacter       = nullptr;
  static PlayerVisuals_GetPlayerTeamRelation_t        g_fnGetPlayerTeamRelation   = nullptr;
  static EcsComponentBeh_GetId_t                      g_fnGetId                   = nullptr;
  static MatchPresentation_TryGetGameModePlayerData_t g_fnTryGetPlayerData        = nullptr;
  static PlayerStanding_GetName_t                     g_fnPlayerStandingGetName   = nullptr;
  static HealthComponent_GetState_t                   g_fnHealthComponentGetState = nullptr;
  static HealthState_GetPercentage_t                  g_fnHealthStateGetPct       = nullptr;
  static Camera_GetMain_t                             g_fnCameraGetMain           = nullptr;
  static Camera_GetAllCameras_t                       g_fnCameraGetAll            = nullptr;
  static Camera_GetOrthographic_t                     g_fnCameraGetOrthographic   = nullptr;
  static Camera_GetDepth_t                            g_fnCameraGetDepth          = nullptr;
  static Camera_WorldToViewportPoint_t                g_fnWorldToViewportPoint    = nullptr;
  static Component_GetTransform_t                     g_fnComponentGetTransform   = nullptr;
  static Transform_GetPosition_t                      g_fnTransformGetPosition    = nullptr;

  static int32_t                                      g_offCharacterView    = offsets::kPlayerVisuals_CharacterView;
  static int32_t                                      g_offMatch            = offsets::kPlayerVisuals_Match;
  static int32_t                                      g_offHealthComponent  = offsets::kPlayerVisuals_HealthComponent;
  static int32_t                                      g_offCurrentHitboxSet = offsets::kPlayerVisuals_CurrentHitboxSet;
  static int32_t                                      g_offRenderedAlive = offsets::kPlayerVisuals_RenderedAliveState;
  static int32_t                                      g_offHeadHitbox    = offsets::kHitboxSet_AimAssistHeadHitbox;
  static int32_t    g_offPlayerName = offsets::kPlayerStanding_Profile + offsets::kParticipantProfile_Name;

  // Double-buffered snapshots: LatePresent builds into g_stagingSnapshots (lock-free),
  // swaps once into g_publishedSnapshots; RenderOverlay copies once into g_renderSnapshots.
  static std::mutex g_snapshotMutex;
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
    bool           overlayOn,
    bool           nameOn,
    bool           healthOn,
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

    if ((!bodyOn && !overlayOn) || !isAlive) {
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

    if (!overlayOn || !camera || !g_fnWorldToViewportPoint || !g_fnComponentGetTransform || !g_fnTransformGetPosition) {
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

    Vector3 feetWorld       = g_fnTransformGetPosition(feetTransform);
    Vector3 headWorld       = {feetWorld.x, feetWorld.y + 2.1f, feetWorld.z};
    Vector3 headCenterWorld = {feetWorld.x, feetWorld.y + 1.75f, feetWorld.z};

    if (g_offCurrentHitboxSet > 0 && g_offHeadHitbox > 0) {
      void* hitboxSet = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(self) + g_offCurrentHitboxSet);
      if (IsValidUnityObject(hitboxSet)) {
        void* headHitbox = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(hitboxSet) + g_offHeadHitbox);
        if (IsValidUnityObject(headHitbox)) {
          void* headTransform = g_fnComponentGetTransform(headHitbox);
          if (IsValidUnityObject(headTransform)) {
            Vector3 h = g_fnTransformGetPosition(headTransform);
            if (h.y > feetWorld.y + 1.2f) {
              headCenterWorld = h;
              headWorld       = {h.x, h.y + 0.35f, h.z};
            }
          }
        }
      }
    }

    Vector3 feetVp       = g_fnWorldToViewportPoint(camera, feetWorld);
    Vector3 headVp       = g_fnWorldToViewportPoint(camera, headWorld);
    Vector3 headCenterVp = g_fnWorldToViewportPoint(camera, headCenterWorld);

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
    snap.feetVx  = feetVp.x;
    snap.feetVy  = feetVp.y;
    snap.headVx  = (headCenterVp.z > 0.01f) ? headCenterVp.x : headVp.x;
    snap.headVy  = (headCenterVp.z > 0.01f) ? headCenterVp.y : headVp.y;
    snap.isEnemy = isEnemy;

    if (healthOn && g_offHealthComponent > 0 && g_fnHealthComponentGetState) {
      void* healthComp = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(self) + g_offHealthComponent);
      if (IsValidUnityObject(healthComp)) {
        // Guard against Photon Fusion throwing InvalidOperationException when NetworkBehaviour::Ptr is null
        void* netPtr =
          *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(healthComp) + offsets::kNetworkBehaviour_Ptr);
        void* statePtr = netPtr ? g_fnHealthComponentGetState(healthComp) : nullptr;
        if (statePtr) {
          float pct = -1.0f;
          if (g_fnHealthStateGetPct) {
            pct = g_fnHealthStateGetPct(statePtr);
          }
          else {
            const auto*   sb    = static_cast<const uint8_t*>(statePtr);
            const int32_t curHp = *reinterpret_cast<const int32_t*>(sb + offsets::kHealthState_Health);
            const int32_t maxHp = *reinterpret_cast<const int32_t*>(sb + offsets::kHealthState_MaxHealth);
            if (maxHp > 0) {
              pct = static_cast<float>(curHp) / static_cast<float>(maxHp);
            }
          }
          if (pct >= 0.0f && std::isfinite(pct)) {
            snap.healthPct = std::clamp(pct, 0.0f, 1.0f);
          }
        }
      }
    }

    if (nameOn) {
      if (
        !tracked.hasName && (nowMs - tracked.lastNameQueryMs) >= 1000 && g_fnGetId && g_fnTryGetPlayerData
        && g_offMatch > 0 && (g_fnPlayerStandingGetName || g_offPlayerName > 0)
      ) {
        tracked.lastNameQueryMs = nowMs;
        void* match             = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(self) + g_offMatch);
        if (match) {
          NetworkId id             = g_fnGetId(self);
          void*     playerStanding = nullptr;
          if (g_fnTryGetPlayerData(match, id, &playerStanding) && playerStanding) {
            const void* nameStr =
              g_fnPlayerStandingGetName
                ? g_fnPlayerStandingGetName(playerStanding)
                : *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(playerStanding) + g_offPlayerName);
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

    const bool      bodyOn     = BodyEsp::IsEnabled();
    const bool      boxOn      = BoxEsp::IsEnabled();
    const bool      nameOn     = NameEsp::IsEnabled();
    const bool      healthOn   = HealthEsp::IsEnabled();
    const bool      snaplineOn = SnaplineEsp::IsEnabled();
    const bool      headDotOn  = HeadDotEsp::IsEnabled();
    const bool      overlayOn  = boxOn || nameOn || healthOn || snaplineOn || headDotOn;

    const ULONGLONG nowMs      = GetTickCount64();
    void*           camera     = overlayOn ? FindGameplayCamera(nowMs) : nullptr;

    g_stagingSnapshots.clear();

    for (auto it = g_trackedVisuals.begin(); it != g_trackedVisuals.end();) {
      auto* pv = reinterpret_cast<void*>(it->first);
      if ((nowMs - it->second.lastSeenMs) > 500 || !IsValidUnityObject(pv)) {
        BodyEsp::OnPlayerPruned(it->first);
        it = g_trackedVisuals.erase(it);
        continue;
      }

      UpdatePlayerEsp(pv, it->first, it->second, camera, bodyOn, overlayOn, nameOn, healthOn, nowMs);
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
      .aobPattern = signatures::kPlayerVisuals_Present,
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

    g_fnGetIsFpsCharacter       = reinterpret_cast<PlayerVisuals_GetIsFpsCharacter_t>(resolveFn({
      .name       = "PlayerVisuals::get_IsFpsCharacter",
      .aobPattern = signatures::kPlayerVisuals_GetIsFpsCharacter,
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "get_IsFpsCharacter",
      .argCount   = 0,
      .staticRva  = RVA::kPlayerVisuals_GetIsFpsCharacter,
    }));

    g_fnGetPlayerTeamRelation   = reinterpret_cast<PlayerVisuals_GetPlayerTeamRelation_t>(resolveFn({
      .name       = "PlayerVisuals::get_PlayerTeamRelation",
      .aobPattern = signatures::kPlayerVisuals_GetPlayerTeamRelation,
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "get_PlayerTeamRelation",
      .argCount   = 0,
      .staticRva  = RVA::kPlayerVisuals_GetPlayerTeamRelation,
    }));

    g_fnGetId                   = reinterpret_cast<EcsComponentBeh_GetId_t>(resolveFn({
      .name       = "EcsComponentBeh::get_Id",
      .namespaze  = "Psa.Core.Modules.Base.Ecs.Entities.Implementation",
      .className  = "EcsComponentBeh",
      .methodName = "get_Id",
      .argCount   = 0,
      .staticRva  = RVA::kEcsComponentBeh_GetId,
    }));

    // Note: MatchPresentation has two 2-arg overloads (PlayerId vs EntityRef); skip Tier 2 name-only lookup
    // so we never bind the PlayerId overload by accident.
    g_fnTryGetPlayerData        = reinterpret_cast<MatchPresentation_TryGetGameModePlayerData_t>(resolveFn({
      .name       = "MatchPresentation::TryGetGameModePlayerData(EntityRef)",
      .aobPattern = signatures::kMatchPresentation_TryGetGameModePlayerData,
      .staticRva  = RVA::kMatchPresentation_TryGetGameModePlayerData,
    }));

    g_fnPlayerStandingGetName   = reinterpret_cast<PlayerStanding_GetName_t>(resolveFn({
      .name       = "PlayerStanding::get_Name",
      .aobPattern = signatures::kPlayerStanding_GetName,
      .namespaze  = kGameModeNs,
      .className  = "PlayerStanding",
      .methodName = "get_Name",
      .argCount   = 0,
      .staticRva  = RVA::kPlayerStanding_GetName,
    }));

    g_fnHealthComponentGetState = reinterpret_cast<HealthComponent_GetState_t>(resolveFn({
      .name       = "HealthComponent::get_State",
      .namespaze  = "Psa.Core.Modules.Health.Components.Implementation",
      .className  = "HealthComponent",
      .methodName = "get_State",
      .argCount   = 0,
      .staticRva  = RVA::kHealthComponent_GetState,
    }));

    g_fnHealthStateGetPct       = reinterpret_cast<HealthState_GetPercentage_t>(resolveFn({
      .name       = "HealthState::get_Percentage",
      .aobPattern = signatures::kHealthState_GetPercentage,
      .staticRva  = RVA::kHealthState_GetPercentage,
    }));

    g_fnCameraGetMain           = reinterpret_cast<Camera_GetMain_t>(resolveFn({
      .name       = "Camera::get_main",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_main",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetMain,
    }));

    g_fnCameraGetAll            = reinterpret_cast<Camera_GetAllCameras_t>(resolveFn({
      .name       = "Camera::get_allCameras",
      .aobPattern = signatures::kCamera_GetAllCameras,
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_allCameras",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetAllCameras,
    }));

    g_fnCameraGetOrthographic   = reinterpret_cast<Camera_GetOrthographic_t>(resolveFn({
      .name       = "Camera::get_orthographic",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_orthographic",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetOrthographic,
    }));

    g_fnCameraGetDepth          = reinterpret_cast<Camera_GetDepth_t>(resolveFn({
      .name       = "Camera::get_depth",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "get_depth",
      .argCount   = 0,
      .staticRva  = RVA::kCamera_GetDepth,
    }));

    g_fnWorldToViewportPoint    = reinterpret_cast<Camera_WorldToViewportPoint_t>(resolveFn({
      .name       = "Camera::WorldToViewportPoint",
      .namespaze  = kUnityNs,
      .className  = "Camera",
      .methodName = "WorldToViewportPoint",
      .argCount   = 1,
      .staticRva  = RVA::kCamera_WorldToViewportPoint,
    }));

    g_fnComponentGetTransform   = reinterpret_cast<Component_GetTransform_t>(resolveFn({
      .name       = "Component::get_transform",
      .namespaze  = kUnityNs,
      .className  = "Component",
      .methodName = "get_transform",
      .argCount   = 0,
      .staticRva  = RVA::kComponent_GetTransform,
    }));

    g_fnTransformGetPosition    = reinterpret_cast<Transform_GetPosition_t>(resolveFn({
      .name       = "Transform::get_position",
      .namespaze  = kUnityNs,
      .className  = "Transform",
      .methodName = "get_position",
      .argCount   = 0,
      .staticRva  = RVA::kTransform_GetPosition,
    }));

    g_offCharacterView          = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_characterView",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_CharacterView,
      .staticOffset = offsets::kPlayerVisuals_CharacterView,
    });

    g_offMatch                  = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_match",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_Match,
      .staticOffset = offsets::kPlayerVisuals_Match,
    });

    g_offHealthComponent        = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_healthComponent",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_HealthComponent,
      .staticOffset = offsets::kPlayerVisuals_HealthComponent,
    });

    g_offCurrentHitboxSet       = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_currentHitboxSet",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_CurrentHitboxSet,
      .staticOffset = offsets::kPlayerVisuals_CurrentHitboxSet,
    });

    g_offRenderedAlive          = Resolver::ResolveFieldOffset({
      .name         = "PlayerVisuals::_renderedAliveState",
      .namespaze    = kPlayerNs,
      .className    = "PlayerVisuals",
      .fieldName    = fields::kPlayerVisuals_RenderedAliveState,
      .staticOffset = offsets::kPlayerVisuals_RenderedAliveState,
    });

    int32_t offProfile          = Resolver::ResolveFieldOffset({
      .name         = "PlayerStanding::<Profile>k__BackingField",
      .namespaze    = kGameModeNs,
      .className    = "PlayerStanding",
      .fieldName    = fields::kPlayerStanding_Profile,
      .staticOffset = offsets::kPlayerStanding_Profile,
    });
    g_offPlayerName  = (offProfile >= 0) ? (offProfile + offsets::kParticipantProfile_Name)
                                         : (offsets::kPlayerStanding_Profile + offsets::kParticipantProfile_Name);

    MH_STATUS status = MH_CreateHook(
      reinterpret_cast<LPVOID>(presentRes.address), reinterpret_cast<LPVOID>(&HookedPlayerVisualsPresent),
      reinterpret_cast<LPVOID*>(&g_origPresent)
    );
    if (status != MH_OK) {
      Logger::Log("[ESP] MH_CreateHook(PlayerVisuals::Present) failed: %d", static_cast<int>(status));
      return false;
    }

    auto latePresentRes = Resolver::ResolveMethod({
      .name       = "PlayerVisualLatePresentSystem::LatePresent",
      .aobPattern = signatures::kPlayerVisualLatePresent_LatePresent,
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
    const bool anyOverlay = BoxEsp::IsEnabled() || NameEsp::IsEnabled() || HealthEsp::IsEnabled()
                         || SnaplineEsp::IsEnabled() || HeadDotEsp::IsEnabled();
    if (!drawList || !anyOverlay) {
      return;
    }

    GetFrameSnapshots(g_renderSnapshots);
    if (g_renderSnapshots.empty()) {
      return;
    }

    SnaplineEsp::Render(drawList, screenWidth, screenHeight, g_renderSnapshots);
    BoxEsp::Render(drawList, screenWidth, screenHeight, g_renderSnapshots);
    HealthEsp::Render(drawList, screenWidth, screenHeight, g_renderSnapshots);
    HeadDotEsp::Render(drawList, screenWidth, screenHeight, g_renderSnapshots);
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
    HealthEsp::RegisterSettings();
    SnaplineEsp::RegisterSettings();
    HeadDotEsp::RegisterSettings();
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
    HealthEsp::Shutdown();
    SnaplineEsp::Shutdown();
    HeadDotEsp::Shutdown();
  }

  void RenderOverlay(ImDrawList* drawList, float screenWidth, float screenHeight)
  {
    EspShared::RenderOverlay(drawList, screenWidth, screenHeight);
  }
}  // namespace Visual
