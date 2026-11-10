#pragma once
#include "../../core/Dungeon.h"
#include "../../core/Font.h"
#include "../../core/SoftwareRenderer.h"
namespace dfpsp {
class PspRenderer {
public:
    bool init(const Vfs& vfs,const Palette& pal);
    // Bakes the classic FONT000?.FNT face into a T8 VRAM atlas for HUD text.
    bool initFont(const Font& f);
    // Screen-space text queued before render(); drawn with the font atlas
    // on top of the HUD bars (1px black drop shadow, original face).
    void queueText(int x,int y,const char* s,u32 argb);
    void render(const Dungeon& dungeon,const Camera& cam,const char* status,float fps);
    void swap();
    void term();
    u32 drawOffset()const{return (swaps_&1)?512u*272u*2u:0u;}
private:
    const Vfs* vfs_=nullptr; Palette pal_;
    struct TexSlot { int archive=-1,record=-1,frame=0; u32 offset=0; int w=0,h=0,srcW=0,srcH=0; u32 last=0; };
    static constexpr int SLOTS=48;
    static constexpr int MAX_TEX=128;
    TexSlot slots_[SLOTS]; u32 tick_=0; u32 swaps_=0;
    u32 texBase_=0, fontBase_=0; const Font* font_=nullptr; bool fontReady_=false;
    struct TextLine { int x=0,y=0; u32 c=0xffffffff; char s[80]; };
    TextLine lines_[4]; int nLines_=0;
    TexSlot& getTex(int a,int r,int frame=0);
    void drawScene(const Scene& s,const Camera& cam);
    void drawEnemies(const Dungeon& d,const Camera& cam);
    void overlay(const char* status,float fps);
    void drawQueuedText();
};
} // namespace
