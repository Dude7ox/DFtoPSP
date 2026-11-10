#include "Cull.h"
#include <cmath>
namespace dfpsp {
void frustumFromCamera(const Camera& cam,float aspect,float nearZ,float farZ,Frustum& out){
    float cp=std::cos(cam.pitch*M_PI/180.f),sp=std::sin(cam.pitch*M_PI/180.f);
    float cy=std::cos(cam.yaw*M_PI/180.f),sy=std::sin(cam.yaw*M_PI/180.f);
    Vec3 fwd{sy*cp,sp,cy*cp},right{cy,0,-sy},up=fwd.cross(right);
    if(up.len()<1e-6f)up=Vec3{0,1,0};else up=up*(1.f/up.len());
    float h=std::tan(cam.fov*0.5f*M_PI/180.f);          // half tan horizontal
    float v=h/ (aspect>0.01f?aspect:1.f);               // half tan vertical
    float ch=1.f/std::sqrt(1.f+h*h),sh=h*ch;
    float cv=1.f/std::sqrt(1.f+v*v),sv=v*cv;
    Vec3 n[6];
    n[0]=fwd;                                            // near
    n[1]=fwd*-1.f;                                       // far
    n[2]=fwd*ch+right*sh;                                // left
    n[3]=fwd*ch-right*sh;                                // right
    n[4]=fwd*cv+up*sv;                                   // bottom
    n[5]=fwd*cv-up*sv;                                   // top
    float d[6]={-fwd.dot(cam.pos)-nearZ,fwd.dot(cam.pos)+farZ,
                -n[2].dot(cam.pos),-n[3].dot(cam.pos),-n[4].dot(cam.pos),-n[5].dot(cam.pos)};
    for(int i=0;i<6;i++){out.p[i][0]=n[i].x;out.p[i][1]=n[i].y;out.p[i][2]=n[i].z;out.p[i][3]=d[i];}
}
bool triInView(const Tri& t,const Frustum& f){
    for(int i=0;i<6;i++){
        const float* pl=f.p[i];
        bool allOut=true;
        for(int k=0;k<3;k++){
            const Vec3& q=t.v[k].p;
            if(pl[0]*q.x+pl[1]*q.y+pl[2]*q.z+pl[3]>=0.f){allOut=false;break;}
        }
        if(allOut)return false;
    }
    return true;
}
bool sphereInView(const Vec3& c,float r,const Frustum& f){
    for(int i=0;i<6;i++){
        const float* pl=f.p[i];
        if(pl[0]*c.x+pl[1]*c.y+pl[2]*c.z+pl[3]<-r)return false;
    }
    return true;
}
} // namespace
