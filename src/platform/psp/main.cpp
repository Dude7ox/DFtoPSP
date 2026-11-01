#include <pspuser.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "../../core/Vfs.h"
#include "../../core/Bsa.h"
#include "../../core/Scene.h"
#include "PspRenderer.h"
PSP_MODULE_INFO("DFtoPSP",0,0,1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);
using namespace dfpsp;
static bool dirExists(const char* p){SceIoStat st;return sceIoGetstat(p,&st)>=0&&(st.st_mode&FIO_S_IFDIR);}
int main(int argc,char**argv){
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    const char* roots[]={"data","ms0:/PSP/GAME/DFtoPSP/data","ms0:/PSP/GAME/DFtoPSP/data/ARENA2","host0:/data"};
    Vfs vfs;bool found=false;
    for(auto r:roots){if(dirExists(r)){vfs.addRoot(r);found=true;break;}}
    if(!found)vfs.addRoot("data");
    vfs.addRoot("data/ARENA2");vfs.loadModList(".");
    Bsa arch3d,blocks;
    if(!arch3d.open(vfs,"ARCH3D.BSA")||!blocks.open(vfs,"BLOCKS.BSA")){
        // fatal: show debug screen
        pspDebugScreenInit();pspDebugScreenPrintf("DFtoPSP: missing ARCH3D.BSA/BLOCKS.BSA\n");
        while(1)sceDisplayWaitVblankStart();
    }
    Palette pal;loadPalette(vfs,"ART_PAL.COL",pal);
    SceneBuilder sb(vfs,arch3d,blocks);Scene scene;
    if(!sb.loadBlock("B0000001.RDB",scene)){pspDebugScreenInit();pspDebugScreenPrintf("Failed to load B0000001.RDB\n");while(1)sceDisplayWaitVblankStart();}
    PspRenderer rr;rr.init(vfs,pal);
    Camera cam;cam.pos=scene.spawn+Vec3{0,64.f,0};cam.yaw=0.5f;
    SceCtrlData pad;u32 last=0;
    while(true){
        sceCtrlReadBufferPositive(&pad,1);
        if(pad.Buttons&PSP_CTRL_HOME)break;
        u32 now=sceKernelGetSystemTimeLow();float dt=(now-last)/1000000.f;if(dt>0.1f)dt=0.1f;last=now;
        float lx=(pad.Lx-128)/128.f,ly=(pad.Ly-128)/128.f;
        float speed=200.f*dt;Vec3 fwd{std::sin(cam.yaw),0,std::cos(cam.yaw)},right{std::cos(cam.yaw),0,-std::sin(cam.yaw)};
        Vec3 delta=fwd*(-ly*speed)+right*(lx*speed);
        cam.pos=moveWithCollision(scene,cam.pos,delta,20.f,64.f);
        float fy=floorHeight(scene,cam.pos,cam.pos.y+50.f);if(fy>-100.f)cam.pos.y=fy+64.f;
        if(pad.Buttons&PSP_CTRL_LEFT)cam.yaw-=1.5f*dt;
        if(pad.Buttons&PSP_CTRL_RIGHT)cam.yaw+=1.5f*dt;
        if(pad.Buttons&PSP_CTRL_UP)cam.pitch+=1.f*dt;
        if(pad.Buttons&PSP_CTRL_DOWN)cam.pitch-=1.f*dt;
        rr.render(scene,cam);rr.swap();
    }
    rr.term();sceKernelExitGame();return 0;
}
