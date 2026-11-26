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

#include "aob.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace AOB
{
  static bool ParsePattern(const char* pattern, std::vector<int>& outBytes)
  {
    outBytes.clear();
    if (!pattern || !*pattern)
      return false;

    const char* p = pattern;
    while (*p) {
      while (*p == ' ' || *p == '\t')
        ++p;
      if (!*p)
        break;

      if (*p == '?') {
        outBytes.push_back(-1);
        ++p;
        if (*p == '?')
          ++p;
      }
      else {
        char*         endPtr = nullptr;
        unsigned long val    = std::strtoul(p, &endPtr, 16);
        if (endPtr == p || val > 0xFF)
          return false;
        outBytes.push_back(static_cast<int>(val));
        p = endPtr;
      }
    }
    return !outBytes.empty();
  }

  uintptr_t ScanRange(uintptr_t start, size_t size, const char* pattern, int32_t offset)
  {
    if (!start || size == 0)
      return 0;

    std::vector<int> bytes;
    if (!ParsePattern(pattern, bytes))
      return 0;

    const size_t patSize = bytes.size();
    if (patSize > size)
      return 0;

    const auto*  data    = reinterpret_cast<const uint8_t*>(start);
    const size_t scanEnd = size - patSize;
    const int    first   = bytes[0];

    for (size_t i = 0; i <= scanEnd; ++i) {
      if (first != -1 && data[i] != static_cast<uint8_t>(first))
        continue;

      bool matched = true;
      for (size_t j = 1; j < patSize; ++j) {
        if (bytes[j] != -1 && data[i + j] != static_cast<uint8_t>(bytes[j])) {
          matched = false;
          break;
        }
      }

      if (matched) {
        return static_cast<uintptr_t>(static_cast<intptr_t>(start + i) + offset);
      }
    }

    return 0;
  }

  uintptr_t ScanModule(HMODULE hModule, const char* pattern, int32_t offset)
  {
    if (!hModule || !pattern || !*pattern)
      return 0;

    const auto  base   = reinterpret_cast<uintptr_t>(hModule);
    const auto* dosHdr = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE)
      return 0;

    const auto* ntHdr = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHdr->e_lfanew);
    if (ntHdr->Signature != IMAGE_NT_SIGNATURE)
      return 0;

    // Scan all executable sections (e.g. .text, il2cpp) for speed and accuracy
    const auto* section       = IMAGE_FIRST_SECTION(ntHdr);
    bool        scannedAnySec = false;
    for (WORD i = 0; i < ntHdr->FileHeader.NumberOfSections; ++i, ++section) {
      if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
        scannedAnySec        = true;
        const size_t secSize = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData;
        if (uintptr_t hit = ScanRange(base + section->VirtualAddress, secSize, pattern, offset)) {
          return hit;
        }
      }
    }

    if (scannedAnySec) {
      return 0;
    }

    return ScanRange(base, ntHdr->OptionalHeader.SizeOfImage, pattern, offset);
  }
}  // namespace AOB
