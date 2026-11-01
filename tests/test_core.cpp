#include "../src/core/Vfs.h"
#include "../src/core/Bsa.h"
#include "../src/core/Arch3d.h"
#include "../src/core/Rdb.h"
#include "../src/core/Maps.h"
#include "../src/core/Texture.h"
#include "../src/core/Scene.h"
#include "../src/core/Dungeon.h"
#include <cassert>
#include <cstdio>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s line %d\n",#x,__LINE__);return 1;}}while(0)
using namespace dfpsp;
int main(int argc,char**argv){
    std::string root=argc>1?argv[1]:"data";
    Vfs vfs;vfs.addRoot(root);vfs.addRoot(root+"/ARENA2");
    Bsa arch3d,blocks,maps;
    CHECK(arch3d.open(vfs,"ARCH3D.BSA"));CHECK(arch3d.count()>0);
    CHECK(blocks.open(vfs,"BLOCKS.BSA"));CHECK(blocks.count()>1000);
    CHECK(maps.open(vfs,"MAPS.BSA"));
    Palette pal;CHECK(loadPalette(vfs,"ART_PAL.COL",pal));
    // ART_PAL.COL is already 8-bit; do not scale palette bytes by 4.
    CHECK(pal.c[1]==rgba(255,229,129));
    Texture tex;CHECK(loadTexture(vfs,2,0,pal,tex));CHECK(tex.w>0&&tex.h>0);CHECK(tex.indices.size()==(size_t)tex.w*tex.h);
    // 0x0900 dungeon records are uncompressed despite the high flag byte.
    CHECK(loadTexture(vfs,122,2,pal,tex));CHECK(tex.w==64&&tex.h==64);
    CHECK(loadTexture(vfs,122,1,pal,tex));CHECK(loadTexture(vfs,321,2,pal,tex));
    Mesh m;CHECK(loadMesh(arch3d,61018,m));CHECK(!m.verts.empty());
    RdbBlock rdb;CHECK(loadRdb(blocks,"B0000001.RDB",rdb));CHECK(!rdb.objects.empty());
    DungeonLayout l;CHECK(loadDungeonLayout(maps,16,18358,l));CHECK(l.blocks.size()==8);
    SceneBuilder sb(vfs,arch3d,blocks);Scene s;CHECK(sb.loadBlock("B0000001.RDB",s));CHECK(!s.tris.empty());
    CHECK(!s.doors.empty());CHECK(hasHeadroom(s,s.spawn,72.f));
    Door* unlocked=nullptr;for(auto& d:s.doors)if(!d.locked){unlocked=&d;break;}CHECK(unlocked);
    CHECK(toggleDoor(*unlocked));updateDoors(s,2.f);CHECK(unlocked->amount>0.99f);
    CHECK(toggleDoor(*unlocked));updateDoors(s,2.f);CHECK(unlocked->amount<0.01f);
    Vec3 p=s.spawn;Vec3 q=moveWithCollision(s,p,{10.f,0,0},20.f,72.f);CHECK(std::isfinite(q.x));
    Dungeon dungeon(vfs,arch3d,blocks);CHECK(dungeon.enter(l));CHECK(dungeon.resident().size()>1);CHECK(dungeon.resident().size()<=9);
    CHECK(dungeon.current()&&dungeon.current()->start);CHECK(dungeon.triangleCount()>s.tris.size());
    Vec3 sp=dungeon.spawn();CHECK(dungeon.headroom(sp,72.f));Vec3 moved=dungeon.move(sp,{16,0,0},20,72);CHECK(std::isfinite(moved.x));
    printf("core tests passed\n");return 0;
}
