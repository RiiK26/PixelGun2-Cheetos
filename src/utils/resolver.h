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

#include <cstdint>

namespace Resolver
{
  enum class Tier
  {
    None,
    AOB,        // Tier 1 (default): Byte pattern scan
    IL2CPP,     // Tier 2: Runtime IL2CPP metadata lookup
    StaticRVA,  // Tier 3: Static RVA from GameAssembly.dll base
  };

  struct MethodSpec
  {
    const char* name         = nullptr;  // Human-readable label for debug logs
    // Tier 1: AOB
    const char* aobPattern   = nullptr;
    int32_t     aobOffset    = 0;
    // Tier 2: IL2CPP
    const char* assemblyName = nullptr;  // Optional; nullptr searches all assemblies
    const char* namespaze    = nullptr;
    const char* className    = nullptr;
    const char* methodName   = nullptr;
    int         argCount     = -1;
    // Tier 3: Static RVA
    uintptr_t   staticRva    = 0;
  };

  struct FieldSpec
  {
    const char* name         = nullptr;
    const char* assemblyName = nullptr;
    const char* namespaze    = nullptr;
    const char* className    = nullptr;
    const char* fieldName    = nullptr;
    int32_t     staticOffset = -1;
  };

  struct ResolveResult
  {
    uintptr_t address{0};
    Tier      tier{Tier::None};

    explicit  operator bool() const { return address != 0; }
  };

  const char*   TierToString(Tier tier);
  ResolveResult ResolveMethod(const MethodSpec& spec);
  int32_t       ResolveFieldOffset(const FieldSpec& spec);
}  // namespace Resolver
