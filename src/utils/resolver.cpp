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

#include "resolver.h"
#include "aob.h"
#include "il2cpp.h"
#include "logger.h"

namespace Resolver
{
  const char* TierToString(Tier tier)
  {
    switch (tier) {
    case Tier::AOB :
      return "Tier 1 (AOB)";
    case Tier::IL2CPP :
      return "Tier 2 (IL2CPP)";
    case Tier::StaticRVA :
      return "Tier 3 (Static RVA)";
    default :
      return "None";
    }
  }

  ResolveResult ResolveMethod(const MethodSpec& spec)
  {
    const char* label = spec.name ? spec.name : (spec.methodName ? spec.methodName : "<unnamed>");
    HMODULE     mod   = IL2CPP::GetGameAssembly();

    // Tier 1: AOB pattern scan
    if (mod && spec.aobPattern && *spec.aobPattern) {
      if (uintptr_t addr = AOB::ScanModule(mod, spec.aobPattern, spec.aobOffset)) {
        Logger::Log(
          "[RESOLVER] %s -> 0x%llX via %s", label, static_cast<unsigned long long>(addr), TierToString(Tier::AOB)
        );
        return {addr, Tier::AOB};
      }
    }

    // Tier 2: IL2CPP runtime metadata lookup
    if (IL2CPP::IsInitialized() && spec.className && spec.methodName) {
      if (
        uintptr_t addr =
          IL2CPP::FindMethod(spec.assemblyName, spec.namespaze, spec.className, spec.methodName, spec.argCount)
      ) {
        Logger::Log(
          "[RESOLVER] %s -> 0x%llX via %s", label, static_cast<unsigned long long>(addr), TierToString(Tier::IL2CPP)
        );
        return {addr, Tier::IL2CPP};
      }
    }

    // Tier 3: Static RVA fallback
    if (mod && spec.staticRva != 0) {
      uintptr_t addr = reinterpret_cast<uintptr_t>(mod) + spec.staticRva;
      Logger::Log(
        "[RESOLVER] %s -> 0x%llX via %s", label, static_cast<unsigned long long>(addr), TierToString(Tier::StaticRVA)
      );
      return {addr, Tier::StaticRVA};
    }

    Logger::Log("[RESOLVER] FAILED to resolve %s across all tiers.", label);
    return {0, Tier::None};
  }

  int32_t ResolveFieldOffset(const FieldSpec& spec)
  {
    const char* label = spec.name ? spec.name : (spec.fieldName ? spec.fieldName : "<unnamed>");

    // Tier 2: IL2CPP runtime field offset lookup
    if (IL2CPP::IsInitialized() && spec.className && spec.fieldName) {
      int32_t offset = IL2CPP::FindFieldOffset(spec.assemblyName, spec.namespaze, spec.className, spec.fieldName);
      if (offset >= 0) {
        Logger::Log("[RESOLVER] Field %s -> +0x%X via %s", label, offset, TierToString(Tier::IL2CPP));
        return offset;
      }
    }

    // Tier 3: Static offset fallback
    if (spec.staticOffset >= 0) {
      Logger::Log("[RESOLVER] Field %s -> +0x%X via %s", label, spec.staticOffset, TierToString(Tier::StaticRVA));
      return spec.staticOffset;
    }

    Logger::Log("[RESOLVER] FAILED to resolve field %s.", label);
    return -1;
  }
}  // namespace Resolver
