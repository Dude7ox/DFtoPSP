#pragma once
#include "Types.h"
#include "Midi.h"
namespace dfpsp {
// Software synthesiser for the HMI songs in MIDI.BSA plus a mixer for the raw
// 8-bit unsigned mono samples in DAGGER.SND. The classic engine sent its music
// to a General MIDI device, so notes are voiced per GM program family with a
// few sine partials, an ADSR envelope, a one pole lowpass and a noise source
// for drums and breath; that keeps the Allegrex cost near zero (no filters
// beyond a single pole, no allocation while running) while the melodies stay
// recognisable.
//
// Everything here is deterministic and portable: the host build renders the
// exact same samples the PSP audio thread pushes into sceAudio, which is what
// makes the pitch and level tests in tests/test_core.cpp meaningful.
class Synth {
public:
    static constexpr int kVoices=28;      // total polyphony
    static constexpr int kSfxVoices=6;    // reserved for DAGGER.SND samples
    static constexpr int kMusicVoices=kVoices-kSfxVoices;
    void init(int sampleRate=22050);
    int sampleRate()const{return rate_;}
    int musicVoices()const{return musicVoices_;}
    // Song handling. loadSong parses an HMI record; setSong takes a song that
    // was built in code (tests, or a mod that ships its own event list).
    bool loadSong(const u8* data,size_t len);
    void setSong(const MidiSong& song);
    const MidiSong& song()const{return song_;}
    bool hasSong()const{return song_.ok;}
    void playSong(bool loop=true);
    void stopSong();                    // stops the sequencer, voices ring out
    void stopAll();                     // stops the sequencer and silences now
    bool songPlaying()const{return playing_;}
    bool looping()const{return loop_;}
    double songPositionSec()const{return posTick_*song_.secondsPerTick();}
    // Moves the sequencer without sounding the notes it skips, so a render can
    // start part way into a track (host samples, quick auditioning).
    void seek(double seconds);
    // Plays one DAGGER.SND record. `pcm` must stay valid until it finishes;
    // vol is 0..127 and pan is -100..100 (left..right).
    void playSample(const u8* pcm,size_t len,int rate,int vol=100,int pan=0);
    // Stops every sample voice that reads `pcm`, used when the sound effect
    // cache evicts the record a voice is still playing.
    void stopSamples(const u8* pcm);
    // Fills `frames` stereo sample pairs and advances the sequencer.
    void render(i16* stereo,int frames);
    void setMusicVolume(int percent){musicVol_=percent<0?0:(percent>100?100:percent);}
    void setSfxVolume(int percent){sfxVol_=percent<0?0:(percent>100?100:percent);}
    int musicVolume()const{return musicVol_;}
    int sfxVolume()const{return sfxVol_;}
    void setMuted(bool m){muted_=m;}
    bool muted()const{return muted_;}
    // Diagnostics used by the HUD, --stats and the tests.
    int activeVoices()const;
    u32 notesStarted()const{return notesStarted_;}
    u32 notesCut()const{return notesCut_;}
    u32 samplesStarted()const{return samplesStarted_;}
    u32 samplesCut()const{return samplesCut_;}
    u32 framesRendered()const{return frames_;}
    int peakAbs()const{return peak_;}
    void resetPeak(){peak_=0;}
private:
    struct Voice {
        bool on=false, sfx=false, held=false, hiss=false;
        int idx=0, ch=0, key=0, partials=0;
        float phase[6]={}, step[6]={}, amp[6]={};
        float env=0, atkStep=0, decK=0, susLevel=0, relMul=1;
        int stage=0;                     // 0 attack, 1 decay, 2 sustain, 3 release
        float gain=0, panL=1, panR=0;
        float lpY=0, lpK=1, noiseMix=0, drop=1, nlp=0;
        u32 rng=0x2545f491;
        const u8* pcm=nullptr; size_t pcmLen=0; float pcmPos=0, pcmStep=0, dc=0;
        u32 age=0;                       // frames rendered, for voice stealing
    };
    void advance(float frames);
    void dispatch(const MidiEvent& e);
    void startNote(const MidiEvent& e);
    void startDrum(const MidiEvent& e);
    void releaseNote(int ch,int key);
    void releaseChannel(int ch);
    void releasePedal(int ch);
    void mix(i16* out,int frames);
    float voiceStep(Voice& v);
    float toneStep(Voice& v);
    float sfxStep(Voice& v);
    Voice* allocVoice(bool sfx);
    void clearVoice(Voice& v);
    void applyPan(Voice& v,int pan)const;
    size_t lowerBoundTick(float tick)const;
    void resetControllers();
    void releaseMusic();
    int rate_=22050, musicVoices_=kMusicVoices;
    int musicVol_=100, sfxVol_=100; bool muted_=false;
    Voice voices_[kVoices];
    u32 noiseSeed_=0x9e3779b9u;
    MidiSong song_;
    size_t evIdx_=0; float posTick_=0, ticksPerSample_=0;
    bool playing_=false, loop_=true;
    u8 prog_[16]={}, vol_[16]={}, pan_[16]={}, expr_[16]={};
    int bend_[16]={};
    bool sus_[16]={};
    u32 notesStarted_=0, notesCut_=0, samplesStarted_=0, samplesCut_=0, frames_=0, tick_=0;
    int peak_=0;
};
} // namespace
