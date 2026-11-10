#include "Synth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace dfpsp {
namespace {
const float TWO_PI=6.28318530718f;
// Per note gain. A dense orchestral passage keeps roughly ten voices alive, so
// this leaves headroom for the limiter instead of clipping.
const float kNoteGain=0.45f;
const float kTrim=0.92f;
// One extra table entry lets the interpolator wrap without a branch.
const float* sineTable(){
    static float t[1025];
    static bool built=false;
    if(!built){for(int i=0;i<=1024;i++)t[i]=std::sin(TWO_PI*(float)i/1024.0f);built=true;}
    return t;
}
const float* noteFreq(){
    static float f[128];
    static bool built=false;
    if(!built){for(int k=0;k<128;k++)f[k]=440.0f*std::exp2((float)(k-69)/12.0f);built=true;}
    return f;
}
// One voicing per GM family (program / 8): partial amplitudes, ADSR in seconds,
// a lowpass cutoff multiple of the note frequency and a noise amount.
struct InstDef {
    int partials;
    float amp[6];
    float attack, decay, sustain, release;
    float bright, noise;
    bool hiss;
};
const InstDef kFamily[16]={
    {4,{1.0f,0.42f,0.22f,0.10f,0,0},0.004f,1.60f,0.16f,0.30f,6.0f,0.00f,false}, // piano
    {3,{1.0f,0.30f,0.12f,0,0,0},       0.002f,0.70f,0.05f,0.25f,8.0f,0.02f,false}, // chromatic percussion
    {5,{1.0f,0.70f,0.45f,0.28f,0.14f,0},0.010f,0.05f,0.94f,0.06f,7.0f,0.01f,false}, // organ
    {4,{1.0f,0.50f,0.26f,0.12f,0,0},   0.003f,0.80f,0.10f,0.25f,5.0f,0.03f,false}, // guitar / lute
    {3,{1.0f,0.35f,0.12f,0,0,0},       0.006f,0.90f,0.24f,0.18f,3.0f,0.00f,false}, // bass
    {6,{1.0f,0.72f,0.50f,0.34f,0.22f,0.14f},0.110f,0.30f,0.82f,0.35f,4.5f,0.02f,false}, // strings
    {5,{1.0f,0.65f,0.42f,0.26f,0.14f,0},0.220f,0.35f,0.80f,0.60f,3.5f,0.04f,false}, // ensemble / choir
    {5,{1.0f,0.78f,0.45f,0.24f,0.10f,0},0.055f,0.25f,0.86f,0.12f,5.5f,0.02f,false}, // brass
    {5,{1.0f,0.45f,0.30f,0.20f,0.14f,0},0.045f,0.20f,0.86f,0.12f,4.5f,0.03f,false}, // reed
    {3,{1.0f,0.18f,0.06f,0,0,0},       0.070f,0.12f,0.90f,0.10f,3.0f,0.07f,true},  // pipe
    {4,{1.0f,0.33f,0.20f,0.14f,0,0},   0.010f,0.10f,0.88f,0.10f,6.0f,0.00f,false}, // synth lead
    {4,{1.0f,0.55f,0.35f,0.20f,0,0},   0.350f,0.40f,0.85f,0.80f,3.0f,0.02f,false}, // synth pad
    {2,{1.0f,0.40f,0,0,0,0},           0.050f,0.60f,0.50f,0.40f,5.0f,0.30f,true},  // synth effects
    {4,{1.0f,0.55f,0.30f,0.16f,0,0},   0.004f,0.60f,0.12f,0.22f,7.0f,0.04f,false}, // ethnic
    {3,{1.0f,0.50f,0.35f,0,0,0},       0.002f,0.35f,0.05f,0.20f,9.0f,0.25f,true},  // percussive
    {1,{1.0f,0,0,0,0,0},               0.020f,0.40f,0.30f,0.30f,2.0f,0.50f,true},  // sound effects
};
inline float noiseSample(u32& rng){
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;
    return (float)(rng>>8)*(1.0f/8388608.0f)-1.0f;
}
inline float lutSin(float ph){
    const float* t=sineTable();
    float f=ph*1024.0f; int i=(int)f; float k=f-(float)i;
    if(i<0)i=0; else if(i>1023)i=1023;
    return t[i]+(t[i+1]-t[i])*k;
}
inline float limit(float x){
    const float KNEE=0.72f;
    if(x>KNEE)x=KNEE+(x-KNEE)*0.35f;
    else if(x<-KNEE)x=-KNEE+(x+KNEE)*0.35f;
    if(x>1.0f)x=1.0f; else if(x<-1.0f)x=-1.0f;
    return x;
}
inline float expStep(float seconds,int rate,float reach){
    // Per sample multiplier that reaches `reach` of the target after `seconds`.
    if(seconds<=0.0f)return 0.0f;
    return 1.0f-std::exp(-reach/(seconds*(float)rate));
}
} // namespace
void Synth::init(int sampleRate){
    rate_=sampleRate>4000?sampleRate:22050;
    musicVoices_=kMusicVoices;
    sineTable(); noteFreq();
    for(int i=0;i<kVoices;i++){
        Voice& v=voices_[i];
        v=Voice();
        v.idx=i; v.rng=0x2545f491u+(u32)i*2654435761u; v.stage=2;
    }
    resetControllers();
    song_=MidiSong(); evIdx_=0; posTick_=0; ticksPerSample_=0;
    playing_=false; loop_=true; noiseSeed_=0x9e3779b9u;
    notesStarted_=notesCut_=samplesStarted_=samplesCut_=frames_=tick_=0; peak_=0;
    musicVol_=100; sfxVol_=100; muted_=false;
}
void Synth::resetControllers(){
    for(int c=0;c<16;c++){prog_[c]=0;vol_[c]=100;pan_[c]=64;expr_[c]=127;bend_[c]=8192;sus_[c]=false;}
}
bool Synth::loadSong(const u8* data,size_t len){
    MidiSong s;
    if(!parseHmiSong(data,len,s))return false;
    setSong(s);
    return true;
}
void Synth::setSong(const MidiSong& song){
    song_=song;
    ticksPerSample_=(float)(song_.ticksPerSecond()/(double)rate_);
    evIdx_=0; posTick_=0; playing_=false;
}
void Synth::playSong(bool loop){
    if(!song_.ok||song_.events.empty())return;
    loop_=loop;
    resetControllers();
    releaseMusic();
    evIdx_=0; posTick_=0;
    ticksPerSample_=(float)(song_.ticksPerSecond()/(double)rate_);
    playing_=true;
}
void Synth::seek(double seconds){
    if(!song_.ok||seconds<0.0)return;
    double spt=song_.secondsPerTick();
    posTick_=(float)(spt>0.0?seconds/spt:0.0);
    if(posTick_>(float)song_.endTick)posTick_=(float)song_.endTick;
    evIdx_=lowerBoundTick(posTick_);
}
void Synth::releaseMusic(){
    for(int i=0;i<musicVoices_;i++){
        Voice& v=voices_[i];
        if(v.on){v.held=false;v.stage=3;}
    }
}
void Synth::stopSong(){
    playing_=false;
    releaseMusic();
}
void Synth::stopAll(){
    playing_=false;
    for(int i=0;i<kVoices;i++)voices_[i].on=false;
}
size_t Synth::lowerBoundTick(float tick)const{
    size_t lo=0,hi=song_.events.size();
    while(lo<hi){
        size_t mid=(lo+hi)/2;
        if((float)song_.events[mid].tick<tick)lo=mid+1;else hi=mid;
    }
    return lo;
}
void Synth::advance(float frames){
    float target=posTick_+frames*ticksPerSample_;
    while(evIdx_<song_.events.size()&&(float)song_.events[evIdx_].tick<=target){
        dispatch(song_.events[evIdx_]);
        evIdx_++;
    }
    posTick_=target;
    if(song_.endTick==0){playing_=false;return;}
    if(posTick_>=(float)song_.endTick){
        if(loop_){
            posTick_=(float)song_.loopStartTick;
            evIdx_=lowerBoundTick(posTick_);
            resetControllers();
            releaseMusic();
        }else{
            playing_=false;
            releaseMusic();
        }
    }
}
void Synth::dispatch(const MidiEvent& e){
    switch(e.kind){
    case MidiNote:
        if(e.ch==9)startDrum(e);else startNote(e);
        break;
    case MidiNoteOff:
        releaseNote(e.ch,e.d1);
        break;
    case MidiProgram:
        prog_[e.ch&15]=e.d1;
        break;
    case MidiControl:
        switch(e.d1){
        case 7:  vol_[e.ch&15]=e.d2; break;
        case 11: expr_[e.ch&15]=e.d2; break;
        case 10: pan_[e.ch&15]=e.d2; break;
        case 64: {
            bool on=e.d2>=64;
            if(sus_[e.ch&15]&&!on)releasePedal(e.ch&15);
            sus_[e.ch&15]=on;
            break;
        }
        case 121: prog_[e.ch&15]=0; vol_[e.ch&15]=100; pan_[e.ch&15]=64; expr_[e.ch&15]=127; bend_[e.ch&15]=8192; break;
        case 120: case 123: releaseChannel(e.ch&15); break;
        default: break;
        }
        break;
    case MidiBend:
        bend_[e.ch&15]=(int)e.d1|((int)e.d2<<7);
        break;
    default: break;
    }
}
void Synth::clearVoice(Voice& v){
    int idx=v.idx; u32 age=v.age;
    v=Voice();
    v.idx=idx; v.age=age; v.stage=2;
    noiseSeed_=noiseSeed_*1664525u+1013904223u;
    v.rng=noiseSeed_|1u;
}
void Synth::applyPan(Voice& v,int pan)const{
    float t=(float)(pan<0?0:(pan>127?127:pan))/127.0f*(TWO_PI*0.25f);
    v.panL=std::cos(t); v.panR=std::sin(t);
}
Synth::Voice* Synth::allocVoice(bool sfx){
    int lo=sfx?musicVoices_:0, hi=sfx?kVoices:musicVoices_;
    Voice* best=nullptr; float bestScore=1e30f;
    for(int i=lo;i<hi;i++){
        Voice& v=voices_[i];
        if(!v.on)return &v;
        float score=v.env*v.gain;
        if(v.stage==3)score*=0.25f;
        if(v.held)score*=0.5f;
        if(score<bestScore){bestScore=score;best=&v;}
    }
    if(best){
        if(sfx)samplesCut_++;else notesCut_++;
        best->on=false;
    }
    return best;
}
void Synth::startNote(const MidiEvent& e){
    if(e.d1>127)return;
    const InstDef& f=kFamily[(prog_[e.ch&15])>>3];
    float freq=noteFreq()[e.d1];
    int bend=bend_[e.ch&15];
    if(bend!=8192)freq*=std::exp2(((float)(bend-8192)*2.0f/8192.0f)/12.0f);
    Voice* v=allocVoice(false); if(!v)return;
    clearVoice(*v);
    v->on=true; v->ch=e.ch&15; v->key=e.d1;
    float nyq=0.45f*(float)rate_;
    int parts=f.partials;
    float sum=0;
    for(int i=0;i<parts;i++){
        if(freq*(float)(i+1)>nyq){parts=i;break;}
        sum+=f.amp[i];
    }
    if(parts<1){parts=1;sum=f.amp[0];}
    if(sum<=0.0f)sum=1.0f;
    v->partials=parts;
    for(int i=0;i<parts;i++){
        v->amp[i]=f.amp[i]/sum;
        v->step[i]=freq*(float)(i+1)/(float)rate_;
        v->phase[i]=(float)i*0.137f;
    }
    float vel=(float)e.d2/127.0f;
    v->gain=kNoteGain*vel*((float)vol_[v->ch]/127.0f)*((float)expr_[v->ch]/127.0f)*(musicVol_/100.0f);
    v->atkStep=f.attack>0.0f?1.0f/(f.attack*(float)rate_):1.0f;
    v->decK=expStep(f.decay,rate_,3.0f);
    v->susLevel=f.sustain;
    v->relMul=std::exp(-5.0f/(f.release>0.02f?f.release:0.02f)/(float)rate_);
    v->stage=0; v->env=0;
    float cut=freq*f.bright; if(cut>nyq)cut=nyq;
    float w=TWO_PI*cut/(float)rate_;
    v->lpK=w/(1.0f+w);
    v->noiseMix=f.noise; v->hiss=f.hiss;
    applyPan(*v,pan_[v->ch]);
    notesStarted_++;
}
void Synth::startDrum(const MidiEvent& e){
    Voice* v=allocVoice(false); if(!v)return;
    clearVoice(*v);
    v->on=true; v->ch=9; v->key=e.d1;
    const int key=e.d1;
    float tone=0, toneAmp=0, noiseAmp=0, dropTo=0;
    bool hiss=false;
    float attack=0.002f, decay=0.25f, release=0.08f;
    switch(key){
    case 35: case 36: tone=158; toneAmp=1.0f; dropTo=46; decay=0.30f; release=0.10f; break;              // bass drum
    case 37: tone=1400; toneAmp=0.30f; noiseAmp=0.60f; hiss=true; decay=0.05f; release=0.03f; break;     // side stick
    case 38: case 40: tone=192; toneAmp=0.55f; noiseAmp=0.75f; decay=0.16f; release=0.07f; break;        // snare
    case 39: noiseAmp=0.90f; hiss=true; decay=0.13f; release=0.06f; break;                              // hand clap
    case 42: case 44: noiseAmp=0.60f; hiss=true; decay=0.05f; release=0.03f; break;                      // closed hat
    case 46: noiseAmp=0.60f; hiss=true; decay=0.38f; release=0.15f; break;                              // open hat
    case 49: case 52: case 55: case 57: case 59: noiseAmp=0.55f; hiss=true; decay=1.10f; release=0.40f; break; // crashes
    case 51: case 53: tone=6400; toneAmp=0.10f; noiseAmp=0.40f; hiss=true; decay=0.90f; release=0.30f; break;  // ride
    case 56: tone=540; toneAmp=0.50f; noiseAmp=0.25f; hiss=true; decay=0.30f; release=0.12f; break;      // cowbell
    case 41: case 43: case 45: case 47: case 48: case 50:                                                 // toms
        tone=key<128?noteFreq()[key]:200.0f; toneAmp=1.0f; dropTo=tone*0.55f; decay=0.28f; release=0.10f; break;
    case 58: noiseAmp=0.50f; hiss=true; decay=0.20f; release=0.08f; break;                              // vibraslap
    case 62: case 63: case 64: tone=key<128?noteFreq()[key]:300.0f; toneAmp=0.80f; decay=0.12f; release=0.06f; break; // congas
    default: tone=key<128?noteFreq()[key]:440.0f; toneAmp=0.70f; decay=0.20f; release=0.08f; break;      // melodic percussion
    }
    int parts=0;
    if(toneAmp>0.0f){
        float nyq=0.45f*(float)rate_;
        if(tone<=nyq){
            v->amp[0]=toneAmp; v->step[0]=tone/(float)rate_; parts=1;
            if(tone*2.0f<=nyq&&noiseAmp<=0.0f){v->amp[1]=toneAmp*0.25f;v->step[1]=tone*2.0f/(float)rate_;parts=2;}
        }
        v->drop=dropTo>0.0f?std::exp(-std::log(tone/dropTo)/(0.10f*(float)rate_)):1.0f;
    }
    v->partials=parts;
    v->phase[0]=0.25f;
    v->noiseMix=noiseAmp; v->hiss=hiss;
    float vel=(float)e.d2/127.0f;
    v->gain=kNoteGain*1.25f*vel*((float)vol_[9]/127.0f)*(musicVol_/100.0f);
    v->atkStep=attack>0.0f?1.0f/(attack*(float)rate_):1.0f;
    v->decK=expStep(decay,rate_,3.0f);
    v->susLevel=0.0f;
    v->relMul=std::exp(-5.0f/(release>0.02f?release:0.02f)/(float)rate_);
    v->stage=0; v->env=0; v->lpK=1.0f;
    applyPan(*v,pan_[9]);
    notesStarted_++;
}
void Synth::releaseNote(int ch,int key){
    for(int i=0;i<musicVoices_;i++){
        Voice& v=voices_[i];
        if(v.on&&v.ch==ch&&v.key==key&&v.stage!=3){
            if(sus_[ch]){v.held=true;continue;}
            v.stage=3; v.held=false;
        }
    }
}
void Synth::releaseChannel(int ch){
    for(int i=0;i<musicVoices_;i++){
        Voice& v=voices_[i];
        if(v.on&&v.ch==ch){v.stage=3;v.held=false;}
    }
}
void Synth::releasePedal(int ch){
    for(int i=0;i<musicVoices_;i++){
        Voice& v=voices_[i];
        if(v.on&&v.ch==ch&&v.held){v.held=false;v.stage=3;}
    }
}
void Synth::playSample(const u8* pcm,size_t len,int rate,int vol,int pan){
    if(!pcm||len==0||rate<=0||vol<=0)return;
    Voice* v=allocVoice(true); if(!v)return;
    clearVoice(*v);
    v->on=true; v->sfx=true; v->pcm=pcm; v->pcmLen=len;
    v->pcmStep=(float)rate/(float)rate_;
    v->gain=0.62f*(float)(vol<0?0:(vol>127?127:vol))/127.0f*(sfxVol_/100.0f);
    v->env=1.0f; v->susLevel=1.0f; v->stage=2; v->partials=0; v->lpK=1.0f;
    int p=pan<-100?-100:(pan>100?100:pan);
    float t=(float)(p+100)/200.0f*(TWO_PI*0.25f);
    v->panL=std::cos(t); v->panR=std::sin(t);
    samplesStarted_++;
}
void Synth::stopSamples(const u8* pcm){
    if(!pcm)return;
    for(int i=musicVoices_;i<kVoices;i++){
        Voice& v=voices_[i];
        if(v.on&&v.sfx&&v.pcm==pcm){v.on=false;v.pcm=nullptr;}
    }
}
float Synth::toneStep(Voice& v){
    float s=0;
    for(int i=0;i<v.partials;i++){
        s+=v.amp[i]*lutSin(v.phase[i]);
        float p=v.phase[i]+v.step[i];
        if(p>=1.0f)p-=1.0f;
        v.phase[i]=p;
        if(v.drop!=1.0f)v.step[i]*=v.drop;
    }
    if(v.lpK<1.0f){v.lpY+=v.lpK*(s-v.lpY);s=v.lpY;}
    if(v.noiseMix>0.0f){
        float n=noiseSample(v.rng);
        if(v.hiss){v.nlp+=0.35f*(n-v.nlp);n-=v.nlp;}
        s+=n*v.noiseMix;
    }
    switch(v.stage){
    case 0: v.env+=v.atkStep; if(v.env>=1.0f){v.env=1.0f;v.stage=1;} break;
    case 1: v.env+=(v.susLevel-v.env)*v.decK;
            if(v.env<=v.susLevel+0.002f){v.env=v.susLevel;v.stage=2;} break;
    case 2: break;
    default:
        v.env*=v.relMul;
        if(v.env<0.0015f){v.on=false;v.env=0;return 0.0f;}
        break;
    }
    return s*v.env*v.gain;
}
float Synth::sfxStep(Voice& v){
    if(!v.pcm||v.pcmPos>=(float)v.pcmLen){v.on=false;return 0.0f;}
    size_t i=(size_t)v.pcmPos;
    float f=v.pcmPos-(float)i;
    float a=(float)((int)v.pcm[i]-128)*(1.0f/128.0f);
    float b=(i+1<v.pcmLen)?(float)((int)v.pcm[i+1]-128)*(1.0f/128.0f):a;
    float s=a+(b-a)*f;
    v.pcmPos+=v.pcmStep;
    v.dc+=0.0025f*(s-v.dc);          // DC blocker: the raw samples sit on 128
    s-=v.dc;
    float fade=0.004f*(float)rate_;
    float remain=(float)v.pcmLen-v.pcmPos;
    float g=1.0f;
    if(v.pcmPos<fade)g=v.pcmPos/fade;
    if(remain<fade){float r=remain/fade;if(r<g)g=r;}
    if(g<0.0f)g=0.0f;
    return s*v.gain*g;
}
float Synth::voiceStep(Voice& v){
    v.age++;
    return v.sfx?sfxStep(v):toneStep(v);
}
void Synth::mix(i16* out,int frames){
    if(muted_){std::memset(out,0,(size_t)frames*4);frames_+=(u32)frames;return;}
    for(int i=0;i<frames;i++){
        float L=0,R=0;
        for(int k=0;k<kVoices;k++){
            Voice& v=voices_[k];
            if(!v.on)continue;
            float s=voiceStep(v);
            if(!v.on)continue;
            L+=s*v.panL; R+=s*v.panR;
        }
        float l=limit(L*kTrim), r=limit(R*kTrim);
        int il=(int)(l*32767.0f), ir=(int)(r*32767.0f);
        if(il>32767)il=32767; else if(il<-32768)il=-32768;
        if(ir>32767)ir=32767; else if(ir<-32768)ir=-32768;
        int m=il<0?-il:il; if(m>peak_)peak_=m;
        m=ir<0?-ir:ir; if(m>peak_)peak_=m;
        out[(size_t)i*2]=(i16)il; out[(size_t)i*2+1]=(i16)ir;
    }
    frames_+=(u32)frames;
}
void Synth::render(i16* stereo,int frames){
    if(!stereo||frames<=0)return;
    tick_++;
    const int CHUNK=64;
    for(int done=0;done<frames;){
        int n=frames-done; if(n>CHUNK)n=CHUNK;
        if(playing_)advance((float)n);
        mix(stereo+(size_t)done*2,n);
        done+=n;
    }
}
int Synth::activeVoices()const{
    int n=0;
    for(int i=0;i<kVoices;i++)if(voices_[i].on)n++;
    return n;
}
} // namespace
