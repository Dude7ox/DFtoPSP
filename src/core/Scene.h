#pragma once
#include "Rdb.h"
#include "Arch3d.h"
#include "Texture.h"
#include <unordered_map>
namespace dfpsp {
struct Tri { Vertex v[3]; int archive=0,record=0; bool doubleSided=false; };
struct Flat { Vec3 pos; int archive=0,record=0; float w=1,h=1; int action=0; };
struct Scene {
    std::vector<Tri> tris;
    std::vector<Flat> flats;
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
// Collision: slide a vertical capsule (approximated by sphere at feet+radius) against triangles.
Vec3 moveWithCollision(const Scene& s, const Vec3& pos, const Vec3& delta, float radius, float height);
float floorHeight(const Scene& s, const Vec3& pos, float maxY);
} // namespace
