#pragma once
#include "Rdb.h"
#include "Arch3d.h"
#include "Texture.h"
#include <unordered_map>
namespace dfpsp {
struct Tri { Vertex v[3]; int archive=0,record=0; bool doubleSided=false; };
struct Flat { Vec3 pos; int archive=0,record=0; float w=1,h=1; int action=0; };
struct Door {
    Vec3 pos; float yaw=0, amount=0, target=0; bool locked=false;
    std::vector<Tri> local, world;
};
struct Scene {
    std::vector<Tri> tris;
    std::vector<Flat> flats;
    std::vector<Door> doors;
    Vec3 spawn{4,2,4};
    std::string blockName;
    size_t modelCount=0, flatCount=0;
};
class SceneBuilder {
public:
    SceneBuilder(const Vfs& vfs,const Bsa& arch3d,const Bsa& blocks):vfs_(vfs),arch3d_(arch3d),blocks_(blocks){}
    bool loadBlock(const std::string& name,Scene& out);
private:
    const Vfs& vfs_; const Bsa& arch3d_; const Bsa& blocks_;
    std::unordered_map<u32,Mesh> meshCache_;
};
Vec3 rotateY(const Vec3& p,float degrees);
void updateDoors(Scene& scene,float dt);
void refreshDoor(Door& door);
// Returns an unlocked door in front of the camera, or nullptr.
Door* aimedDoor(Scene& scene,const Vec3& eye,const Vec3& forward,float range=180.f);
bool toggleDoor(Door& door);
// Horizontal capsule collision. Floor/ceiling handling is separate so that
// walking on a floor does not produce a sideways collision impulse.
Vec3 moveWithCollision(const Scene& s, const Vec3& feet, const Vec3& delta, float radius, float height);
float floorHeight(const Scene& s, const Vec3& pos, float maxY);
bool hasHeadroom(const Scene& s,const Vec3& feet,float height);
} // namespace
