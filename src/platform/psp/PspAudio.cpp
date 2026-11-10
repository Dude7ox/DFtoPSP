#include "PspAudio.h"
#include <pspaudio.h>
#include <pspuser.h>
#include <cstring>
namespace dfpsp {
namespace {
// Two blocks so mixing overlaps the DMA of the previous one. 512 stereo frames
// at 22050 Hz is about 23 ms per block, so a swing lands on the frame it was
// requested and the whole buffer set is only 4 KB of .bss.
i16 g_pcm[2][1024] __attribute__((aligned(64)));
// sceKernelStartThread only hands the new thread a pointer to the argument
// block, so the owner lives at file scope (the app has exactly one PspAudio).
PspAudio* g_self=nullptr;
}
int PspAudio::entry(SceSize args,void* argp){
    (void)args;(void)argp;
    PspAudio* self=g_self;
    if(self)self->threadMain();
    return 0;
}
void PspAudio::push(int type,int a,int b,int c){
    unsigned t=tail_,next=(t+1u)&63u;
    if(next==head_)return;             // ring full: drop the cue, never block play
    Cmd cmd;cmd.type=type;cmd.a=a;cmd.b=b;cmd.c=c;
    ring_[t]=cmd;
    tail_=next;
}
void PspAudio::drain(){
    while(head_!=tail_){
        Cmd cmd=ring_[head_];
        head_=(head_+1u)&63u;
        switch(cmd.type){
        case CMD_SFX: director_.playSfxIndex(cmd.a,cmd.b,cmd.c); break;
        case CMD_SONG: director_.playSongIndex(cmd.a,true); break;
        case CMD_STOP: director_.stopSong(); break;
        case CMD_MUSIC_VOLUME: director_.synth().setMusicVolume(cmd.a); break;
        case CMD_SFX_VOLUME: director_.synth().setSfxVolume(cmd.a); break;
        default: break;
        }
    }
}
void PspAudio::threadMain(){
    int which=0;
    while(!stop_){
        drain();
        director_.mix(g_pcm[which],frames_);
        int rc=src_?sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX,g_pcm[which])
                   :sceAudioOutputBlocking(channel_,PSP_AUDIO_VOLUME_MAX,g_pcm[which]);
        if(rc<0)lastError_=rc;
        blocks_++;
        which^=1;
    }
    director_.synth().stopAll();
    exited_=true;
}
bool PspAudio::init(const Vfs& vfs){
    ready_=false; stop_=false; exited_=false; head_=tail_=0; blocks_=0; lastError_=0;
    frames_=512;                        // must stay a multiple of 64
    // The SRC channel accepts 22050 Hz, which halves the synth work against the
    // fixed 44100 Hz hardware channels. Firmware before 3.30 lacks it, so fall
    // back rather than ship a silent port.
    if(sceAudioSRCChReserve(frames_,22050,2)>=0){
        src_=true; channel_=-1; rate_=22050;
    }else{
        src_=false; rate_=44100;
        channel_=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,frames_,PSP_AUDIO_FORMAT_STEREO);
        if(channel_<0){lastError_=channel_;return false;}
    }
    director_.init(rate_);
    musicReady_=director_.openMusic(vfs);
    sfxReady_=director_.openSfx(vfs);
    if(!musicReady_&&!sfxReady_){
        // No audio data at all: keep the channel released and stay silent.
        if(src_)sceAudioSRCChRelease();else sceAudioChRelease(channel_);
        channel_=-1;
        return false;
    }
    std::memset((void*)ring_,0,sizeof(ring_));
    thread_=sceKernelCreateThread("df_audio",entry,0x12,0x8000,THREAD_ATTR_USER|THREAD_ATTR_VFPU,nullptr);
    if(thread_<0){lastError_=thread_;return false;}
    g_self=this;
    int rc=sceKernelStartThread(thread_,sizeof(g_self),&g_self);
    if(rc<0){lastError_=rc;thread_=-1;return false;}
    ready_=true;
    return true;
}
void PspAudio::term(){
    if(!ready_)return;
    stop_=true;
    for(int i=0;i<300&&!exited_;i++)sceKernelDelayThread(1000);   // up to 300 ms
    ready_=false;
    if(src_)sceAudioSRCChRelease();
    else if(channel_>=0)sceAudioChRelease(channel_);
    channel_=-1;
}
} // namespace
