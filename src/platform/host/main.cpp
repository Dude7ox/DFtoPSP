#include "../../core/Vfs.h"
#include "../../core/Bsa.h"
#include "../../core/Maps.h"
#include "../../core/Dungeon.h"
#include "../../core/SoftwareRenderer.h"
#include "../../core/Font.h"
#include "../../core/Ui.h"
#include "../../core/Automap.h"
#include "../../core/SaveGame.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace dfpsp;
int main(int argc,char**argv){
    std::string root="data",block="B0000001.RDB",out="frame.bmp";int w=480,h=272;bool layout=false,dungeonMode=false;float cyaw=0.5f,cpitch=-0.1f;bool haveCam=false;Vec3 cpos{};
    std::string uiMode;int region=16,exterior=18358;float tick=0.f;std::string savePath,loadPath;int playerLevel=3;bool withEnemies=true;float place=-1.f;bool hurt=false,kill=false;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--root")&&i+1<argc)root=argv[++i];
        else if(!strcmp(argv[i],"--block")&&i+1<argc)block=argv[++i];
        else if(!strcmp(argv[i],"--out")&&i+1<argc)out=argv[++i];
        else if(!strcmp(argv[i],"--layout"))layout=true;
        else if(!strcmp(argv[i],"--dungeon"))dungeonMode=true;
        else if(!strcmp(argv[i],"--width")&&i+1<argc)w=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--height")&&i+1<argc)h=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--yaw")&&i+1<argc)cyaw=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--pitch")&&i+1<argc)cpitch=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--tick")&&i+1<argc)tick=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--save")&&i+1<argc)savePath=argv[++i];
        else if(!strcmp(argv[i],"--load")&&i+1<argc)loadPath=argv[++i];
        else if(!strcmp(argv[i],"--level")&&i+1<argc)playerLevel=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--no-enemies"))withEnemies=false;
        else if(!strcmp(argv[i],"--place")&&i+1<argc)place=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--hurt"))hurt=true;
        else if(!strcmp(argv[i],"--kill"))kill=true;
        else if(!strcmp(argv[i],"--ui")&&i+1<argc)uiMode=argv[++i];
        else if(!strcmp(argv[i],"--region")&&i+1<argc)region=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--exterior")&&i+1<argc)exterior=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--cam")&&i+3<argc){haveCam=true;cpos={(float)atof(argv[i+1]),(float)atof(argv[i+2]),(float)atof(argv[i+3])};i+=3;}
    }
    Vfs vfs;vfs.addRoot(root);vfs.addRoot(root+"/ARENA2");vfs.loadModList(root);vfs.loadModList(".");
    Bsa arch3d,blocks,maps,monsters;
    if(!arch3d.open(vfs,"ARCH3D.BSA")){fprintf(stderr,"ARCH3D.BSA missing\n");return 1;}
    if(!blocks.open(vfs,"BLOCKS.BSA")){fprintf(stderr,"BLOCKS.BSA missing\n");return 1;}
    bool haveMonsters=monsters.open(vfs,"MONSTER.BSA");
    if(layout||dungeonMode){
        if(!maps.open(vfs,"MAPS.BSA")){fprintf(stderr,"MAPS.BSA missing\n");return 1;}
        DungeonLayout l;if(!loadDungeonLayout(maps,region,exterior,l)){fprintf(stderr,"layout missing\n");return 1;}
        if(layout){printf("%s region=%d ext=%d int=%d type=%d blocks=%zu\n",l.name.c_str(),l.region,l.exteriorId,l.interiorId,l.dungeonType,l.blocks.size());for(auto&b:l.blocks)printf("  %d %d %s%s\n",b.x,b.z,b.name.c_str(),b.start?" START":"");return 0;}
        Dungeon dungeon(vfs,arch3d,blocks,monsters);
        if(!dungeon.enter(l,playerLevel)){fprintf(stderr,"dungeon load failed\n");return 1;}
        MsgLog log;dungeon.setLog(&log);log.push("You enter "+l.name+".");
        PlayerState player;player.level=playerLevel;
        Vec3 feet=dungeon.spawn();
        if(!loadPath.empty()){
            SaveData sd;
            if(readSave(loadPath,sd)){
                player=sd.player;feet=player.pos;cyaw=player.yaw;cpitch=player.pitch;
                std::vector<Enemy> es;for(auto& s:sd.enemies)es.push_back(toEnemy(s));
                dungeon.setEnemies(es,sd.deadKeys,sd.spawnedKeys);
                fprintf(stderr,"loaded save: %zu enemies, hp %d/%d\n",es.size(),player.hp,player.maxHp);
            }else fprintf(stderr,"warning: cannot read %s\n",loadPath.c_str());
        }
        player.pos=feet;
        float yaw=cyaw,pitch=cpitch;
        if(place>0.f&&withEnemies){
            // Debug helper: put the nearest monster right in front of the camera.
            std::vector<Enemy> es=dungeon.enemies();
            if(!es.empty()){
                size_t bi=0;float bd=1e9f;
                for(size_t i=0;i<es.size();i++){if(es[i].state==EnemyDead)continue;float d=(es[i].pos-feet).len();if(d<bd){bd=d;bi=i;}}
                es[bi].pos=feet+Vec3{std::sin(yaw),0,std::cos(yaw)}*place;es[bi].flying=false;
                if(hurt){es[bi].hp=es[bi].maxHp/2;es[bi].state=EnemyHurt;es[bi].hurtTimer=1.f;}
                if(kill){es[bi].hp=0;es[bi].state=EnemyDead;}
                float fy=dungeon.floor(es[bi].pos,es[bi].pos.y+96.f);if(fy>-10000.f)es[bi].pos.y=fy;
                dungeon.setEnemies(es,dungeon.deadKeys(),dungeon.spawnedKeys());
                fprintf(stderr,"placed %s at %.0f %.0f %.0f\n",dungeon.book().name(es[bi].mobileId).c_str(),es[bi].pos.x,es[bi].pos.y,es[bi].pos.z);
            }
        }
        Vec3 fwd{std::sin(yaw),0,std::cos(yaw)};
        if(tick>0.f){
            // Headless simulation so screenshots and tests see live monsters.
            int steps=(int)(tick*30.f);
            for(int i=0;i<steps;i++){
                float dt=1.f/30.f;
                player.pos=feet;
                dungeon.update(feet);dungeon.tick(dt);
                int dmg=dungeon.tickEnemies(dt,player,fwd);
                if(dmg>0){player.hp-=dmg;if(player.hp<0)player.hp=0;}
                player.attackCooldown-=dt;
            }
        }
        if(!savePath.empty()){
            SaveData sd;sd.player=player;sd.player.pos=feet;sd.player.yaw=yaw;sd.player.pitch=pitch;
            sd.region=l.region;sd.exteriorId=l.exteriorId;sd.dungeonName=l.name;
            for(auto& e:dungeon.enemies())sd.enemies.push_back(fromEnemy(e));
            sd.deadKeys=dungeon.deadKeys();sd.spawnedKeys=dungeon.spawnedKeys();
            if(writeSave(savePath,sd))fprintf(stderr,"wrote %s (%zu enemies)\n",savePath.c_str(),sd.enemies.size());
            else fprintf(stderr,"save write failed\n");
        }
        Scene merged;merged.blockName="dungeon";for(auto& r:dungeon.resident()){merged.tris.insert(merged.tris.end(),r.scene.tris.begin(),r.scene.tris.end());merged.flats.insert(merged.flats.end(),r.scene.flats.begin(),r.scene.flats.end());for(auto& d:r.scene.doors)merged.tris.insert(merged.tris.end(),d.world.begin(),d.world.end());}
        Palette pal;loadPalette(vfs,"ART_PAL.COL",pal);Camera cam;cam.pos=haveCam?cpos:feet+Vec3{0,64.f,0};cam.yaw=yaw;cam.pitch=pitch;
        Framebuffer fb;fb.resize(w,h);SoftwareRenderer rr(vfs,pal);rr.render(merged,cam,fb);
        if(withEnemies)rr.renderEnemies(dungeon.enemies(),cam,fb);
        // HUD text in the original FONT0001 face when the data provides it.
        Font font;font.load(vfs,"FONT0001.FNT");
        Font fontSmall;fontSmall.load(vfs,"FONT0003.FNT");
        if(font.loaded()){
            size_t live=0;for(auto& e:dungeon.enemies())if(e.state!=EnemyDead)live++;
            char hud[96];
            snprintf(hud,sizeof(hud),"HP %3d/%3d Lv%d %dg Aggro %zu",player.hp>0?player.hp:0,player.maxHp,player.level,player.gold,live);
            drawText(fb,font,4,3,hud,rgba(255,255,255));
            drawText(fb,font,4,h-16,l.name.c_str(),rgba(255,232,128));
            std::vector<std::string> body;bool tail=false;const char* title="";
            if(uiMode=="char"){title="CHARACTER";body=charSheetLines(player,l.name);}
            else if(uiMode=="help"){title="HELP";body=helpLines();}
            else if(uiMode=="log"){title="MESSAGE LOG";body=log.lines();tail=true;}
            else if(uiMode=="travel"){title="TRAVEL";std::vector<LocationEntry> tr;listRegionDungeons(maps,l.region,tr);int first=0;body=travelLines(tr,0,14,first,l.name);}
            else if(uiMode=="map"){
                AutomapData am;buildAutomap(dungeon,player.pos,yaw,1500.f,am);
                int w=440,h=232,x=(480-w)/2,y=(272-h)/2;
                for(int Y=y;Y<y+h;Y++)for(int X=x;X<x+w;X++)fb.color[Y*fb.w+X]=rgba(24,18,12);
                for(int X=x;X<x+w;X++){fb.color[y*fb.w+X]=rgba(150,120,70);fb.color[(y+h-1)*fb.w+X]=rgba(150,120,70);}
                for(int Y=y;Y<y+h;Y++){fb.color[Y*fb.w+x]=rgba(150,120,70);fb.color[Y*fb.w+x+w-1]=rgba(150,120,70);}
                drawText(fb,font,(480-font.textWidth("MAP"))/2,y+4,"MAP",rgba(255,232,128));
                float cxw=(am.minx+am.maxx)*0.5f,czw=(am.minz+am.maxz)*0.5f;
                float sc=(w-24)/(am.maxx-am.minx);float s2=(h-44)/(am.maxz-am.minz);if(s2<sc)sc=s2;
                float cx=x+w*0.5f,cy=y+20+(h-28)*0.5f;
                auto seg=[&](const std::vector<MapSeg>& v,u32 col){
                    for(auto& g:v)drawLine(fb,(int)(cx+(g.x0-cxw)*sc),(int)(cy-(g.z0-czw)*sc),
                                              (int)(cx+(g.x1-cxw)*sc),(int)(cy-(g.z1-czw)*sc),col);
                };
                seg(am.walls,rgba(200,180,120));seg(am.doors,rgba(120,200,120));seg(am.marker,rgba(255,255,255));
                body.clear();
            }
            if(!body.empty()){
                int total=(int)body.size();int lines=total>14?14:total;
                int w=360,wh=4+16+lines*10+6,x=(480-w)/2,y=(272-wh)/2;
                for(int Y=y;Y<y+wh;Y++)for(int X=x;X<x+w;X++)fb.color[Y*fb.w+X]=rgba(24,18,12);
                for(int X=x;X<x+w;X++){fb.color[y*fb.w+X]=rgba(150,120,70);fb.color[(y+wh-1)*fb.w+X]=rgba(150,120,70);}
                for(int Y=y;Y<y+wh;Y++){fb.color[Y*fb.w+x]=rgba(150,120,70);fb.color[Y*fb.w+x+w-1]=rgba(150,120,70);}
                drawText(fb,font,(480-font.textWidth(title))/2,y+4,title,rgba(255,232,128));
                int ly=y+20,start=tail?total-lines:0;
                for(int i=0;i<lines;i++){drawText(fb,fontSmall,x+12,ly,body[(size_t)(start+i)],rgba(255,255,255));ly+=10;}
            }
        }
        if(!writeBmp(out,fb))return 1;
        size_t alive=0,dead=0;for(auto& e:dungeon.enemies())(e.state==EnemyDead?dead:alive)++;
        fprintf(stderr,"dungeon: %zu resident blocks, %zu tris, spawn %.0f %.0f %.0f, monsters %zu alive %zu dead %zu, player Lv%d xp%d %dg (book %s)\n",
            dungeon.resident().size(),dungeon.triangleCount(),dungeon.spawn().x,dungeon.spawn().y,dungeon.spawn().z,
            dungeon.enemies().size(),alive,dead,player.level,player.xp,player.gold,haveMonsters?"MONSTER.BSA":"static");
        for(auto& e:dungeon.enemies()){
            if(e.state==EnemyDead)continue;
            Vec3 d=e.pos-feet;
            if(d.len()<900.f)fprintf(stderr,"  %s lvl%d hp%d/%d at %.0f %.0f %.0f (%.0f away)\n",dungeon.book().name(e.mobileId).c_str(),e.level,e.hp,e.maxHp,e.pos.x,e.pos.y,e.pos.z,d.len());
        }
        printf("%s\n",out.c_str());return 0;
    }
    Palette pal;loadPalette(vfs,"ART_PAL.COL",pal);
    SceneBuilder sb(vfs,arch3d,blocks);Scene scene;
    if(!sb.loadBlock(block,scene)){fprintf(stderr,"failed to load %s\n",block.c_str());return 1;}
    fprintf(stderr,"loaded %s: %zu tris, %zu models, %zu flats, %zu doors, %zu markers\n",block.c_str(),scene.tris.size(),scene.modelCount,scene.flatCount,scene.doors.size(),scene.markers.size());
    Camera cam;cam.pos=haveCam?cpos:scene.spawn+Vec3{0,64.f,0};cam.yaw=cyaw;cam.pitch=cpitch;
    Framebuffer fb;fb.resize(w,h);SoftwareRenderer rr(vfs,pal);rr.render(scene,cam,fb);
    if(!writeBmp(out,fb)){fprintf(stderr,"write failed\n");return 1;}printf("%s\n",out.c_str());return 0;
}
