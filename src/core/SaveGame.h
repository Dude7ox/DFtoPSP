#pragma once
#include "Enemy.h"
#include "Player.h"
namespace dfpsp {
struct SavedEnemy {
    int mobileId=0;
    Vec3 pos;
    float yaw=0;
    bool female=false, passive=false, water=false, flying=false;
    int state=0, level=1, maxHp=1, hp=1, armor=0;
    float speed=90;
    int animState=0, facing=0, dir=0, frame=0;
    float bodyW=64, bodyH=64;
    bool aggro=false, mirrored=false, grounded=true;
    std::string key;
};
struct SavedDoor { std::string key; float amount=0, target=0; };
struct SaveData {
    u32 version=1;
    PlayerState player;
    int region=0, exteriorId=0;
    std::string dungeonName;
    std::vector<SavedEnemy> enemies;
    std::vector<std::string> spawnedKeys, deadKeys;
    std::vector<SavedDoor> doors;
};
Enemy toEnemy(const SavedEnemy& s);
SavedEnemy fromEnemy(const Enemy& e);
bool writeSave(const std::string& path, const SaveData& d);
bool readSave(const std::string& path, SaveData& d);
} // namespace dfpsp
