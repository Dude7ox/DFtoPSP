#include "SoftwareRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace dfpsp {
Texture& SoftwareRenderer::texture(int a,int r){u32 key=(u32(a)<<16)|u32(r);auto it=cache_.find(key);if(it!=cache_.end())return it->second;Texture t;loadTexture(vfs_,a,r,pal_,t);if(t.w==0){t.w=t.h=1;t.rgba={255,0,255,255};}return cache_.emplace(key,std::move(t)).first->second;}
static Vec3 toCam(const Vec3& p,const Camera& c){Vec3 q=p-c.pos;float cy=std::cos(-c.yaw),sy=std::sin(-c.yaw);float x=q.x*cy+q.z*sy,z=-q.x*sy+q.z*cy;float cp=std::cos(-c.pitch),sp=std::sin(-c.pitch);float y=q.y*cp-z*sp;z=q.y*sp+z*cp;return {x,y,z};}
void SoftwareRenderer::render(const Scene& s,const Camera& cam,Framebuffer& fb){
    std::fill(fb.color.begin(),fb.color.end(),0xff101018);std::fill(fb.depth.begin(),fb.depth.end(),0.f);
    float f=1.f/std::tan(cam.fov*3.14159265f/360.f),aspect=fb.w/(float)fb.h;
    auto proj=[&](const Vec3& v,float& sx,float& sy,float& iz){if(v.z<0.05f)return false;iz=1.f/v.z;sx=(v.x*f/aspect*iz*0.5f+0.5f)*fb.w;sy=(0.5f-v.y*f*iz*0.5f)*fb.h;return true;};
    auto drawTri=[&](const Vertex& a,const Vertex& b,const Vertex& c,Texture& tex){
        Vec3 ca=toCam(a.p,cam),cb=toCam(b.p,cam),cc=toCam(c.p,cam);
        if(ca.z<0.05f||cb.z<0.05f||cc.z<0.05f)return;
        float ax,ay,az,bx,by,bz,cx,cy,cz;proj(ca,ax,ay,az);proj(cb,bx,by,bz);proj(cc,cx,cy,cz);
        float area=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax);if(std::fabs(area)<1e-6f)return;
        int minx=std::max(0,(int)std::floor(std::min({ax,bx,cx}))),maxx=std::min(fb.w-1,(int)std::ceil(std::max({ax,bx,cx})));
        int miny=std::max(0,(int)std::floor(std::min({ay,by,cy}))),maxy=std::min(fb.h-1,(int)std::ceil(std::max({ay,by,cy})));
        float ia=1.f/area;
        for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){
            float px=x+0.5f,py=y+0.5f;
            float w0=((bx-px)*(cy-py)-(by-py)*(cx-px))*ia;
            float w1=((cx-px)*(ay-py)-(cy-py)*(ax-px))*ia;
            float w2=1-w0-w1;
            if(w0<-0.001f||w1<-0.001f||w2<-0.001f)continue;
            float iz=w0*az+w1*bz+w2*cz;int idx=y*fb.w+x;if(iz<=fb.depth[idx])continue;
            float u=(w0*a.uv.x*az+w1*b.uv.x*bz+w2*c.uv.x*cz)/iz;
            float v=(w0*a.uv.y*az+w1*b.uv.y*bz+w2*c.uv.y*cz)/iz;
            int tx=((int)std::floor(u))%tex.w,ty=((int)std::floor(v))%tex.h;if(tx<0)tx+=tex.w;if(ty<0)ty+=tex.h;
            u8* tp=&tex.rgba[(ty*tex.w+tx)*4];if(tp[3]==0)continue;
            float shade=std::min(1.f,0.25f+0.75f/ (1.f+ (1.f/iz)*0.08f));
            u8 r=(u8)(tp[0]*shade),g=(u8)(tp[1]*shade),bl=(u8)(tp[2]*shade);
            fb.color[idx]=rgba(r,g,bl);fb.depth[idx]=iz;
        }
    };
    for(auto& t:s.tris){Texture& tex=texture(t.archive,t.record);drawTri(t.v[0],t.v[1],t.v[2],tex);}
    for(auto& fl:s.flats){
        Texture& tex=texture(fl.archive,fl.record);
        Vec3 right={std::cos(cam.yaw),0,-std::sin(cam.yaw)};Vec3 up{0,1,0};
        Vec3 p0=fl.pos-right*(fl.w*0.5f)+up*fl.h;
        Vec3 p1=fl.pos+right*(fl.w*0.5f)+up*fl.h;
        Vec3 p2=fl.pos+right*(fl.w*0.5f);
        Vec3 p3=fl.pos-right*(fl.w*0.5f);
        Vertex a{p0,{0,0},{0,1,0}},b{p1,{(float)tex.w,0},{0,1,0}},c{p2,{(float)tex.w,(float)tex.h},{0,1,0}},d{p3,{0,(float)tex.h},{0,1,0}};
        drawTri(a,b,c,tex);drawTri(a,c,d,tex);
    }
}
bool writeBmp(const std::string& path,const Framebuffer& fb){FILE*f=fopen(path.c_str(),"wb");if(!f)return false;int row=(fb.w*3+3)&~3;u32 sz=54+row*fb.h;u8 hdr[54]={};hdr[0]='B';hdr[1]='M';*(u32*)(hdr+2)=sz;*(u32*)(hdr+10)=54;*(u32*)(hdr+14)=40;*(i32*)(hdr+18)=fb.w;*(i32*)(hdr+22)=fb.h;*(u16*)(hdr+26)=1;*(u16*)(hdr+28)=24;*(u32*)(hdr+34)=row*fb.h;fwrite(hdr,1,54,f);std::vector<u8> line(row);for(int y=fb.h-1;y>=0;y--){for(int x=0;x<fb.w;x++){u32 c=fb.color[y*fb.w+x];line[x*3]=getb(c);line[x*3+1]=getg(c);line[x*3+2]=getr(c);}fwrite(line.data(),1,row,f);}fclose(f);return true;}
} // namespace
