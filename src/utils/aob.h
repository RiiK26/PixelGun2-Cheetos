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
#include <cstddef>
#include <cstdint>

namespace AOB
{
  // Scans an explicit memory range for an IDA-style byte pattern (e.g. "48 89 5C 24 ? 57").
  // Returns the matched address + offset, or 0 if not found.
  uintptr_t ScanRange(uintptr_t start, size_t size, const char* pattern, int32_t offset = 0);

  // Scans a loaded module's executable (.text) section (falling back to full image) for a pattern.
  // Returns the matched address + offset, or 0 if not found.
  uintptr_t ScanModule(HMODULE hModule, const char* pattern, int32_t offset = 0);
}  // namespace AOB
