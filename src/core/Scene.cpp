#include "Scene.h"
#include <algorithm>
#include <cmath>
namespace dfpsp {
static Vec3 rotateY(const Vec3& p,float deg){float r=deg*3.14159265358979f/180.f,c=std::cos(r),s=std::sin(r);return {p.x*c+p.z*s,p.y,-p.x*s+p.z*c};}
bool SceneBuilder::loadBlock(const std::string& name,Scene& out){
    RdbBlock rdb; if(!loadRdb(blocks_,name,rdb))return false;
    out=Scene{};out.blockName=name;
    for(auto& obj:rdb.objects){
        if(obj.type==1&&obj.modelIndex>=0&&obj.modelIndex<(int)rdb.refs.size()){
            u32 id=rdb.refs[obj.modelIndex].id; if(id==0)continue;
            auto it=meshCache_.find(id); if(it==meshCache_.end()){Mesh m; if(!loadMesh(arch3d_,id,m))continue; it=meshCache_.emplace(id,std::move(m)).first;}
            const Mesh& m=it->second; out.modelCount++;
            for(size_t i=0;i+2<m.verts.size();i+=3){
                Tri t; t.archive=m.texArchive[i/3];t.record=m.texRecord[i/3];
                for(int k=0;k<3;k++){Vertex v=m.verts[i+k]; v.p=rotateY(v.p,obj.rot.y)+obj.pos; v.n=rotateY(v.n,obj.rot.y); t.v[k]=v;}
                out.tris.push_back(t);
            }
        } else if(obj.type==3){
            Flat f;f.pos=obj.pos;f.archive=obj.texArchive;f.record=obj.texRecord;f.action=obj.flatAction;
            Texture tex; if(loadTexture(vfs_,f.archive,f.record,Palette{},tex)){f.w=(float)tex.w;f.h=(float)tex.h;} else {f.w=f.h=0.5f;}
            out.flats.push_back(f);out.flatCount++;
        }
    }
    // Spawn on a walkable floor tile nearest the block center.
    float bestD=1e9f; out.spawn={1024.f,1280.f,1024.f};
    for(int z=128;z<2048;z+=128)for(int x=128;x<2048;x+=128){
        float fy=floorHeight(out,{(float)x,0,(float)z},2000.f);
        if(fy<-100.f)continue;
        float d=(x-1024.f)*(x-1024.f)+(z-1024.f)*(z-1024.f);
        if(d<bestD){bestD=d;out.spawn={(float)x,fy,(float)z};}
    }
    return true;
}
static bool pointInTriXZ(const Vec3& p,const Vec3& a,const Vec3& b,const Vec3& c){
    float d1=(p.x-b.x)*(a.z-b.z)-(a.x-b.x)*(p.z-b.z);
    float d2=(p.x-c.x)*(b.z-c.z)-(b.x-c.x)*(p.z-c.z);
    float d3=(p.x-a.x)*(c.z-a.z)-(c.x-a.x)*(p.z-a.z);
    bool neg=(d1<0)||(d2<0)||(d3<0),pos=(d1>0)||(d2>0)||(d3>0);return !(neg&&pos);
}
float floorHeight(const Scene& s,const Vec3& pos,float maxY){
    float best=-1e9f;
    for(auto& t:s.tris){
        Vec3 n=(t.v[1].p-t.v[0].p).cross(t.v[2].p-t.v[0].p); float l=n.len(); if(l<1e-8f)continue; n=n*(1.f/l);
        if(std::fabs(n.y)<0.5f)continue;
        if(!pointInTriXZ(pos,t.v[0].p,t.v[1].p,t.v[2].p))continue;
        float y=t.v[0].p.y - (n.x*(pos.x-t.v[0].p.x)+n.z*(pos.z-t.v[0].p.z))/n.y;
        if(y<=maxY&&y>best)best=y;
    }
    return best;
}
static Vec3 closestPointTri(const Vec3& p,const Tri& t){
    // Ericson real-time collision detection triangle closest point.
    Vec3 a=t.v[0].p,b=t.v[1].p,c=t.v[2].p,ab=b-a,ac=c-a,ap=p-a;
    float d1=ab.dot(ap),d2=ac.dot(ap); if(d1<=0&&d2<=0)return a;
    Vec3 bp=p-b; float d3=ab.dot(bp),d4=ac.dot(bp); if(d3>=0&&d4<=d3)return b;
    float vc=d1*d4-d3*d2; if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
    Vec3 cp=p-c; float d5=ab.dot(cp),d6=ac.dot(cp); if(d6>=0&&d5<=d6)return c;
    float vb=d5*d2-d1*d6; if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
    float va=d3*d6-d5*d4; if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
    float denom=1.f/(va+vb+vc); float v=vb*denom,w=vc*denom; return a+ab*v+ac*w;
}
Vec3 moveWithCollision(const Scene& s,const Vec3& pos,const Vec3& delta,float radius,float height){
    Vec3 p=pos; int steps=std::max(1,(int)std::ceil(delta.len()/(radius*0.5f))); Vec3 step=delta*(1.f/steps);
    for(int i=0;i<steps;i++){
        p=p+step;
        for(int iter=0;iter<3;iter++){
            bool moved=false;
            Vec3 center={p.x,p.y+height*0.5f,p.z};
            for(auto& t:s.tris){
                Vec3 cp=closestPointTri(center,t); Vec3 d=center-cp; float l=d.len();
                if(l<radius&&l>1e-6f){ Vec3 push=d*(1.f/l)*(radius-l); p=p+push; center=center+push; moved=true; }
                else if(l<=1e-6f){ Vec3 n=(t.v[1].p-t.v[0].p).cross(t.v[2].p-t.v[0].p).norm(); p=p+n*radius; center=center+n*radius; moved=true; }
            }
            if(!moved)break;
        }
    }
    return p;
}
} // namespace
