#include "PspRenderer.h"
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
namespace dfpsp {
static unsigned int __attribute__((aligned(16))) list[262144];
struct GuVertex { float u,v; unsigned int color; float x,y,z; };
static GuVertex __attribute__((aligned(16))) verts[4096];
static void* vramTop=nullptr;
static void* texBase=nullptr;
bool PspRenderer::init(const Vfs& vfs,const Palette& pal){
    vfs_=&vfs;pal_=pal;
    void* fbp0=guGetStaticVramBuffer(512,272,GU_PSM_5650);
    void* fbp1=guGetStaticVramBuffer(512,272,GU_PSM_5650);
    void* zbp=guGetStaticVramBuffer(512,272,GU_PSM_4444);
    vramTop=(void*)((u8*)zbp+512*272*2); texBase=vramTop;
    sceGuInit();
    sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_5650,fbp0,512);
    sceGuDispBuffer(480,272,fbp1,512);
    sceGuDepthBuffer(zbp,512);
    sceGuOffset(2048-240,2048-136);
    sceGuViewport(2048,2048,480,272);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,480,272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    return true;
}
void* PspRenderer::allocTex(int w,int h){
    // simple bump allocator in remaining VRAM, rounded to 16 bytes
    size_t sz=(size_t)w*h*2; sz=(sz+15)&~15;
    void* p=vramTop; vramTop=(void*)((u8*)vramTop+sz);
    if((u8*)vramTop+sz>(u8*)0x200000){ // 2MB VRAM, offsets are relative
        vramTop=texBase;
        p=vramTop; vramTop=(void*)((u8*)vramTop+sz);
    }
    return p;
}
PspRenderer::TexSlot& PspRenderer::getTex(int a,int r){
    for(auto& s:slots_) if(s.archive==a&&s.record==r){s.last=++tick_;return s;}
    int idx=0;u32 best=0xffffffff;
    for(int i=0;i<SLOTS;i++)if(slots_[i].last<best){best=slots_[i].last;idx=i;}
    TexSlot& s=slots_[idx];
    Texture t; if(!loadTexture(*vfs_,a,r,pal_,t)||t.w==0){t.w=t.h=1;t.rgba={255,0,255,255};}
    int w=1;while(w<t.w)w<<=1;int h=1;while(h<t.h)h<<=1;if(w>128)w=128;if(h>128)h=128;
    if(!s.data||s.w!=w||s.h!=h)s.data=allocTex(w,h);
    u16* dst=(u16*)s.data;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        int sx=x*t.w/w,sy=y*t.h/h;u8* p=&t.rgba[(sy*t.w+sx)*4];
        u16 c=((p[0]>>3)<<11)|((p[1]>>2)<<5)|(p[2]>>3)|(p[3]&0x80?0x8000:0);
        // GU 5551: bit15 alpha
        c=((p[0]>>3)<<10)|((p[1]>>3)<<5)|(p[2]>>3)|(p[3]>=128?0x8000:0);
        dst[y*w+x]=c;
    }
    s.archive=a;s.record=r;s.w=w;s.h=h;s.last=++tick_;
    return s;
}
void PspRenderer::render(const Scene& s,const Camera& cam){
    sceGuStart(GU_DIRECT,list);
    sceGuClearColor(0xff181010);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION);sceGumLoadIdentity();
    sceGumPerspective(cam.fov,480.f/272.f,1.f,4096.f);
    sceGumMatrixMode(GU_VIEW);sceGumLoadIdentity();
    sceGumRotateX(-cam.pitch);sceGumRotateY(-cam.yaw);
    ScePspFVector3 neg={-cam.pos.x,-cam.pos.y,-cam.pos.z};sceGumTranslate(&neg);
    sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
    sceGuTexMode(GU_PSM_5551,0,0,0);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST,GU_NEAREST);
    sceGuTexScale(1.f,1.f);sceGuTexOffset(0,0);
    sceGuAmbientColor(0xffffffff);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    // group triangles by texture to reduce binds
    int n=0;int curA=-1,curR=-1;
    auto flush=[&](){if(n>0){sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,n,0,verts);n=0;}};
    for(auto& t:s.tris){
        if(t.archive!=curA||t.record!=curR){flush();TexSlot& ts=getTex(t.archive,t.record);sceGuTexImage(0,ts.w,ts.h,ts.w,ts.data);curA=t.archive;curR=t.record;}
        if(n+3>4096)flush();
        for(int k=0;k<3;k++){const Vertex& v=t.v[k];verts[n++]={v.uv.x,v.uv.y,0xffffffff,v.p.x,v.p.y,v.p.z};}
    }
    flush();
    // flats as billboards
    sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    for(auto& f:s.flats){
        TexSlot& ts=getTex(f.archive,f.record);sceGuTexImage(0,ts.w,ts.h,ts.w,ts.data);
        float cy=std::cos(cam.yaw),sy=std::sin(cam.yaw);Vec3 right={cy,0,-sy};
        Vec3 p0=f.pos-right*(f.w*0.5f)+Vec3{0,f.h,0},p1=f.pos+right*(f.w*0.5f)+Vec3{0,f.h,0},p2=f.pos+right*(f.w*0.5f),p3=f.pos-right*(f.w*0.5f);
        verts[0]={0,0,0xffffffff,p0.x,p0.y,p0.z};verts[1]={(float)ts.w,0,0xffffffff,p1.x,p1.y,p1.z};verts[2]={(float)ts.w,(float)ts.h,0xffffffff,p2.x,p2.y,p2.z};
        verts[3]={0,0,0xffffffff,p0.x,p0.y,p0.z};verts[4]={(float)ts.w,(float)ts.h,0xffffffff,p2.x,p2.y,p2.z};verts[5]={0,(float)ts.h,0xffffffff,p3.x,p3.y,p3.z};
        sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,6,0,verts);
    }
    sceGuDisable(GU_BLEND);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
}
void PspRenderer::swap(){sceDisplayWaitVblankStart();sceGuSwapBuffers();}
void PspRenderer::term(){sceGuTerm();}
} // namespace
