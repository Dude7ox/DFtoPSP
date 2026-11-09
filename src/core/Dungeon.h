#pragma once
#include "Scene.h"
#include "Maps.h"
#include "Enemy.h"
#include "Player.h"
#include <map>
#include <set>
namespace dfpsp {
// A small resident set: the current block and its eight layout neighbours.
// RDB coordinates remain native; one block is exactly 2048 units wide.
class Dungeon {
public:
    static constexpr float BLOCK_SIDE=2048.f;
    struct Resident { DungeonBlockRef ref; Scene scene; };
    Dungeon(const Vfs& vfs,const Bsa& arch3d,const Bsa& blocks,const Bsa& monsters)
        :builder_(vfs,arch3d,blocks){book_.load(monsters);}
    bool enter(const DungeonLayout& layout,int playerLevel);
    bool update(const Vec3& position);
    Vec3 spawn()const{return spawn_;}
    const DungeonLayout& layout()const{return layout_;}
    const DungeonBlockRef* current()const;
    const std::vector<Resident>& resident()const{return resident_;}
    void tick(float dt);
    // Advances every monster in resident blocks; returns damage dealt to the player.
    int tickEnemies(float dt,const PlayerState& player,const Vec3& camForward);
    // Player melee swing at the best enemy in front; returns damage dealt.
    int playerAttack(const PlayerState& player,const Vec3& camForward,float range=140.f);
    Door* aimedDoor(const Vec3& eye,const Vec3& forward);
    Vec3 move(const Vec3& feet,const Vec3& delta,float radius,float height)const;
    float floor(const Vec3& pos,float maxY)const;
    bool headroom(const Vec3& feet,float height)const;
    bool lineOfSight(const Vec3& a,const Vec3& b)const;
    size_t triangleCount()const;
    const EnemyBook& book()const{return book_;}
    const std::vector<Enemy>& enemies()const{return enemies_;}
    const std::vector<std::string>& deadKeys()const{return deadKeys_;}
    const std::vector<std::string>& spawnedKeys()const{return spawnedKeys_;}
    void setEnemies(const std::vector<Enemy>& e,const std::vector<std::string>& dead,const std::vector<std::string>& spawned);
private:
    SceneBuilder builder_; DungeonLayout layout_; std::vector<Resident> resident_;
    EnemyBook book_;
    std::vector<Enemy> enemies_;
    std::vector<std::string> deadKeys_, spawnedKeys_;
    std::set<std::string> deadSet_, spawnedSet_;
    DfRandom rng_;
    int encounterLand_[256]={0}, encounterWater_[256]={0};
    int playerLevel_=3;
    int currentX_=0,currentZ_=0;Vec3 spawn_;
    std::map<std::string,std::vector<std::pair<float,float>>> doorStates_;
    const DungeonBlockRef* refAt(int x,int z)const;
    const Resident* residentAt(const Vec3& p)const;
    void rememberDoors();
    void spawnBlockEnemies(const Resident& r);
};
} // namespace dfpsp
