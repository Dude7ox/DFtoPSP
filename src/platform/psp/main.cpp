#include <pspuser.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspge.h>
#include <pspiofilemgr.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "../../core/Dungeon.h"
#include "PspRenderer.h"
PSP_MODULE_INFO("DFtoPSP",0,0,2);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);
using namespace dfpsp;
static volatile int exitRequested=0;
static int exitCallback(int arg1,int arg2,void* common){(void)arg1;(void)arg2;(void)common;exitRequested=1;return 0;}
static int callbackThread(SceSize args,void* argp){(void)args;(void)argp;int cb=sceKernelCreateCallback("exit",exitCallback,nullptr);sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
static void setupExit(){int th=sceKernelCreateThread("exit_cb",callbackThread,0x11,0x1000,THREAD_ATTR_USER,0);if(th>=0)sceKernelStartThread(th,0,nullptr);}
static bool dirExists(const char* p){SceIoStat st;return sceIoGetstat(p,&st)>=0&&(st.st_mode&FIO_S_IFDIR);}
static void fatal(const char* msg){pspDebugScreenInit();pspDebugScreenPrintf("DFtoPSP\n\n%s\n\nPress HOME to exit.\n",msg);while(!exitRequested)sceDisplayWaitVblankStart();sceKernelExitGame();}
int main(int argc,char**argv){
    (void)argc;(void)argv;setupExit();
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    const char* roots[]={"data","ms0:/PSP/GAME/DFtoPSP/data","ms0:/PSP/GAME/DFtoPSP/data/ARENA2","host0:/data"};
    Vfs vfs;bool found=false;for(auto r:roots){if(dirExists(r)){vfs.addRoot(r);found=true;break;}}
    if(!found)vfs.addRoot("data");vfs.addRoot("data/ARENA2");vfs.loadModList(".");
    Bsa arch3d,blocks,maps;
    if(!arch3d.open(vfs,"ARCH3D.BSA"))fatal("Missing data/ARENA2/ARCH3D.BSA");
    if(!blocks.open(vfs,"BLOCKS.BSA"))fatal("Missing data/ARENA2/BLOCKS.BSA");
    if(!maps.open(vfs,"MAPS.BSA"))fatal("Missing data/ARENA2/MAPS.BSA");
    Palette pal;if(!loadPalette(vfs,"ART_PAL.COL",pal))fatal("Missing ART_PAL.COL");
    DungeonLayout layout;if(!loadDungeonLayout(maps,16,18358,layout))fatal("Cannot load test dungeon layout");
    Dungeon dungeon(vfs,arch3d,blocks);if(!dungeon.enter(layout))fatal("Cannot load starting dungeon block");
    PspRenderer rr;if(!rr.init(vfs,pal))fatal("PSP GU renderer init failed");
    pspDebugScreenInitEx(sceGeEdramGetAddr(),PSP_DISPLAY_PIXEL_FORMAT_565,0);
    Vec3 feet=dungeon.spawn();float yaw=0.5f,pitch=-0.05f,vertical=0.f;bool noclip=false,help=true;
    SceCtrlData pad{},old{};u64 last=sceKernelGetSystemTimeWide();float fps=60.f;u32 frames=0;float fpsTimer=0.f;
    char status[160];
    while(!exitRequested){
        sceCtrlReadBufferPositive(&pad,1);u32 pressed=pad.Buttons&~old.Buttons;old=pad;
        if(pad.Buttons&PSP_CTRL_HOME)break;
        u64 now=sceKernelGetSystemTimeWide();float dt=(float)(now-last)/1000000.f;last=now;if(dt>0.1f)dt=0.1f;if(dt<=0)dt=1.f/60.f;
        fpsTimer+=dt;frames++;if(fpsTimer>=0.5f){fps=frames/fpsTimer;frames=0;fpsTimer=0;}
        if(pressed&PSP_CTRL_TRIANGLE)noclip=!noclip;
        if(pressed&PSP_CTRL_START){feet=dungeon.spawn();yaw=0.5f;pitch=-0.05f;vertical=0;}
        if(pressed&PSP_CTRL_SELECT)help=!help;
        float lx=(pad.Lx-128)/128.f,ly=(pad.Ly-128.f)/128.f;if(std::fabs(lx)<0.15f)lx=0;if(std::fabs(ly)<0.15f)ly=0;
        float speed=(pad.Buttons&PSP_CTRL_CIRCLE)?360.f:200.f;Vec3 fwd{std::sin(yaw),0,std::cos(yaw)},right{std::cos(yaw),0,-std::sin(yaw)};
        Vec3 delta=fwd*(-ly*speed*dt)+right*(lx*speed*dt);
        if(noclip){feet=feet+delta;feet.y+=((pad.Buttons&PSP_CTRL_RTRIGGER)?120.f:0.f)*dt-((pad.Buttons&PSP_CTRL_LTRIGGER)?120.f:0.f)*dt;}
        else{
            feet=dungeon.move(feet,delta,20.f,72.f);
            float fy=dungeon.floor(feet,feet.y+48.f);
            if(fy>-100.f){feet.y=fy;vertical=0;}else{vertical-=900.f*dt;feet.y+=vertical*dt;float below=dungeon.floor(feet,feet.y+4.f);if(below>-100.f&&feet.y<=below){feet.y=below;vertical=0;}}
            if(feet.y<-2000.f){feet=dungeon.spawn();vertical=0;}
        }
        if(pad.Buttons&PSP_CTRL_LEFT)yaw-=1.8f*dt;if(pad.Buttons&PSP_CTRL_RIGHT)yaw+=1.8f*dt;
        if(pad.Buttons&PSP_CTRL_UP)pitch+=1.2f*dt;if(pad.Buttons&PSP_CTRL_DOWN)pitch-=1.2f*dt;
        if(pitch>1.2f)pitch=1.2f;if(pitch<-1.2f)pitch=-1.2f;
        dungeon.update(feet);dungeon.tick(dt);
        Camera cam;cam.pos=feet+Vec3{0,64.f,0};cam.yaw=yaw;cam.pitch=pitch;
        Vec3 look{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
        Door* door=dungeon.aimedDoor(cam.pos,look);
        if((pressed&PSP_CTRL_CROSS)&&door){if(!toggleDoor(*door))snprintf(status,sizeof(status),"LOCKED");}
        const char* block=dungeon.current()?dungeon.current()->name.c_str():"?";
        if(!(pressed&PSP_CTRL_CROSS))snprintf(status,sizeof(status),"%s  %s  %.0f FPS",block,noclip?"NOCLIP":"",fps);
        rr.render(dungeon,cam,status,fps);
        // Text overlay through the SDK debug screen, synchronized to the GE draw buffer.
        pspDebugScreenSetOffset((int)rr.drawOffset());pspDebugScreenSetBackColor(0x00000000);pspDebugScreenSetTextColor(0xffffffff);
        pspDebugScreenSetXY(0,0);pspDebugScreenPrintf("%-67s",status);
        pspDebugScreenSetXY(0,32);
        if(help)pspDebugScreenPrintf("Stick move  DPad look  X door  O run  Tri noclip  Start reset  Home exit");
        else if(door)pspDebugScreenPrintf("%s",door->locked?"Locked door":"X: open / close door");
        rr.swap();
    }
    rr.term();sceKernelExitGame();return 0;
}
