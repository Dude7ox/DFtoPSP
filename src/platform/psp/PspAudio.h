#pragma once
#include <pspkernel.h>
#include "../../core/Audio.h"
namespace dfpsp {
// Drives the core AudioDirector from its own thread so the game loop never
// waits on samples. The main thread only pushes commands into a ring (one
// producer, one consumer, so no lock is needed on the single core Allegrex) and
// the audio thread owns every piece of mutable audio state: it loads the HMI
// song, sequences it, mixes DAGGER.SND samples and hands stereo blocks to
// sceAudio.
//
// The class is also the dungeon's SoundSink, so gameplay cues (swings, hits,
// monster barks, doors) reach the mixer without the core knowing about PSP
// threads.
class PspAudio : public SoundSink {
public:
    bool init(const Vfs& vfs);
    void term();
    bool ready()const{return ready_;}
    bool musicReady()const{return musicReady_;}
    bool sfxReady()const{return sfxReady_;}
    int sampleRate()const{return rate_;}
    int framesPerBlock()const{return frames_;}
    bool usingSrcChannel()const{return src_;}
    int blocksRendered()const{return (int)blocks_;}
    int lastError()const{return lastError_;}
    int songIndex(const std::string& name)const{return director_.songIndex(name);}
    std::string songName(int index)const{return director_.songName(index);}
    size_t songCount()const{return director_.songCount();}
    size_t sfxCount()const{return director_.sfxCount();}
    std::string currentSongName()const{return director_.currentSongName();}
    int currentSong()const{return director_.currentSong();}
    // SoundSink, called from the game loop.
    void play(int soundId,int vol=100,int pan=0)override{push(CMD_SFX,soundId,vol,pan);}
    void playSong(int songIndex){push(CMD_SONG,songIndex,0,0);}
    void stopSong(){push(CMD_STOP,0,0,0);}
    void setMusicVolume(int percent){push(CMD_MUSIC_VOLUME,percent,0,0);}
    void setSfxVolume(int percent){push(CMD_SFX_VOLUME,percent,0,0);}
private:
    enum { CMD_SFX=1, CMD_SONG=2, CMD_STOP=3, CMD_MUSIC_VOLUME=4, CMD_SFX_VOLUME=5 };
    struct Cmd { int type,a,b,c; };
    void push(int type,int a,int b,int c);
    void drain();
    static int entry(SceSize args,void* argp);
    void threadMain();
    AudioDirector director_;
    int thread_=-1, channel_=-1, rate_=22050, frames_=512;
    bool ready_=false, src_=false, musicReady_=false, sfxReady_=false;
    volatile bool stop_=false, exited_=false;
    volatile unsigned head_=0, tail_=0;
    Cmd ring_[64];
    volatile unsigned blocks_=0;
    int lastError_=0;
};
} // namespace
