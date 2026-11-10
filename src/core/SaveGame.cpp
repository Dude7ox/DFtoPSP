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
    w.pu8((u8)(d.character.valid?1:0));
    if(d.character.valid){
        const SavedCharacter& ch=d.character;
        w.pi32(ch.race);w.pi32(ch.gender);w.pi32(ch.classIndex);w.pi32(ch.biographyIndex);w.pi32(ch.faceIndex);
        w.pstr(ch.name);
        for(int i=0;i<8;i++)w.pi32(ch.attributes[i]);
        for(int i=0;i<8;i++)w.pi32(ch.rolledAttributes[i]);
        w.pi32(ch.bonusPool);
        for(int i=0;i<35;i++)w.pi32(ch.skillValues[i]);
        for(int i=0;i<3;i++)w.pi32(ch.skillPool[i]);
        for(int i=0;i<12;i++)w.pi32(ch.answers[i]);
        w.pstr(ch.career.name);w.pi32(ch.career.hpPerLevel);w.pf32(ch.career.advancement);
        for(int i=0;i<8;i++)w.pi32(ch.career.attributes[i]);
        for(int i=0;i<3;i++)w.pi32(ch.career.primary[i]);
        for(int i=0;i<3;i++)w.pi32(ch.career.major[i]);
        for(int i=0;i<6;i++)w.pi32(ch.career.minor[i]);
        w.pu32(ch.career.resistance);w.pu32(ch.career.immunity);w.pu32(ch.career.lowTolerance);w.pu32(ch.career.criticalWeakness);
        w.pu32(ch.career.specialAbilities);
        w.pi32(ch.career.rapidHealing);w.pi32(ch.career.regeneration);w.pi32(ch.career.spellAbsorption);w.pi32(ch.career.attackModifier);
        w.pu32(ch.career.forbiddenMaterials);w.pu32(ch.career.weaponArmorShields);
        w.pu8((u8)(ch.career.custom?1:0));
        w.pi32(ch.reflexes);w.pi32(ch.mageryOption);
        w.pu32((u32)ch.pickPrim.size());
        for(size_t i=0;i<ch.pickPrim.size();i++){
            w.pi32(ch.pickPrim[i]);w.pi32(ch.pickSec[i]);w.pi32(ch.pickCost[i]);w.pu8((u8)ch.pickDis[i]);
        }
    }
    w.pu32((u32)d.items.size());
    for(auto& it:d.items){
        w.pi32(it.templateIndex);w.pi32(it.stack);w.pi32(it.value);w.pi32(it.weightQ);w.pi32(it.material);
        w.pi32(it.maxCondition);w.pi32(it.condition);w.pi32(it.variant);w.pi32(it.dye);w.pi32(it.slot);
        w.pi32(it.playerArchive);w.pi32(it.playerRecord);
    }
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
    if(d.version<1||d.version>5)return false;
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
    if(d.version>=4){
        SavedCharacter& ch=d.character;
        ch.valid=r.u8v()!=0;
        if(ch.valid){
            ch.race=r.i32v();ch.gender=r.i32v();ch.classIndex=r.i32v();ch.biographyIndex=r.i32v();ch.faceIndex=r.i32v();
            ch.name=r.str();
            for(int i=0;i<8;i++)ch.attributes[i]=r.i32v();
            for(int i=0;i<8;i++)ch.rolledAttributes[i]=r.i32v();
            ch.bonusPool=r.i32v();
            for(int i=0;i<35;i++)ch.skillValues[i]=r.i32v();
            for(int i=0;i<3;i++)ch.skillPool[i]=r.i32v();
            for(int i=0;i<12;i++)ch.answers[i]=r.i32v();
            ch.career.name=r.str();ch.career.hpPerLevel=r.i32v();ch.career.advancement=r.f32v();
            for(int i=0;i<8;i++)ch.career.attributes[i]=r.i32v();
            for(int i=0;i<3;i++)ch.career.primary[i]=r.i32v();
            for(int i=0;i<3;i++)ch.career.major[i]=r.i32v();
            for(int i=0;i<6;i++)ch.career.minor[i]=r.i32v();
            ch.career.resistance=r.u32v();ch.career.immunity=r.u32v();ch.career.lowTolerance=r.u32v();ch.career.criticalWeakness=r.u32v();
            ch.career.specialAbilities=r.u32v();
            ch.career.rapidHealing=r.i32v();ch.career.regeneration=r.i32v();ch.career.spellAbsorption=r.i32v();ch.career.attackModifier=r.i32v();
            ch.career.forbiddenMaterials=r.u32v();ch.career.weaponArmorShields=r.u32v();
            ch.career.custom=r.u8v()!=0;
            if(d.version>=5){
                ch.reflexes=r.i32v();ch.mageryOption=r.i32v();
                u32 np=r.u32v();if(!r.ok||np>64)return false;
                for(u32 i=0;i<np;i++){
                    ch.pickPrim.push_back(r.i32v());ch.pickSec.push_back(r.i32v());
                    ch.pickCost.push_back(r.i32v());ch.pickDis.push_back(r.u8v());
                }
            }
        }
        u32 ni=r.u32v();
        if(!r.ok||ni>100000)return false;
        for(u32 i=0;i<ni;i++){
            SavedItem it;
            it.templateIndex=r.i32v();it.stack=r.i32v();it.value=r.i32v();it.weightQ=r.i32v();it.material=r.i32v();
            it.maxCondition=r.i32v();it.condition=r.i32v();it.variant=r.i32v();it.dye=r.i32v();it.slot=r.i32v();
            it.playerArchive=r.i32v();it.playerRecord=r.i32v();
            if(!r.ok)return false;
            d.items.push_back(it);
        }
    }
    return r.ok;
}
SavedItem fromItem(const Item& it){
    SavedItem s;
    s.templateIndex=it.templateIndex;s.stack=it.stack;s.value=it.value;s.weightQ=it.weightQ;
    s.material=it.material;s.maxCondition=it.maxCondition;s.condition=it.condition;
    s.variant=it.variant;s.dye=it.dye;s.slot=it.slot;
    s.playerArchive=it.playerArchive;s.playerRecord=it.playerRecord;
    return s;
}
Item toItem(const SavedItem& s){
    Item it;
    it.templateIndex=s.templateIndex;it.stack=s.stack;it.value=s.value;it.weightQ=s.weightQ;
    it.material=s.material;it.maxCondition=s.maxCondition;it.condition=s.condition;
    it.variant=s.variant;it.dye=s.dye;it.slot=s.slot;
    it.playerArchive=s.playerArchive;it.playerRecord=s.playerRecord;
    return it;
}
SavedCharacter fromCharacter(const CharacterDoc& d){
    SavedCharacter c;
    c.valid=true;
    c.race=d.race;c.gender=d.gender;c.classIndex=d.classIndex;c.biographyIndex=d.biographyIndex;
    c.faceIndex=d.faceIndex;c.name=d.name;
    c.reflexes=d.reflexes;c.mageryOption=d.mageryOption;
    for(size_t i=0;i<d.picks.size();i++){
        c.pickPrim.push_back(d.picks[i].prim);c.pickSec.push_back(d.picks[i].sec);
        c.pickCost.push_back(d.picks[i].cost);c.pickDis.push_back(d.picks[i].dis?1:0);
    }
    for(int i=0;i<8;i++){c.attributes[i]=d.attributes[i];c.rolledAttributes[i]=d.rolledAttributes[i];}
    c.bonusPool=d.bonusPool;
    for(int i=0;i<35;i++)c.skillValues[i]=d.skillValues[i];
    for(int i=0;i<3;i++)c.skillPool[i]=d.skillPool[i];
    for(int i=0;i<12;i++)c.answers[i]=d.answers[i];
    c.career=d.career;
    return c;
}
void toCharacter(const SavedCharacter& s,CharacterDoc& d){
    d.race=s.race;d.gender=s.gender;d.classIndex=s.classIndex;d.biographyIndex=s.biographyIndex;
    d.faceIndex=s.faceIndex;d.name=s.name;
    d.reflexes=s.reflexes;d.mageryOption=s.mageryOption;
    d.picks.clear();
    for(size_t i=0;i<s.pickPrim.size();i++){
        AdvPick p;p.prim=s.pickPrim[i];p.sec=s.pickSec[i];p.cost=s.pickCost[i];p.dis=s.pickDis[i]!=0;
        d.picks.push_back(p);
    }
    for(int i=0;i<8;i++){d.attributes[i]=s.attributes[i];d.rolledAttributes[i]=s.rolledAttributes[i];}
    d.bonusPool=s.bonusPool;
    for(int i=0;i<35;i++)d.skillValues[i]=s.skillValues[i];
    for(int i=0;i<3;i++)d.skillPool[i]=s.skillPool[i];
    for(int i=0;i<12;i++)d.answers[i]=s.answers[i];
    d.career=s.career;
    d.rolled=true;
    finalizeCharacter(d);
}
} // namespace dfpsp
