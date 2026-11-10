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
#include "../src/core/ItemDefs.h"
#include "../src/core/Font.h"
#include "../src/core/Ui.h"
#include "../src/core/Automap.h"
#include "../src/core/Cull.h"
#include "../src/core/Midi.h"
#include "../src/core/Synth.h"
#include "../src/core/Sfx.h"
#include "../src/core/Audio.h"
#include "../src/core/SoftwareRenderer.h"
#include "../src/core/Random.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s line %d\n",#x,__LINE__);return 1;}}while(0)
using namespace dfpsp;
static const double kPi=3.14159265358979323846;
// Single frequency magnitude over interleaved stereo samples (left channel).
static double goertzel(const std::vector<i16>& pcm,size_t start,size_t n,double freq,int rate){
    double w=2.0*kPi*freq/(double)rate,coeff=2.0*std::cos(w),s1=0,s2=0;
    for(size_t i=0;i<n&&start+i<pcm.size()/2;i++){
        double x=(double)pcm[(start+i)*2],s0=x+coeff*s1-s2;s2=s1;s1=s0;
    }
    return std::sqrt(s1*s1+s2*s2-coeff*s1*s2)/(double)n;
}
static double rms16(const std::vector<i16>& pcm){
    if(pcm.empty())return 0;double sum=0;
    for(size_t i=0;i<pcm.size();i++)sum+=(double)pcm[i]*pcm[i];
    return std::sqrt(sum/(double)pcm.size());
}
static int peak16(const std::vector<i16>& pcm){
    int p=0;for(size_t i=0;i<pcm.size();i++){int a=pcm[i]<0?-(int)pcm[i]:pcm[i];if(a>p)p=a;}return p;
}
// Collects the sound effect indices the dungeon asks for.
struct CueLog : dfpsp::SoundSink {
    std::vector<int> ids;
    void play(int soundId,int vol,int pan)override{(void)vol;(void)pan;ids.push_back(soundId);}
    bool has(int id)const{for(int v:ids)if(v==id)return true;return false;}
    bool inRange(int lo,int hi)const{for(int v:ids)if(v>=lo&&v<=hi)return true;return false;}
};
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
    Vec3 corridor{0,0,1};   // open hall north of the start marker (validated by probe)
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
    // Travel: region dungeon table and the TRAVEL window list.
    {
        std::vector<LocationEntry> tr;CHECK(listRegionDungeons(maps,16,tr));
        CHECK(tr.size()>=4);
        bool found=false;for(auto& e:tr)if(e.exteriorId==18358){found=true;CHECK(e.name==l.name);}
        CHECK(found);
        int first=0;auto lines=travelLines(tr,0,5,first,l.name);
        CHECK(lines.size()==5);CHECK(first==0);
        CHECK(lines[0].rfind("> ",0)==0);
        int cur=(int)tr.size()-1;auto tail=travelLines(tr,cur,5,first,l.name);
        CHECK(first==(int)tr.size()-5);CHECK(tail.size()==5);
        CHECK(tail[4].rfind("> ",0)==0);
        // Actually travel to another dungeon of the region and back.
        int other=-1;for(size_t i=0;i<tr.size();i++)if(tr[i].exteriorId!=18358){other=(int)i;break;}
        CHECK(other>=0);
        DungeonLayout nl;CHECK(loadDungeonLayout(maps,16,tr[other].exteriorId,nl));
        CHECK(nl.name==tr[other].name);
        Dungeon d2(vfs,arch3d,blocks,monsters);
        CHECK(d2.enter(nl,3));
        Vec3 s2=d2.spawn();CHECK(std::isfinite(s2.x)&&std::isfinite(s2.z));
        CHECK(d2.resident().size()>=1);
        CHECK(dungeon.enter(l,3));   // back home for the save test
    }
    // Automap: wall outline, door outlines and the player arrow.
    {
        AutomapData am;buildAutomap(dungeon,sp,0.f,1500.f,am);
        CHECK(am.walls.size()>100);
        CHECK(!am.doors.empty());
        CHECK(am.marker.size()==3);
        CHECK(am.minx==sp.x-1500.f&&am.maxx==sp.x+1500.f);
        CHECK(am.minz==sp.z-1500.f&&am.maxz==sp.z+1500.f);
        bool inside=false;
        for(auto& g:am.walls)if(g.x0>=am.minx&&g.x0<=am.maxx&&g.z0>=am.minz&&g.z0<=am.maxz){inside=true;break;}
        CHECK(inside);
    }
    // Frustum culling: geometry behind or beside the camera is rejected.
    {
        Camera cam;cam.pos=Vec3{0,64,0};cam.yaw=0;cam.pitch=0;
        Frustum fr;frustumFromCamera(cam,480.f/272.f,2.f,4096.f,fr);
        Tri in;for(int k=0;k<3;k++)in.v[k].p=Vec3{(float)(k-1)*10,64,200};
        CHECK(triInView(in,fr));
        Tri behind;for(int k=0;k<3;k++)behind.v[k].p=Vec3{(float)(k-1)*10,64,-200};
        CHECK(!triInView(behind,fr));
        Tri side;for(int k=0;k<3;k++)side.v[k].p=Vec3{2000+(float)k*10,64,100};
        CHECK(!triInView(side,fr));
        CHECK(sphereInView(Vec3{0,64,500},50,fr));
        CHECK(!sphereInView(Vec3{0,64,-500},50,fr));
        Camera cam2=cam;cam2.yaw=90.f;   // looking +X flips what is ahead
        Frustum fr2;frustumFromCamera(cam2,480.f/272.f,2.f,4096.f,fr2);
        CHECK(triInView(side,fr2));
        CHECK(!triInView(in,fr2));
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
        CHECK(back.version==4);
        CHECK(back.region==sd.region);CHECK(back.exteriorId==sd.exteriorId);
        // Older saves (v1-v3 layout: no character/item tail) must still load.
        {
            FILE* f=fopen("test_core_save.dfs","rb");
            fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
            std::vector<u8> b((size_t)n);fread(b.data(),1,b.size(),f);fclose(f);
            b.resize(b.size()-5);                 // drop v4 character flag + item count
            b[4]=3;b[5]=b[6]=b[7]=0;              // claim version 3
            f=fopen("test_core_save.dfs","wb");fwrite(b.data(),1,b.size(),f);fclose(f);
            SaveData old;CHECK(readSave("test_core_save.dfs",old));
            CHECK(old.version==3&&!old.character.valid&&old.items.empty());
            CHECK(old.enemies.size()==sd.enemies.size());
        }
        remove("test_core_save.dfs");
    }
    // --- Audio: HMI music from MIDI.BSA, samples from DAGGER.SND, synth ---
    {
        // Both archives are ordinary Daggerfall BSAs: 0x0100 keeps a named
        // directory, 0x0200 an id/size one, both stored at the end of the file.
        Bsa midi;CHECK(midi.open(vfs,"MIDI.BSA"));CHECK(midi.count()==131);
        CHECK(midi.find("D1.HMI")>=0);CHECK(midi.find("DUNGEON.HMI")>=0);CHECK(midi.find("TAVERN.HMI")>=0);
        std::vector<u8> raw;CHECK(midi.read("D1.HMI",raw));CHECK(raw.size()==19372);
        CHECK(raw[0]=='H'&&raw[3]=='-'&&raw[11]=='G');
        MidiSong song;CHECK(parseHmiSong(raw.data(),raw.size(),song));
        CHECK(song.ok);CHECK(song.tracks==4);
        CHECK(song.ppqn==480);CHECK(song.tempoUs==4000000);
        CHECK(song.notes==454);CHECK(song.endTick==26926);
        // SOS ran HMI songs at four seconds per quarter note, i.e. the uint16 at
        // 0xD4 (120) ticks per second: the Daggerfall theme lasts about 3:44.
        CHECK(song.seconds()>220.0&&song.seconds()<230.0);
        CHECK(song.loopEndTick>song.loopStartTick);
        // Every note carries a duration and a matching synthesised note off.
        size_t notes=0,offs=0;u32 prev=0;bool sorted=true;
        for(auto& e:song.events){
            if(e.tick<prev)sorted=false;prev=e.tick;
            if(e.kind==MidiNote){notes++;CHECK(e.dur>0);CHECK(e.d1<128);}
            else if(e.kind==MidiNoteOff)offs++;
        }
        CHECK(sorted);CHECK(notes==song.notes);CHECK(offs==song.notes);
        // The whole archive parses: 131 songs, every track reaching its EOT.
        size_t corpusNotes=0;int corpusTracks=0;
        for(size_t i=0;i<midi.count();i++){
            std::vector<u8> data;CHECK(midi.read((u32)i,data));
            MidiSong s;
            CHECK(parseHmiSong(data.data(),data.size(),s));
            CHECK(s.ok);CHECK(!s.events.empty());CHECK(s.notes>0);
            CHECK(s.seconds()>20.0&&s.seconds()<600.0);
            corpusNotes+=s.notes;corpusTracks+=s.tracks;
        }
        CHECK(corpusNotes>100000);CHECK(corpusTracks>900);
        // A truncated or bogus record is refused instead of reading out of bounds.
        MidiSong bad;CHECK(!parseHmiSong(raw.data(),64,bad));
        std::vector<u8> junk(raw);junk[0]='X';CHECK(!parseHmiSong(junk.data(),junk.size(),bad));
    }
    {
        // Synth pitch: a synthetic A4 piano note must peak at 440 Hz and stay
        // silent before it starts.
        const int RATE=22050;
        Synth synth;synth.init(RATE);
        MidiSong test=makeTestSong(69,0,2.0);
        CHECK(test.ok);CHECK(test.notes==1);
        synth.setSong(test);synth.playSong(false);
        std::vector<i16> pcm((size_t)RATE*4,0);
        synth.render(pcm.data(),RATE*2);
        for(int i=0;i<RATE/10;i++)CHECK(pcm[(size_t)i*2]==0&&pcm[(size_t)i*2+1]==0);   // 100 ms of silence
        size_t at=(size_t)(RATE*0.5);
        double f440=goertzel(pcm,at,8192,440.0,RATE);
        double fLow=goertzel(pcm,at,8192,440.0*std::exp2(-1.0/12.0),RATE);
        double fHigh=goertzel(pcm,at,8192,440.0*std::exp2(1.0/12.0),RATE);
        double f880=goertzel(pcm,at,8192,880.0,RATE);
        CHECK(f440>200.0);
        CHECK(f440>fLow*5.0);CHECK(f440>fHigh*5.0);
        CHECK(f880<f440);CHECK(f880>f440*0.05);          // the second partial
        CHECK(synth.notesStarted()==1);
        int pk=peak16(pcm);CHECK(pk>1000);CHECK(pk<32767);   // audible, not clipped
        // Two independent synths render bit identical samples.
        Synth a,b;a.init(RATE);b.init(RATE);
        a.setSong(test);b.setSong(test);a.playSong(false);b.playSong(false);
        std::vector<i16> pa((size_t)RATE*2,0),pb((size_t)RATE*2,0);
        a.render(pa.data(),RATE);b.render(pb.data(),RATE);
        CHECK(pa==pb);
        // Real music: the theme renders, keeps polyphony and never clips.
        Bsa midi;CHECK(midi.open(vfs,"MIDI.BSA"));
        std::vector<u8> raw;CHECK(midi.read("D1.HMI",raw));
        Synth real;real.init(RATE);CHECK(real.loadSong(raw.data(),raw.size()));
        CHECK(real.song().notes==454);
        real.playSong(true);
        const int FRAMES=RATE*10;
        std::vector<i16> music((size_t)FRAMES*2,0);
        int maxVoices=0;
        for(int done=0;done+512<=FRAMES;done+=512){
            real.render(&music[(size_t)done*2],512);
            int v=real.activeVoices();if(v>maxVoices)maxVoices=v;
        }
        CHECK(real.notesStarted()>8);CHECK(maxVoices>0);CHECK(maxVoices<=Synth::kMusicVoices);
        CHECK(rms16(music)>200.0);CHECK(peak16(music)<32768);
        CHECK(real.songPositionSec()>9.0);
        // Looping rewinds the sequencer instead of stopping.
        Synth loop;loop.init(RATE);loop.loadSong(raw.data(),raw.size());
        MidiSong tiny=loop.song();tiny.endTick=240;loop.setSong(tiny);   // two seconds
        loop.playSong(true);
        std::vector<i16> lp((size_t)RATE*8,0);loop.render(lp.data(),RATE*4);
        CHECK(!loop.songPlaying()||loop.songPositionSec()<2.1);
        CHECK(rms16(lp)>0.0);
    }
    {
        // DAGGER.SND: 459 records of raw 8-bit unsigned mono PCM at 11025 Hz.
        SfxBank bank;CHECK(bank.open(vfs,"DAGGER.SND"));CHECK(bank.count()==459);
        CHECK(bank.recordSize(0)==39363);
        CHECK(bank.recordSize(SfxId::SwingHighPitch)==1287);
        CHECK(bank.recordSize(SfxId::ArenaHitSound)==3112);
        CHECK(bank.recordSize(SfxId::DungeonDoorOpen)==23320);
        CHECK(bank.recordSize(99999)==-1);
        std::vector<u8> swing;CHECK(bank.extract(SfxId::SwingHighPitch,swing));
        CHECK(swing.size()==1287);
        // The record starts on the unsigned silence level 128.
        long sum=0;for(int i=0;i<200;i++)sum+=swing[(size_t)i];
        CHECK(std::labs(sum/200-128)<=2);
        SfxClip clip;CHECK(bank.clip(SfxId::SwingHighPitch,clip));
        CHECK(clip.len==swing.size());CHECK(clip.rate==SFX_SAMPLE_RATE);
        CHECK(std::memcmp(clip.pcm,swing.data(),swing.size())==0);
        CHECK(bank.cachedBytes()==swing.size());
        // A tight budget evicts the oldest record and reports its old pointer so
        // the synth can stop any voice still reading it.
        SfxBank small(40000);CHECK(small.open(vfs,"DAGGER.SND"));
        SfxClip first;CHECK(small.clip(0,first));
        const u8* firstPtr=first.pcm;CHECK(first.len==39363);
        SfxClip second;SfxEvicted evicted;CHECK(small.clip(3,second,&evicted));
        CHECK(evicted.n==1);CHECK(evicted.ptr[0]==firstPtr);
        CHECK(small.cachedBytes()<=40000);
        // A sound effect mixed over music raises the level and never clips.
        AudioDirector audio;audio.init(22050);
        CHECK(audio.openMusic(vfs));CHECK(audio.openSfx(vfs));
        CHECK(audio.songCount()==131);CHECK(audio.sfxCount()==459);
        CHECK(audio.songIndex("TAVERN.HMI")>=0);
        CHECK(audio.songName(audio.songIndex("TAVERN.HMI"))=="TAVERN.HMI");
        CHECK(audio.playSongName("TAVERN.HMI"));
        CHECK(audio.currentSongName()=="TAVERN.HMI");
        CHECK(audio.playSfxIndex(SfxId::SwingHighPitch));
        CHECK(audio.synth().samplesStarted()==1);
        std::vector<i16> mixed((size_t)22050*4,0);
        audio.synth().render(mixed.data(),22050*2);
        CHECK(rms16(mixed)>200.0);CHECK(peak16(mixed)<32768);
        audio.stopSong();CHECK(audio.currentSong()<0);
        // WAV writer: canonical 44 byte header for 16-bit stereo.
        std::vector<i16> tiny(2205,7);
        CHECK(AudioDirector::writeWav("test_audio.wav",22050,tiny));
        FILE* f=fopen("test_audio.wav","rb");CHECK(f!=nullptr);
        u8 h[44];CHECK(fread(h,1,44,f)==44);
        CHECK(std::memcmp(h,"RIFF",4)==0);CHECK(std::memcmp(h+8,"WAVE",4)==0);
        CHECK(std::memcmp(h+12,"fmt ",4)==0);CHECK(std::memcmp(h+36,"data",4)==0);
        u32 dataBytes=(u32)h[40]|((u32)h[41]<<8)|((u32)h[42]<<16)|((u32)h[43]<<24);
        CHECK(dataBytes==tiny.size()*2);
        u32 riff=(u32)h[4]|((u32)h[5]<<8)|((u32)h[6]<<16)|((u32)h[7]<<24);
        CHECK(riff==36+dataBytes);
        u32 rate=(u32)h[24]|((u32)h[25]<<8)|((u32)h[26]<<16)|((u32)h[27]<<24);
        CHECK(rate==22050);CHECK(h[22]==2);CHECK(h[34]==16);
        i16 sample=0;CHECK(fread(&sample,2,1,f)==1);CHECK(sample==7);
        fclose(f);remove("test_audio.wav");
        // Raw records convert to 16-bit stereo around the 128 midpoint.
        std::vector<u8> u8pcm={128,0,255,64};
        std::vector<i16> conv;AudioDirector::unsigned8ToStereo16(u8pcm.data(),u8pcm.size(),conv);
        CHECK(conv.size()==8);CHECK(conv[0]==0);CHECK(conv[2]==-32768);CHECK(conv[4]==32512);CHECK(conv[6]==-16384);
    }
    {
        // Gameplay wiring: swings, hits, monster voices and lockpicks reach the
        // sound sink with the classic DAGGER.SND indices.
        Dungeon d3(vfs,arch3d,blocks,monsters);
        CHECK(d3.enter(l,3));
        CueLog cues;d3.setSoundSink(&cues);
        PlayerState p;p.pos=d3.spawn();p.skill=120;p.weaponMin=20;p.weaponMax=30;
        Vec3 fwd{std::sin(0.5f),0,std::cos(0.5f)};
        d3.playerAttack(p,fwd);
        CHECK(cues.has(SfxId::SwingLowPitch)||cues.has(SfxId::SwingHighPitch));
        // Put the nearest monster right in front of the player and swing.
        std::vector<Enemy> es=d3.enemies();
        CHECK(!es.empty());
        size_t bi=0;float bd=1e9f;
        for(size_t i=0;i<es.size();i++){
            if(es[i].state==EnemyDead)continue;
            float d=(es[i].pos-p.pos).len();if(d<bd){bd=d;bi=i;}
        }
        Enemy& target=es[bi];
        const EnemyStatic* st=findEnemyStatic(target.mobileId);CHECK(st!=nullptr);
        target.pos=p.pos+fwd*70.0f;target.flying=false;
        float fy=d3.floor(target.pos,target.pos.y+96.f);if(fy>-10000.f)target.pos.y=fy;
        target.aggro=false;target.passive=false;target.seen=true;target.state=EnemyIdle;
        target.hp=target.maxHp;
        d3.setEnemies(es,d3.deadKeys(),d3.spawnedKeys());
        cues.ids.clear();
        d3.update(p.pos);
        d3.tickEnemies(0.1f,p,fwd);
        // A monster that has just noticed the player uses its bark sound.
        CHECK(cues.has(st->barkSound));
        cues.ids.clear();
        bool hit=false;
        for(int i=0;i<16&&!hit;i++){
            cues.ids.clear();
            p.attackCooldown=0.f;
            d3.playerAttack(p,fwd);
            hit=cues.inRange(SfxId::Hit1,SfxId::Hit5);
            if(hit)CHECK(cues.has(st->barkSound));
        }
        CHECK(hit);
        // Lockpicking a locked door raises the classic pick sound.
        Door door;door.locked=true;
        cues.ids.clear();
        d3.pickLock(p,door);
        CHECK(cues.has(SfxId::ActivateGrind));
        d3.setSoundSink(nullptr);
        cues.ids.clear();
        d3.playerAttack(p,fwd);
        CHECK(cues.ids.empty());
    }
    // --- character creation, items, paper doll and save v4 ---
    {
        std::vector<Career> classes;CHECK(loadClasses(vfs,classes));
        CHECK(classes.size()==18);
        CHECK(classes[0].name=="Mage");
        CHECK(classes[0].hpPerLevel==6);
        CHECK(std::fabs(classes[0].advancement-1.04f)<0.01f);
        CHECK(classes[0].attributes[0]==42&&classes[0].attributes[1]==60);
        CHECK(classes[0].primary[0]==27&&classes[0].major[0]==24);
        CHECK(classes[17].name=="Knight");
        NameGen names;CHECK(names.open(vfs));
        DfRandom nr(99);
        std::string bn=names.fullName(RaceBreton,0,nr);
        CHECK(!bn.empty()&&bn.find(' ')!=std::string::npos);
        std::string rg=names.fullName(RaceRedguard,1,nr);
        CHECK(!rg.empty()&&rg.find(' ')==std::string::npos);   // Redguards carry one name
        TextRsc text;CHECK(text.open(vfs));
        CHECK(text.has(2003)&&text.has(2100)&&text.has(4116));
        for(int ci=0;ci<18;ci++){
            BiogFile b;CHECK(b.load(vfs,ci));
            CHECK(b.questions.size()==12);
            for(size_t q=0;q<b.questions.size();q++)CHECK(!b.questions[q].answers.empty());
        }
        BiogEffects be;
        CHECK(parseBiogEffect("22 +6",be)&&be.skillMod[22]==6);
        CHECK(parseBiogEffect("GP +10",be)&&be.goldMod==10);
        CHECK(parseBiogEffect("IT 3 3 0",be)&&be.items.size()==1&&be.items[0].group==3);
        CHECK(!parseBiogEffect("#4150",be));
        Font body;body.load(vfs,"FONT0003.FNT");
        for(int ci=0;ci<19;ci++){                              // 18 classes + custom
            bool custom=ci==18;
            BiogFile biog;CHECK(biog.load(vfs,custom?0:ci));
            DfRandom rng(1000+ci);
            CharGenFlow f;
            charGenAdvance(f,ActConfirm,classes,biog,names,rng);
            charGenAdvance(f,ActConfirm,classes,biog,names,rng);
            if(!custom){
                for(int k=0;k<ci;k++)charGenAdvance(f,ActDown,classes,biog,names,rng);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng);
            }else{
                for(int k=0;k<18;k++)charGenAdvance(f,ActDown,classes,biog,names,rng);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng);
                charGenAdvance(f,ActRight,classes,biog,names,rng);      // hp/level 9
                for(int k=0;k<12;k++){charGenAdvance(f,ActDown,classes,biog,names,rng);
                    charGenAdvance(f,ActRight,classes,biog,names,rng);} // skill slots
                charGenAdvance(f,ActDown,classes,biog,names,rng);       // onto Done
                charGenAdvance(f,ActConfirm,classes,biog,names,rng);
            }
            int guard=0;
            while(f.step!=StepDone&&guard++<600){
                if(f.step==StepBiography)charGenAdvance(f,ActConfirm,classes,biog,names,rng);
                else if(f.step==StepBonusStats){
                    for(int k=0;k<14&&f.doc.bonusPool>0;k++)charGenAdvance(f,ActRight,classes,biog,names,rng);
                    charGenAdvance(f,ActConfirm,classes,biog,names,rng);
                }else if(f.step==StepBonusSkills){
                    for(int k=0;k<80&&(f.doc.skillPool[0]+f.doc.skillPool[1]+f.doc.skillPool[2])>0;k++){
                        charGenAdvance(f,ActRight,classes,biog,names,rng);
                        charGenAdvance(f,ActDown,classes,biog,names,rng);
                    }
                    charGenAdvance(f,ActConfirm,classes,biog,names,rng);
                }else charGenAdvance(f,ActConfirm,classes,biog,names,rng);
            }
            CHECK(f.step==StepDone);
            CharacterDoc& d=f.doc;
            CHECK(d.bonusPool==0);
            CHECK(d.skillPool[0]==0&&d.skillPool[1]==0&&d.skillPool[2]==0);
            CHECK(d.maxHp==25+d.career.hpPerLevel);
            CHECK(d.hp==d.maxHp);
            CHECK(d.gold>=0);
            CHECK(!d.name.empty());
            CHECK(d.faceIndex>=0&&d.faceIndex<10);
            if(custom)CHECK(d.career.custom&&d.career.hpPerLevel==9);
            Inventory inv;inv.gold=d.gold;
            grantBiogItems(inv,d.biog,d.gender,d.race,rng);
            assignStartingGear(inv,d.classIndex,d.classIndex<0,d.gender,d.race,rng);
            CHECK(inv.atSlot(SlotChestClothes)!=0);
            CHECK(inv.atSlot(SlotLegsClothes)!=0);
            CHECK(inv.isUpperClothed()&&inv.isLowerClothed());
            SaveData sd;sd.character=fromCharacter(d);
            for(size_t i=0;i<inv.items.size();i++)sd.items.push_back(fromItem(inv.items[i]));
            CHECK(writeSave("/tmp/dfpsp_test_save.bin",sd));
            SaveData rd;CHECK(readSave("/tmp/dfpsp_test_save.bin",rd));
            CHECK(rd.version==4&&rd.character.valid);
            CHECK(rd.character.name==d.name);
            CHECK(rd.character.career.hpPerLevel==d.career.hpPerLevel);
            CHECK(rd.items.size()==inv.items.size());
            if(ci==0){
                CHECK(inv.items.size()>=4);
                Item w=makeWeapon(ItemDef::Weapons_Shortsword,WMatSteel,false);
                const ItemTemplate& t=itemTemplate(ItemDef::Weapons_Shortsword);
                CHECK(w.value==t.basePrice*3*2);
                CHECK(w.maxCondition==t.hitPoints*6/4);
                CHECK(itemGroupOf(ItemDef::Weapons_Shortsword)==3);
                CHECK(itemTemplateOfOrdinal(3,itemOrdinalInGroup(ItemDef::Weapons_Shortsword))==ItemDef::Weapons_Shortsword);
                Item mi=makeWeapon(ItemDef::Weapons_Shortsword,WMatIron,false);
                Item fw=makeWeapon(ItemDef::Weapons_Shortsword,WMatIron,true);
                CHECK(fw.playerArchive==mi.playerArchive-1);
                Inventory tw;
                tw.add(makeItem(ItemDef::Jewellery_Ring));tw.add(makeItem(ItemDef::Jewellery_Ring));
                CHECK(tw.equip(0)&&tw.equip(1));
                CHECK(tw.atSlot(SlotRing0)&&tw.atSlot(SlotRing1));
                PaperDoll doll;CHECK(doll.load(vfs,d.race,d.gender));
                CHECK(doll.equipIndexAt(0,0)==SlotNone);
                Framebuffer fb;fb.resize(320,200);
                doll.draw(fb,200,8,inv,d.faceIndex,pal);
                size_t drawn=0;
                for(int y=8;y<192;y+=7)for(int x=200;x<310;x+=7)
                    if(fb.color[(size_t)y*320+x]!=0xff000000)drawn++;
                CHECK(drawn>50);
                drawInventoryScreen(fb,inv,doll,d.faceIndex,0,0,vfs,pal,body,inv.gold);
                drawCharGenScreen(fb,f,classes,biog,text,vfs,pal,body,body);
            }
        }
    }
    printf("core tests passed\n");return 0;
}
