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

enum class AbilityCardCategory : int
{
  None     = 0,
  Damage   = 1,
  Gadget   = 2,
  Survival = 3,
  Wrecking = 4,
  Movement = 5,
  Special  = 6,
};

struct AbilityCardData
{
  AbilityCardCategory category;
  std::wstring        id;
};

static inline std::vector<std::wstring> ability_card_categories = {
  L"DAMAGE", L"GADGET", L"SURVIVAL", L"WRECKING", L"MOVEMENT", L"SPECIAL",
};

static inline std::vector<AbilityCardData> ability_card_names = {
  // Damage
  {AbilityCardCategory::Damage,   L"DAMAGE BOOST"              },
  {AbilityCardCategory::Damage,   L"BURNING HIT"               },
  {AbilityCardCategory::Damage,   L"DAMAGE CHARGE"             },
  {AbilityCardCategory::Damage,   L"MELEE DAMAGE"              },
  {AbilityCardCategory::Damage,   L"HEADSHOT DAMAGE"           },
  {AbilityCardCategory::Damage,   L"RANGE DAMAGE BOOST"        },
  {AbilityCardCategory::Damage,   L"POISON HIT"                },
  {AbilityCardCategory::Damage,   L"BLEEDING HIT"              },

  // Gadget
  {AbilityCardCategory::Gadget,   L"COOLDOWN REDUCTION"        },
  {AbilityCardCategory::Gadget,   L"RECHARGE ULTIMATE"         },
  {AbilityCardCategory::Gadget,   L"RECHARGE GADGET"           },
  {AbilityCardCategory::Gadget,   L"QUICK GADGET"              },
  {AbilityCardCategory::Gadget,   L"QUICK ULTIMATE"            },

  // Survival
  {AbilityCardCategory::Survival, L"MAX HEALTH"                },
  {AbilityCardCategory::Survival, L"LEECH"                     },
  {AbilityCardCategory::Survival, L"VAMPIRISM"                 },
  {AbilityCardCategory::Survival, L"HEALTH REGEN"              },
  {AbilityCardCategory::Survival, L"HEAL ON KILL"              },
  {AbilityCardCategory::Survival, L"MAX HP ON KILL"            },
  {AbilityCardCategory::Survival, L"ASSIST HEAL"               },
  {AbilityCardCategory::Survival, L"HEALTH ENERGIZER"          },

  // Wrecking
  {AbilityCardCategory::Wrecking, L"DESTRUCTION DAMAGE"        },
  {AbilityCardCategory::Wrecking, L"EXPLOSION RADIUS"          },
  {AbilityCardCategory::Wrecking, L"PROGRESS BOOST DESTRUCTION"},

  // Movement
  {AbilityCardCategory::Movement, L"MOVE SPEED"                },
  {AbilityCardCategory::Movement, L"LOW HP SPEED"              },
  {AbilityCardCategory::Movement, L"PICKUP EFFECT: SPEED"      },
  {AbilityCardCategory::Movement, L"SPAWN MOMENTUM"            },

  // Special
  {AbilityCardCategory::Special,  L"AMMO ON KILL"              },
  {AbilityCardCategory::Special,  L"SCORE GAIN BOOST"          },
  {AbilityCardCategory::Special,  L"PROGRESS BOOST ASSIST"     },
  {AbilityCardCategory::Special,  L"PROGRESS BOOST PICKUP"     },
  {AbilityCardCategory::Special,  L"PROGRESS BOOST KILL"       },
  {AbilityCardCategory::Special,  L"PICKUP POWERUP"            },
  {AbilityCardCategory::Special,  L"AMMO ENERGIZER"            },
};
