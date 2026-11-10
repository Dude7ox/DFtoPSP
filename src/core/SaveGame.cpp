#include "SaveGame.h"
#include <cstdio>
#include <cstring>
namespace dfpsp {
Enemy toEnemy(const SavedEnemy& s){
    Enemy e;
    e.mobileId=s.mobileId;e.pos=s.pos;e.yaw=s.yaw;
    e.female=s.female;e.passive=s.passive;e.water=s.water;e.flying=s.flying;
    e.state=(EnemyState)s.state;e.level=s.level;e.maxHp=s.maxHp;e.hp=s.hp;e.armor=s.armor;
    e.speed=s.speed;e.animState=s.animState;e.facing=s.facing;e.dir=s.dir;e.frame=s.frame;
    e.bodyW=s.bodyW;e.bodyH=s.bodyH;e.aggro=s.aggro;e.mirrored=s.mirrored;e.grounded=s.grounded;
    e.key=s.key;
    return e;
}
SavedEnemy fromEnemy(const Enemy& e){
    SavedEnemy s;
    s.mobileId=e.mobileId;s.pos=e.pos;s.yaw=e.yaw;
    s.female=e.female;s.passive=e.passive;s.water=e.water;s.flying=e.flying;
    s.state=(int)e.state;s.level=e.level;s.maxHp=e.maxHp;s.hp=e.hp;s.armor=e.armor;
    s.speed=e.speed;s.animState=e.animState;s.facing=e.facing;s.dir=e.dir;s.frame=e.frame;
    s.bodyW=e.bodyW;s.bodyH=e.bodyH;s.aggro=e.aggro;s.mirrored=e.mirrored;s.grounded=e.grounded;
    s.key=e.key;
    return s;
}
namespace {
struct W {
    std::vector<u8> b;
    void bytes(const void* p,size_t n){const u8* q=(const u8*)p;for(size_t i=0;i<n;i++)b.push_back(q[i]);}
    void pu32(u32 v){bytes(&v,4);}
    void pi32(i32 v){bytes(&v,4);}
    void pf32(float v){bytes(&v,4);}
    void pu8(u8 v){b.push_back(v);}
    void pstr(const std::string& s){pu32((u32)s.size());bytes(s.data(),s.size());}
};
struct R {
    const std::vector<u8>& b; size_t p=0; bool ok=true;
    R(const std::vector<u8>& v):b(v){}
    void take(void* dst,size_t n){if(p+n>b.size()){ok=false;return;}memcpy(dst,b.data()+p,n);p+=n;}
    u32 u32v(){u32 v=0;take(&v,4);return v;}
    i32 i32v(){i32 v=0;take(&v,4);return v;}
    float f32v(){float v=0;take(&v,4);return v;}
    u8 u8v(){u8 v=0;take(&v,1);return v;}
    std::string str(){u32 n=u32v();if(!ok||p+n>b.size()){ok=false;return std::string();}std::string s((const char*)b.data()+p,n);p+=n;return s;}
};
} // namespace
bool writeSave(const std::string& path,const SaveData& d){
    W w;
    w.bytes("DFPS",4);
    w.pu32(d.version);
    w.pi32(d.player.level);w.pi32(d.player.hp);w.pi32(d.player.maxHp);
    w.pi32(d.player.weaponMin);w.pi32(d.player.weaponMax);w.pi32(d.player.skill);w.pi32(d.player.armor);
    w.pi32(d.player.lockpick);w.pi32(d.player.xp);w.pi32(d.player.gold);w.pi32(d.player.kills);
    w.pf32(d.player.attackCooldown);w.pu8(d.player.dead?1:0);
    w.pf32(d.player.pos.x);w.pf32(d.player.pos.y);w.pf32(d.player.pos.z);
    w.pf32(d.player.yaw);w.pf32(d.player.pitch);
    w.pi32(d.region);w.pi32(d.exteriorId);
    w.pstr(d.dungeonName);
    w.pu32((u32)d.enemies.size());
    for(auto& e:d.enemies){
        w.pi32(e.mobileId);w.pf32(e.pos.x);w.pf32(e.pos.y);w.pf32(e.pos.z);w.pf32(e.yaw);
        w.pu8((u8)(e.female?1:0)|(e.passive?2:0)|(e.water?4:0)|(e.flying?8:0));
        w.pi32(e.state);w.pi32(e.level);w.pi32(e.maxHp);w.pi32(e.hp);w.pi32(e.armor);
        w.pf32(e.speed);w.pi32(e.animState);w.pi32(e.facing);w.pi32(e.dir);w.pi32(e.frame);
        w.pf32(e.bodyW);w.pf32(e.bodyH);
        w.pu8((u8)(e.aggro?1:0)|(e.mirrored?2:0)|(e.grounded?4:0));
        w.pstr(e.key);
    }
    w.pu32((u32)d.spawnedKeys.size());
    for(auto& s:d.spawnedKeys)w.pstr(s);
    w.pu32((u32)d.deadKeys.size());
    for(auto& s:d.deadKeys)w.pstr(s);
    w.pu32((u32)d.doors.size());
    for(auto& dr:d.doors){w.pstr(dr.key);w.pf32(dr.amount);w.pf32(dr.target);}
    FILE* f=fopen(path.c_str(),"wb");
    if(!f)return false;
    size_t n=fwrite(w.b.data(),1,w.b.size(),f);
    fclose(f);
    return n==w.b.size();
}
bool readSave(const std::string& path,SaveData& d){
    FILE* f=fopen(path.c_str(),"rb");
    if(!f)return false;
    fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
    if(n<8){fclose(f);return false;}
    std::vector<u8> b((size_t)n);
    size_t got=fread(b.data(),1,b.size(),f);fclose(f);
    if(got!=b.size())return false;
    R r(b);
    char magic[4];r.take(magic,4);
    if(memcmp(magic,"DFPS",4)!=0)return false;
    d=SaveData{};
    d.version=r.u32v();
    if(d.version<1||d.version>3)return false;
    d.player.level=r.i32v();d.player.hp=r.i32v();d.player.maxHp=r.i32v();
    d.player.weaponMin=r.i32v();d.player.weaponMax=r.i32v();d.player.skill=r.i32v();d.player.armor=r.i32v();
    if(d.version>=2){d.player.lockpick=r.i32v();d.player.xp=r.i32v();d.player.gold=r.i32v();}
    if(d.version>=3){d.player.kills=r.i32v();}
    d.player.attackCooldown=r.f32v();d.player.dead=r.u8v()!=0;
    d.player.pos.x=r.f32v();d.player.pos.y=r.f32v();d.player.pos.z=r.f32v();
    d.player.yaw=r.f32v();d.player.pitch=r.f32v();
    d.region=r.i32v();d.exteriorId=r.i32v();
    d.dungeonName=r.str();
    u32 ne=r.u32v();
    if(!r.ok||ne>100000)return false;
    for(u32 i=0;i<ne;i++){
        SavedEnemy e;
        e.mobileId=r.i32v();e.pos.x=r.f32v();e.pos.y=r.f32v();e.pos.z=r.f32v();e.yaw=r.f32v();
        u8 fl=r.u8v();e.female=(fl&1)!=0;e.passive=(fl&2)!=0;e.water=(fl&4)!=0;e.flying=(fl&8)!=0;
        e.state=r.i32v();e.level=r.i32v();e.maxHp=r.i32v();e.hp=r.i32v();e.armor=r.i32v();
        e.speed=r.f32v();e.animState=r.i32v();e.facing=r.i32v();e.dir=r.i32v();e.frame=r.i32v();
        e.bodyW=r.f32v();e.bodyH=r.f32v();
        u8 fl2=r.u8v();e.aggro=(fl2&1)!=0;e.mirrored=(fl2&2)!=0;e.grounded=(fl2&4)!=0;
        e.key=r.str();
        if(!r.ok)return false;
        d.enemies.push_back(e);
    }
    u32 ns=r.u32v();if(!r.ok||ns>100000)return false;
    for(u32 i=0;i<ns;i++)d.spawnedKeys.push_back(r.str());
    u32 nd=r.u32v();if(!r.ok||nd>100000)return false;
    for(u32 i=0;i<nd;i++)d.deadKeys.push_back(r.str());
    u32 ndo=r.u32v();if(!r.ok||ndo>100000)return false;
    for(u32 i=0;i<ndo;i++){SavedDoor dr;dr.key=r.str();dr.amount=r.f32v();dr.target=r.f32v();if(!r.ok)return false;d.doors.push_back(dr);}
    return r.ok;
}
} // namespace dfpsp
