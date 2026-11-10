#include "Midi.h"
#include <algorithm>
namespace dfpsp {
namespace {
struct HmiReader {
    const u8* d=nullptr; size_t n=0, p=0;
    u16 u16at(size_t i)const{return (u16)((u16)d[i]|((u16)d[i+1]<<8));}
    u32 u32at(size_t i)const{
        return (u32)d[i]|((u32)d[i+1]<<8)|((u32)d[i+2]<<16)|((u32)d[i+3]<<24);
    }
    u8 byte(){return p<n?d[p++]:0;}
    // Standard SMF variable length quantity: the high bit asks for another byte.
    u32 vlq(){
        u32 v=0;
        for(int i=0;i<4&&p<n;i++){u8 c=d[p++];v=(v<<7)|(c&0x7f);if(!(c&0x80))break;}
        return v;
    }
};
int rankOf(u8 k){
    switch(k){
    case MidiProgram: case MidiControl: return 0;
    case MidiNoteOff: return 1;
    case MidiNote: case MidiBend: return 2;
    default: return 3;
    }
}
} // namespace
bool parseHmiSong(const u8* data,size_t len,MidiSong& out){
    out=MidiSong();
    static const char SONG_MAGIC[12]={'H','M','I','-','M','I','D','I','S','O','N','G'};
    static const char TRACK_MAGIC[13]={'H','M','I','-','M','I','D','I','T','R','A','C','K'};
    if(!data||len<0xEC){out.error="song shorter than an HMI header";return false;}
    for(int i=0;i<12;i++)if(data[i]!=(u8)SONG_MAGIC[i]){out.error="not an HMI song";return false;}
    HmiReader r; r.d=data; r.n=len;
    u16 pulses=r.u16at(0xD4);
    out.ppqn=(u32)(pulses?pulses:120)<<2;
    u32 trackCount=r.u32at(0xE4), tableOff=r.u32at(0xE8);
    if(trackCount==0||trackCount>64){out.error="track count out of range";return false;}
    if(tableOff>=len||(size_t)tableOff+4*(size_t)trackCount>len){out.error="track table past the end";return false;}
    out.tracks=(int)trackCount;
    bool haveLoop=false;
    for(u32 t=0;t<trackCount;t++){
        u32 start=r.u32at(tableOff+4*(size_t)t);
        u32 end=(t+1<trackCount)?r.u32at(tableOff+4*(size_t)(t+1)):(u32)len;
        if(start>=len||end>len||end<=start){out.error="track range out of bounds";return false;}
        if(end-start<0x5B){out.error="track header truncated";return false;}
        for(int i=0;i<13;i++)if(data[start+i]!=(u8)TRACK_MAGIC[i]){out.error="not an HMI track";return false;}
        u32 dataOff=r.u32at(start+0x57);
        if(dataOff>=end-start){out.error="track events past the track";return false;}
        r.p=start+dataOff;
        u32 trackEnd=end;
        u32 tick=0, lastTick=0; u8 running=0xff;
        while(r.p<trackEnd){
            u32 delta=r.vlq();
            // Some HMI files carry corrupt four byte deltas; clamp them instead
            // of walking off the track (the same guard other HMI players use).
            if(delta>0xFFFFu)delta=0;
            tick+=delta;
            if(r.p>=trackEnd)break;
            u8 status=r.byte();
            if(status==0xFF){                       // meta event
                running=0xff;
                u8 type=r.byte(); u32 ml=r.vlq();
                if(ml>trackEnd-r.p){out.error="meta event past the track";return false;}
                if(type==0x51&&ml>=3)out.tempoUs=(u32)data[r.p]<<16|(u32)data[r.p+1]<<8|data[r.p+2];
                r.p+=ml;
                if(type==0x2F){lastTick=std::max(lastTick,tick);break;}  // end of track
            }else if(status==0xF0||status==0xF7){    // system exclusive
                running=0xff;
                u32 sl=r.vlq();
                if(sl>trackEnd-r.p){out.error="sysex past the track";return false;}
                r.p+=sl;
            }else if(status==0xFE){                  // HMI specific sub event
                running=0xff;
                u8 sub=r.byte();
                if(sub==0x10){
                    if(r.p+2>trackEnd){out.error="hmi 0x10 truncated";return false;}
                    r.p+=2; u32 extra=r.byte();
                    if((u32)extra+4>trackEnd-r.p){out.error="hmi 0x10 block truncated";return false;}
                    r.p+=extra+4;
                }else if(sub==0x12||sub==0x14){
                    if(r.p+2>trackEnd){out.error="hmi loop start truncated";return false;}
                    r.p+=2;
                    if(sub==0x14){
                        MidiEvent e;e.tick=tick;e.kind=MidiLoopStart;out.events.push_back(e);
                        out.loopStartTick=haveLoop?std::min(out.loopStartTick,tick):tick;haveLoop=true;
                    }
                }else if(sub==0x13||sub==0x15){
                    u32 skip=(sub==0x13)?10u:6u;
                    if(r.p+skip>trackEnd){out.error="hmi loop end truncated";return false;}
                    r.p+=skip;
                    if(sub==0x15){
                        MidiEvent e;e.tick=tick;e.kind=MidiLoopEnd;out.events.push_back(e);
                        out.loopEndTick=std::max(out.loopEndTick,tick);
                    }
                }else{out.error="unknown hmi sub event";return false;}
            }else if(status<=0xEF){                  // voice message
                u8 actual,d1;
                if(status>=0x80){actual=status;d1=r.byte();running=status;}
                else{
                    if(running==0xff){out.error="running status without a previous message";return false;}
                    actual=running;d1=status;
                }
                u8 type=(u8)(actual&0xF0), ch=(u8)(actual&0x0F);
                if(type==0xC0||type==0xD0){          // program change / channel pressure
                    if(type==0xC0){MidiEvent e;e.tick=tick;e.kind=MidiProgram;e.ch=ch;e.d1=d1;out.events.push_back(e);}
                }else{
                    if(r.p>=trackEnd){out.error="voice message truncated";return false;}
                    u8 d2=r.byte();
                    MidiEvent e;e.tick=tick;e.ch=ch;e.d1=d1;e.d2=d2;
                    if(type==0x90&&d2>0){            // note on, duration follows
                        u32 dur=r.vlq();
                        e.kind=MidiNote;e.dur=dur;out.events.push_back(e);
                        out.notes++;
                        MidiEvent off;off.tick=tick+dur;off.kind=MidiNoteOff;off.ch=ch;off.d1=d1;
                        out.events.push_back(off);
                        lastTick=std::max(lastTick,tick+dur);
                    }else if(type==0xB0){
                        e.kind=MidiControl;out.events.push_back(e);
                    }else if(type==0xE0){
                        e.kind=MidiBend;out.events.push_back(e);
                    }
                    // 0x80 note off and 0xA0 poly pressure carry no HMI meaning.
                }
                lastTick=std::max(lastTick,tick);
            }else{out.error="unexpected status byte";return false;}
        }
        out.endTick=std::max(out.endTick,lastTick);
    }
    if(!haveLoop)out.loopStartTick=0;
    if(out.loopEndTick<out.loopStartTick)out.loopEndTick=out.endTick;
    std::stable_sort(out.events.begin(),out.events.end(),[](const MidiEvent& a,const MidiEvent& b){
        if(a.tick!=b.tick)return a.tick<b.tick;
        return rankOf(a.kind)<rankOf(b.kind);
    });
    out.ok=true;
    return true;
}
MidiSong makeTestSong(int key,int program,double seconds,double tempoUs,u32 ppqn){
    MidiSong s;
    s.ppqn=ppqn;s.tempoUs=(u32)tempoUs;
    double tps=s.ticksPerSecond();
    u32 end=(u32)(seconds*tps);
    MidiEvent p;p.tick=0;p.kind=MidiProgram;p.ch=0;p.d1=(u8)program;s.events.push_back(p);
    MidiEvent v;v.tick=0;v.kind=MidiControl;v.ch=0;v.d1=7;v.d2=127;s.events.push_back(v);
    MidiEvent on;on.tick=24;on.kind=MidiNote;on.ch=0;on.d1=(u8)key;on.d2=100;on.dur=end-48;s.events.push_back(on);
    MidiEvent off;off.tick=end-24;off.kind=MidiNoteOff;off.ch=0;off.d1=(u8)key;s.events.push_back(off);
    s.notes=1;s.tracks=1;s.endTick=end;s.loopStartTick=0;s.loopEndTick=end;s.ok=true;
    std::stable_sort(s.events.begin(),s.events.end(),[](const MidiEvent& a,const MidiEvent& b){
        if(a.tick!=b.tick)return a.tick<b.tick;
        return rankOf(a.kind)<rankOf(b.kind);
    });
    return s;
}
} // namespace
