#pragma once
#include "Types.h"
#include "Bsa.h"
#include "Sfx.h"
#include "Synth.h"
#include "Vfs.h"
namespace dfpsp {
// One place that knows how Daggerfall audio is wired together: MIDI.BSA holds
// the HMI songs, DAGGER.SND holds the sound effects, and the synth turns both
// into stereo samples. The PSP drives it from its audio thread while the main
// thread only pushes indices (see platform/psp/PspAudio); the host renders the
// very same object into a WAV file, so what the tests measure is what a device
// plays.
class AudioDirector {
public:
    void init(int sampleRate=22050);
    bool openMusic(const Vfs& vfs,const std::string& name="MIDI.BSA");
    bool openSfx(const Vfs& vfs,const std::string& name="DAGGER.SND");
    bool musicReady()const{return musicReady_;}
    bool sfxReady()const{return sfxReady_;}
    const std::string& lastError()const{return error_;}
    // Read only lookups, safe from any thread once the archives are open.
    size_t songCount()const{return midi_.count();}
    int songIndex(const std::string& name)const{return midi_.find(name);}
    std::string songName(int index)const;
    size_t sfxCount()const{return sfx_.count();}
    // Audio thread side.
    bool playSongIndex(int index,bool loop=true);
    bool playSongName(const std::string& name,bool loop=true);
    void stopSong();
    void seekSong(double seconds){synth_.seek(seconds);}
    bool playSfxIndex(int index,int vol=100,int pan=0);
    void mix(i16* stereo,int frames);
    int currentSong()const{return currentSong_;}
    std::string currentSongName()const;
    Synth& synth(){return synth_;}
    const Synth& synth()const{return synth_;}
    SfxBank& sfx(){return sfx_;}
    const Bsa& musicArchive()const{return midi_;}
    // Host helpers: render the live mix, or convert one raw record.
    bool renderWav(const std::string& path,double seconds);
    static bool writeWav(const std::string& path,int rate,const std::vector<i16>& pcm);
    static void unsigned8ToStereo16(const u8* src,size_t n,std::vector<i16>& out);
private:
    Synth synth_;
    Bsa midi_;
    SfxBank sfx_;
    bool musicReady_=false, sfxReady_=false;
    int currentSong_=-1;
    std::string error_;
};
} // namespace
