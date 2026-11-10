#include "Automap.h"
#include <cmath>
namespace dfpsp {
static void collect(const std::vector<Tri>& tris,std::vector<MapSeg>& out){
    for(auto& t:tris){
        Vec3 n=(t.v[1].p-t.v[0].p).cross(t.v[2].p-t.v[0].p);
        float l=n.len();if(l<1e-6f)continue;n=n*(1.f/l);
        if(std::fabs(n.y)>=0.65f)continue;          // floor/ceilings are not walls
        for(int k=0;k<3;k++){
            const Vec3& a=t.v[k].p;const Vec3& b=t.v[(k+1)%3].p;
            if(std::fabs(a.y-b.y)>=4.f)continue;    // keep horizontal wall edges only
            MapSeg s;s.x0=a.x;s.z0=a.z;s.x1=b.x;s.z1=b.z;out.push_back(s);
        }
    }
}
void buildAutomap(const Dungeon& d,const Vec3& player,float yaw,float radius,AutomapData& out){
    out=AutomapData{};
    for(auto& r:d.resident()){
        collect(r.scene.tris,out.walls);
        for(auto& door:r.scene.doors)collect(door.world,out.doors);
    }
    Vec3 fwd{std::sin(yaw),0,std::cos(yaw)};
    Vec3 tip=player+fwd*70.f,back=player-fwd*35.f;
    float wl=yaw+2.5f,wr=yaw-2.5f;
    Vec3 lw=player+Vec3{std::sin(wl),0,std::cos(wl)}*45.f;
    Vec3 rw=player+Vec3{std::sin(wr),0,std::cos(wr)}*45.f;
    out.marker.push_back({tip.x,tip.z,lw.x,lw.z});
    out.marker.push_back({tip.x,tip.z,rw.x,rw.z});
    out.marker.push_back({player.x,player.z,back.x,back.z});
    out.minx=player.x-radius;out.maxx=player.x+radius;
    out.minz=player.z-radius;out.maxz=player.z+radius;
}
} // namespace
