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

enum class LoadoutCategory : int
{
  None    = 0,
  Primary = 1,
  Backup  = 2,
  Melee   = 3,
  Armor   = 4,
  Gadget  = 5,
};

struct LoadoutData
{
  LoadoutCategory category;
  std::wstring    id;
};

static inline std::vector<LoadoutData> loadout_names = {
  // Primary
  {LoadoutCategory::Primary, L"ASSAULT RIFLE"      },
  {LoadoutCategory::Primary, L"BURST RIFLE"        },
  {LoadoutCategory::Primary, L"SNIPER RIFLE"       },
  {LoadoutCategory::Primary, L"ANTI-MATERIAL RIFLE"},
  {LoadoutCategory::Primary, L"COMBAT SHOTGUN"     },
  {LoadoutCategory::Primary, L"MARKSMAN RIFLE"     },
  {LoadoutCategory::Primary, L"LIGHT MACHINE GUN"  },
  {LoadoutCategory::Primary, L"MINIGUN"            },
  {LoadoutCategory::Primary, L"SMG"                },
  {LoadoutCategory::Primary, L"ROCKET LAUNCHER"    },
  {LoadoutCategory::Primary, L"FLAMETHROWER"       },
  {LoadoutCategory::Primary, L"MAGIC BOOK"         },
  {LoadoutCategory::Primary, L"SECRET FORMULA"     },

  // Backup
  {LoadoutCategory::Backup,  L"COMBAT PISTOL"      },
  {LoadoutCategory::Backup,  L"COMPACT SMG"        },
  {LoadoutCategory::Backup,  L"SHORTY"             },
  {LoadoutCategory::Backup,  L"DESERT EAGLE"       },

  // Melee
  {LoadoutCategory::Melee,   L"KNIFE"              },
  {LoadoutCategory::Melee,   L"ENERGY HAMMER"      },
  {LoadoutCategory::Melee,   L"FIRE DEMON"         },
  {LoadoutCategory::Melee,   L"MAGIC SPEAR"        },
  {LoadoutCategory::Melee,   L"SCREAMING SAW"      },

  // Armor
  {LoadoutCategory::Armor,   L"BASIC ARMOR"        },
  {LoadoutCategory::Armor,   L"LIGHT ARMOR"        },
  {LoadoutCategory::Armor,   L"MEDIUM ARMOR"       },
  {LoadoutCategory::Armor,   L"HEAVY ARMOR"        },
  {LoadoutCategory::Armor,   L"TACTICAL ARMOR"     },
  {LoadoutCategory::Armor,   L"LIGHTWEIGHT ARMOR"  },
  {LoadoutCategory::Armor,   L"HEADHUNTER ARMOR"   },
  {LoadoutCategory::Armor,   L"ENGINEER ARMOR"     },
  {LoadoutCategory::Armor,   L"CHEM PROOF ARMOR"   },
  {LoadoutCategory::Armor,   L"FULL BODY ARMOR"    },
  {LoadoutCategory::Armor,   L"DESTROYER ARMOR"    },
  {LoadoutCategory::Armor,   L"STEALTH ARMOR"      },

  // Gadgets
  {LoadoutCategory::Gadget,  L"GRENADE"            },
  {LoadoutCategory::Gadget,  L"MINE"               },
  {LoadoutCategory::Gadget,  L"SHURIKENS"          },
  {LoadoutCategory::Gadget,  L"SHIELD BRACELET"    },
  {LoadoutCategory::Gadget,  L"STEALTH BRACELET"   },
  {LoadoutCategory::Gadget,  L"GRAVITY GRENADE"    },
  {LoadoutCategory::Gadget,  L"SLOWDOWN GRENADE"   },
  {LoadoutCategory::Gadget,  L"NUKE GRENADE"       },
  {LoadoutCategory::Gadget,  L"DRAGON'S BREATH"    },
  {LoadoutCategory::Gadget,  L"XRAY GLASSES"       },
  {LoadoutCategory::Gadget,  L"ELECTRIC FIELD"     },
  {LoadoutCategory::Gadget,  L"HOMING ROCKET"      },
  {LoadoutCategory::Gadget,  L"INFINITE AMMO"      },
  {LoadoutCategory::Gadget,  L"REARM KIT"          },
  {LoadoutCategory::Gadget,  L"ROCKET MECH"        },
};

static inline std::vector<std::wstring> loadout_tier_list = {
  L"COMMON", L"RARE", L"EPIC", L"LEGENDARY", L"MYTHIC",
};
