#pragma once
#include "Types.h"
#include "Bsa.h"
#include "Vfs.h"
namespace dfpsp {
// DAGGER.SND is a type 0x0200 BSA holding 459 records of raw 8-bit unsigned
// mono PCM at about 11025 samples per second, addressed by record index (the
// record ids in the tail directory are unrelated). The names below are the
// classic sound effect indices taken from Daggerfall Unity SoundClips.cs (MIT
// License, Copyright (C) 2009-2023 Daggerfall Workshop); the per monster
// move/bark/attack indices live in the generated EnemyData tables so creatures
// keep their original voices.
namespace SfxId {
enum {
    SpookyHigh=0, SpookyMid=1, SpookyLow=2,
    ArrowShoot=3, ArrowHit=4,
    Invalid=5,
    AmbientCrickets=6, PlayerDoorBash=7,
    BodyFall=15,
    ActivateGears=19, ActivateDoorClose=20,
    ArenaFootstep1=22, ArenaFootstep2=23,
    DungeonDoorClose=24, DungeonDoorOpen=25,
    AmbientDrums=28, ArenaHitSound=30,
    ArenaFanfareLevelUp=32, ActivateGrind=40,
    AmbientDripShort=63, AmbientWindMoan=65, AmbientMonsterRoar=73,
    DrawWeapon=78,
    CastSpell1=81, SpellImpactMagic=86, SpellImpactFire=89,
    FallDamage=91, FallHard=92,
    NormalDoorClose=93, NormalDoorOpen=94, LongSlam=95, LevelUp=96,
    SwingLowPitch=105, SwingHighPitch=106,
    Hit1=108, Hit2=109, Hit3=110, Hit4=111, Hit5=112
};
} // namespace SfxId
constexpr int SFX_SAMPLE_RATE=11025;
struct SfxClip {
    const u8* pcm=nullptr;
    size_t len=0;
    int rate=SFX_SAMPLE_RATE;
};
// Gameplay audio sink. The dungeon reports classic DAGGER.SND record indices
// here and the platform decides what to do with them: the PSP pushes them into
// its audio thread, the host appends them to a cue log so tests can assert that
// a swing, a hit or a monster spotting the player raises the right sound.
class SoundSink {
public:
    virtual ~SoundSink(){}
    virtual void play(int soundId,int vol=100,int pan=0)=0;
};
// Reads and caches sound effect records. Records are cached least recently used
// style inside `budget` bytes; when a record is evicted its old pointer is
// reported so the caller can stop any voice still playing it.
struct SfxEvicted {
    const u8* ptr[4]={nullptr,nullptr,nullptr,nullptr};
    int n=0;
    void add(const u8* p){if(n<4)ptr[n++]=p;}
};
class SfxBank {
public:
    explicit SfxBank(size_t budget=1200000):budget_(budget){}
    bool open(const Vfs& vfs,const std::string& name="DAGGER.SND");
    bool ready()const{return ready_;}
    size_t count()const{return count_;}
    int recordSize(int index)const;
    // Copies one record out, used by the host WAV dump and the tests.
    bool extract(int index,std::vector<u8>& out)const;
    // Cached pointer to one record, or false when the archive lacks it.
    bool clip(int index,SfxClip& out,SfxEvicted* evicted=nullptr);
    void clearCache();
    size_t cachedBytes()const{return cached_;}
    size_t cacheBudget()const{return budget_;}
    const Bsa& archive()const{return bsa_;}
private:
    Bsa bsa_;
    bool ready_=false;
    size_t count_=0, cached_=0, budget_=1200000;
    std::vector<std::vector<u8>> data_;
    std::vector<int> order_;      // cached record indices, least recent first
};
} // namespace
