#include "../../core/Vfs.h"
#include "../../core/Bsa.h"
#include "../../core/Maps.h"
#include "../../core/Dungeon.h"
#include "../../core/SoftwareRenderer.h"
#include "../../core/Font.h"
#include "../../core/Ui.h"
#include "../../core/Automap.h"
#include "../../core/Audio.h"
#include "../../core/SaveGame.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
using namespace dfpsp;
int main(int argc,char**argv){
    std::string root="data",block="B0000001.RDB",out="frame.bmp";int w=480,h=272;bool layout=false,dungeonMode=false;float cyaw=0.5f,cpitch=-0.1f;bool haveCam=false;Vec3 cpos{};
    std::string music,audioOut,sfxArg,sfxDumpPath;int sfxDumpId=-1,audioRate=22050;double audioSecs=10.0,audioSkip=0.0;bool listSongs=false,listSfx=false;
    std::string uiMode;bool showStats=false;int region=16,exterior=18358;
    std::string chargenPrefix,invShot;int cgSeed=7,cgRace=1,cgClass=0,cgGender=0;float tick=0.f;std::string savePath,loadPath;int playerLevel=3;bool withEnemies=true;float place=-1.f;bool hurt=false,kill=false;
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
        else if(!strcmp(argv[i],"--stats"))showStats=true;
        else if(!strcmp(argv[i],"--music")&&i+1<argc)music=argv[++i];
        else if(!strcmp(argv[i],"--audio-out")&&i+1<argc)audioOut=argv[++i];
        else if(!strcmp(argv[i],"--audio-secs")&&i+1<argc)audioSecs=atof(argv[++i]);
        else if(!strcmp(argv[i],"--audio-skip")&&i+1<argc)audioSkip=atof(argv[++i]);
        else if(!strcmp(argv[i],"--audio-rate")&&i+1<argc)audioRate=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--sfx")&&i+1<argc)sfxArg=argv[++i];
        else if(!strcmp(argv[i],"--sfx-dump")&&i+2<argc){sfxDumpId=atoi(argv[++i]);sfxDumpPath=argv[++i];}
        else if(!strcmp(argv[i],"--list-songs"))listSongs=true;
        else if(!strcmp(argv[i],"--list-sfx"))listSfx=true;
        else if(!strcmp(argv[i],"--region")&&i+1<argc)region=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--chargen-shots")&&i+1<argc)chargenPrefix=argv[++i];
        else if(!strcmp(argv[i],"--inventory-shot")&&i+1<argc)invShot=argv[++i];
        else if(!strcmp(argv[i],"--seed")&&i+1<argc)cgSeed=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--race")&&i+1<argc)cgRace=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--gender")&&i+1<argc)cgGender=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--class")&&i+1<argc)cgClass=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--exterior")&&i+1<argc)exterior=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--cam")&&i+3<argc){haveCam=true;cpos={(float)atof(argv[i+1]),(float)atof(argv[i+2]),(float)atof(argv[i+3])};i+=3;}
    }
    Vfs vfs;vfs.addRoot(root);vfs.addRoot(root+"/ARENA2");vfs.loadModList(root);vfs.loadModList(".");
    // Character creation / inventory validation: scripted flow over the
    // original screens, one BMP per step, plus the inventory with paper doll.
    if(!chargenPrefix.empty()||!invShot.empty()){
        std::vector<Career> classes;
        if(!loadClasses(vfs,classes)){fprintf(stderr,"classes failed\n");return 1;}
        NameGen names;names.open(vfs);
        TextRsc text;text.open(vfs);
        Font title,body;title.load(vfs,"FONT0001.FNT");body.load(vfs,"FONT0003.FNT");
        Palette pal;loadPalette(vfs,"ART_PAL.COL",pal);
        DfRandom rng((u32)cgSeed);
        BiogFile biog;biog.load(vfs,cgClass);
        Framebuffer fb;fb.resize(320,200);
        CharGenFlow f;
        int shot=0;
        auto shoot=[&](const char* nm){
            drawCharGenScreen(fb,f,classes,biog,text,vfs,pal,body,title);
            if(!chargenPrefix.empty()){
                char path[256];snprintf(path,sizeof(path),"%s_%02d_%s.bmp",chargenPrefix.c_str(),shot,nm);
                writeBmp(path,fb);
            }
            shot++;
        };
        {int rx=160,ry=100;racePixel(vfs,cgRace,rx,ry);f.raceX=rx;f.raceY=ry;}
        shoot("race");
        charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);   // map -> province?
        shoot("raceConfirm");
        charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);   // yes -> gender
        shoot("gender");
        if(cgGender){charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);}
        charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);   // gender -> questions
        shoot("classQuestions");
        for(int q=0;q<10;q++){
            charGenAdvance(f,ActRight,classes,biog,names,rng,vfs);
            charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);
        }
        shoot("classRecommend");
        if(f.step==StepClassRecommend&&f.cqRecommended==cgClass&&cgClass>=0&&cgClass<(int)classes.size()){
            charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);   // accept recommendation
        }else{
            if(f.step==StepClassRecommend){
                charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);   // no -> class list
                shoot("class");
            }
            for(int k=0;k<cgClass;k++)charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
            charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);
        }
        const char* stepNames[]={"race","raceConfirm","gender","classQuestions","classRecommend","class",
            "custom","advList","advPrimary","advSecondary","biography","bonusStats","bonusSkills",
            "face","name","summary","done"};
        int guard=0;bool advVisited=false;
        while(f.step!=StepDone&&guard++<400){
            drawCharGenScreen(fb,f,classes,biog,text,vfs,pal,body,title);
            char path[256];snprintf(path,sizeof(path),"%s_%02d_%s.bmp",chargenPrefix.c_str(),shot,stepNames[f.step]);
            if(!chargenPrefix.empty())writeBmp(path,fb);
            fprintf(stderr,"step %s\n",stepNames[f.step]);
            shot++;
            if(f.step==StepCustom){
                if(!advVisited){
                    for(int k=0;k<12;k++){charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
                        charGenAdvance(f,ActRight,classes,biog,names,rng,vfs);}
                    charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);   // advantages row
                    charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs); // -> list
                    continue;
                }
                charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);       // -> done
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepAdvList){
                if(!advVisited){advVisited=true;
                    charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs); // -> primary
                }else{
                    charGenAdvance(f,ActBack,classes,biog,names,rng,vfs);    // -> custom row 13
                }
                continue;}
            if(f.step==StepAdvPrimary||f.step==StepAdvSecondary){
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepBiography){charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepBonusStats){
                for(int k=0;k<4;k++)charGenAdvance(f,ActRight,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepBonusSkills){
                for(int g=0;g<3;g++){for(int k=0;k<2;k++)charGenAdvance(f,ActRight,classes,biog,names,rng,vfs);
                    charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);}
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepFace){charGenAdvance(f,ActRight,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepName){charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            if(f.step==StepSummary){charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActDown,classes,biog,names,rng,vfs);
                charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);continue;}
            charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);
        }
        CharacterDoc& d=f.doc;
        Inventory inv;inv.gold=d.gold;
        grantBiogItems(inv,d.biog,d.gender,d.race,rng);
        assignStartingGear(inv,d.classIndex,d.classIndex<0,d.gender,d.race,rng);
        for(size_t i=0;i<inv.items.size();i++)
            if(naturalSlot(inv.items[i])!=SlotNone)inv.equip((int)i);
        PaperDoll doll;doll.load(vfs,d.race,d.gender);
        drawCharGenScreen(fb,f,classes,biog,text,vfs,pal,body,title);
        if(!chargenPrefix.empty()){
            char path[256];snprintf(path,sizeof(path),"%s_99_inventory.bmp",chargenPrefix.c_str());
            drawInventoryScreen(fb,inv,doll,d.faceIndex,0,0,vfs,pal,body,inv.gold);
            writeBmp(path,fb);
        }
        if(!invShot.empty()){
            drawInventoryScreen(fb,inv,doll,d.faceIndex,0,0,vfs,pal,body,inv.gold);
            if(!writeBmp(invShot,fb))return 1;
        }
        fprintf(stderr,"created %s, %s %s of class %s: hp %d gold %d, %zu items\n",
                d.name.c_str(),kGenderNames[d.gender],raceInfo(d.race)->name,d.career.name.c_str(),
                d.maxHp,d.gold,inv.items.size());
        printf("%s done\n",chargenPrefix.empty()?invShot.c_str():chargenPrefix.c_str());
        return 0;
    }
    Bsa arch3d,blocks,maps,monsters;
    // Audio only path: HMI music from MIDI.BSA and sound effects from
    // DAGGER.SND, rendered with the same synth the PSP audio thread drives.
    if(!music.empty()||!audioOut.empty()||!sfxArg.empty()||listSongs||listSfx||sfxDumpId>=0){
        AudioDirector audio;audio.init(audioRate);
        bool haveMusic=audio.openMusic(vfs),haveSfx=audio.openSfx(vfs);
        fprintf(stderr,"audio: MIDI.BSA %s (%zu songs), DAGGER.SND %s (%zu records)\n",
                haveMusic?"ok":"missing",audio.songCount(),haveSfx?"ok":"missing",audio.sfxCount());
        if(listSongs){
            for(size_t i=0;i<audio.songCount();i++){
                std::vector<u8> data;audio.musicArchive().read((u32)i,data);
                MidiSong song;bool ok=parseHmiSong(data.data(),data.size(),song);
                printf("%3zu %-14s %7zu B %s\n",i,audio.songName((int)i).c_str(),data.size(),
                       ok?([&]{char b[96];snprintf(b,sizeof(b),"%d tracks %5u notes %6.1f s loop %.1f-%.1f s",
                            song.tracks,song.notes,song.seconds(),
                            song.loopStartTick*song.secondsPerTick(),song.loopEndTick*song.secondsPerTick());
                            return std::string(b);}()).c_str()
                         :("BAD: "+song.error).c_str());
            }
            return 0;
        }
        if(listSfx){
            for(size_t i=0;i<audio.sfxCount();i++)
                printf("%3zu %7d B %5.2f s\n",i,audio.sfx().recordSize((int)i),
                       (double)audio.sfx().recordSize((int)i)/(double)SFX_SAMPLE_RATE);
            return 0;
        }
        if(sfxDumpId>=0){
            std::vector<u8> raw;
            if(!audio.sfx().extract(sfxDumpId,raw)||raw.empty()){fprintf(stderr,"no sfx record %d\n",sfxDumpId);return 1;}
            std::vector<i16> pcm;AudioDirector::unsigned8ToStereo16(raw.data(),raw.size(),pcm);
            if(!AudioDirector::writeWav(sfxDumpPath,SFX_SAMPLE_RATE,pcm)){fprintf(stderr,"cannot write %s\n",sfxDumpPath.c_str());return 1;}
            fprintf(stderr,"sfx %d: %zu samples (%.2f s at %d Hz) -> %s\n",sfxDumpId,raw.size(),
                    (double)raw.size()/(double)SFX_SAMPLE_RATE,SFX_SAMPLE_RATE,sfxDumpPath.c_str());
            printf("%s\n",sfxDumpPath.c_str());return 0;
        }
        if(!music.empty()){
            if(!audio.playSongName(music)){fprintf(stderr,"music failed: %s\n",audio.lastError().c_str());return 1;}
            if(audioSkip>0.0)audio.seekSong(audioSkip);
            const MidiSong& song=audio.synth().song();
            fprintf(stderr,"song %s: %d tracks, %u notes, %zu events, %.1f s, loop %.1f-%.1f s, ppqn %u\n",
                    music.c_str(),song.tracks,song.notes,song.events.size(),song.seconds(),
                    song.loopStartTick*song.secondsPerTick(),song.loopEndTick*song.secondsPerTick(),song.ppqn);
            if(audioSkip>0.0)fprintf(stderr,"started at %.1f s into the track\n",audio.synth().songPositionSec());
        }
        // --sfx takes "id[@seconds]" entries so a render can stage a swing, an
        // impact and a door the way the game would fire them.
        std::vector<std::pair<double,int>> cues;
        if(!sfxArg.empty()){
            for(size_t pos=0;pos<sfxArg.size();){
                size_t comma=sfxArg.find(',',pos);
                std::string tok=sfxArg.substr(pos,comma==std::string::npos?std::string::npos:comma-pos);
                size_t at=tok.find('@');
                int id=atoi(tok.substr(0,at).c_str());
                double when=at==std::string::npos?0.0:atof(tok.c_str()+at+1);
                cues.push_back({when,id});
                if(comma==std::string::npos)break;
                pos=comma+1;
            }
            std::sort(cues.begin(),cues.end());
        }
        if(!audioOut.empty()){
            int rate=audio.synth().sampleRate();
            size_t total=(size_t)(audioSecs*(double)rate);
            std::vector<i16> pcm(total*2,0);
            double sum=0;int peak=0,maxVoices=0;const size_t BLOCK=512;
            size_t cueIdx=0;
            auto mixStart=std::chrono::steady_clock::now();
            for(size_t done=0;done<total;done+=BLOCK){
                size_t n=std::min(BLOCK,total-done);
                while(cueIdx<cues.size()&&cues[cueIdx].first*(double)rate<=(double)(done+n)){
                    if(!audio.playSfxIndex(cues[cueIdx].second))
                        fprintf(stderr,"sfx %d unavailable\n",cues[cueIdx].second);
                    cueIdx++;
                }
                audio.synth().render(&pcm[done*2],(int)n);
                int v=audio.synth().activeVoices();if(v>maxVoices)maxVoices=v;
                for(size_t i=0;i<n*2;i++){int x=pcm[done*2+i];sum+=(double)x*x;int a=x<0?-x:x;if(a>peak)peak=a;}
            }
            if(!AudioDirector::writeWav(audioOut,rate,pcm)){fprintf(stderr,"cannot write %s\n",audioOut.c_str());return 1;}
            auto mixEnd=std::chrono::steady_clock::now();
            double mixSec=std::chrono::duration<double>(mixEnd-mixStart).count();
            double rms=total>0?std::sqrt(sum/(double)(total*2)):0.0;
            fprintf(stderr,"audio: %s %.1f s @ %d Hz stereo, peak %d, rms %.0f, max voices %d, notes %u (cut %u), samples %u (cut %u)\n",
                    audioOut.c_str(),audioSecs,rate,peak,rms,maxVoices,
                    audio.synth().notesStarted(),audio.synth().notesCut(),
                    audio.synth().samplesStarted(),audio.synth().samplesCut());
            fprintf(stderr,"audio: mixed in %.3f s wall, %.0fx realtime on this host\n",
                    mixSec,mixSec>0.0?audioSecs/mixSec:0.0);
            printf("%s\n",audioOut.c_str());
        }
        return 0;
    }
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
        if(showStats)fprintf(stderr,"stats: submitted %d tris, culled %d, flats %d of %zu tris\n",
                             rr.stats.tris,rr.stats.culled,rr.stats.flats,merged.tris.size());
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
