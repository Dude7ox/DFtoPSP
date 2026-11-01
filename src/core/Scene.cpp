#include "Scene.h"
#include <algorithm>
#include <cmath>
namespace dfpsp {
Vec3 rotateY(const Vec3& p,float deg){float r=deg*3.14159265358979f/180.f,c=std::cos(r),s=std::sin(r);return {p.x*c+p.z*s,p.y,-p.x*s+p.z*c};}
static bool isDoorModel(const RdbModelRef& ref){
    return ref.id!=72100 && (ref.desc=="DOR"||ref.desc=="DDR"||ref.desc=="NEW"||ref.desc=="CAV");
}
void refreshDoor(Door& d){
    d.world=d.local;
    for(auto& t:d.world)for(auto& v:t.v){v.p=rotateY(v.p,d.yaw-90.f*d.amount)+d.pos;v.n=rotateY(v.n,d.yaw-90.f*d.amount);}
}
void updateDoors(Scene& s,float dt){
    for(auto& d:s.doors){float old=d.amount;float step=dt/1.5f;d.amount+=std::max(-step,std::min(step,d.target-d.amount));if(d.amount!=old)refreshDoor(d);}
}
Door* aimedDoor(Scene& s,const Vec3& eye,const Vec3& forward,float range){
    Door* best=nullptr;float bestScore=-1.f;
    for(auto& d:s.doors){
        Vec3 center=d.pos+Vec3{0,64,0};Vec3 delta=center-eye;float dist=delta.len();if(dist>range)continue;
        float score=delta.norm().dot(forward);if(score<0.65f)continue;
        // Prefer the center of the door, but also allow interaction near its edge.
        bool found=score>0.9f;
        for(auto& t:d.world)for(auto& v:t.v){Vec3 q=v.p-eye;if(q.len()<range&&q.norm().dot(forward)>0.96f)found=true;}
        if(found&&score>bestScore){best=&d;bestScore=score;}
    }
    return best;
}
bool toggleDoor(Door& d){if(d.locked||!d.world.empty()&&d.amount>0.01f&&d.amount<0.99f)return false;d.target=d.target>0.5f?0.f:1.f;return true;}
bool SceneBuilder::loadBlock(const std::string& name,Scene& out){
    RdbBlock rdb; if(!loadRdb(blocks_,name,rdb))return false;
    out=Scene{};out.blockName=name;
    bool haveStart=false;Vec3 start;
    for(auto& obj:rdb.objects){
        if(obj.type==1&&obj.modelIndex>=0&&obj.modelIndex<(int)rdb.refs.size()){
            const auto& ref=rdb.refs[obj.modelIndex];u32 id=ref.id;if(id==0)continue;
            if(meshCache_.size()>192)meshCache_.clear();
            auto it=meshCache_.find(id);if(it==meshCache_.end()){Mesh m;if(!loadMesh(arch3d_,id,m))continue;it=meshCache_.emplace(id,std::move(m)).first;}
            const Mesh& m=it->second;out.modelCount++;
            bool doorModel=isDoorModel(ref);Door door;
            if(doorModel){door.pos=obj.pos;door.yaw=obj.rot.y;door.locked=(obj.trigger>>4)!=0;}
            for(size_t i=0;i+2<m.verts.size();i+=3){
                Tri t;t.archive=m.texArchive[i/3];t.record=m.texRecord[i/3];
                for(int k=0;k<3;k++)t.v[k]=m.verts[i+k];
                if(doorModel)door.local.push_back(t);
                else {for(auto& v:t.v){v.p=rotateY(v.p,obj.rot.y)+obj.pos;v.n=rotateY(v.n,obj.rot.y);}out.tris.push_back(t);}
            }
            if(doorModel){refreshDoor(door);out.doors.push_back(std::move(door));}
        }else if(obj.type==3){
            if(obj.texArchive==199){if(obj.texRecord==10){start=obj.pos;haveStart=true;}continue;}
            Flat f;f.pos=obj.pos;f.archive=obj.texArchive;f.record=obj.texRecord;f.action=obj.flatAction;
            Texture tex;Palette pal;if(loadPalette(vfs_,"ART_PAL.COL",pal)&&loadTexture(vfs_,f.archive,f.record,pal,tex)){f.w=(float)tex.w;f.h=(float)tex.h;}else{f.w=f.h=32.f;}
            out.flats.push_back(f);out.flatCount++;
        }
    }
    // Prefer the original start marker. Otherwise choose the lowest floor with
    // headroom near the center; choosing the highest surface puts the player on
    // the ceiling in multi-level blocks.
    out.spawn={1024.f,1024.f,1024.f};
    if(haveStart&&floorHeight(out,start,start.y+32.f)>-100.f){out.spawn=start;}
    else{
        float best=1e9f;bool found=false;
        for(int z=128;z<2048;z+=64)for(int x=128;x<2048;x+=64){
            Vec3 p{(float)x,0,(float)z};
            // Scan horizontal slabs from low to high, retaining the first clear floor.
            for(float ceiling=256.f;ceiling<=2048.f;ceiling+=256.f){
                float y=floorHeight(out,p,ceiling);if(y<-100.f)continue;p.y=y;
                if(!hasHeadroom(out,p,80.f))continue;
                float score=(x-1024.f)*(x-1024.f)+(z-1024.f)*(z-1024.f)+y*0.1f;
                if(score<best){best=score;out.spawn=p;found=true;}break;
            }
        }
        if(!found)out.spawn={1024.f,1024.f,1024.f};
    }
    return true;
}
static bool pointInTriXZ(const Vec3& p,const Vec3& a,const Vec3& b,const Vec3& c){
    float d1=(p.x-b.x)*(a.z-b.z)-(a.x-b.x)*(p.z-b.z),d2=(p.x-c.x)*(b.z-c.z)-(b.x-c.x)*(p.z-c.z),d3=(p.x-a.x)*(c.z-a.z)-(c.x-a.x)*(p.z-a.z);
    return !(((d1<0)||(d2<0)||(d3<0))&&((d1>0)||(d2>0)||(d3>0)));
}
static float triFloor(const Tri& t,const Vec3& pos,float maxY){
    Vec3 n=(t.v[1].p-t.v[0].p).cross(t.v[2].p-t.v[0].p);float l=n.len();if(l<1e-6f)return -1e9f;n=n*(1.f/l);
    if(std::fabs(n.y)<0.65f||!pointInTriXZ(pos,t.v[0].p,t.v[1].p,t.v[2].p))return -1e9f;
    float y=t.v[0].p.y-(n.x*(pos.x-t.v[0].p.x)+n.z*(pos.z-t.v[0].p.z))/n.y;
    return y<=maxY?y:-1e9f;
}
float floorHeight(const Scene& s,const Vec3& pos,float maxY){float best=-1e9f;for(auto& t:s.tris)best=std::max(best,triFloor(t,pos,maxY));return best;}
static Vec3 closestPointTri(const Vec3& p,const Tri& t){
    Vec3 a=t.v[0].p,b=t.v[1].p,c=t.v[2].p,ab=b-a,ac=c-a,ap=p-a;
    float d1=ab.dot(ap),d2=ac.dot(ap);if(d1<=0&&d2<=0)return a;
    Vec3 bp=p-b;float d3=ab.dot(bp),d4=ac.dot(bp);if(d3>=0&&d4<=d3)return b;
    float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
    Vec3 cp=p-c;float d5=ab.dot(cp),d6=ac.dot(cp);if(d6>=0&&d5<=d6)return c;
    float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
    float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
    float denom=va+vb+vc;if(std::fabs(denom)<1e-9f)return a;float v=vb/denom,w=vc/denom;return a+ab*v+ac*w;
}
static void collideTriangles(Vec3& p,const std::vector<Tri>& tris,float radius,float height){
    for(int iter=0;iter<4;iter++){
        bool moved=false;
        for(auto& t:tris){
            float lo=std::min({t.v[0].p.y,t.v[1].p.y,t.v[2].p.y}),hi=std::max({t.v[0].p.y,t.v[1].p.y,t.v[2].p.y});
            if(hi<=p.y+8.f||lo>=p.y+height)continue;
            Vec3 n=(t.v[1].p-t.v[0].p).cross(t.v[2].p-t.v[0].p).norm();if(std::fabs(n.y)>0.65f)continue;
            float low=std::max(p.y+radius,lo),high=std::min(p.y+height-radius,hi);if(low>high)continue;
            for(float y:{low,(low+high)*0.5f,high}){
                Vec3 center{p.x,y,p.z},cp=closestPointTri(center,t),d=center-cp;d.y=0;float l=d.len();
                if(l<radius){Vec3 push=l>1e-5f?d*(1.f/l):Vec3{n.x,0,n.z};if(push.len()<1e-5f)continue;push=push.norm()*(radius-l+0.05f);p=p+push;moved=true;}
            }
        }
        if(!moved)break;
    }
}
Vec3 moveWithCollision(const Scene& s,const Vec3& pos,const Vec3& delta,float radius,float height){
    Vec3 p=pos;int steps=std::max(1,(int)std::ceil(delta.len()/(radius*0.5f)));steps=std::min(steps,64);Vec3 step=delta*(1.f/steps);
    for(int i=0;i<steps;i++){p=p+step;collideTriangles(p,s.tris,radius,height);for(auto& d:s.doors)collideTriangles(p,d.world,radius,height);}return p;
}
bool hasHeadroom(const Scene& s,const Vec3& feet,float height){
    auto clear=[&](const std::vector<Tri>& tris){for(auto& t:tris){float lo=std::min({t.v[0].p.y,t.v[1].p.y,t.v[2].p.y}),hi=std::max({t.v[0].p.y,t.v[1].p.y,t.v[2].p.y});if(hi<=feet.y+8.f||lo>=feet.y+height)continue;for(float y:{feet.y+24.f,feet.y+height-4.f})if(closestPointTri({feet.x,y,feet.z},t).len()<18.f)return false;}return true;};
    if(!clear(s.tris))return false;for(auto& d:s.doors)if(!clear(d.world))return false;return true;
}
} // namespace
