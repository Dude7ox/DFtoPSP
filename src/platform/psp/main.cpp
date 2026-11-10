#include <pspuser.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspge.h>
#include <pspiofilemgr.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "../../core/Dungeon.h"
#include "../../core/SaveGame.h"
#include "../../core/Ui.h"
#include "../../core/Automap.h"
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
static bool fileOpens(const char* p){FILE* f=fopen(p,"rb");if(!f)return false;fclose(f);return true;}
static void fatal(const char* msg){pspDebugScreenInit();pspDebugScreenPrintf("DFtoPSP\n\n%s\n\nPress HOME to exit.\n",msg);while(!exitRequested)sceDisplayWaitVblankStart();sceKernelExitGame();}
// Boot diagnostics: every candidate root, the cwd and a stdio self test, so a
// device failure names the path that did not work instead of guessing.
static void fatalDiag(const char* msg,const std::string& diag){
    pspDebugScreenInit();
    pspDebugScreenPrintf("DFtoPSP\n\n%s\n\n%s\nPress HOME to exit.\n",msg,diag.c_str());
    while(!exitRequested)sceDisplayWaitVblankStart();
    sceKernelExitGame();
}
// Classic-style window panel: dark plate, thin border, FONT0001 title and
// FONT0003 body lines. `tail` shows the newest lines (message log).
static void queueWindow(PspRenderer& rr,const Font& f1,const Font& f3,const char* title,
                        const std::vector<std::string>& body,bool tail,int maxLines){
    int total=(int)body.size();int lines=total>maxLines?maxLines:total;
    if(lines<=0)return;
    int w=360,h=4+16+lines*10+6,x=(480-w)/2,y=(272-h)/2;
    rr.queueRect(x,y,w,h,rgba(24,18,12,224));
    rr.queueRect(x,y,w,1,rgba(150,120,70,255));
    rr.queueRect(x,y+h-1,w,1,rgba(150,120,70,255));
    rr.queueRect(x,y,1,h,rgba(150,120,70,255));
    rr.queueRect(x+w-1,y,1,h,rgba(150,120,70,255));
    rr.queueText((480-f1.textWidth(title))/2,y+4,title,rgba(255,232,128));
    int ly=y+20,start=tail?total-lines:0;
    for(int i=0;i<lines;i++){rr.queueText(x+12,ly,body[(size_t)(start+i)].c_str(),rgba(255,255,255),&f3);ly+=10;}
}
int main(int argc,char**argv){
    (void)argc;(void)argv;setupExit();
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    // Every layout that ever shipped: relative to the current directory, the
    // classic memstick path, internal storage (PSP Go) and the host bridge.
    // All existing candidates become roots; no first-match break, so a nested
    // data/ARENA2 layout works regardless of the emulated current directory.
    const char* cand[]={
        "data","data/ARENA2",
        "ms0:/PSP/GAME/DFtoPSP/data","ms0:/PSP/GAME/DFtoPSP/data/ARENA2",
        "ef0:/PSP/GAME/DFtoPSP/data","ef0:/PSP/GAME/DFtoPSP/data/ARENA2",
        "host0:/data","host0:/data/ARENA2"};
    Vfs vfs;std::string base;
    for(auto c:cand)if(dirExists(c)){vfs.addRoot(c);if(base.empty())base=c;}
    if(!base.empty()){std::string arena2=base+"/ARENA2";if(dirExists(arena2.c_str()))vfs.addRoot(arena2);}
    vfs.addRoot("data");vfs.addRoot("data/ARENA2");
    if(base.empty())base="data";
    // SAVE0.DFS sits next to the game folder so saves survive on the stick.
    std::string saveDir=base;
    if(saveDir.size()>7&&saveDir.compare(saveDir.size()-7,7,"/ARENA2")==0)saveDir=saveDir.substr(0,saveDir.size()-7);
    if(saveDir.size()>5&&saveDir.compare(saveDir.size()-5,5,"/data")==0)saveDir=saveDir.substr(0,saveDir.size()-5);
    if(saveDir.empty())saveDir=".";
    vfs.loadModList(base);vfs.loadModList(saveDir);vfs.loadModList(".");
    std::string savePath=saveDir+"/SAVE0.DFS";
    auto bootDiag=[&](const char* missing)->std::string{
        char cwd[128]="?";if(!getcwd(cwd,sizeof(cwd)))cwd[0]='?';
        char line[160];std::string out;
        snprintf(line,sizeof(line),"cwd=%s\n",cwd);out+=line;
        for(auto c:cand){snprintf(line,sizeof(line),"[%s] %s\n",dirExists(c)?"dir":" . ",c);out+=line;}
        for(auto& r:vfs.roots()){
            snprintf(line,sizeof(line),"[%s] %s/%s\n",fileOpens((r+"/"+missing).c_str())?"file":" . ",r.c_str(),missing);
            out+=line;
        }
        std::string tp=saveDir+"/_iotest.txt";
        FILE* f=fopen(tp.c_str(),"wb");bool wok=f&&fwrite("ok",1,2,f)==2;if(f)fclose(f);
        f=fopen(tp.c_str(),"rb");char b[4]={0,0,0,0};bool rok=f&&fread(b,1,2,f)==2&&b[0]=='o';if(f)fclose(f);
        remove(tp.c_str());
        snprintf(line,sizeof(line),"stdio wr=%d rd=%d at %s\n",wok?1:0,rok?1:0,saveDir.c_str());out+=line;
        return out;
    };
    Bsa arch3d,blocks,maps,monsters;
    if(!arch3d.open(vfs,"ARCH3D.BSA"))fatalDiag("Missing ARCH3D.BSA",bootDiag("ARCH3D.BSA"));
    if(!blocks.open(vfs,"BLOCKS.BSA"))fatalDiag("Missing BLOCKS.BSA",bootDiag("BLOCKS.BSA"));
    if(!maps.open(vfs,"MAPS.BSA"))fatalDiag("Missing MAPS.BSA",bootDiag("MAPS.BSA"));
    bool haveMonsters=monsters.open(vfs,"MONSTER.BSA");
    Palette pal;if(!loadPalette(vfs,"ART_PAL.COL",pal))fatalDiag("Missing ART_PAL.COL",bootDiag("ART_PAL.COL"));
    DungeonLayout layout;if(!loadDungeonLayout(maps,16,18358,layout))fatal("Cannot load test dungeon layout");
    PlayerState player;
    SaveData sd;bool restored=false;
    if(readSave(savePath,sd)){
        DungeonLayout saved;
        if(loadDungeonLayout(maps,sd.region,sd.exteriorId,saved)&&saved.name==sd.dungeonName){layout=saved;player=sd.player;restored=true;}
    }
    Dungeon dungeon(vfs,arch3d,blocks,monsters);
    if(!dungeon.enter(layout,player.level))fatal("Cannot load starting dungeon block");
    if(restored){
        std::vector<Enemy> es;for(auto& s:sd.enemies)es.push_back(toEnemy(s));
        dungeon.setEnemies(es,sd.deadKeys,sd.spawnedKeys);
    }
    PspRenderer rr;if(!rr.init(vfs,pal))fatal("PSP GU renderer init failed");
    Font font;font.load(vfs,"FONT0001.FNT");
    Font fontSmall;fontSmall.load(vfs,"FONT0003.FNT");
    bool haveFont=rr.initFont(font,fontSmall);
    pspDebugScreenInitEx(sceGeEdramGetAddr(),PSP_DISPLAY_PIXEL_FORMAT_565,0);
    Vec3 feet=restored?player.pos:dungeon.spawn();
    float yaw=restored?player.yaw:0.5f,pitch=restored?player.pitch:-0.05f,vertical=0.f;
    bool noclip=false;int win=0;   // 0 none, 1 help, 2 character, 3 log, 4 travel
    std::vector<LocationEntry> travel;listRegionDungeons(maps,layout.region,travel);int travelCur=0;
    SceCtrlData pad{},old{};u64 last=sceKernelGetSystemTimeWide();float fps=60.f;u32 frames=0;float fpsTimer=0.f;
    char status[160];status[0]=0;
    MsgLog log;dungeon.setLog(&log);log.push("You enter "+layout.name+".");
    float hurtFlash=0.f;
    while(!exitRequested){
        sceCtrlReadBufferPositive(&pad,1);u32 pressed=pad.Buttons&~old.Buttons;old=pad;
        if(pad.Buttons&PSP_CTRL_HOME)break;
        u64 now=sceKernelGetSystemTimeWide();float dt=(float)(now-last)/1000000.f;last=now;if(dt>0.1f)dt=0.1f;if(dt<=0)dt=1.f/60.f;
        fpsTimer+=dt;frames++;if(fpsTimer>=0.5f){fps=frames/fpsTimer;frames=0;fpsTimer=0;}
        bool world=win==0;
        if(win==4){
            if(pressed&PSP_CTRL_UP&&travelCur>0)travelCur--;
            if(pressed&PSP_CTRL_DOWN&&travelCur+1<(int)travel.size())travelCur++;
            if((pressed&PSP_CTRL_CROSS)&&!travel.empty()){
                DungeonLayout nl;
                if(loadDungeonLayout(maps,layout.region,travel[travelCur].exteriorId,nl)&&dungeon.enter(nl,player.level)){
                    layout=nl;feet=dungeon.spawn();yaw=0.5f;pitch=-0.05f;vertical=0;
                    log.push("You travel to "+layout.name+".");
                    snprintf(status,sizeof(status),"TRAVELED");
                }else snprintf(status,sizeof(status),"TRAVEL FAILED");
            }
        }
        if(pressed&PSP_CTRL_TRIANGLE)noclip=!noclip;
        if(pressed&PSP_CTRL_SELECT)win=(win+1)%6;
        if(pressed&PSP_CTRL_START){
            SaveData out;out.player=player;out.player.pos=feet;out.player.yaw=yaw;out.player.pitch=pitch;
            out.region=layout.region;out.exteriorId=layout.exteriorId;out.dungeonName=layout.name;
            for(auto& e:dungeon.enemies())out.enemies.push_back(fromEnemy(e));
            out.deadKeys=dungeon.deadKeys();out.spawnedKeys=dungeon.spawnedKeys();
            snprintf(status,sizeof(status),writeSave(savePath,out)?"SAVED":"SAVE FAILED");
        }
        float lx=world?(pad.Lx-128)/128.f:0.f,ly=world?(pad.Ly-128.f)/128.f:0.f;if(std::fabs(lx)<0.15f)lx=0;if(std::fabs(ly)<0.15f)ly=0;
        float speed=(pad.Buttons&PSP_CTRL_CIRCLE)?360.f:200.f;Vec3 fwd{std::sin(yaw),0,std::cos(yaw)},right{std::cos(yaw),0,-std::sin(yaw)};
        Vec3 delta=fwd*(-ly*speed*dt)+right*(lx*speed*dt);
        if(noclip){feet=feet+delta;feet.y+=((pad.Buttons&PSP_CTRL_RTRIGGER)?120.f:0.f)*dt-((pad.Buttons&PSP_CTRL_LTRIGGER)?120.f:0.f)*dt;}
        else{
            feet=dungeon.move(feet,delta,20.f,72.f);
            float fy=dungeon.floor(feet,feet.y+48.f);
            if(fy>-100.f){feet.y=fy;vertical=0;}else{vertical-=900.f*dt;feet.y+=vertical*dt;float below=dungeon.floor(feet,feet.y+4.f);if(below>-100.f&&feet.y<=below){feet.y=below;vertical=0;}}
            if(feet.y<-2000.f){feet=dungeon.spawn();vertical=0;}
        }
        if(world&&pad.Buttons&PSP_CTRL_LEFT)yaw-=1.8f*dt;if(world&&pad.Buttons&PSP_CTRL_RIGHT)yaw+=1.8f*dt;
        if(world&&pad.Buttons&PSP_CTRL_UP)pitch+=1.2f*dt;if(world&&pad.Buttons&PSP_CTRL_DOWN)pitch-=1.2f*dt;
        if(pitch>1.2f)pitch=1.2f;if(pitch<-1.2f)pitch=-1.2f;
        player.attackCooldown-=dt;hurtFlash-=dt;
        dungeon.update(feet);dungeon.tick(dt);
        player.pos=feet;
        Camera cam;cam.pos=feet+Vec3{0,64.f,0};cam.yaw=yaw;cam.pitch=pitch;
        Vec3 look{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
        Door* door=dungeon.aimedDoor(cam.pos,look);
        if(world&&(pressed&PSP_CTRL_CROSS)&&door){
            if(door->locked){
                if(dungeon.pickLock(player,*door)){snprintf(status,sizeof(status),"LOCK PICKED");toggleDoor(*door);}
                else snprintf(status,sizeof(status),"LOCKED");
            }else toggleDoor(*door);
        }
        if(world&&(pressed&PSP_CTRL_SQUARE)&&player.attackCooldown<=0.f&&!player.dead){
            player.attackCooldown=0.8f;
            int dmg=dungeon.playerAttack(player,look);
            if(dmg>0)snprintf(status,sizeof(status),"HIT %d",dmg);
            else snprintf(status,sizeof(status),"MISS");
        }
        int hurt=world?dungeon.tickEnemies(dt,player,look):0;
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
        char hud[96];
        snprintf(hud,sizeof(hud),"HP %3d/%3d Lv%d %dg aggro %zu %dfps%s%s",player.hp>0?player.hp:0,player.maxHp,player.level,player.gold,alive,(int)fps,status[0]?"  ":"",status);
        char hint[96];hint[0]=0;bool smallHint=false;
        if(win==0){
            if(door)snprintf(hint,sizeof(hint),"%s",door->locked?"Locked door":"X: open / close door");
            else{snprintf(hint,sizeof(hint),"Stick move DPad look X door/pick Sq attack O run Start save");smallHint=true;}
        }else{snprintf(hint,sizeof(hint),"Select: close window");smallHint=true;}
        if(haveFont){
            rr.queueText(4,2,hud,hurtFlash>0.f?rgba(255,80,80):rgba(255,255,255));
            if(hint[0])rr.queueText(4,smallHint?259:256,hint,rgba(255,232,128),smallHint?&fontSmall:nullptr);
            if(win==1)queueWindow(rr,font,fontSmall,"HELP",helpLines(),false,8);
            else if(win==2)queueWindow(rr,font,fontSmall,"CHARACTER",charSheetLines(player,layout.name),false,10);
            else if(win==3)queueWindow(rr,font,fontSmall,"MESSAGE LOG",log.lines(),true,14);
            else if(win==4){int first=0;queueWindow(rr,font,fontSmall,"TRAVEL",travelLines(travel,travelCur,12,first,layout.name),false,12);}
            else if(win==5){
                AutomapData am;buildAutomap(dungeon,feet,yaw,1500.f,am);
                int w=440,h=232,x=(480-w)/2,y=(272-h)/2;
                rr.queueRect(x,y,w,h,rgba(24,18,12,224));
                rr.queueRect(x,y,w,1,rgba(150,120,70,255));rr.queueRect(x,y+h-1,w,1,rgba(150,120,70,255));
                rr.queueRect(x,y,1,h,rgba(150,120,70,255));rr.queueRect(x+w-1,y,1,h,rgba(150,120,70,255));
                rr.queueText((480-font.textWidth("MAP"))/2,y+4,"MAP",rgba(255,232,128));
                rr.queueMap(am,x+12,y+20,w-24,h-28);
            }
        }
        rr.render(dungeon,cam,status,fps);
        if(!haveFont){
            // SDK debug screen fallback for data sets without FONT0001.FNT.
            pspDebugScreenSetOffset((int)rr.drawOffset());pspDebugScreenSetBackColor(0x00000000);
            pspDebugScreenSetTextColor(hurtFlash>0.f?0x0000ff00:0xffffffff);
            pspDebugScreenSetXY(0,0);pspDebugScreenPrintf("%s",hud);
            pspDebugScreenSetXY(0,32);pspDebugScreenPrintf("%s",hint);
        }
        rr.swap();
    }
    rr.term();sceKernelExitGame();return 0;
}
