#pragma once
#include "Types.h"
namespace dfpsp {
// The classic engine's linear congruential generator. Encounter tables and
// enemy rolls reproduce the original sequences when seeded with the location id.
class DfRandom {
public:
    explicit DfRandom(u32 seed=1):next_(seed?seed:1u){}
    void seed(u32 s){next_=s?s:1u;}
    u32 state()const{return (u32)next_;}
    u32 rand(){next_=next_*1103515245ull+12345ull;return (u32)((next_>>16)&0x7fffu);}
    int range(int maxExclusive){return maxExclusive>0?(int)(rand()%(u32)maxExclusive):0;}
    int range(int min,int maxExclusive){return maxExclusive>min?(int)(rand()%(u32)(maxExclusive-min))+min:min;}
    int rangeInclusive(int min,int maxInclusive){return maxInclusive>=min?(int)(rand()%(u32)(maxInclusive-min+1))+min:min;}
    int percent(){return rangeInclusive(1,100);}
private:
    u64 next_;
};
} // namespace dfpsp
