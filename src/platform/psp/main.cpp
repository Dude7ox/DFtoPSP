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
#include "../../core/SaveGame.h"
#include "PspRenderer.h"
PSP_MODULE_INFO("DFtoPSP",0,0,3);
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
    Vfs vfs;bool found=false;std::string base=".";
    for(auto r:roots){if(dirExists(r)){vfs.addRoot(r);found=true;base=r;break;}}
    if(!found)vfs.addRoot("data");vfs.addRoot("data/ARENA2");vfs.loadModList(".");
    // SAVE0.DFS sits next to the data folder so saves survive on the memory stick.
    std::string savePath=base;
    if(savePath.size()>5&&savePath.compare(savePath.size()-5,5,"/data")==0)savePath=savePath.substr(0,savePath.size()-5);
    savePath+="/SAVE0.DFS";
    Bsa arch3d,blocks,maps,monsters;
    if(!arch3d.open(vfs,"ARCH3D.BSA"))fatal("Missing data/ARENA2/ARCH3D.BSA");
    if(!blocks.open(vfs,"BLOCKS.BSA"))fatal("Missing data/ARENA2/BLOCKS.BSA");
    if(!maps.open(vfs,"MAPS.BSA"))fatal("Missing data/ARENA2/MAPS.BSA");
    bool haveMonsters=monsters.open(vfs,"MONSTER.BSA");
    Palette pal;if(!loadPalette(vfs,"ART_PAL.COL",pal))fatal("Missing ART_PAL.COL");
    DungeonLayout layout;if(!loadDungeonLayout(maps,16,18358,layout))fatal("Cannot load test dungeon layout");
    PlayerState player;
    SaveData sd;bool restored=false;
    if(readSave(savePath,sd)&&sd.dungeonName==layout.name){player=sd.player;restored=true;}
    Dungeon dungeon(vfs,arch3d,blocks,monsters);
    if(!dungeon.enter(layout,player.level))fatal("Cannot load starting dungeon block");
    if(restored){
        std::vector<Enemy> es;for(auto& s:sd.enemies)es.push_back(toEnemy(s));
        dungeon.setEnemies(es,sd.deadKeys,sd.spawnedKeys);
    }
    PspRenderer rr;if(!rr.init(vfs,pal))fatal("PSP GU renderer init failed");
    pspDebugScreenInitEx(sceGeEdramGetAddr(),PSP_DISPLAY_PIXEL_FORMAT_565,0);
    Vec3 feet=restored?player.pos:dungeon.spawn();
    float yaw=restored?player.yaw:0.5f,pitch=restored?player.pitch:-0.05f,vertical=0.f;
    bool noclip=false,help=true;
    SceCtrlData pad{},old{};u64 last=sceKernelGetSystemTimeWide();float fps=60.f;u32 frames=0;float fpsTimer=0.f;
    char status[160];status[0]=0;
    float hurtFlash=0.f;
    while(!exitRequested){
        sceCtrlReadBufferPositive(&pad,1);u32 pressed=pad.Buttons&~old.Buttons;old=pad;
        if(pad.Buttons&PSP_CTRL_HOME)break;
        u64 now=sceKernelGetSystemTimeWide();float dt=(float)(now-last)/1000000.f;last=now;if(dt>0.1f)dt=0.1f;if(dt<=0)dt=1.f/60.f;
        fpsTimer+=dt;frames++;if(fpsTimer>=0.5f){fps=frames/fpsTimer;frames=0;fpsTimer=0;}
        if(pressed&PSP_CTRL_TRIANGLE)noclip=!noclip;
        if(pressed&PSP_CTRL_SELECT)help=!help;
        if(pressed&PSP_CTRL_START){
            SaveData out;out.player=player;out.player.pos=feet;out.player.yaw=yaw;out.player.pitch=pitch;
            out.region=layout.region;out.exteriorId=layout.exteriorId;out.dungeonName=layout.name;
            for(auto& e:dungeon.enemies())out.enemies.push_back(fromEnemy(e));
            out.deadKeys=dungeon.deadKeys();out.spawnedKeys=dungeon.spawnedKeys();
            snprintf(status,sizeof(status),writeSave(savePath,out)?"SAVED":"SAVE FAILED");
        }
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
        player.attackCooldown-=dt;hurtFlash-=dt;
        dungeon.update(feet);dungeon.tick(dt);
        player.pos=feet;
        Camera cam;cam.pos=feet+Vec3{0,64.f,0};cam.yaw=yaw;cam.pitch=pitch;
        Vec3 look{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
        Door* door=dungeon.aimedDoor(cam.pos,look);
        if((pressed&PSP_CTRL_CROSS)&&door){if(!toggleDoor(*door))snprintf(status,sizeof(status),"LOCKED");}
        if((pressed&PSP_CTRL_SQUARE)&&player.attackCooldown<=0.f&&!player.dead){
            player.attackCooldown=0.8f;
            int dmg=dungeon.playerAttack(player,look);
            if(dmg>0)snprintf(status,sizeof(status),"HIT %d",dmg);
            else snprintf(status,sizeof(status),"MISS");
        }
        int hurt=dungeon.tickEnemies(dt,player,look);
        if(hurt>0){
            player.hp-=hurt;hurtFlash=0.6f;
            snprintf(status,sizeof(status),"-%d HP",hurt);
            if(player.hp<=0){
                player.hp=player.maxHp;feet=dungeon.spawn();vertical=0;
                snprintf(status,sizeof(status),"YOU DIED - back at the entrance");
            }
        }
        const char* block=dungeon.current()?dungeon.current()->name.c_str():"?";
        size_t alive=0;for(auto& e:dungeon.enemies())if(e.state!=EnemyDead&&e.aggro)alive++;
        if(!(pressed&(PSP_CTRL_CROSS|PSP_CTRL_SQUARE|PSP_CTRL_START)))
            snprintf(status,sizeof(status),"%s  %s  %.0f FPS",block,noclip?"NOCLIP":"",fps);
        rr.render(dungeon,cam,status,fps);
        // Text overlay through the SDK debug screen, synchronized to the GE draw buffer.
        pspDebugScreenSetOffset((int)rr.drawOffset());pspDebugScreenSetBackColor(0x00000000);
        pspDebugScreenSetTextColor(hurtFlash>0.f?0x0000ff00:0xffffffff);
        pspDebugScreenSetXY(0,0);pspDebugScreenPrintf("HP %3d/%3d  Lv%d  aggro %zu  %-40s",player.hp>0?player.hp:0,player.maxHp,player.level,alive,status);
        pspDebugScreenSetXY(0,32);
        if(help)pspDebugScreenPrintf("Stick move DPad look X door Sq attack O run Start save Sel help");
        else if(door)pspDebugScreenPrintf("%s",door->locked?"Locked door":"X: open / close door");
        rr.swap();
    }
    rr.term();sceKernelExitGame();return 0;
}
