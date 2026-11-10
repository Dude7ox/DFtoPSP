#pragma once
#include "Types.h"
namespace dfpsp {
// Minimal but persistent character sheet: enough for dungeon combat and saves.
struct PlayerState {
    int level=3;
    int hp=40, maxHp=40;
    int weaponMin=4, weaponMax=14;   // equipped weapon damage range
    int skill=45;                    // weapon skill percent
    int lockpick=30;                 // lockpicking skill percent
    int armor=10;                    // worn armour value
    int xp=0, gold=0;                // progression counters
    int kills=0;                     // monsters defeated this career
    float attackCooldown=0;          // seconds until the next swing
    bool dead=false;
    Vec3 pos;
    float yaw=0, pitch=0;
};
} // namespace dfpsp
