#include "Dungeon.h"
#include <algorithm>
#include <cmath>
namespace dfpsp {
const DungeonBlockRef* Dungeon::refAt(int x,int z)const{for(auto& b:layout_.blocks)if(b.x==x&&b.z==z)return &b;return nullptr;}
const DungeonBlockRef* Dungeon::current()const{return refAt(currentX_,currentZ_);}
bool Dungeon::enter(const DungeonLayout& layout){
    const DungeonBlockRef* start=nullptr;for(auto& b:layout.blocks)if(b.start){start=&b;break;}if(!start&& !layout.blocks.empty())start=&layout.blocks[0];if(!start)return false;
    Scene first;if(!builder_.loadBlock(start->name,first))return false;
    layout_=layout;resident_.clear();doorStates_.clear();currentX_=start->x;currentZ_=start->z;
    spawn_=first.spawn+Vec3{(float)start->x*BLOCK_SIDE,0,(float)start->z*BLOCK_SIDE};
    resident_.push_back({*start,std::move(first)});currentX_=start->x+1000000;return update(spawn_);
}
void Dungeon::rememberDoors(){for(auto& r:resident_){std::vector<std::pair<float,float>> states;for(auto& d:r.scene.doors)states.push_back({d.amount,d.target});doorStates_[r.ref.name+":"+std::to_string(r.ref.x)+":"+std::to_string(r.ref.z)]=std::move(states);}}
bool Dungeon::update(const Vec3& p){
    int x=(int)std::floor(p.x/BLOCK_SIDE),z=(int)std::floor(p.z/BLOCK_SIDE);if(!refAt(x,z))return false;
    if(x==currentX_&&z==currentZ_&&!resident_.empty())return true;
    rememberDoors();currentX_=x;currentZ_=z;std::vector<Resident> next;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
        auto* ref=refAt(x+dx,z+dz);if(!ref)continue;
        bool loaded=false;for(auto& old:resident_)if(old.ref.name==ref->name&&old.ref.x==ref->x&&old.ref.z==ref->z){next.push_back(std::move(old));loaded=true;break;}
        if(loaded)continue;Scene s;if(!builder_.loadBlock(ref->name,s))continue;
        Vec3 offset{(float)ref->x*BLOCK_SIDE,0,(float)ref->z*BLOCK_SIDE};
        for(auto& t:s.tris)for(auto& v:t.v)v.p=v.p+offset;
        for(auto& f:s.flats)f.pos=f.pos+offset;
        for(auto& d:s.doors)d.pos=d.pos+offset;
        s.spawn=s.spawn+offset;
        auto saved=doorStates_.find(ref->name+":"+std::to_string(ref->x)+":"+std::to_string(ref->z));if(saved!=doorStates_.end())for(size_t i=0;i<s.doors.size()&&i<saved->second.size();i++){s.doors[i].amount=saved->second[i].first;s.doors[i].target=saved->second[i].second;}
        for(auto& door:s.doors)refreshDoor(door);next.push_back({*ref,std::move(s)});
    }
    resident_=std::move(next);return !resident_.empty();
}
void Dungeon::tick(float dt){for(auto& r:resident_)updateDoors(r.scene,dt);}
Door* Dungeon::aimedDoor(const Vec3& eye,const Vec3& forward){Door* best=nullptr;float dist=1e9f;for(auto& r:resident_){Door* d=dfpsp::aimedDoor(r.scene,eye,forward);if(d&&(d->pos-eye).len()<dist){best=d;dist=(d->pos-eye).len();}}return best;}
Vec3 Dungeon::move(const Vec3& feet,const Vec3& delta,float radius,float height)const{
    Vec3 p=feet;int steps=std::max(1,(int)std::ceil(delta.len()/(radius*0.5f)));steps=std::min(steps,64);Vec3 step=delta*(1.f/steps);
    for(int i=0;i<steps;i++){Vec3 candidate=p+step;Vec3 result=candidate;
        for(auto& r:resident_)result=moveWithCollision(r.scene,result,Vec3{},radius,height);
        auto valid=[&](const Vec3& q){return refAt((int)std::floor(q.x/BLOCK_SIDE),(int)std::floor(q.z/BLOCK_SIDE))!=nullptr;};
        if(valid(result))p=result;
        else{
            Vec3 px{p.x+step.x,p.y,p.z};for(auto& r:resident_)px=moveWithCollision(r.scene,px,Vec3{},radius,height);
            Vec3 pz{p.x,p.y,p.z+step.z};for(auto& r:resident_)pz=moveWithCollision(r.scene,pz,Vec3{},radius,height);
            if(valid(px))p=px;else if(valid(pz))p=pz;
        }
    }
    return p;
}
float Dungeon::floor(const Vec3& p,float maxY)const{float y=-1e9f;for(auto& r:resident_)y=std::max(y,floorHeight(r.scene,p,maxY));return y;}
bool Dungeon::headroom(const Vec3& p,float h)const{for(auto& r:resident_)if(!hasHeadroom(r.scene,p,h))return false;return true;}
size_t Dungeon::triangleCount()const{size_t n=0;for(auto& r:resident_){n+=r.scene.tris.size();for(auto& d:r.scene.doors)n+=d.world.size();}return n;}
} // namespace
