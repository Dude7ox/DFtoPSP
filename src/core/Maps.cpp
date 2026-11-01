#include "Maps.h"
#include "Stream.h"
#include <algorithm>
#include <cctype>
namespace dfpsp {
static std::string lower(std::string s){for(auto&c:s)c=(char)std::tolower((unsigned char)c);return s;}
static bool readRecordElement(Reader& r,size_t end,std::string& name,int& id){
    u32 doors=r.u32_(); if(doors>10000)return false; r.skip(doors*6); size_t h=r.p; if(h+112>end)return false;
    r.seek(h+33); id=r.u16_(); r.seek(h+71); name=r.str(32); r.seek(h+112); return true;
}
bool loadDungeonLayout(const Bsa& maps,int region,int exteriorId,DungeonLayout& out){
    char fn[32]; snprintf(fn,sizeof(fn),"MAPDITEM.%03d",region); std::vector<u8> b; if(!maps.read(fn,b))return false;
    Reader r(b); u32 cnt=r.u32_(); if(cnt>100000)return false;
    for(u32 i=0;i<cnt;i++){ u32 off=r.u32_(); u16 isdu=r.u16_(); u16 ext=r.u16_(); if(ext!=exteriorId)continue;
        size_t p=4+(size_t)cnt*8+off; if(p+4>b.size())return false; r.seek(p); u32 doors=r.u32_(); if(doors>10000)return false; size_t h=p+4+(size_t)doors*6; if(h+129>b.size())return false;
        std::string nm; int id; Reader er(b.data()+h,b.size()-h); if(!readRecordElement(er,b.size(),nm,id))return false;
        out=DungeonLayout{};out.region=region;out.exteriorId=exteriorId;out.interiorId=id;out.name=nm;
        size_t q=h+112; Reader dr(b.data()+q,b.size()-q); dr.u16_();dr.u32_();dr.u32_(); int bc=dr.u16_(); dr.skip(5);
        if(bc<0||bc>4096)return false;
        for(int j=0;j<bc;j++){ int x=(int8_t)dr.u8_(),z=(int8_t)dr.u8_(); u16 bits=dr.u16_(); int idx=bits>>11; if(idx>4)return false; char name[16]; snprintf(name,sizeof(name),"%c%07u.RDB","BNWMS"[idx],bits&1023); out.blocks.push_back({x,z,name,(bits&0x400)!=0}); }
        if(id==50015&&out.blocks.size()>13)out.blocks[13].z=-2;
        return true;
    }
    return false;
}
bool findDungeonByName(const Bsa& maps,const std::string& needle,DungeonLayout& out){
    std::string n=lower(needle);
    #ifndef __PSP__
    for(int reg=0;reg<62;reg++){ try{
#else
    for(int reg=0;reg<62;reg++){
#endif
        char fn[32];snprintf(fn,sizeof(fn),"MAPNAMES.%03d",reg);std::vector<u8> nb; if(!maps.read(fn,nb))continue; Reader nr(nb); u32 nc=nr.u32_(); if(nc>100000)continue;
        snprintf(fn,sizeof(fn),"MAPPITEM.%03d",reg);std::vector<u8> pb; if(!maps.read(fn,pb))continue;
        snprintf(fn,sizeof(fn),"MAPDITEM.%03d",reg);std::vector<u8> db; if(!maps.read(fn,db))continue; Reader dr(db); u32 dc=dr.u32_(); if(dc>100000)continue;
        for(u32 i=0;i<nc;i++){ if(nr.left()<32)break; std::string name=nr.str(32); if(lower(name).find(n)==std::string::npos)continue;
            if((size_t)i*4+4>pb.size())continue; u32 off=pb[i*4]|pb[i*4+1]<<8|pb[i*4+2]<<16|u32(pb[i*4+3])<<24; size_t p=(size_t)nc*4+off; if(p+4>pb.size())continue; Reader pr(pb.data()+p,pb.size()-p); u32 doors=pr.u32_(); if(doors>10000)continue; size_t h=p+4+(size_t)doors*6; if(h+35>pb.size())continue; int ext=pb[h+33]|pb[h+34]<<8;
            #ifndef __PSP__
            try{if(loadDungeonLayout(maps,reg,ext,out)){out.name=name;return true;}}catch(...){}
#else
            if(loadDungeonLayout(maps,reg,ext,out)){out.name=name;return true;}
#endif
        }
#ifndef __PSP__
    }catch(...){}
#endif
    }
    return false;
}
} // namespace
