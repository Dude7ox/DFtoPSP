#include "SoftwareRenderer.h"
#include "Enemy.h"
#include "Font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace dfpsp {
Texture& SoftwareRenderer::texture(int a,int r){return texture(a,r,0);}
Texture& SoftwareRenderer::texture(int a,int r,int frame){
    u32 key=(((u32(a)<<7)|u32(r&127))<<4)|u32(frame&15);
    auto it=cache_.find(key);if(it!=cache_.end())return it->second;
    if(cache_.size()>384)cache_.clear();
    Texture t;loadTexture(vfs_,a,r,pal_,t,frame);
    if(t.w==0){t.w=t.h=1;t.rgba={255,0,255,255};}
    return cache_.emplace(key,std::move(t)).first->second;
}
static Vec3 toCam(const Vec3& p,const Camera& c){Vec3 q=p-c.pos;float cy=std::cos(-c.yaw),sy=std::sin(-c.yaw);float x=q.x*cy+q.z*sy,z=-q.x*sy+q.z*cy;float cp=std::cos(-c.pitch),sp=std::sin(-c.pitch);float y=q.y*cp-z*sp;z=q.y*sp+z*cp;return {x,y,z};}
namespace {
struct Raster {
    const Camera* cam; Framebuffer* fb; float f, aspect;
    void begin(){f=1.f/std::tan(cam->fov*3.14159265f/360.f);aspect=fb->w/(float)fb->h;}
    bool proj(const Vec3& v,float& sx,float& sy,float& iz)const{
        if(v.z<0.05f)return false;iz=1.f/v.z;
        sx=(v.x*f/aspect*iz*0.5f+0.5f)*fb->w;sy=(0.5f-v.y*f*iz*0.5f)*fb->h;return true;
    }
    void tri(const Vertex& a,const Vertex& b,const Vertex& c,Texture& tex){
        Vec3 ca=toCam(a.p,*cam),cb=toCam(b.p,*cam),cc=toCam(c.p,*cam);
        if(ca.z<0.05f||cb.z<0.05f||cc.z<0.05f)return;
        float ax,ay,az,bx,by,bz,cx,cy,cz;proj(ca,ax,ay,az);proj(cb,bx,by,bz);proj(cc,cx,cy,cz);
        float area=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax);if(std::fabs(area)<1e-6f)return;
        int minx=std::max(0,(int)std::floor(std::min({ax,bx,cx}))),maxx=std::min(fb->w-1,(int)std::ceil(std::max({ax,bx,cx})));
        int miny=std::max(0,(int)std::floor(std::min({ay,by,cy}))),maxy=std::min(fb->h-1,(int)std::ceil(std::max({ay,by,cy})));
        float ia=1.f/area;
        for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){
            float px=x+0.5f,py=y+0.5f;
            float w0=((bx-px)*(cy-py)-(by-py)*(cx-px))*ia;
            float w1=((cx-px)*(ay-py)-(cy-py)*(ax-px))*ia;
            float w2=1-w0-w1;
            if(w0<-0.001f||w1<-0.001f||w2<-0.001f)continue;
            float iz=w0*az+w1*bz+w2*cz;int idx=y*fb->w+x;if(iz<=fb->depth[idx])continue;
            float u=(w0*a.uv.x*az+w1*b.uv.x*bz+w2*c.uv.x*cz)/iz;
            float v=(w0*a.uv.y*az+w1*b.uv.y*bz+w2*c.uv.y*cz)/iz;
            int tx=((int)std::floor(u))%tex.w,ty=((int)std::floor(v))%tex.h;if(tx<0)tx+=tex.w;if(ty<0)ty+=tex.h;
            u8* tp=&tex.rgba[(ty*tex.w+tx)*4];if(tp[3]==0)continue;
            float shade=std::min(1.f,0.25f+0.75f/(1.f+(1.f/iz)*0.08f));
            u8 r=(u8)(tp[0]*shade),g=(u8)(tp[1]*shade),bl=(u8)(tp[2]*shade);
            fb->color[idx]=rgba(r,g,bl);fb->depth[idx]=iz;
        }
    }
    void quad(const Vec3& p0,const Vec3& p1,const Vec3& p2,const Vec3& p3,float u0,float u1,Texture& tex){
        Vertex a{p0,{u0,0},{0,1,0}},b{p1,{u1,0},{0,1,0}},c{p2,{u1,(float)tex.h},{0,1,0}},d{p3,{u0,(float)tex.h},{0,1,0}};
        tri(a,b,c,tex);tri(a,c,d,tex);
    }
};
} // namespace
void SoftwareRenderer::render(const Scene& s,const Camera& cam,Framebuffer& fb){
    std::fill(fb.color.begin(),fb.color.end(),0xff101018);std::fill(fb.depth.begin(),fb.depth.end(),0.f);
    Raster r;r.cam=&cam;r.fb=&fb;r.begin();
    for(auto& t:s.tris)r.tri(t.v[0],t.v[1],t.v[2],texture(t.archive,t.record));
    for(auto& fl:s.flats){
        Texture& tex=texture(fl.archive,fl.record);
        Vec3 right{std::cos(cam.yaw),0,-std::sin(cam.yaw)};
        Vec3 p0=fl.pos-right*(fl.w*0.5f)+Vec3{0,fl.h,0};
        Vec3 p1=fl.pos+right*(fl.w*0.5f)+Vec3{0,fl.h,0};
        Vec3 p2=fl.pos+right*(fl.w*0.5f);
        Vec3 p3=fl.pos-right*(fl.w*0.5f);
        r.quad(p0,p1,p2,p3,0,(float)tex.w,tex);
    }
}
void SoftwareRenderer::renderEnemies(const std::vector<Enemy>& enemies,const Camera& cam,Framebuffer& fb){
    Raster r;r.cam=&cam;r.fb=&fb;r.begin();
    Vec3 right{std::cos(cam.yaw),0,-std::sin(cam.yaw)};
    for(auto& e:enemies){
        Vec3 to=e.pos-cam.pos;
        if(to.len()>2600.f)continue;
        const EnemyStatic* st=findEnemyStatic(e.mobileId);
        int archive,record,frame=e.frame;
        if(e.state==EnemyDead){
            if(!st)continue;
            archive=st->corpseArchive;record=st->corpseRecord;frame=0;
        }else{
            if(!st)continue;
            archive=(e.female&&st->femaleArchive)?st->femaleArchive:st->archive;
            record=enemyRecord(e);
        }
        Texture& tex=texture(archive,record,frame);
        float w=e.bodyW,h=e.bodyH;
        if(e.state==EnemyDead)h=std::min(h,w*0.6f);
        Vec3 p0=e.pos-right*(w*0.5f)+Vec3{0,h,0};
        Vec3 p1=e.pos+right*(w*0.5f)+Vec3{0,h,0};
        Vec3 p2=e.pos+right*(w*0.5f);
        Vec3 p3=e.pos-right*(w*0.5f);
        float u0=0,u1=(float)tex.w;
        if(e.mirrored&&e.state!=EnemyDead){u0=(float)tex.w;u1=0;}
        r.quad(p0,p1,p2,p3,u0,u1,tex);
        if(e.state!=EnemyDead&&e.hp<e.maxHp){
            Vec3 head=e.pos+Vec3{0,h+10.f,0};
            Vec3 cv=toCam(head,cam);float sx,sy,iz;
            if(!r.proj(cv,sx,sy,iz))continue;
            int bw=26,bh=3;
            int x0=(int)sx-bw/2,y0=(int)sy;
            float frac=e.maxHp>0?(float)e.hp/e.maxHp:0.f;
            for(int y=y0;y<y0+bh;y++)for(int x=x0;x<x0+bw;x++){
                if(x<0||y<0||x>=fb.w||y>=fb.h)continue;
                int idx=y*fb.w+x;
                if(fb.depth[idx]>iz*1.05f)continue;
                u32 c=0xff401010;
                if(y==y0+1&&x-x0<(int)(frac*(bw-2))+1)c=0xff30d030;
                fb.color[idx]=c;
            }
        }
    }
}
bool writeBmp(const std::string& path,const Framebuffer& fb){FILE*f=fopen(path.c_str(),"wb");if(!f)return false;int row=(fb.w*3+3)&~3;u32 sz=54+row*fb.h;u8 hdr[54]={};hdr[0]='B';hdr[1]='M';*(u32*)(hdr+2)=sz;*(u32*)(hdr+10)=54;*(u32*)(hdr+14)=40;*(i32*)(hdr+18)=fb.w;*(i32*)(hdr+22)=fb.h;*(u16*)(hdr+26)=1;*(u16*)(hdr+28)=24;*(u32*)(hdr+34)=row*fb.h;fwrite(hdr,1,54,f);std::vector<u8> line(row);for(int y=fb.h-1;y>=0;y--){for(int x=0;x<fb.w;x++){u32 c=fb.color[y*fb.w+x];line[x*3]=getb(c);line[x*3+1]=getg(c);line[x*3+2]=getr(c);}fwrite(line.data(),1,row,f);}fclose(f);return true;}
void drawLine(Framebuffer& fb,int x0,int y0,int x1,int y1,u32 argb){
    int dx=x1-x0,dy=y1-y0;int steps=std::abs(dx)>std::abs(dy)?std::abs(dx):std::abs(dy);if(steps<=0)steps=1;
    for(int i=0;i<=steps;i++){
        int x=x0+dx*i/steps,y=y0+dy*i/steps;
        if(x<0||y<0||x>=fb.w||y>=fb.h)continue;
        fb.color[y*fb.w+x]=argb;
    }
}
void drawText(Framebuffer& fb,const Font& f,int x,int y,const std::string& s,u32 argb,bool shadow){
    if(!f.loaded())return;
    int px=x;
    for(char c:s){
        int w=f.glyphWidth(c);
        for(int gy=0;gy<f.fixedHeight();gy++){
            for(int gx=0;gx<w;gx++){
                if(!f.pixel(c,gx,gy))continue;
                if(shadow){int X=px+gx+1,Y=y+gy+1;if(X>=0&&X<fb.w&&Y>=0&&Y<fb.h)fb.color[Y*fb.w+X]=rgba(0,0,0);}
                int X=px+gx,Y=y+gy;if(X>=0&&X<fb.w&&Y>=0&&Y<fb.h)fb.color[Y*fb.w+X]=argb;
            }
        }
        px+=f.advance(c);
    }
}
} // namespace dfpsp
