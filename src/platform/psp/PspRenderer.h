#pragma once
#include "../../core/Dungeon.h"
#include "../../core/SoftwareRenderer.h"
namespace dfpsp {
class PspRenderer {
public:
    bool init(const Vfs& vfs,const Palette& pal);
    void render(const Dungeon& dungeon,const Camera& cam,const char* status,float fps);
    void swap();
    void term();
    u32 drawOffset()const{return (swaps_&1)?512u*272u*2u:0u;}
private:
    const Vfs* vfs_=nullptr; Palette pal_;
    struct TexSlot { int archive=-1,record=-1; u32 offset=0; int w=0,h=0,srcW=0,srcH=0; u32 last=0; };
    static constexpr int SLOTS=48;
    static constexpr int MAX_TEX=128;
    TexSlot slots_[SLOTS]; u32 tick_=0; u32 texBase_=0; u32 swaps_=0;
    TexSlot& getTex(int a,int r);
    void drawScene(const Scene& s,const Camera& cam);
    void overlay(const char* status,float fps);
};
} // namespace
