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
#include <cstdint>
#include <unordered_map>

namespace Visual::BodyEsp
{
  enum class TypeVisualEffects : int32_t
  {
    None            = 0,
    EnemyOutline    = 1,
    TeammateOutline = 2,
    Invisible       = 4,
    XrayOutline     = 8,
    XrayFill        = 16,
    XrayTeammate    = 32,
    XrayEnemy       = 64,
    GlowTeammate    = 128,
    GlowEnemy       = 256,
    XrayFillRed     = 512,
    Invincibility   = 1024,
  };

  using PlayerVisuals_ApplyVisualEffect_t                           = void (*)(void* self, TypeVisualEffects effect);
  using PlayerVisuals_RemoveVisualEffect_t                          = void (*)(void* self, TypeVisualEffects effect);

  static bool                                             g_enabled = false;  // esp body enable off by default
  static PlayerVisuals_ApplyVisualEffect_t                g_fnApplyVisualEffect    = nullptr;
  static PlayerVisuals_RemoveVisualEffect_t               g_fnRemoveVisualEffect   = nullptr;
  static PlayerVisuals_RemoveVisualEffect_t               g_origRemoveVisualEffect = nullptr;
  static std::unordered_map<uintptr_t, TypeVisualEffects> g_appliedXray;
  static bool                                             g_inInternalEffectChange = false;

  static void                                             ClearBodyEffectIfApplied(void* playerVisuals, uintptr_t key)
  {
    auto it = g_appliedXray.find(key);
    if (it == g_appliedXray.end()) {
      return;
    }

    auto removeFn = g_origRemoveVisualEffect ? g_origRemoveVisualEffect : g_fnRemoveVisualEffect;
    if (removeFn && it->second != TypeVisualEffects::None) {
      g_inInternalEffectChange = true;
      removeFn(playerVisuals, it->second);
      g_inInternalEffectChange = false;
    }
    g_appliedXray.erase(it);
  }

  static void HookedRemoveVisualEffect(void* self, TypeVisualEffects effect)
  {
    if (g_origRemoveVisualEffect) {
      g_origRemoveVisualEffect(self, effect);
    }

    if (
      g_inInternalEffectChange || !g_enabled || !self || !g_fnApplyVisualEffect || !g_origRemoveVisualEffect
      || effect != TypeVisualEffects::Invincibility
    ) {
      return;
    }

    auto it = g_appliedXray.find(reinterpret_cast<uintptr_t>(self));
    if (it == g_appliedXray.end() || it->second == TypeVisualEffects::None) {
      return;
    }

    // Only re-apply X-Ray when spawn Invincibility ends (never during death/kill-cam teardown)
    g_inInternalEffectChange = true;
    g_origRemoveVisualEffect(self, it->second);
    g_fnApplyVisualEffect(self, it->second);
    g_inInternalEffectChange = false;
  }

  void RegisterSettings()
  {
    auto& group = SettingsRegistry::AddGroup("visual", "-- VISUAL --", true);
    group.AddBool("esp_body", "ESP Body", &g_enabled);
  }

  bool Init()
  {
    constexpr const char* kPlayerNs = "Psa.Core.Modules.Player.Components.Implementation";

    auto                  applyRes  = Resolver::ResolveMethod({
      .name       = "PlayerVisuals::ApplyVisualEffect",
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "ApplyVisualEffect",
      .argCount   = 1,
      .staticRva  = RVA::kPlayerVisuals_ApplyVisualEffect,
    });

    auto                  removeRes = Resolver::ResolveMethod({
      .name       = "PlayerVisuals::RemoveVisualEffect",
      .namespaze  = kPlayerNs,
      .className  = "PlayerVisuals",
      .methodName = "RemoveVisualEffect",
      .argCount   = 1,
      .staticRva  = RVA::kPlayerVisuals_RemoveVisualEffect,
    });

    g_fnApplyVisualEffect           = reinterpret_cast<PlayerVisuals_ApplyVisualEffect_t>(applyRes.address);
    g_fnRemoveVisualEffect          = reinterpret_cast<PlayerVisuals_RemoveVisualEffect_t>(removeRes.address);

    if (!g_fnApplyVisualEffect || !g_fnRemoveVisualEffect) {
      Logger::Log("[BODY_ESP] Failed to resolve ApplyVisualEffect / RemoveVisualEffect.");
      return false;
    }

    MH_STATUS status = MH_CreateHook(
      reinterpret_cast<LPVOID>(g_fnRemoveVisualEffect), reinterpret_cast<LPVOID>(&HookedRemoveVisualEffect),
      reinterpret_cast<LPVOID*>(&g_origRemoveVisualEffect)
    );
    if (status != MH_OK) {
      Logger::Log("[BODY_ESP] Warning: MH_CreateHook(RemoveVisualEffect) failed: %d", static_cast<int>(status));
    }

    return true;
  }

  void Shutdown()
  {
    g_enabled = false;
    g_appliedXray.clear();
  }

  bool IsEnabled() { return g_enabled; }

  void OnPlayerUpdate(void* playerVisuals, uintptr_t key, bool shouldShow, bool isEnemy)
  {
    auto removeFn = g_origRemoveVisualEffect ? g_origRemoveVisualEffect : g_fnRemoveVisualEffect;
    if (!g_fnApplyVisualEffect || !removeFn) {
      return;
    }

    if (!shouldShow) {
      ClearBodyEffectIfApplied(playerVisuals, key);
      return;
    }

    // XrayFillRed (512) for enemies; XrayTeammate (32) for teammates
    TypeVisualEffects desired = isEnemy ? TypeVisualEffects::XrayFillRed : TypeVisualEffects::XrayTeammate;
    auto              it      = g_appliedXray.find(key);
    if (it != g_appliedXray.end() && it->second == desired) {
      return;
    }

    g_inInternalEffectChange = true;
    if (it != g_appliedXray.end()) {
      removeFn(playerVisuals, it->second);
    }
    g_fnApplyVisualEffect(playerVisuals, desired);
    g_inInternalEffectChange = false;
    g_appliedXray[key]       = desired;
  }

  void OnPlayerPruned(uintptr_t key) { g_appliedXray.erase(key); }
}  // namespace Visual::BodyEsp
