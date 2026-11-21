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

struct ImDrawList;

namespace RendererHook
{
  using OverlayCallback = void (*)(ImDrawList* drawList, float screenWidth, float screenHeight);

  // Detects available Direct3D 11 / Direct3D 12 runtimes, creates dummy swapchains to extract
  // VTable pointers for Present/Present1/ResizeBuffers/ExecuteCommandLists, and detours them.
  bool Init();

  // Releases all Direct3D 11 / Direct3D 12 ImGui backends and graphics resources.
  void Shutdown();

  // Returns true if renderer Present detours were successfully installed.
  bool IsHookInstalled();

  // Registers an optional callback invoked every frame on the background draw list.
  void SetOverlayCallback(OverlayCallback callback);
}  // namespace RendererHook
