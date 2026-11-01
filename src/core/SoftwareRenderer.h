#pragma once
#include "Scene.h"
namespace dfpsp {
struct Camera { Vec3 pos{0,1,0}; float yaw=0,pitch=0; float fov=70.f; };
struct Framebuffer { int w=0,h=0; std::vector<u32> color; std::vector<float> depth; void resize(int W,int H){w=W;h=H;color.assign(w*h,0xff000000);depth.assign(w*h,0.f);} };
class SoftwareRenderer {
public:
    SoftwareRenderer(const Vfs& vfs,const Palette& pal):vfs_(vfs),pal_(pal){}
    void render(const Scene& s,const Camera& cam,Framebuffer& fb);
    Texture& texture(int a,int r);
private:
    const Vfs& vfs_; const Palette& pal_;
    std::unordered_map<u32,Texture> cache_;
};
bool writeBmp(const std::string& path,const Framebuffer& fb);
} // namespace
