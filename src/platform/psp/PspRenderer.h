#pragma once
#include "../../core/Scene.h"
#include "../../core/SoftwareRenderer.h"
namespace dfpsp {
class PspRenderer {
public:
    bool init(const Vfs& vfs,const Palette& pal);
    void render(const Scene& s,const Camera& cam);
    void swap();
    void term();
private:
    const Vfs* vfs_=nullptr; Palette pal_;
    struct TexSlot { int archive=-1,record=-1; void* data=nullptr; int w=0,h=0; u32 last=0; };
    static constexpr int SLOTS=48;
    TexSlot slots_[SLOTS]; u32 tick_=0;
    void* allocTex(int w,int h);
    TexSlot& getTex(int a,int r);
};
} // namespace
