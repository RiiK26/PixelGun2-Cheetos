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

#include "renderer.h"
#include "../ui/menu.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include "../utils/settings_registry.h"

#include <MinHook.h>
#include <windows.h>
#include <chrono>
#include <string>
#include <thread>

static void PopulateMenuFromRegistry()
{
  Menu::ClearMenuItems();
  for (const auto& group : SettingsRegistry::GetGroups()) {
    Menu::AddMenuItem(Menu::MenuItem::Header(group.menuHeader, group.defaultExpanded));
    for (const auto& entry : group.entries) {
      switch (entry.type) {
      case SettingsRegistry::SettingType::Bool :
        if (auto* const ptr = std::get_if<bool*>(&entry.target); ptr && *ptr) {
          Menu::AddMenuItem(Menu::MenuItem::Toggle(entry.menuLabel, *ptr));
        }
        break;
      case SettingsRegistry::SettingType::Float :
        if (auto* const ptr = std::get_if<float*>(&entry.target); ptr && *ptr) {
          Menu::AddMenuItem(Menu::MenuItem::Slider(entry.menuLabel, *ptr, entry.fMin, entry.fMax, entry.fStep));
        }
        break;
      case SettingsRegistry::SettingType::Int :
        if (auto* const ptr = std::get_if<int*>(&entry.target); ptr && *ptr) {
          Menu::AddMenuItem(Menu::MenuItem::Slider(entry.menuLabel, *ptr, entry.iMin, entry.iMax, entry.iStep));
        }
        break;
      case SettingsRegistry::SettingType::Action :
        if (auto* const fn = std::get_if<SettingsRegistry::ActionFn>(&entry.target); fn && *fn) {
          Menu::AddMenuItem(Menu::MenuItem::Action(entry.menuLabel, *fn));
        }
        break;
      }
    }
  }
}

static DWORD WINAPI MainThread(LPVOID lpParam)
{
  auto* hModule = static_cast<HMODULE>(lpParam);
  Logger::Log("[MAIN] MainThread started.");

  char dllPathBuf[MAX_PATH]{};
  if (GetModuleFileNameA(hModule, dllPathBuf, MAX_PATH) > 0) {
    Config::SetConfigPath(dllPathBuf);
  }

  if (MH_Initialize() != MH_OK) {
    Logger::Log("[MAIN] Failed to initialize MinHook.");
    Logger::Shutdown();
    FreeLibraryAndExitThread(hModule, 1);
    return 1;
  }

  SettingsRegistry::Clear();
  auto& configGroup = SettingsRegistry::AddGroup("config", "-- CONFIG --", true);
  configGroup.AddAction("Save Config", Config::Save);
  configGroup.AddAction("Load Config", Config::Load);

  Config::Load();
  PopulateMenuFromRegistry();

  if (!RendererHook::Init()) {
    Logger::Log("[MAIN] Failed to initialize RendererHook.");
    MH_Uninitialize();
    Logger::Shutdown();
    FreeLibraryAndExitThread(hModule, 1);
    return 1;
  }

  MH_EnableHook(MH_ALL_HOOKS);
  Logger::Log("[MAIN] initialized. Press END to unload.");

  // Wait for unload key (END)
  while (true) {
    if (GetAsyncKeyState(VK_END) & 1)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  Logger::Log("[MAIN] Unloading...");
  Config::Save();
  RendererHook::Shutdown();
  Menu::Shutdown();
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();
  Logger::Shutdown();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  FreeLibraryAndExitThread(hModule, 0);
  return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/)
{
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    Logger::Init(hModule);
    Logger::Log("[MAIN] DLL_PROCESS_ATTACH received.");
    if (HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr)) {
      CloseHandle(hThread);
    }
  }
  return TRUE;
}
