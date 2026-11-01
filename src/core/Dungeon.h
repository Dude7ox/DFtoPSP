#pragma once
#include "Scene.h"
#include "Maps.h"
#include <map>
namespace dfpsp {
// A small resident set: the current block and its eight layout neighbours.
// RDB coordinates remain native; one block is exactly 2048 units wide.
class Dungeon {
public:
    static constexpr float BLOCK_SIDE=2048.f;
    struct Resident { DungeonBlockRef ref; Scene scene; };
    Dungeon(const Vfs& vfs,const Bsa& arch3d,const Bsa& blocks):builder_(vfs,arch3d,blocks){}
    bool enter(const DungeonLayout& layout);
    bool update(const Vec3& position);
    Vec3 spawn()const{return spawn_;}
    const DungeonLayout& layout()const{return layout_;}
    const DungeonBlockRef* current()const;
    const std::vector<Resident>& resident()const{return resident_;}
    void tick(float dt);
    Door* aimedDoor(const Vec3& eye,const Vec3& forward);
    Vec3 move(const Vec3& feet,const Vec3& delta,float radius,float height)const;
    float floor(const Vec3& pos,float maxY)const;
    bool headroom(const Vec3& feet,float height)const;
    size_t triangleCount()const;
private:
    SceneBuilder builder_; DungeonLayout layout_; std::vector<Resident> resident_;
    int currentX_=0,currentZ_=0;Vec3 spawn_;
    std::map<std::string,std::vector<std::pair<float,float>>> doorStates_;
    const DungeonBlockRef* refAt(int x,int z)const;
    void rememberDoors();
};
} // namespace
