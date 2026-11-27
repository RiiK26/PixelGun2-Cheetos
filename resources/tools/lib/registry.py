#!/usr/bin/env python3
#
#  Copyright 2026 - RiiK
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

"""Centralized manifest of tracked IL2CPP symbols, fields, and enums.

Add new entries here whenever a feature introduces a new method/field/enum in
src/utils/offsets.h or src/features/ so `doctor` and `autoheal` verify them.
"""

from pathlib import Path
from typing import Dict, List, Optional, Tuple

STEAM_APP_ID = "3456950"
GAME_EXE_NAME = "Pixel Gun 2.exe"
GAME_ASSEMBLY_NAME = "GameAssembly.dll"
METADATA_REL_PATH = Path("Pixel Gun 2_Data/il2cpp_data/Metadata/global-metadata.dat")

REQUIRED_IL2CPP_EXPORTS: List[str] = [
    "il2cpp_domain_get",
    "il2cpp_domain_get_assemblies",
    "il2cpp_assembly_get_image",
    "il2cpp_image_get_name",
    "il2cpp_class_from_name",
    "il2cpp_class_get_method_from_name",
    "il2cpp_class_get_field_from_name",
    "il2cpp_field_get_offset",
]

# const_name -> (namespace, class_name, method_name, arg_count, param_substr_or_None)
TRACKED_METHODS: Dict[str, Tuple[str, str, str, int, Optional[str]]] = {
    "kPlayerVisuals_Present": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "Present",
        0,
        None,
    ),
    "kPlayerVisuals_GetIsFpsCharacter": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "get_IsFpsCharacter",
        0,
        None,
    ),
    "kPlayerVisuals_GetPlayerTeamRelation": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "get_PlayerTeamRelation",
        0,
        None,
    ),
    "kPlayerVisuals_ApplyVisualEffect": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "ApplyVisualEffect",
        1,
        None,
    ),
    "kPlayerVisuals_RemoveVisualEffect": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "RemoveVisualEffect",
        1,
        None,
    ),
    "kPlayerVisualLatePresent_LatePresent": (
        "Psa.Core.Modules.Player",
        "PlayerVisualLatePresentSystem",
        "LatePresent",
        2,
        None,
    ),
    "kEcsComponentBeh_GetId": (
        "Psa.Core.Modules.Base.Ecs.Entities.Implementation",
        "EcsComponentBeh",
        "get_Id",
        0,
        None,
    ),
    "kMatchPresentation_TryGetGameModePlayerData": (
        "Psa.Core.Modules.RoundFlow.GameModes",
        "MatchPresentation",
        "TryGetGameModePlayerData",
        2,
        "EntityRef",
    ),
    "kPlayerStanding_GetName": (
        "Psa.Core.Modules.RoundFlow.GameModes",
        "PlayerStanding",
        "get_Name",
        0,
        None,
    ),
    "kHealthComponent_GetState": (
        "Psa.Core.Modules.Health.Components.Implementation",
        "HealthComponent",
        "get_State",
        0,
        None,
    ),
    "kHealthState_GetPercentage": (
        "",
        "HealthComponent.HealthState",
        "get_Percentage",
        0,
        None,
    ),
    "kCamera_GetMain": ("UnityEngine", "Camera", "get_main", 0, None),
    "kCamera_GetAllCameras": ("UnityEngine", "Camera", "get_allCameras", 0, None),
    "kCamera_GetOrthographic": ("UnityEngine", "Camera", "get_orthographic", 0, None),
    "kCamera_GetDepth": ("UnityEngine", "Camera", "get_depth", 0, None),
    "kCamera_WorldToViewportPoint": (
        "UnityEngine",
        "Camera",
        "WorldToViewportPoint",
        1,
        None,
    ),
    "kComponent_GetTransform": ("UnityEngine", "Component", "get_transform", 0, None),
    "kTransform_GetPosition": ("UnityEngine", "Transform", "get_position", 0, None),
    "kTransform_GetRoot": ("UnityEngine", "Transform", "get_root", 0, None),
    "kPhysics_Linecast": (
        "UnityEngine",
        "Physics",
        "Linecast",
        5,
        "QueryTriggerInteraction",
    ),
    "kRaycastHit_GetCollider": ("UnityEngine", "RaycastHit", "get_collider", 0, None),
}

# offset_const_name -> (namespace, class_name, field_name)
TRACKED_FIELDS: Dict[str, Tuple[str, str, str]] = {
    "kPlayerVisuals_CharacterView": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "_characterView",
    ),
    "kPlayerVisuals_Match": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "_match",
    ),
    "kPlayerVisuals_HealthComponent": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "_healthComponent",
    ),
    "kPlayerVisuals_CurrentHitboxSet": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "_currentHitboxSet",
    ),
    "kPlayerVisuals_RenderedAliveState": (
        "Psa.Core.Modules.Player.Components.Implementation",
        "PlayerVisuals",
        "_renderedAliveState",
    ),
    "kHitboxSet_AimAssistHeadHitbox": (
        "",
        "HitboxSetComponent",
        "AimAssistHeadHitbox",
    ),
    "kPlayerStanding_Profile": (
        "Psa.Core.Modules.RoundFlow.GameModes",
        "PlayerStanding",
        "<Profile>k__BackingField",
    ),
    "kParticipantProfile_Name": (
        "Psa.Core.Modules.RoundFlow.GameModes",
        "ParticipantProfile",
        "<Name>k__BackingField",
    ),
    "kHealthState_Health": (
        "",
        "HealthComponent.HealthState",
        "Health",
    ),
    "kHealthState_MaxHealth": (
        "",
        "HealthComponent.HealthState",
        "_maxHealth",
    ),
}

# (source_rel_path, cpp_enum_name, dump_namespace, dump_enum_name)
TRACKED_ENUMS: List[Tuple[str, str, str, str]] = [
    (
        "src/features/visual/body_esp.cpp",
        "TypeVisualEffects",
        "Psa.Core.Modules.Player.Components.Implementation",
        "TypeVisualEffects",
    ),
    (
        "src/features/visual/esp_shared.cpp",
        "TeamRelation",
        "Psa.Core.Modules.RoundFlow.GameModes",
        "TeamRelation",
    ),
]
