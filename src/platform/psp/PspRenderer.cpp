#include "PspRenderer.h"
#include <pspgu.h>
#include <pspgum.h>
#include <pspge.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cmath>
namespace dfpsp {
static unsigned int __attribute__((aligned(16))) list[262144];
struct GuVertex { float u,v; unsigned int color; float x,y,z; };
static GuVertex __attribute__((aligned(16))) verts[4096];
static u32 __attribute__((aligned(16))) clut[256];
static u32 __attribute__((aligned(16))) textClut[256];
// PSP GE expects VRAM offsets; CPU writes need the uncached EDRAM mapping.
static inline void* cpuPtr(u32 offset){return (void*)((u8*)sceGeEdramGetAddr()+offset);}
bool PspRenderer::init(const Vfs& vfs,const Palette& pal){
    vfs_=&vfs;pal_=pal;
    void* fbp0=guGetStaticVramBuffer(512,272,GU_PSM_5650);
    void* fbp1=guGetStaticVramBuffer(512,272,GU_PSM_5650);
    void* zbp=guGetStaticVramBuffer(512,272,GU_PSM_4444);
    texBase_=(u32)(uintptr_t)zbp+512*272*2;
    texBase_=(texBase_+15)&~15u;
    for(int i=0;i<SLOTS;i++)slots_[i].offset=texBase_+(u32)i*MAX_TEX*MAX_TEX;
    for(int i=0;i<256;i++){u32 c=pal.c[i];clut[i]=(geta(c)<<24)|(getb(c)<<16)|(getg(c)<<8)|getr(c);}
    clut[0]=0;
    sceGuInit();sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_5650,fbp0,512);sceGuDispBuffer(480,272,fbp1,512);sceGuDepthBuffer(zbp,512);
    sceGuOffset(2048-240,2048-136);sceGuViewport(2048,2048,480,272);
    sceGuDepthRange(65535,0);sceGuScissor(0,0,480,272);sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);sceGuEnable(GU_DEPTH_TEST);sceGuFrontFace(GU_CW);sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_TEXTURE_2D);sceGuEnable(GU_CLIP_PLANES);sceGuDisable(GU_CULL_FACE);
    sceGuClutMode(GU_PSM_8888,0,0xff,0);sceGuClutLoad(256/8,clut);
    sceGuFinish();sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);return true;
}
bool PspRenderer::initFont(const Font& main,const Font& small){
    if(!main.loaded())return false;
    // The atlas lives behind the 48 world slots: one 256x512 T8 texture,
    // 16x16 cells, 16 columns; main face occupies v 0..255, the small face
    // v 256..511 (~128 KB of the 2 MB EDRAM).
    fontBase_=texBase_+(u32)SLOTS*MAX_TEX*MAX_TEX;
    u8* dst=(u8*)cpuPtr(fontBase_);
    std::memset(dst,0,256*512);
    auto bake=[&](const Font& f,int vbase){
        for(int i=0;i<Font::kGlyphCount;i++){
            char c=(char)(Font::kAsciiStart+i);
            const u16* rows=f.rows(c);
            if(!rows)continue;
            int gx=(i%16)*16,gy=vbase+(i/16)*16;
            int w=f.glyphWidth(c),h=f.fixedHeight();
            for(int y=0;y<h&&y<16;y++)for(int x=0;x<w&&x<16;x++)
                if(rows[y]&(0x8000u>>x))dst[(gy+y)*256+gx+x]=1;
        }
    };
    bake(main,0);
    if(small.loaded())bake(small,256);
    sceKernelDcacheWritebackInvalidateRange(dst,256*512);
    for(int i=0;i<256;i++)textClut[i]=0;
    font_=&main;fontSmall_=small.loaded()?&small:nullptr;
    fontReady_=true;return true;
}
void PspRenderer::queueText(int x,int y,const char* s,u32 argb,const Font* f){
    if(nLines_>=20)return;
    TextLine& l=lines_[nLines_++];
    l.x=x;l.y=y;l.c=argb;l.f=f;
    std::strncpy(l.s,s,sizeof(l.s)-1);l.s[sizeof(l.s)-1]=0;
}
void PspRenderer::queueRect(int x,int y,int w,int h,u32 argb){
    if(nRects_>=8)return;
    UiRect& r=rects_[nRects_++];
    r.x=x;r.y=y;r.w=w;r.h=h;r.c=argb;
}
void PspRenderer::drawQueuedText(){
    if(!fontReady_||nLines_==0)return;
    sceGuTexMode(GU_PSM_T8,0,0,0);sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST,GU_NEAREST);sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuTexImage(0,256,512,256,(void*)(uintptr_t)fontBase_);
    sceGuTexScale(1.f,1.f);sceGuTexOffset(0,0);
    sceGuEnable(GU_TEXTURE_2D);
    for(int pass=0;pass<2;pass++){                 // 0 = shadow, 1 = ink
        int n=0;u32 lastColor=0;
        auto flush=[&](){if(n){sceGuClutMode(GU_PSM_8888,0,0xff,0);sceGuClutLoad(256/8,textClut);sceGumDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,n,0,verts);n=0;}};
        for(int li=0;li<nLines_;li++){
            TextLine& l=lines_[li];
            u32 c=(pass==0)?0xff000000:l.c;
            if(c!=lastColor){flush();textClut[1]=c;lastColor=c;}
            int px=l.x+pass,py=l.y+pass;
            const Font* lf=(l.f&&fontSmall_&&l.f==fontSmall_)?fontSmall_:font_;
            int vbase=(lf==fontSmall_)?256:0;
            int h=lf->fixedHeight();
            for(const char* sp=l.s;*sp;++sp){
                char ch=*sp;int w=lf->glyphWidth(ch);
                const u16* rows=lf->rows(ch);
                if(rows&&w>0){
                    unsigned idx=(unsigned char)ch-Font::kAsciiStart;
                    float gx=(float)((idx%16)*16),gy=(float)(vbase+(idx/16)*16);
                    if(n+2>4096)flush();
                    verts[n++]={gx,gy,0xffffffff,(float)px,(float)py,0.f};
                    verts[n++]={gx+(float)w,gy+(float)h,0xffffffff,(float)(px+w),(float)(py+h),0.f};
                }
                px+=lf->advance(ch);
            }
        }
        flush();
    }
}
PspRenderer::TexSlot& PspRenderer::getTex(int a,int r,int frame){
    for(auto& s:slots_)if(s.archive==a&&s.record==r&&s.frame==frame){s.last=++tick_;return s;}
    int idx=0;u32 best=0xffffffff;for(int i=0;i<SLOTS;i++)if(slots_[i].last<best){best=slots_[i].last;idx=i;}
    TexSlot& s=slots_[idx];Texture t;
    if(!loadTexture(*vfs_,a,r,pal_,t,frame)||t.w==0){t.w=t.h=1;t.indices.assign(1,0);t.rgba={255,0,255,255};}
    int w=1,h=1;while(w<t.w)w<<=1;while(h<t.h)h<<=1;if(w>MAX_TEX)w=MAX_TEX;if(h>MAX_TEX)h=MAX_TEX;
    u8* dst=(u8*)cpuPtr(s.offset);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){int sx=x*t.w/w,sy=y*t.h/h;dst[y*w+x]=t.indices[sy*t.w+sx];}
    sceKernelDcacheWritebackInvalidateRange(dst,w*h);
    s.archive=a;s.record=r;s.frame=frame;s.w=w;s.h=h;s.srcW=t.w;s.srcH=t.h;s.last=++tick_;return s;
}
void PspRenderer::drawScene(const Scene& s,const Camera& cam){
    int n=0,curA=-1,curR=-1;
    auto flush=[&](){if(n>0){sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,n,0,verts);n=0;}};
    auto bind=[&](int a,int r){TexSlot& ts=getTex(a,r);sceGuTexImage(0,ts.w,ts.h,ts.w,(void*)(uintptr_t)ts.offset);sceGuTexScale(1.f/ts.srcW,1.f/ts.srcH);curA=a;curR=r;};
    auto tri=[&](const Tri& t){if(t.archive!=curA||t.record!=curR){flush();bind(t.archive,t.record);}if(n+3>4096)flush();for(int k=0;k<3;k++){const Vertex& v=t.v[k];verts[n++]={v.uv.x,v.uv.y,0xffffffff,v.p.x,v.p.y,v.p.z};}};
    sceGuTexMode(GU_PSM_T8,0,0,0);sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);sceGuTexFilter(GU_NEAREST,GU_NEAREST);sceGuTexWrap(GU_REPEAT,GU_REPEAT);sceGuTexOffset(0,0);sceGuAmbientColor(0xffffffff);sceGuEnable(GU_TEXTURE_2D);sceGuDisable(GU_BLEND);
    for(auto& t:s.tris)tri(t);for(auto& d:s.doors)for(auto& t:d.world)tri(t);flush();
    sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    for(auto& f:s.flats){
        if((f.pos-cam.pos).len()>2048.f)continue;
        TexSlot& ts=getTex(f.archive,f.record);sceGuTexImage(0,ts.w,ts.h,ts.w,(void*)(uintptr_t)ts.offset);sceGuTexScale(1.f/ts.srcW,1.f/ts.srcH);
        float cy=std::cos(cam.yaw),sy=std::sin(cam.yaw);Vec3 right{cy,0,-sy};
        Vec3 p0=f.pos-right*(f.w*0.5f)+Vec3{0,f.h,0},p1=f.pos+right*(f.w*0.5f)+Vec3{0,f.h,0},p2=f.pos+right*(f.w*0.5f),p3=f.pos-right*(f.w*0.5f);
        float w=f.w,h=f.h;
        verts[0]={0,0,0xffffffff,p0.x,p0.y,p0.z};verts[1]={w,0,0xffffffff,p1.x,p1.y,p1.z};verts[2]={w,h,0xffffffff,p2.x,p2.y,p2.z};
        verts[3]={0,0,0xffffffff,p0.x,p0.y,p0.z};verts[4]={w,h,0xffffffff,p2.x,p2.y,p2.z};verts[5]={0,h,0xffffffff,p3.x,p3.y,p3.z};
        sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,6,0,verts);
    }
    flush();sceGuDisable(GU_BLEND);
}
void PspRenderer::drawEnemies(const Dungeon& d,const Camera& cam){
    sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    float cy=std::cos(cam.yaw),sy=std::sin(cam.yaw);Vec3 right{cy,0,-sy};
    Vec3 fwd{sy,0,cy};
    for(auto& e:d.enemies()){
        Vec3 to=e.pos-cam.pos;
        float dist=to.len();
        if(dist>2048.f)continue;
        if(dist>1.f&&to.norm().dot(fwd)<-0.25f)continue;
        const EnemyStatic* st=findEnemyStatic(e.mobileId);
        if(!st)continue;
        int archive,record,frame=e.frame;
        if(e.state==EnemyDead){archive=st->corpseArchive;record=st->corpseRecord;frame=0;}
        else{archive=(e.female&&st->femaleArchive)?st->femaleArchive:st->archive;record=enemyRecord(e);}
        TexSlot& ts=getTex(archive,record,frame);
        sceGuTexImage(0,ts.w,ts.h,ts.w,(void*)(uintptr_t)ts.offset);sceGuTexScale(1.f/ts.srcW,1.f/ts.srcH);
        float w=e.bodyW,h=e.bodyH;
        if(e.state==EnemyDead)h=std::min(h,w*0.6f);
        Vec3 p0=e.pos-right*(w*0.5f)+Vec3{0,h,0},p1=e.pos+right*(w*0.5f)+Vec3{0,h,0},p2=e.pos+right*(w*0.5f),p3=e.pos-right*(w*0.5f);
        float u0=0,u1=w;
        if(e.mirrored&&e.state!=EnemyDead){u0=w;u1=0;}
        verts[0]={u0,0,0xffffffff,p0.x,p0.y,p0.z};verts[1]={u1,0,0xffffffff,p1.x,p1.y,p1.z};verts[2]={u1,h,0xffffffff,p2.x,p2.y,p2.z};
        verts[3]={u0,0,0xffffffff,p0.x,p0.y,p0.z};verts[4]={u1,h,0xffffffff,p2.x,p2.y,p2.z};verts[5]={u0,h,0xffffffff,p3.x,p3.y,p3.z};
        sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,6,0,verts);
        if(e.state!=EnemyDead&&e.hp<e.maxHp){
            // Small health bar floating over the head, depth tested like the sprite.
            sceGuDisable(GU_TEXTURE_2D);
            float bw=w*0.7f,frac=e.maxHp>0?(float)e.hp/(float)e.maxHp:0.f;
            Vec3 b0=e.pos-right*(bw*0.5f)+Vec3{0,h+8.f,0},b1=e.pos+right*(bw*0.5f)+Vec3{0,h+8.f,0};
            Vec3 b2=e.pos+right*(bw*0.5f)+Vec3{0,h+12.f,0},b3=e.pos-right*(bw*0.5f)+Vec3{0,h+12.f,0};
            verts[0]={0,0,0xc0000000,b0.x,b0.y,b0.z};verts[1]={0,0,0xc0000000,b1.x,b1.y,b1.z};verts[2]={0,0,0xc0000000,b2.x,b2.y,b2.z};
            verts[3]={0,0,0xc0000000,b0.x,b0.y,b0.z};verts[4]={0,0,0xc0000000,b2.x,b2.y,b2.z};verts[5]={0,0,0xc0000000,b3.x,b3.y,b3.z};
            sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,6,0,verts);
            Vec3 f1=e.pos-right*(bw*0.5f)+right*(bw*frac)+Vec3{0,h+9.f,0};
            Vec3 f2=e.pos-right*(bw*0.5f)+right*(bw*frac)+Vec3{0,h+11.f,0};
            Vec3 f0=e.pos-right*(bw*0.5f)+Vec3{0,h+9.f,0},f3=e.pos-right*(bw*0.5f)+Vec3{0,h+11.f,0};
            verts[0]={0,0,0xff30d030,f0.x,f0.y,f0.z};verts[1]={0,0,0xff30d030,f1.x,f1.y,f1.z};verts[2]={0,0,0xff30d030,f2.x,f2.y,f2.z};
            verts[3]={0,0,0xff30d030,f0.x,f0.y,f0.z};verts[4]={0,0,0xff30d030,f2.x,f2.y,f2.z};verts[5]={0,0,0xff30d030,f3.x,f3.y,f3.z};
            sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,6,0,verts);
            sceGuEnable(GU_TEXTURE_2D);
        }
    }
    sceGuDisable(GU_BLEND);
}
void PspRenderer::overlay(const char* status,float fps){
    sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_TEXTURE_2D);sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    sceGumMatrixMode(GU_PROJECTION);sceGumLoadIdentity();sceGumOrtho(0,480,272,0,-1,1);sceGumMatrixMode(GU_VIEW);sceGumLoadIdentity();sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
    int n=0;auto flush=[&](){if(n){sceGumDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,n,0,verts);n=0;}};
    auto rect=[&](float x,float y,float w,float h,u32 c){if(n+2>4096)flush();verts[n++]={0,0,c,x,y,0};verts[n++]={0,0,c,x+w,y+h,0};};
    rect(238,134,5,1,0xc0ffffff);rect(240,132,1,5,0xc0ffffff);
    rect(0,0,480,18,0xa0000000);rect(0,254,480,18,0xa0000000);
    for(int i=0;i<nRects_;i++)rect(rects_[i].x,rects_[i].y,rects_[i].w,rects_[i].h,rects_[i].c);
    nRects_=0;
    flush();
    drawQueuedText();nLines_=0;
    sceGuEnable(GU_DEPTH_TEST);
}
void PspRenderer::render(const Dungeon& d,const Camera& cam,const char* status,float fps){
    sceGuStart(GU_DIRECT,list);sceGuClearColor(0xff100c08);sceGuClearDepth(0);sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION);sceGumLoadIdentity();sceGumPerspective(cam.fov,480.f/272.f,2.f,4096.f);
    sceGumMatrixMode(GU_VIEW);sceGumLoadIdentity();sceGumRotateX(-cam.pitch);sceGumRotateY(-cam.yaw);ScePspFVector3 neg={-cam.pos.x,-cam.pos.y,-cam.pos.z};sceGumTranslate(&neg);
    sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
    sceGuClutMode(GU_PSM_8888,0,0xff,0);sceGuClutLoad(256/8,clut);
    for(auto& r:d.resident())drawScene(r.scene,cam);
    drawEnemies(d,cam);
    overlay(status,fps);
    sceGuFinish();sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
}
void PspRenderer::swap(){sceDisplayWaitVblankStart();sceGuSwapBuffers();swaps_++;}
void PspRenderer::term(){sceGuTerm();}
} // namespace
