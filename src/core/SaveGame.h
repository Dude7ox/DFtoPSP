#pragma once
#include "Enemy.h"
#include "Player.h"
#include "CharGen.h"
#include "Inventory.h"
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
struct SavedItem {
    int templateIndex=0,stack=1,value=0,weightQ=0,material=-1;
    int maxCondition=0,condition=0,variant=0,dye=0,slot=-1;
    int playerArchive=0,playerRecord=0;
};
struct SavedCharacter {
    bool valid=false;
    int race=1,gender=0,classIndex=0,biographyIndex=0,faceIndex=0;
    std::string name;
    int attributes[8]={0},rolledAttributes[8]={0};
    int bonusPool=0,skillValues[35]={0},skillPool[3]={0},answers[12]={0};
    int reflexes=2,mageryOption=-1;
    std::vector<int> pickPrim,pickSec,pickCost,pickDis;
    Career career;
};
struct SaveData {
    u32 version=5;
    PlayerState player;
    int region=0, exteriorId=0;
    std::string dungeonName;
    std::vector<SavedEnemy> enemies;
    std::vector<std::string> spawnedKeys, deadKeys;
    std::vector<SavedDoor> doors;
    SavedCharacter character;      // v4: created character and worn kit
    std::vector<SavedItem> items;
};
SavedItem fromItem(const Item& it);
Item toItem(const SavedItem& s);
SavedCharacter fromCharacter(const CharacterDoc& d);
void toCharacter(const SavedCharacter& s,CharacterDoc& d);
Enemy toEnemy(const SavedEnemy& s);
SavedEnemy fromEnemy(const Enemy& e);
bool writeSave(const std::string& path, const SaveData& d);
bool readSave(const std::string& path, SaveData& d);
} // namespace dfpsp
