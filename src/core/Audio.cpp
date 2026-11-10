#include "Audio.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace dfpsp {
void AudioDirector::init(int sampleRate){
    synth_.init(sampleRate);
    musicReady_=false; sfxReady_=false; currentSong_=-1; error_.clear();
}
bool AudioDirector::openMusic(const Vfs& vfs,const std::string& name){
    musicReady_=midi_.open(vfs,name);
    if(!musicReady_)error_="cannot open "+name;
    return musicReady_;
}
bool AudioDirector::openSfx(const Vfs& vfs,const std::string& name){
    sfxReady_=sfx_.open(vfs,name);
    if(!sfxReady_)error_="cannot open "+name;
    return sfxReady_;
}
std::string AudioDirector::songName(int index)const{
    const BsaEntry* e=midi_.entry((u32)index);
    return e?e->name:std::string();
}
std::string AudioDirector::currentSongName()const{
    return currentSong_>=0?songName(currentSong_):std::string();
}
bool AudioDirector::playSongIndex(int index,bool loop){
    if(!musicReady_)return false;
    std::vector<u8> data;
    if(!midi_.read((u32)index,data)||data.empty()){error_="cannot read song record";return false;}
    if(!synth_.loadSong(data.data(),data.size())){
        error_="cannot parse song: "+synth_.song().error;
        currentSong_=-1;
        return false;
    }
    synth_.playSong(loop);
    currentSong_=index;
    return true;
}
bool AudioDirector::playSongName(const std::string& name,bool loop){
    int idx=songIndex(name);
    if(idx<0){error_="no song named "+name;return false;}
    return playSongIndex(idx,loop);
}
void AudioDirector::stopSong(){
    synth_.stopSong();
    currentSong_=-1;
}
bool AudioDirector::playSfxIndex(int index,int vol,int pan){
    if(!sfxReady_)return false;
    SfxClip clip; SfxEvicted evicted;
    if(!sfx_.clip(index,clip,&evicted))return false;
    for(int i=0;i<evicted.n;i++)synth_.stopSamples(evicted.ptr[i]);
    synth_.playSample(clip.pcm,clip.len,clip.rate,vol,pan);
    return true;
}
void AudioDirector::mix(i16* stereo,int frames){
    synth_.render(stereo,frames);
}
void AudioDirector::unsigned8ToStereo16(const u8* src,size_t n,std::vector<i16>& out){
    out.resize(n*2);
    for(size_t i=0;i<n;i++){
        int s=((int)src[i]-128)*256;
        if(s>32767)s=32767; else if(s<-32768)s=-32768;
        out[i*2]=(i16)s; out[i*2+1]=(i16)s;
    }
}
bool AudioDirector::writeWav(const std::string& path,int rate,const std::vector<i16>& pcm){
    FILE* f=fopen(path.c_str(),"wb");
    if(!f)return false;
    u32 dataBytes=(u32)(pcm.size()*2);
    u32 riff=36+dataBytes;
    u16 chans=2, bits=16, align=4, blockAlign=4;
    u32 byteRate=(u32)rate*4u;
    auto put32=[&](u32 v){u8 b[4]={u8(v),u8(v>>8),u8(v>>16),u8(v>>24)};fwrite(b,1,4,f);};
    auto put16=[&](u16 v){u8 b[2]={u8(v),u8(v>>8)};fwrite(b,1,2,f);};
    fwrite("RIFF",1,4,f); put32(riff); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); put32(16); put16(1); put16(chans); put32((u32)rate); put32(byteRate);
    put16(blockAlign); put16(bits);
    (void)align;
    fwrite("data",1,4,f); put32(dataBytes);
    if(!pcm.empty())fwrite(pcm.data(),1,pcm.size()*2,f);
    fclose(f);
    return true;
}
bool AudioDirector::renderWav(const std::string& path,double seconds){
    int rate=synth_.sampleRate();
    size_t total=(size_t)(seconds*(double)rate);
    if(total==0)return false;
    std::vector<i16> pcm(total*2);
    const size_t BLOCK=1024;
    for(size_t done=0;done<total;){
        size_t n=std::min(BLOCK,total-done);
        synth_.render(&pcm[done*2],(int)n);
        done+=n;
    }
    return writeWav(path,rate,pcm);
}
} // namespace
