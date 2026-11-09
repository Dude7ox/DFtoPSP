#include "Scene.h"
#include <algorithm>
#include <cmath>
namespace dfpsp {
Vec3 rotateY(const Vec3& p,float deg){float r=deg*3.14159265358979f/180.f,c=std::cos(r),s=std::sin(r);return {p.x*c+p.z*s,p.y,-p.x*s+p.z*c};}
static bool isDoorModel(const RdbModelRef& ref){
    return ref.id!=72100 && (ref.desc=="DOR"||ref.desc=="DDR"||ref.desc=="NEW"||ref.desc=="CAV");
}
static bool wallTri(const Tri& t){
    Vec3 n=t.v[0].n+t.v[1].n+t.v[2].n;
    float nl=n.len();if(nl<1e-6f)return false;
    return std::fabs(n.y/nl)<=0.5f;   // floors, ceilings and stairs stay visible
}
void buildWallGrid(Scene& s){
    s.walls.clear();
    for(auto& t:s.tris)if(wallTri(t))s.walls.push_back(t);
    for(auto& d:s.doors)for(auto& t:d.world)if(wallTri(t))s.walls.push_back(t);
}
static bool segTri(const Vec3& o,const Vec3& d,const Tri& t){
    Vec3 e1=t.v[1].p-t.v[0].p,e2=t.v[2].p-t.v[0].p;
    Vec3 p=d.cross(e2);float det=e1.dot(p);
    if(std::fabs(det)<1e-8f)return false;
    float inv=1.f/det;Vec3 tv=o-t.v[0].p;
    float u=tv.dot(p)*inv;if(u<-0.001f||u>1.001f)return false;
    Vec3 q=tv.cross(e1);float v=d.dot(q)*inv;
    if(v<-0.001f||u+v>1.001f)return false;
    float tt=e2.dot(q)*inv;return tt>=-0.001f&&tt<=1.001f;
}
bool sceneBlocksSight(const Scene& s,const Vec3& a,const Vec3& b){
    Vec3 d=b-a;
    float dist=d.len();
    if(dist<1e-3f)return false;
    Vec3 lo{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};
    Vec3 hi{std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};
    for(auto& t:s.walls){
        Vec3 tlo{1e9f,1e9f,1e9f},thi{-1e9f,-1e9f,-1e9f};
        for(auto& v:t.v){
            tlo.x=std::min(tlo.x,v.p.x);tlo.y=std::min(tlo.y,v.p.y);tlo.z=std::min(tlo.z,v.p.z);
            thi.x=std::max(thi.x,v.p.x);thi.y=std::max(thi.y,v.p.y);thi.z=std::max(thi.z,v.p.z);
        }
        if(tlo.x>hi.x||thi.x<lo.x||tlo.y>hi.y||thi.y<lo.y||tlo.z>hi.z||thi.z<lo.z)continue;
        if(segTri(a,d,t))return true;
    }
    return false;
}
static bool segAabb(const Vec3& a,const Vec3& b,const Vec3& mn,const Vec3& mx){
    Vec3 d=b-a;
    float t0=0.f,t1=1.f;
    const float pa[3]={a.x,a.y,a.z},pb[3]={d.x,d.y,d.z},lo[3]={mn.x,mn.y,mn.z},hi[3]={mx.x,mx.y,mx.z};
    for(int i=0;i<3;i++){
        if(std::fabs(pb[i])<1e-6f){if(pa[i]<lo[i]||pa[i]>hi[i])return false;continue;}
        float ta=(lo[i]-pa[i])/pb[i],tb=(hi[i]-pa[i])/pb[i];
        if(ta>tb){float tmp=ta;ta=tb;tb=tmp;}
        t0=std::max(t0,ta);t1=std::min(t1,tb);
        if(t0>t1)return false;
    }
    return true;
}
bool doorBlocksSegment(const Door& d,const Vec3& a,const Vec3& b){
    if(d.amount>0.5f)return false;
    if(d.bmin.x>d.bmax.x)return false;
    return segAabb(a,b,d.bmin,d.bmax);
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
            if(doorModel){refreshDoor(door);
                for(auto& t:door.world)for(auto& v:t.v){
                    door.bmin.x=std::min(door.bmin.x,v.p.x);door.bmin.y=std::min(door.bmin.y,v.p.y);door.bmin.z=std::min(door.bmin.z,v.p.z);
                    door.bmax.x=std::max(door.bmax.x,v.p.x);door.bmax.y=std::max(door.bmax.y,v.p.y);door.bmax.z=std::max(door.bmax.z,v.p.z);
                }
                out.doors.push_back(std::move(door));}
        }else if(obj.type==3){
            if(obj.texArchive==199){
                if(obj.texRecord==10){start=obj.pos;haveStart=true;}
                else if(obj.texRecord==15||obj.texRecord==16){
                    // Editor enemy markers. Fixed markers carry the mobile id in
                    // the magnitude byte and the gender in the sound byte while
                    // random markers carry the editor slot in Flags.
                    SpawnMarker m;m.pos=obj.pos;m.fixed=(obj.texRecord==16);
                    m.mobileType=m.fixed?obj.magnitude:0;
                    m.passive=m.fixed&&obj.flatAction==99;
                    m.flags=obj.flatFlags;m.soundIndex=obj.soundIndex;
                    m.flatId=(obj.texArchive<<7)|obj.texRecord;
                    out.markers.push_back(m);
                }
                continue;
            }
            Flat f;f.pos=obj.pos;f.archive=obj.texArchive;f.record=obj.texRecord;f.action=obj.flatAction;
            TextureInfo ti;
            if(textureInfo(vfs_,f.archive,f.record,ti)){
                f.w=(float)(ti.w+(int)((float)ti.w*((float)ti.scaleX/256.f)));
                f.h=(float)(ti.h+(int)((float)ti.h*((float)ti.scaleY/256.f)));
            }else{f.w=f.h=32.f;}
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
    buildWallGrid(out);
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
