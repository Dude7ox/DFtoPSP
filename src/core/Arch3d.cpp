#include "Arch3d.h"
#include "Stream.h"
#include <cstring>
#include <algorithm>
namespace dfpsp {
static int unpackUV(int u){ if(u>-14336&&u<14336&&u!=-7168)return u; int next=((u-1)>>13)+1; next<<=13; int prev=next-8192; int mult=(u-prev<next-u)?prev:next; return u-mult; }
bool loadMesh(const Bsa& bsa,u32 id,Mesh& out){
    std::vector<u8> b; if(!bsa.readId(id,b))return false; if(b.size()<64)return false;
    Reader r(b); char ver[5]={}; memcpy(ver,b.data(),4); r.seek(4); i32 pointCount=r.i32_(),planeCount=r.i32_(); r.skip(4+8); i32 planeDataOff=r.i32_(); r.skip(4+4+4+8); i32 pointListOff=r.i32_(),normalListOff=r.i32_(); r.skip(4); i32 planeListOff=r.i32_();
    bool v25=(ver[3]=='5'); if(planeCount<0||planeCount>10000||pointCount<0||pointCount>100000)return false;
    out=Mesh{};
    size_t planeCursor=(size_t)planeListOff;
    for(int f=0;f<planeCount;f++){
        size_t hp=planeCursor; if(hp+8>b.size())return false; Reader pr(b.data()+hp,8); int n=pr.u8_(); pr.u8_(); u16 tex=pr.u16_(); int archive=tex>>7, record=tex&127;
        planeCursor=hp+8+(size_t)n*8;
        if(n<3||n>64)continue;
        std::vector<Vec3> p(n); std::vector<Vec2> uv(n); std::vector<Vec3> norm(n);
        size_t pp=hp+8;
        bool ok=true;
        for(int i=0;i<n;i++){
            if(pp+(size_t)i*8+8>b.size()){ok=false;break;}
            Reader qr(b.data()+pp+(size_t)i*8,8); i32 off=qr.i32_(); int u=qr.i16_(),v=qr.i16_();
            if(id<905&&i<3){u=unpackUV(u);v=unpackUV(v);}
            size_t pos=(size_t)pointListOff+(size_t)off*(v25?3:1);
            if(pos+12>b.size()){ok=false;break;}
            Reader xr(b.data()+pos,12); p[i]={xr.i32_()/256.f,-xr.i32_()/256.f,xr.i32_()/256.f};
            size_t np=(size_t)normalListOff+(size_t)f*12; Vec3 nn{0,1,0};
            if(np+12<=b.size()){Reader nr(b.data()+np,12); nn={nr.i32_()/256.f,-nr.i32_()/256.f,nr.i32_()/256.f};}
            norm[i]=nn; uv[i]={u/16.f,v/16.f};
        }
        if(!ok)continue;
        // cumulative first three UVs
        uv[1]={uv[1].x+uv[0].x,uv[1].y+uv[0].y}; uv[2]={uv[2].x+uv[1].x,uv[2].y+uv[1].y};
        if(n>3){
            Vec3 v0=(p[1]-p[0]).norm(); Vec3 v1=p[2]-p[0]; v1=(v1-v0*v1.dot(v0)).norm();
            Vec2 q0{p[0].dot(v0),p[0].dot(v1)},q1{p[1].dot(v0),p[1].dot(v1)},q2{p[2].dot(v0),p[2].dot(v1)};
            float ax=q1.x-q0.x,ay=q1.y-q0.y,bx=q2.x-q0.x,by=q2.y-q0.y,det=ax*by-ay*bx;
            if(std::fabs(det)>1e-6f){
                for(int i=3;i<n;i++){Vec2 q{p[i].dot(v0),p[i].dot(v1)}; float dx=q.x-q0.x,dy=q.y-q0.y; float s=(dx*by-dy*bx)/det,t=(ax*dy-ay*dx)/det; uv[i]={uv[0].x+s*(uv[1].x-uv[0].x)+t*(uv[2].x-uv[0].x),uv[0].y+s*(uv[1].y-uv[0].y)+t*(uv[2].y-uv[0].y)};}
            }
        }
        for(int i=1;i+1<n;i++){
            Vertex a{p[0],uv[0],norm[0]},bb{p[i],uv[i],norm[i]},c{p[i+1],uv[i+1],norm[i+1]};
            out.verts.push_back(a);out.verts.push_back(bb);out.verts.push_back(c);
            out.texArchive.push_back(archive);out.texRecord.push_back(record);
            for(auto* v:{&a,&bb,&c}){out.min.x=std::min(out.min.x,v->p.x);out.min.y=std::min(out.min.y,v->p.y);out.min.z=std::min(out.min.z,v->p.z);out.max.x=std::max(out.max.x,v->p.x);out.max.y=std::max(out.max.y,v->p.y);out.max.z=std::max(out.max.z,v->p.z);}
        }
    }
    return !out.verts.empty();
}
} // namespace
