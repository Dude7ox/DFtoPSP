#include "../src/core/Vfs.h"
#include "../src/core/Bsa.h"
#include "../src/core/Arch3d.h"
#include "../src/core/Rdb.h"
#include "../src/core/Maps.h"
#include "../src/core/Texture.h"
#include "../src/core/Scene.h"
#include "../src/core/Dungeon.h"
#include "../src/core/Enemy.h"
#include "../src/core/SaveGame.h"
#include "../src/core/Font.h"
#include "../src/core/Ui.h"
#include "../src/core/SoftwareRenderer.h"
#include "../src/core/Random.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s line %d\n",#x,__LINE__);return 1;}}while(0)
using namespace dfpsp;
int main(int argc,char**argv){
    std::string root=argc>1?argv[1]:"data";
    Vfs vfs;vfs.addRoot(root);vfs.addRoot(root+"/ARENA2");
    Bsa arch3d,blocks,maps,monsters;
    CHECK(arch3d.open(vfs,"ARCH3D.BSA"));CHECK(arch3d.count()>0);
    CHECK(blocks.open(vfs,"BLOCKS.BSA"));CHECK(blocks.count()>1000);
    CHECK(maps.open(vfs,"MAPS.BSA"));
    bool haveMonsters=monsters.open(vfs,"MONSTER.BSA");
    Palette pal;CHECK(loadPalette(vfs,"ART_PAL.COL",pal));
    // ART_PAL.COL is already 8-bit; do not scale palette bytes by 4.
    CHECK(pal.c[1]==rgba(255,229,129));
    Texture tex;CHECK(loadTexture(vfs,2,0,pal,tex));CHECK(tex.w>0&&tex.h>0);CHECK(tex.indices.size()==(size_t)tex.w*tex.h);
    // 0x0900 dungeon records are uncompressed despite the high flag byte.
    CHECK(loadTexture(vfs,122,2,pal,tex));CHECK(tex.w==64&&tex.h==64);
    CHECK(loadTexture(vfs,122,1,pal,tex));CHECK(loadTexture(vfs,321,2,pal,tex));
    // Animated monster archives carry eight frames per move record.
    TextureInfo ti;CHECK(textureInfo(vfs,255,0,ti));CHECK(ti.frames==8);CHECK(ti.w==65&&ti.h==67);
    // Classic billboard scale: size + size*(scale/256) with scale -128 halves the rat.
    TextureRecord rec;CHECK(loadTextureRecord(vfs,255,0,rec));CHECK(rec.frames==8);
    CHECK(rec.displayWidth()==33.f);CHECK(rec.displayHeight()==34.f);
    for(int f=0;f<8;f++){Texture fr;CHECK(decodeTextureFrame(rec,f,pal,fr));CHECK(fr.indices.size()==(size_t)65*67);}
    // RLE records: row offsets are relative to the record, table at dataOffset.
    CHECK(loadTexture(vfs,253,0,pal,tex));CHECK(tex.w==17&&tex.h==13);
    CHECK(loadTexture(vfs,253,2,pal,tex));CHECK(loadTexture(vfs,253,15,pal,tex));
    // Original FONT000?.FNT faces (DFU-compatible): header, metrics, bitfields.
    {
        Font font;
        CHECK(font.load(vfs,"FONT0001.FNT"));
        CHECK(font.fixedWidth()==14&&font.fixedHeight()==14);
        CHECK(font.glyphWidth(' ')==13);       // synthesized space: fixedWidth-1
        CHECK(font.glyphWidth('!')==2);        // glyph 0 == ascii 33
        CHECK(font.glyphWidth('A')==12);
        CHECK(font.rows(' ')==nullptr);CHECK(font.rows('A')!=nullptr);
        for(char c='0';c<='9';c++)CHECK(font.glyphWidth(c)>0);
        for(char c='A';c<='Z';c++)CHECK(font.glyphWidth(c)>0);
        CHECK(font.textWidth("")==0);
        CHECK(font.textWidth("AB")==font.advance('A')+font.advance('B'));
        CHECK(font.textWidth("ABC")>font.textWidth("AB"));
        int ink=0;for(int y=0;y<font.fixedHeight();y++)for(int x=0;x<16;x++)if(font.pixel('.',x,y))ink++;
        CHECK(ink>=1&&ink<=24);                // '.' is a small but real glyph
        CHECK(!font.pixel(' ',2,2));           // space never has ink
        // Tiny FONT0003 face loads from the same parser.
        Font small;CHECK(small.load(vfs,"FONT0003.FNT"));
        CHECK(small.fixedWidth()==5&&small.fixedHeight()==7);
        // Blit: white 'A' with shadow lights pixels in the framebuffer.
        Framebuffer tfb;tfb.resize(80,24);
        drawText(tfb,font,4,4,"A",rgba(255,255,255),false);
        int lit=0;for(size_t i=0;i<tfb.color.size();i++)if(tfb.color[i]!=rgba(0,0,0))lit++;
        CHECK(lit>0);
        Framebuffer sfb;sfb.resize(80,24);
        drawText(sfb,font,4,4,"A",rgba(255,255,255),true);
        int slit=0;for(size_t i=0;i<sfb.color.size();i++)if(sfb.color[i]!=rgba(0,0,0))slit++;
        CHECK(slit>=lit);                      // the drop shadow only adds pixels
    }
    Mesh m;CHECK(loadMesh(arch3d,61018,m));CHECK(!m.verts.empty());
    RdbBlock rdb;CHECK(loadRdb(blocks,"B0000001.RDB",rdb));CHECK(!rdb.objects.empty());
    DungeonLayout l;CHECK(loadDungeonLayout(maps,16,18358,l));CHECK(l.blocks.size()==8);
    CHECK(l.dungeonType==6);   // NaturalCave for the test dungeon
    SceneBuilder sb(vfs,arch3d,blocks);Scene s;CHECK(sb.loadBlock("B0000001.RDB",s));CHECK(!s.tris.empty());
    CHECK(!s.doors.empty());CHECK(hasHeadroom(s,s.spawn,72.f));
    // Five random enemy markers and a coarse wall grid for line of sight.
    CHECK(s.markers.size()==5);
    CHECK(s.walls.size()>100);
    {
        // Synthetic wall: sight through it is blocked, over it is not.
        Scene t;
        Tri w;w.v[0]={{0,0,100},{0,0},{0,0,-1}};w.v[1]={{200,0,100},{0,0},{0,0,-1}};w.v[2]={{200,200,100},{0,0},{0,0,-1}};
        t.tris.push_back(w);
        buildWallGrid(t);
        CHECK(sceneBlocksSight(t,{100,50,0},{100,50,200}));
        CHECK(!sceneBlocksSight(t,{100,300,0},{100,300,200}));
    }
    Door* unlocked=nullptr;for(auto& d:s.doors)if(!d.locked){unlocked=&d;break;}CHECK(unlocked);
    CHECK(toggleDoor(*unlocked));updateDoors(s,2.f);CHECK(unlocked->amount>0.99f);
    CHECK(toggleDoor(*unlocked));updateDoors(s,2.f);CHECK(unlocked->amount<0.01f);
    Vec3 p=s.spawn;Vec3 q=moveWithCollision(s,p,{10.f,0,0},20.f,72.f);CHECK(std::isfinite(q.x));
    // Enemy careers come from the original MONSTER.BSA when present.
    EnemyBook book;
    if(haveMonsters){CHECK(book.load(monsters));const EnemyCareer* c=book.career(0);CHECK(c&&c->valid);CHECK(c->hitPointsPerLevel>=1);}
    DfRandom rng(1234);
    for(int i=0;i<200;i++){int id=book.rollEncounter(l.dungeonType,3,rng);CHECK(findEnemyStatic(id)!=nullptr);}
    Dungeon dungeon(vfs,arch3d,blocks,monsters);CHECK(dungeon.enter(l,3));CHECK(dungeon.resident().size()>1);CHECK(dungeon.resident().size()<=9);
    CHECK(dungeon.current()&&dungeon.current()->start);CHECK(dungeon.triangleCount()>s.tris.size());
    CHECK(!dungeon.enemies().empty());
    for(auto& e:dungeon.enemies()){CHECK(e.maxHp>=1);CHECK(e.hp<=e.maxHp);CHECK(std::isfinite(e.pos.x));}
    Vec3 sp=dungeon.spawn();CHECK(dungeon.headroom(sp,72.f));Vec3 moved=dungeon.move(sp,{16,0,0},20,72);CHECK(std::isfinite(moved.x));
    CHECK(dungeon.lineOfSight(sp,sp+Vec3{0,40,0}));
    // Simulate a while: monsters must stay inside finite bounds and sane health.
    PlayerState player;player.pos=sp;
    Vec3 fwd{0,0,1};
    for(int i=0;i<150;i++){
        player.pos=sp;
        dungeon.tick(1.f/30.f);
        int dmg=dungeon.tickEnemies(1.f/30.f,player,fwd);
        CHECK(dmg>=0);
        player.hp-=dmg;
    }
    for(auto& e:dungeon.enemies()){CHECK(std::isfinite(e.pos.x));CHECK(e.hp>=0&&e.hp<=e.maxHp);}
    // Melee: an enemy placed down the open corridor must die to repeated swings.
    Vec3 corridor{std::sin(-1.82f),0,std::cos(-1.82f)};
    {
        std::vector<Enemy> es=dungeon.enemies();
        CHECK(!es.empty());
        es[0].pos=sp+corridor*100.f;es[0].state=EnemyMove;es[0].hp=es[0].maxHp;
        dungeon.setEnemies(es,dungeon.deadKeys(),dungeon.spawnedKeys());
        int before=dungeon.enemies()[0].hp;
        int dealt=0;for(int i=0;i<40&&dealt==0;i++)dealt=dungeon.playerAttack(player,corridor);
        CHECK(dealt>0);
        CHECK(dungeon.enemies()[0].hp<before);
    }
    // Progression: defeating the placed enemy grants xp and gold.
    {
        PlayerState pr;pr.pos=sp;
        std::vector<Enemy> es=dungeon.enemies();
        es[0].pos=sp+corridor*100.f;es[0].hp=1;es[0].maxHp=1;es[0].state=EnemyMove;
        dungeon.setEnemies(es,dungeon.deadKeys(),dungeon.spawnedKeys());
        int xpBefore=pr.xp,goldBefore=pr.gold;
        for(int i=0;i<300&&dungeon.enemies()[0].state!=EnemyDead;i++)dungeon.playerAttack(pr,corridor);
        CHECK(dungeon.enemies()[0].state==EnemyDead);
        CHECK(pr.xp>xpBefore);CHECK(pr.gold>goldBefore);
    }
    // Lockpicking eventually opens a locked door and never wedges the state.
    {
        Door* locked=nullptr;
        for(auto& r:dungeon.resident())for(auto& d:r.scene.doors)if(d.locked){locked=&d;break;}
        CHECK(locked!=nullptr);
        PlayerState pr;pr.lockpick=80;pr.level=5;
        bool opened=false;
        for(int i=0;i<300&&!opened;i++)opened=dungeon.pickLock(pr,*locked);
        CHECK(opened);CHECK(!locked->locked);
    }
    // UI windows: message-log ring and shared window content.
    {
        MsgLog log;
        for(int i=0;i<40;i++)log.push("msg "+std::to_string(i));
        CHECK(log.size()==32);
        CHECK(log.lines().front()=="msg 8");
        CHECK(log.lines().back()=="msg 39");
        PlayerState ps;ps.level=3;ps.xp=60;ps.gold=120;ps.kills=7;
        auto sheet=charSheetLines(ps,"Test Place");
        CHECK(sheet.size()==6);
        CHECK(sheet[0]=="Level 3  XP 60/300");
        CHECK(sheet[4]=="Gold 120  Kills 7");
        CHECK(sheet[5]=="Test Place");
        auto help=helpLines();
        CHECK(help.size()==6);
        Font f3;CHECK(f3.load(vfs,"FONT0003.FNT"));
        for(auto& l:sheet)CHECK(f3.textWidth(l)<=336);   // fits the 360px panel
        for(auto& l:help)CHECK(f3.textWidth(l)<=336);
    }
    // Combat messages reach the log and kills are counted.
    {
        MsgLog log;dungeon.setLog(&log);
        PlayerState pr;pr.pos=sp;
        std::vector<Enemy> es=dungeon.enemies();
        es[0].pos=sp+corridor*100.f;es[0].hp=1;es[0].maxHp=1;es[0].state=EnemyMove;
        dungeon.setEnemies(es,dungeon.deadKeys(),dungeon.spawnedKeys());
        for(int i=0;i<300&&dungeon.enemies()[0].state!=EnemyDead;i++)dungeon.playerAttack(pr,corridor);
        bool death=false;for(auto& m:log.lines())if(m.find("dies")!=std::string::npos)death=true;
        CHECK(death);CHECK(pr.kills==1);
        dungeon.setLog(nullptr);
    }
    // Save / load round trip.
    {
        SaveData sd;sd.player=player;sd.player.pos=sp;sd.region=l.region;sd.exteriorId=l.exteriorId;sd.dungeonName=l.name;
        sd.player.lockpick=31;sd.player.xp=42;sd.player.gold=43;sd.player.kills=9;
        for(auto& e:dungeon.enemies())sd.enemies.push_back(fromEnemy(e));
        sd.deadKeys=dungeon.deadKeys();sd.spawnedKeys=dungeon.spawnedKeys();
        CHECK(writeSave("test_core_save.dfs",sd));
        SaveData back;CHECK(readSave("test_core_save.dfs",back));
        CHECK(back.enemies.size()==sd.enemies.size());
        CHECK(back.dungeonName==l.name);
        CHECK(back.spawnedKeys.size()==sd.spawnedKeys.size());
        CHECK(back.player.lockpick==31);CHECK(back.player.xp==42);CHECK(back.player.gold==43);CHECK(back.player.kills==9);
        CHECK(back.version==3);
        remove("test_core_save.dfs");
    }
    printf("core tests passed\n");return 0;
}
