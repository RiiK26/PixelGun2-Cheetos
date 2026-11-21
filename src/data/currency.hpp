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

#include <string>
#include <vector>

enum class CurrencyEnum : int
{
  None = 0,
  Hard = 7000001,  // Shards
  Soft = 7000002,  // Coins
};

enum class ResourceEnum : int
{
  None = 0,
  Xp   = 22000001,  // XP
};

static inline std::vector<std::wstring> currency_names = {
  L"Coins", L"Shards", L"Cores", L"Pass Points", L"XP",
};
