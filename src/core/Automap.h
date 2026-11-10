#pragma once
#include "Dungeon.h"
namespace dfpsp {
struct MapSeg { float x0=0,z0=0,x1=0,z1=0; };
struct AutomapData {
    std::vector<MapSeg> walls, doors, marker;
    float minx=0,maxx=0,minz=0,maxz=0;   // fixed window around the player
};
// Top-down outline of the resident blocks: horizontal edges of the vertical
// wall triangles, door outlines and a three-segment player arrow. The bounds
// are a fixed-radius window centred on the player so the map does not jump.
void buildAutomap(const Dungeon& d,const Vec3& player,float yaw,float radius,AutomapData& out);
} // namespace
