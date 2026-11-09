#pragma once
// GENERATED FILE - do not edit by hand.
// Regenerate with: python3 tools/gen_enemy_data.py --dfu /path/to/daggerfall-unity
//
// The tables in EnemyData.cpp are derived from Daggerfall Unity
// (MIT License, Copyright (C) 2009-2023 Daggerfall Workshop,
//  http://www.dfworkshop.net, https://github.com/Interkarma/daggerfall-unity):
//   Assets/Scripts/Utility/EnemyBasics.cs
//   Assets/Scripts/Utility/RandomEncounters.cs
//   Assets/Scripts/DaggerfallUnityEnums.cs
// They reproduce enemy combat numbers and encounter tables that live inside the
// original FALL.EXE rather than in the game data files. Names, hit points per
// level, attributes and resistances are read from the original MONSTER.BSA at
// run time instead, so mods that replace MONSTER.BSA still take effect.
namespace dfpsp {
enum EnemyBehaviour { BehaviourGeneral=0, BehaviourFlying=1, BehaviourAquatic=2, BehaviourSpectral=3, BehaviourGuard=4 };
// Animation records inside a monster texture archive: 5 records per state, each
// record holding the frames for one of eight compass directions.
enum EnemyAnimState { AnimMove=0, AnimPrimaryAttack=1, AnimHurt=2, AnimIdle=3, AnimRangedAttack1=4, AnimSpell=5, AnimRangedAttack2=6 };
enum EnemyFlag { EnemyHasIdle=1, EnemyHasRanged1=2, EnemyHasRanged2=4, EnemyCanOpenDoors=8, EnemyCastsMagic=16, EnemyNoShadow=32, EnemyParrySounds=64 };
struct EnemyStatic {
    int id;             // MobileTypes value: 0-42 monsters, 128-146 humanoids
    const char* name;   // fallback name, MONSTER.BSA wins when available
    int archive;        // texture archive holding the animation records
    int femaleArchive;  // archive used for female humanoids
    int corpseArchive, corpseRecord;
    int level;
    int minHealth, maxHealth;
    int minDamage[3], maxDamage[3];
    int armorValue;
    int minMetalToHit;
    unsigned char behaviour, affinity, team, flags;
    signed char attack1[10], attack2[10], attack3[10]; // -2 terminates a sequence
    unsigned char chance2, chance3;
    int soulPts;
};
extern const int kEnemyCount;
extern const EnemyStatic kEnemies[];
// 20 classic encounter tables: indices 0-18 are the dungeon types, 19 is underwater.
extern const short kEncounterTables[20][20];
const EnemyStatic* findEnemyStatic(int mobileId);
const char* enemyFallbackName(int mobileId);
} // namespace dfpsp
