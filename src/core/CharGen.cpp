#include "CharGen.h"
#include "ImgFile.h"
#include "Stream.h"
#include <algorithm>
#include <cstring>
#include <cmath>
namespace dfpsp {
const char* const kStatNames[8]={"Strength","Intelligence","Willpower","Agility",
    "Endurance","Personality","Speed","Luck"};
const char* const kStatShort[8]={"STR","INT","WIL","AGI","END","PER","SPD","LCK"};
const char* const kSkillNames[35]={"Medical","Etiquette","Streetwise","Jumping","Orcish",
    "Harpy","Giantish","Dragonish","Nymph","Daedric","Spriggan","Centaurian","Impish",
    "Lockpicking","Mercantile","Pickpocket","Stealth","Swimming","Climbing","Backstabbing",
    "Dodging","Running","Destruction","Restoration","Illusion","Alteration","Thaumaturgy",
    "Mysticism","Short Blade","Long Blade","Hand To Hand","Axe","Blunt Weapon","Archery",
    "Critical Strike"};
const char* const kGenderNames[2]={"Male","Female"};
static const RaceInfo kRaces[9]={
 {RaceNone,"",0,0,"","","","","","",0,0,0,0,0,0},
 {RaceBreton,"Breton",2003,209,"SCBG00I0.IMG","BODY00I0.IMG","BODY00I1.IMG","BODY10I0.IMG","BODY10I1.IMG",
  "FACE00I0.CIF","FACE10I0.CIF",EffMagic,0,0,0,0},
 {RaceRedguard,"Redguard",2002,210,"SCBG01I0.IMG","BODY01I0.IMG","BODY01I1.IMG","BODY11I0.IMG","BODY11I1.IMG",
  "FACE01I0.CIF","FACE11I0.CIF",0,0,0,0,0},
 {RaceNord,"Nord",2000,211,"SCBG02I0.IMG","BODY02I0.IMG","BODY02I1.IMG","BODY12I0.IMG","BODY12I1.IMG",
  "FACE02I0.CIF","FACE12I0.CIF",EffFrost,0,0,0,0},
 {RaceDarkElf,"Dark Elf",2007,212,"SCBG03I0.IMG","BODY03I0.IMG","BODY03I1.IMG","BODY13I0.IMG","BODY13I1.IMG",
  "FACE03I0.CIF","FACE13I0.CIF",0,0,0,0,0},
 {RaceHighElf,"High Elf",2006,213,"SCBG04I0.IMG","BODY04I0.IMG","BODY04I1.IMG","BODY14I0.IMG","BODY14I1.IMG",
  "FACE04I0.CIF","FACE14I0.CIF",0,EffParalysis,0,0,0},
 {RaceWoodElf,"Wood Elf",2005,214,"SCBG05I0.IMG","BODY05I0.IMG","BODY05I1.IMG","BODY15I0.IMG","BODY15I1.IMG",
  "FACE05I0.CIF","FACE15I0.CIF",0,0,0,0,0},
 {RaceKhajiit,"Khajiit",2001,215,"SCBG06I0.IMG","BODY06I0.IMG","BODY06I1.IMG","BODY16I0.IMG","BODY16I1.IMG",
  "FACE06I0.CIF","FACE16I0.CIF",0,0,0,0,0},
 {RaceArgonian,"Argonian",2004,216,"SCBG07I0.IMG","BODY07I0.IMG","BODY07I1.IMG","BODY17I0.IMG","BODY17I1.IMG",
  "FACE07I0.CIF","FACE17I0.CIF",0,0,0,0,0},
};
const int kRaceCount=8;
const RaceInfo* raceInfo(int id){
    if(id<1||id>8)return &kRaces[0];
    return &kRaces[id];
}
int Career::skillGroup(int s)const{
    for(int i=0;i<3;i++)if(primary[i]==s)return 0;
    for(int i=0;i<3;i++)if(major[i]==s)return 1;
    for(int i=0;i<6;i++)if(minor[i]==s)return 2;
    return -1;
}
bool parseCareer(const std::vector<u8>& d,Career& out){
    if(d.size()<74)return false;
    Reader rd(d);
    out.resistance=rd.u8_();out.immunity=rd.u8_();out.lowTolerance=rd.u8_();out.criticalWeakness=rd.u8_();
    rd.seek(4);rd.u16_();                       // ability flags and spell points multiplier
    out.rapidHealing=rd.u8_();out.regeneration=rd.u8_();rd.u8_();
    out.spellAbsorption=rd.u8_();out.attackModifier=(int8_t)rd.u8_();
    out.forbiddenMaterials=rd.u16_();
    u8 a=rd.u8_(),b=rd.u8_(),c=rd.u8_();
    out.weaponArmorShields=((unsigned)a<<16)|((unsigned)c<<8)|b;
    for(int i=0;i<3;i++)out.primary[i]=rd.u8_();
    for(int i=0;i<3;i++)out.major[i]=rd.u8_();
    for(int i=0;i<6;i++)out.minor[i]=rd.u8_();
    out.name=rd.str(16);
    rd.skip(8);                                 // unknown padding up to offset 52
    out.hpPerLevel=rd.u16_();
    u32 mult=rd.u32_();
    out.advancement=(float)((mult>>16)+(mult&0xffff)/65536.0);
    for(int i=0;i<8;i++)out.attributes[i]=rd.u16_();
    for(int i=0;i<3;i++)if(out.primary[i]>=35)out.primary[i]=-1;
    for(int i=0;i<3;i++)if(out.major[i]>=35)out.major[i]=-1;
    for(int i=0;i<6;i++)if(out.minor[i]>=35)out.minor[i]=-1;
    return true;
}
bool loadClasses(const Vfs& vfs,std::vector<Career>& out){
    out.clear();
    for(int i=0;i<18;i++){                      // CLASS18.CFG duplicates the Knight
        char name[16];
        std::snprintf(name,sizeof(name),"CLASS%02d.CFG",i);
        std::vector<u8> data;
        if(!vfs.readFile(name,data))return false;
        Career c;
        if(!parseCareer(data,c))return false;
        out.push_back(c);
    }
    return !out.empty();
}
void BiogEffects::clear(){
    std::memset(skillMod,0,sizeof(skillMod));
    goldMod=0;std::memset(socialRep,0,sizeof(socialRep));
    resistPoison=fatigue=reaction=resistDisease=resistMagic=avoidHit=0;
    items.clear();
}
void BiogEffects::accumulate(const BiogEffects& o){
    for(int i=0;i<35;i++)skillMod[i]+=o.skillMod[i];
    goldMod+=o.goldMod;
    for(int i=0;i<5;i++)socialRep[i]+=o.socialRep[i];
    resistPoison+=o.resistPoison;fatigue+=o.fatigue;reaction+=o.reaction;
    resistDisease+=o.resistDisease;resistMagic+=o.resistMagic;avoidHit+=o.avoidHit;
    items.insert(items.end(),o.items.begin(),o.items.end());
}
// "<skill> <delta>" | "GP <n>" | "IT <g> <i> <m>" | "r<n> <d>" | "rf<n> <d>" |
// "RP <n>" | "FT <n>" | "RR <n>" | "RD <n>" | "MR <n>" | "TH <n>"
bool parseBiogEffect(const std::string& line,BiogEffects& out){
    std::vector<std::string> tok;
    std::string cur;
    for(char c:line){
        if(c==' '||c=='\t'){if(!cur.empty()){tok.push_back(cur);cur.clear();}}
        else cur+=c;
    }
    if(!cur.empty())tok.push_back(cur);
    if(tok.empty())return true;
    const std::string& k=tok[0];
    if(k=="GP"&&tok.size()>1){out.goldMod+=std::atoi(tok[1].c_str());return true;}
    if(k=="IT"&&tok.size()>3){
        BiogEffects::ItemGrant g;
        g.group=std::atoi(tok[1].c_str());g.index=std::atoi(tok[2].c_str());g.material=std::atoi(tok[3].c_str());
        out.items.push_back(g);return true;
    }
    if(k=="RP"&&tok.size()>1){out.resistPoison+=std::atoi(tok[1].c_str());return true;}
    if(k=="FT"&&tok.size()>1){out.fatigue+=std::atoi(tok[1].c_str());return true;}
    if(k=="RR"&&tok.size()>1){out.reaction+=std::atoi(tok[1].c_str());return true;}
    if(k=="RD"&&tok.size()>1){out.resistDisease+=std::atoi(tok[1].c_str());return true;}
    if(k=="MR"&&tok.size()>1){out.resistMagic+=std::atoi(tok[1].c_str());return true;}
    if(k=="TH"&&tok.size()>1){out.avoidHit+=std::atoi(tok[1].c_str());return true;}
    if(k.size()>1&&k[0]=='r'&&k[1]=='f'&&tok.size()>1){
        out.socialRep[4]+=std::atoi(tok[1].c_str());return true;   // faction reputation
    }
    if(k.size()==2&&k[0]=='r'&&k[1]>='0'&&k[1]<='4'&&tok.size()>1){
        out.socialRep[k[1]-'0']+=std::atoi(tok[1].c_str());return true;
    }
    if(tok.size()>1&&k[0]>='0'&&k[0]<='9'){
        int skill=std::atoi(k.c_str()),delta=std::atoi(tok[1].c_str());
        if(skill>=0&&skill<35){out.skillMod[skill]+=delta;return true;}
    }
    return false;                               // '#' '!' '?' text macros, AE/AF/AO...
}
static std::string trim(const std::string& s){
    size_t a=0,b=s.size();
    while(a<b&&(s[a]==' '||s[a]=='\t'))a++;
    while(b>a&&(s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'))b--;
    return s.substr(a,b-a);
}
bool BiogFile::load(const Vfs& vfs,int ci,int bioIndex){
    questions.clear();classIndex=ci;backstoryId=4116+ci;
    char name[24];
    std::snprintf(name,sizeof(name),"BIOG%02dT%d.TXT",ci,bioIndex);
    std::vector<u8> data;
    if(!vfs.readFile(name,data))return false;
    std::string text((const char*)data.data(),data.size());
    std::vector<std::string> lines;
    {
        std::string cur;
        for(char c:text){
            if(c=='\n'){lines.push_back(cur);cur.clear();}
            else if(c!='\r')cur+=c;
        }
        if(!cur.empty())lines.push_back(cur);
    }
    size_t i=0;
    if(i<lines.size()&&!lines[i].empty()&&lines[i][0]=='#'){
        backstoryId=std::atoi(lines[i].c_str()+1);
        i++;
    }
    auto isQuestion=[](const std::string& l){
        size_t dot=l.find('.');
        if(dot==std::string::npos||dot==0||dot>2)return false;
        for(size_t k=0;k<dot;k++)if(l[k]<'0'||l[k]>'9')return false;
        return l.size()>dot+1;
    };
    auto isAnswer=[](const std::string& l){
        return l.size()>2&&l[1]=='.'&&((l[0]>='a'&&l[0]<='z')||(l[0]>='A'&&l[0]<='Z'));
    };
    while(i<lines.size()&&questions.size()<12){
        std::string l=trim(lines[i]);
        if(!isQuestion(l)){i++;continue;}
        BiogQuestion q;
        q.text.push_back(trim(l.substr(l.find('.')+1)));
        i++;
        while(i<lines.size()){
            std::string t=trim(lines[i]);
            if(t.empty()||isAnswer(t)||isQuestion(t))break;
            q.text.push_back(t);
            i++;
            if(q.text.size()>=2)break;          // question panels are two lines tall
        }
        while(i<lines.size()&&isAnswer(trim(lines[i]))){
            BiogAnswer ans;
            ans.text=trim(lines[i].substr(2));
            i++;
            while(i<lines.size()){
                std::string el=trim(lines[i]);
                if(el.empty()||isAnswer(el)||isQuestion(el))break;
                ans.effects.push_back(el);
                i++;
            }
            q.answers.push_back(ans);
        }
        questions.push_back(q);
    }
    return questions.size()==12;
}
bool NameGen::open(const Vfs& vfs){
    loaded_=false;
    std::vector<u8> data;
    if(!vfs.readFile("NAMEGEN.DAT",data)||data.size()<528)return false;
    u32 offset[11][6],count[11][6];
    for(int g=0;g<11;g++)for(int b=0;b<6;b++)
        offset[g][b]=(u32)data[g*48+b*4]|((u32)data[g*48+b*4+1]<<8)|((u32)data[g*48+b*4+2]<<16)|((u32)data[g*48+b*4+3]<<24);
    for(int g=0;g<11;g++)for(int b=0;b<6;b++)
        count[g][b]=(u32)data[g*48+24+b*4]|((u32)data[g*48+24+b*4+1]<<8)|((u32)data[g*48+24+b*4+2]<<16)|((u32)data[g*48+24+b*4+3]<<24);
    for(int g=0;g<11;g++)for(int b=0;b<6;b++){
        Bank& bank=groups_[g].banks[b];
        bank.parts.clear();
        for(u32 e=0;e<count[g][b];e++){
            size_t at=offset[g][b]+e*10;
            if(at+10>data.size())return false;
            char buf[11];std::memset(buf,0,sizeof(buf));
            std::memcpy(buf,data.data()+at,10);
            bank.parts.push_back(trim(buf));
        }
    }
    loaded_=true;
    return true;
}
std::string NameGen::firstName(int race,int gender,DfRandom& rng)const{
    int g=race-1;
    if(g<0||g>7)g=0;
    const Bank& a=groups_[g].banks[gender?2:0];
    const Bank& b=groups_[g].banks[gender?3:1];
    std::string n;
    if(!a.parts.empty())n=a.parts[(size_t)rng.range((int)a.parts.size())];
    if(!b.parts.empty())n+=b.parts[(size_t)rng.range((int)b.parts.size())];
    return n;
}
std::string NameGen::surname(int race,DfRandom& rng)const{
    int g=race-1;
    if(g<0||g>7)g=0;
    if(race==RaceRedguard)return std::string();   // Redguards carry a single name
    const Bank& a=groups_[g].banks[4];
    const Bank& b=groups_[g].banks[5];
    std::string n;
    if(!a.parts.empty())n=a.parts[(size_t)rng.range((int)a.parts.size())];
    if(!b.parts.empty())n+=b.parts[(size_t)rng.range((int)b.parts.size())];
    return n;
}
std::string NameGen::fullName(int race,int gender,DfRandom& rng)const{
    std::string n=firstName(race,gender,rng);
    std::string s=surname(race,rng);
    if(!s.empty())n+=" "+s;
    return n;
}
CharRoll rollStats(const Career& c,DfRandom& rng){
    CharRoll r;
    for(int i=0;i<8;i++)r.attributes[i]=c.attributes[i]+rng.range(11);
    r.bonusPool=6+rng.range(9);                   // 6..14
    return r;
}
SkillRoll rollSkills(const Career& c,DfRandom& rng){
    SkillRoll s;
    std::memset(s.values,0,sizeof(s.values));
    for(int i=0;i<3;i++)if(c.primary[i]>=0)s.values[c.primary[i]]=28+rng.range(4);
    for(int i=0;i<3;i++)if(c.major[i]>=0)s.values[c.major[i]]=18+rng.range(4);
    for(int i=0;i<6;i++)if(c.minor[i]>=0)s.values[c.minor[i]]=13+rng.range(4);
    s.pool[0]=6;s.pool[1]=6;s.pool[2]=6;
    return s;
}
CharacterDoc::CharacterDoc(){
    for(int i=0;i<12;i++)answers[i]=-1;
}
int CharacterDoc::spellPoints()const{
    if(mageryOption>=0&&mageryOption<5)
        return (int)(attributes[StatIntelligence]*kMageryMult[mageryOption]+0.5f);
    return (int)(attributes[StatIntelligence]*career.advancement+0.5f);
}
int CharacterDoc::maxEncumbrance()const{return attributes[StatStrength]*3/2;}
int CharacterDoc::damageMod()const{return (attributes[StatStrength]-50)/5;}
int CharacterDoc::toHitMod()const{return attributes[StatAgility]/10-5;}
int CharacterDoc::magicResist()const{return attributes[StatWillpower]/10;}
int CharacterDoc::healingRate()const{return (attributes[StatEndurance]-10)/5+biog.fatigue/2;}
int CharacterDoc::hpModifier()const{return attributes[StatEndurance]/10-5;}
int CharacterDoc::fatigueModifier()const{return attributes[StatEndurance]/10-5;}
int rollMaxHealth(const Career& c,int endurance,int level){
    int hp=25+c.hpPerLevel;
    for(int l=2;l<=level;l++){
        int roll=c.hpPerLevel+endurance/10-5;     // CalculateHitPointsPerLevelUp
        hp+=roll<1?1:roll;
    }
    return hp;
}
void rollCharacter(CharacterDoc& d,DfRandom& rng){
    CharRoll r=rollStats(d.career,rng);
    for(int i=0;i<8;i++){d.attributes[i]=r.attributes[i];d.rolledAttributes[i]=r.attributes[i];}
    d.bonusPool=r.bonusPool;
    SkillRoll s=rollSkills(d.career,rng);
    std::memcpy(d.skillValues,s.values,sizeof(s.values));
    for(int i=0;i<35;i++)d.skillValues[i]+=d.biog.skillMod[i];   // biography is permanent
    for(int i=0;i<35;i++)if(d.skillValues[i]<0)d.skillValues[i]=0;
    std::memcpy(d.skillPool,s.pool,sizeof(s.pool));
    d.rolled=true;
    finalizeCharacter(d);
}
void finalizeCharacter(CharacterDoc& d){
    d.maxHp=rollMaxHealth(d.career,d.attributes[StatEndurance],d.level);
    d.hp=d.maxHp;
    d.gold=d.biog.goldMod<0?0:d.biog.goldMod;
}
// --- class questions (FALL.EXE v1.07.213 answer table via DFU, MIT) ---
const unsigned char kAnswerTable[120]={
 0,2,1, 0,2,1, 0,1,2, 2,0,1, 0,1,2, 1,0,2, 0,1,2, 2,0,1, 0,2,1, 0,1,2,
 0,1,2, 0,1,2, 0,2,1, 0,1,2, 1,0,2, 1,2,0, 2,0,1, 1,0,2, 0,1,2, 2,0,1,
 1,0,2, 0,1,2, 0,2,1, 2,1,0, 1,0,2, 0,2,1, 2,1,0, 2,0,1, 2,1,0, 2,1,0,
 2,0,1, 1,0,2, 0,2,1, 2,1,0, 1,2,0, 2,0,1, 2,0,1, 1,2,0, 0,1,2, 1,2,0};
bool classQuestion(const TextRsc& t,int index,ClassQuestion& out){
    out=ClassQuestion();
    std::vector<std::string> ls=t.subLines(9000,index+1);
    if(ls.empty())return false;
    int cur=-1;
    for(size_t i=0;i<ls.size();i++){
        const std::string& l=ls[i];
        std::string s=l;
        while(!s.empty()&&s[0]==' ')s.erase(s.begin());
        if(s.size()>2&&(s[0]=='a'||s[0]=='b'||s[0]=='c')&&s[1]==')'){cur=s[0]-'a';s.erase(0,2);
            while(!s.empty()&&s[0]==' ')s.erase(s.begin());}
        if(cur<0)out.text.push_back(s);
        else out.answers[cur].push_back(s);
    }
    return !out.text.empty()&&out.answers[0].size()>0;
}
void classQuestionsRoll(CharGenFlow& f,DfRandom& rng){
    bool picked[40]={false};
    f.cqAnswered=0;f.cqWeights[0]=f.cqWeights[1]=f.cqWeights[2]=0;
    for(int i=0;i<10;i++){
        int idx=rng.range(40);
        while(picked[idx])idx=(idx+1)%40;
        picked[idx]=true;f.cq[i]=idx;
    }
}
int classQuestionsRecommend(const Vfs& vfs,const int weights[3]){
    std::vector<u8> d;
    if(!vfs.readFile("CLASSES.DAT",d)||d.size()<66)return -1;
    for(int i=0;i<66;i++){
        size_t o=18+(size_t)i*3;
        if(o+2>=d.size())break;
        if(d[o]==weights[0]&&d[o+1]==weights[1]&&d[o+2]==weights[2]){
            int header=i/4;
            if(header>=(int)d.size())return -1;
            u8 r=d[header];
            if(header>3)r&=0x0f;
            return r;
        }
    }
    return -1;
}
// --- special advantages / disadvantages (costs: DFU difficultyDict, MIT) ---
struct AdvDef { const char* label; const char* subs[10]; int nSub; int cost[10]; int kind; };
static const AdvDef kAdv[11]={
 {"Acute Hearing",{0},0,{1},0},
 {"Adrenaline Rush",{0},0,{4},1},
 {"Athleticism",{0},0,{4},2},
 {"Bonus To Hit",{"Animals","Daedra","Humanoids","Undead"},4,{6,3,6,6},3},
 {"Expertise In",{0},0,{2},4},          // secondary filled from the skill list
 {"Immunity To",{0},0,{10},5},
 {"Increased Magery",{"Spell points = INT x1.5","Spell points = INT x1.75","Spell points = INT x2","Spell points = INT x3","Spell points = INT"},5,{4,6,8,10,2},6},
 {"Rapid Healing",{"General","In darkness","In light"},3,{4,3,2},7},
 {"Regenerate Health",{"General","In darkness","In light","While immersed"},4,{14,10,6,2},8},
 {"Resistance To",{0},0,{5},9},
 {"Spell Absorption",{"General","In darkness","In light"},3,{14,12,8},10}};
static const AdvDef kDis[11]={
 {"Critical Weakness",{0},0,{-14},0},
 {"Damage",{"From holy places","From sunlight"},2,{-6,-10},1},
 {"Darkness Powered Magery",{"Lower magic ability in daylight","Unable to use magic in daylight"},2,{-7,-10},2},
 {"Forbidden Armor Type",{"Chain","Leather","Plate"},3,{-2,-1,-5},3},
 {"Forbidden Material",{"Adamantium","Daedric","Dwarven","Ebony","Elven","Iron","Mithril","Orcish","Silver","Steel"},10,{-5,-2,-7,-5,-9,-1,-6,-3,-6,-10},4},
 {"Forbidden Shield Types",{0},0,{-1},5},
 {"Forbidden Weaponry",{0},0,{-2},6},
 {"Inability To Regenerate",{0},0,{-14},7},
 {"Light Powered Magery",{"Lower magic ability in darkness","Unable to use magic in darkness"},2,{-10,-14},8},
 {"Low Tolerance",{0},0,{-5},9},
 {"Phobia",{0},0,{-4},10}};
static const char* kEffectSubs[7]={"Disease","Fire","Frost","Magic","Paralysis","Poison","Shock"};
static const unsigned kEffectBits[7]={64,8,16,2,1,4,32};
static const char* kEnemySubs[4]={"Animals","Daedra","Humanoids","Undead"};
static const char* kMaterialSubs[10]={"Adamantium","Daedric","Dwarven","Ebony","Elven","Iron","Mithril","Orcish","Silver","Steel"};
static const int kMaterialIdx[10]={6,9,4,7,3,0,5,8,2,1};
const float kMageryMult[5]={1.0f,1.5f,1.75f,2.0f,3.0f};
int advCount(bool dis){return 11;}
const char* advLabel(bool dis,int prim){return (dis?kDis:kAdv)[prim].label;}
int advSubCount(bool dis,int prim){
    const AdvDef& d=(dis?kDis:kAdv)[prim];
    if(d.nSub>0)return d.nSub;
    if(!dis){switch(d.kind){case 3:return 4;case 4:return 35;case 5:case 9:return 7;default:return 0;}}
    switch(d.kind){case 0:case 9:return 7;case 10:return 4;default:return 0;}
}
const char* advSubLabel(bool dis,int prim,int sec){
    const AdvDef& d=(dis?kDis:kAdv)[prim];
    if(d.nSub>0)return d.subs[sec<0?0:(sec>=d.nSub?d.nSub-1:sec)];
    if(!dis){
        switch(d.kind){
        case 3:return kEnemySubs[sec<0?0:(sec>3?3:sec)];
        case 4:return kSkillNames[sec<0?0:(sec>34?34:sec)];
        case 5:case 9:return kEffectSubs[sec<0?0:(sec>6?6:sec)];
        default:return "";}
    }
    switch(d.kind){
    case 0:case 9:return kEffectSubs[sec<0?0:(sec>6?6:sec)];
    case 10:return kEnemySubs[sec<0?0:(sec>3?3:sec)];
    default:return "";}
}
int advCost(bool dis,int prim,int sec){
    const AdvDef& d=(dis?kDis:kAdv)[prim];
    if(d.nSub>0)return d.cost[sec<0?0:(sec>=d.nSub?d.nSub-1:sec)];
    return d.cost[0];
}
int difficultyPoints(const CharacterDoc& d){
    int sum=0;
    for(size_t i=0;i<d.picks.size();i++)sum+=d.picks[i].cost;
    return sum;
}
void applyAdvPick(CharacterDoc& d,const AdvPick& p){
    Career& c=d.career;
    const AdvDef& def=(p.dis?kDis:kAdv)[p.prim];
    switch(def.kind){
    case 5: if(!p.dis)c.immunity|=kEffectBits[p.sec]; else c.weaponArmorShields|=1u<<(8+p.sec); break;
    case 9: if(!p.dis)c.resistance|=kEffectBits[p.sec]; else c.lowTolerance|=kEffectBits[p.sec]; break;
    case 0: if(p.dis)c.criticalWeakness|=kEffectBits[p.sec]; break;
    case 4: if(p.dis)c.forbiddenMaterials|=1u<<kMaterialIdx[p.sec]; break;
    case 3: if(p.dis)c.weaponArmorShields|=1u<<p.sec; break;          // armor byte
    case 6: if(p.dis)c.weaponArmorShields|=1u<<(16+p.sec); break;     // weapon byte
    case 7: if(!p.dis)c.rapidHealing=p.sec+1; else c.regeneration=0; break;
    case 8: if(!p.dis)c.regeneration=p.sec+1; break;
    case 10: if(!p.dis)c.spellAbsorption=p.sec+1; break;
    default: break;
    }
    if(!p.dis&&def.kind==6)d.mageryOption=p.sec;
    if(!p.dis&&def.kind==4&&p.sec>=0&&p.sec<35)d.expertise[p.sec]=1;
}
void unapplyAdvPick(CharacterDoc& d,const AdvPick& p){
    Career& c=d.career;
    const AdvDef& def=(p.dis?kDis:kAdv)[p.prim];
    switch(def.kind){
    case 5: if(!p.dis)c.immunity&=~kEffectBits[p.sec]; else c.weaponArmorShields&=~(1u<<(8+p.sec)); break;
    case 9: if(!p.dis)c.resistance&=~kEffectBits[p.sec]; else c.lowTolerance&=~kEffectBits[p.sec]; break;
    case 0: if(p.dis)c.criticalWeakness&=~kEffectBits[p.sec]; break;
    case 4: if(p.dis)c.forbiddenMaterials&=~(1u<<kMaterialIdx[p.sec]); break;
    case 3: if(p.dis)c.weaponArmorShields&=~(1u<<p.sec); break;
    case 6: if(p.dis)c.weaponArmorShields&=~(1u<<(16+p.sec)); break;
    case 7: if(!p.dis)c.rapidHealing=0; break;
    case 8: if(!p.dis)c.regeneration=0; break;
    case 10: if(!p.dis)c.spellAbsorption=0; break;
    default: break;
    }
    if(!p.dis&&def.kind==6)d.mageryOption=-1;
    if(!p.dis&&def.kind==4&&p.sec>=0&&p.sec<35)d.expertise[p.sec]=0;
}
// --- flow ---
static const int kCustomHpMin=4,kCustomHpMax=30;
bool racePixel(const Vfs& vfs,int race,int& x,int& y){
    static ImgFile tam;
    if(!tam.count()&&!tam.open(vfs,"TAMRIEL2.IMG"))return false;
    if(!tam.decode(0))return false;
    const ImgRecord* r=tam.decoded(0);
    if(!r)return false;
    for(int py=0;py<r->h;py++)for(int px=0;px<r->w;px++){
        if(r->idx[(size_t)py*r->w+px]==race){
            // aim a little inside the province so neighbour pixels agree
            for(int d=1;d<6;d++){
                int cx=px+d,cy=py+d;
                if(cx<r->w&&cy<r->h&&r->idx[(size_t)cy*r->w+cx]==race){x=cx;y=cy;return true;}
            }
            x=px;y=py;return true;
        }
    }
    return false;
}
static int raceAtCursor(const Vfs& vfs,int x,int y){
    static ImgFile tam;
    if(!tam.count()&&!tam.open(vfs,"TAMRIEL2.IMG"))return -1;
    if(!tam.decode(0))return -1;
    const ImgRecord* r=tam.decoded(0);
    if(!r)return -1;
    if(x<0||y<0||x>=r->w||y>=r->h)return -1;
    return r->idx[(size_t)y*r->w+x];
}
struct CustomField { int kind; int idx; };       // 0 hp, 1 skill slot, 2 advantage, 3 done
int raceAt(const Vfs& vfs,int x,int y){return raceAtCursor(vfs,x,y);}
bool charGenAdvance(CharGenFlow& f,CharGenAction act,const std::vector<Career>& classes,
                    const BiogFile& biog,const NameGen& names,DfRandom& rng,const Vfs& vfs){
    int before=f.step;
    f.message.clear();
    switch(f.step){
    case StepRace:{
        // Classic province map: the cursor walks TAMRIEL2.IMG and the pixel
        // index under it is the race id.
        int stepPx=act==ActLeft||act==ActRight?4:3;
        if(act==ActUp)f.raceY-=stepPx;
        else if(act==ActDown)f.raceY+=stepPx;
        else if(act==ActLeft)f.raceX-=stepPx;
        else if(act==ActRight)f.raceX+=stepPx;
        if(f.raceX<2)f.raceX=2;if(f.raceX>317)f.raceX=317;
        if(f.raceY<2)f.raceY=2;if(f.raceY>197)f.raceY=197;
        if(act==ActConfirm){
            int id=raceAtCursor(vfs,f.raceX,f.raceY);
            if(id>=1&&id<=8){f.doc.race=id;f.step=StepRaceConfirm;f.cursor=0;}
            else f.message="Choose a province of Tamriel.";
        }
        break;}
    case StepRaceConfirm:
        if(act==ActUp||act==ActDown)f.cursor^=1;
        else if(act==ActConfirm){
            if(f.cursor==0){f.step=StepGender;f.cursor=0;}
            else{f.step=StepRace;}
        }else if(act==ActBack)f.step=StepRace;
        break;
    case StepGender:
        if(act==ActUp||act==ActDown)f.cursor^=1;
        else if(act==ActConfirm){
            f.doc.gender=f.cursor;f.step=StepClassQuestions;f.cursor=0;
            classQuestionsRoll(f,rng);
        }
        else if(act==ActBack){f.step=StepRaceConfirm;f.cursor=0;}
        break;
    case StepClassQuestions:
        if(act==ActUp||act==ActLeft)f.cursor=(f.cursor+2)%3;
        else if(act==ActDown||act==ActRight)f.cursor=(f.cursor+1)%3;
        else if(act==ActConfirm){
            f.cqHist[f.cqAnswered]=f.cursor;
            f.cqWeights[kAnswerTable[f.cq[f.cqAnswered]*3+f.cursor]]++;
            f.cqAnswered++;
            if(f.cqAnswered==10){
                f.cqRecommended=classQuestionsRecommend(vfs,f.cqWeights);
                f.step=StepClassRecommend;f.cursor=0;
            }
        }else if(act==ActBack){
            if(f.cqAnswered>0){
                f.cqAnswered--;
                f.cqWeights[kAnswerTable[f.cq[f.cqAnswered]*3+f.cqHist[f.cqAnswered]]]--;
                f.cursor=f.cqHist[f.cqAnswered];
            }else{f.step=StepGender;f.cursor=f.doc.gender;}
        }
        break;
    case StepClassRecommend:
        if(act==ActUp||act==ActDown)f.cursor^=1;
        else if(act==ActConfirm){
            if(f.cursor==0&&f.cqRecommended>=0&&f.cqRecommended<(int)classes.size()){
                f.doc.classIndex=f.cqRecommended;
                f.doc.career=classes[(size_t)f.cqRecommended];
                f.doc.career.custom=false;
                f.step=StepBiography;f.cursor=0;f.doc.biog.clear();
            }else{f.step=StepClass;f.cursor=0;f.page=0;}
        }else if(act==ActBack){f.step=StepClassQuestions;f.cursor=0;}
        break;
    case StepClass:{
        int n=(int)classes.size()+1;              // + custom
        if(act==ActUp)f.cursor=(f.cursor+n-1)%n;
        else if(act==ActDown)f.cursor=(f.cursor+1)%n;
        else if(act==ActConfirm){
            if(f.cursor<(int)classes.size()){
                f.doc.classIndex=f.cursor;f.doc.career=classes[(size_t)f.cursor];
                f.doc.career.custom=false;
                f.step=StepBiography;f.cursor=0;
                f.doc.biog.clear();
            }else{
                f.doc.classIndex=-1;
                f.doc.career=Career();
                f.doc.career.name="Custom";f.doc.career.custom=true;
                f.doc.career.hpPerLevel=8;
                f.step=StepCustom;f.cursor=0;f.subCursor=0;
            }
        }else if(act==ActBack){f.step=StepClassRecommend;f.cursor=0;}
        break;}
    case StepCustom:{
        // Fields: hp/level, 12 skill slots, advantages, disadvantages, done.
        const int fields=16;
        if(act==ActUp)f.cursor=(f.cursor+fields-1)%fields;
        else if(act==ActDown)f.cursor=(f.cursor+1)%fields;
        else if(act==ActLeft||act==ActRight){
            int dir=act==ActRight?1:-1;
            if(f.cursor==0){
                f.doc.career.hpPerLevel+=dir;
                if(f.doc.career.hpPerLevel<kCustomHpMin)f.doc.career.hpPerLevel=kCustomHpMin;
                if(f.doc.career.hpPerLevel>kCustomHpMax)f.doc.career.hpPerLevel=kCustomHpMax;
            }else if(f.cursor<=12){
                int slot=f.cursor-1;
                int& target=slot<3?f.doc.career.primary[slot]
                    :slot<6?f.doc.career.major[slot-3]:f.doc.career.minor[slot-6];
                target+=dir;
                if(target< -1)target=34;
                if(target>34)target=-1;
            }
        }else if(act==ActConfirm){
            if(f.cursor==13){f.step=StepAdvList;f.cursor=0;f.advDis=false;}
            else if(f.cursor==14){f.step=StepAdvList;f.cursor=0;f.advDis=true;}
            else if(f.cursor==15){
                // The classic ships no biography file for custom careers, so a
                // custom class proceeds straight to the bonus rolls.
                f.step=StepBonusStats;f.cursor=0;f.doc.biog.clear();
                rollCharacter(f.doc,rng);
            }
        }else if(act==ActBack){f.step=StepClass;f.cursor=(int)classes.size();}
        break;}
    case StepAdvList:{
        int n=0;
        for(size_t i=0;i<f.doc.picks.size();i++)if(f.doc.picks[i].dis==f.advDis)n++;
        if(act==ActUp)f.cursor=(f.cursor+n)%(n+1);
        else if(act==ActDown)f.cursor=(f.cursor+1)%(n+1);
        else if(act==ActConfirm){
            if(f.cursor==n){f.step=StepAdvPrimary;f.cursor=0;}
            else{
                int seen=0;
                for(size_t i=0;i<f.doc.picks.size();i++){
                    if(f.doc.picks[i].dis!=f.advDis)continue;
                    if(seen==f.cursor){unapplyAdvPick(f.doc,f.doc.picks[i]);
                        f.doc.picks.erase(f.doc.picks.begin()+i);break;}
                    seen++;
                }
                if(f.cursor>n-1)f.cursor=n>0?n-1:0;
            }
        }else if(act==ActBack){f.step=StepCustom;f.cursor=f.advDis?14:13;}
        break;}
    case StepAdvPrimary:
        if(act==ActUp)f.cursor=(f.cursor+advCount(f.advDis)-1)%advCount(f.advDis);
        else if(act==ActDown)f.cursor=(f.cursor+1)%advCount(f.advDis);
        else if(act==ActConfirm){
            int ns=advSubCount(f.advDis,f.cursor);
            if(ns>0){f.step=StepAdvSecondary;f.subCursor=0;}
            else{
                AdvPick p;p.prim=f.cursor;p.sec=-1;p.dis=f.advDis;
                p.cost=advCost(f.advDis,f.cursor,0);
                if(difficultyPoints(f.doc)+p.cost<-12||difficultyPoints(f.doc)+p.cost>40)
                    f.message="Difficulty points must stay within -12..40.";
                else{applyAdvPick(f.doc,p);f.doc.picks.push_back(p);f.step=StepAdvList;f.cursor=0;}
            }
        }else if(act==ActBack){f.step=StepAdvList;f.cursor=0;}
        break;
    case StepAdvSecondary:{
        int ns=advSubCount(f.advDis,f.cursor);
        if(act==ActUp)f.subCursor=(f.subCursor+ns-1)%ns;
        else if(act==ActDown)f.subCursor=(f.subCursor+1)%ns;
        else if(act==ActConfirm){
            AdvPick p;p.prim=f.cursor;p.sec=f.subCursor;p.dis=f.advDis;
            p.cost=advCost(f.advDis,f.cursor,f.subCursor);
            if(difficultyPoints(f.doc)+p.cost<-12||difficultyPoints(f.doc)+p.cost>40)
                f.message="Difficulty points must stay within -12..40.";
            else{applyAdvPick(f.doc,p);f.doc.picks.push_back(p);f.step=StepAdvList;f.cursor=0;}
        }else if(act==ActBack){f.step=StepAdvPrimary;f.cursor=0;}
        break;}
    case StepBiography:{
        int qi=f.cursor/1;                        // one cursor over questions*answers
        (void)qi;
        if(biog.questions.empty()){f.step=StepBonusStats;f.cursor=0;break;}
        int q=f.subCursor;
        const BiogQuestion& bq=biog.questions[(size_t)q];
        int na=(int)bq.answers.size();
        if(na<1)na=1;
        if(act==ActUp)f.cursor=(f.cursor+na-1)%na;
        else if(act==ActDown)f.cursor=(f.cursor+1)%na;
        else if(act==ActConfirm){
            f.doc.answers[q]=f.cursor;
            BiogEffects e;
            for(const std::string& s:bq.answers[(size_t)f.cursor].effects){
                BiogEffects one;
                parseBiogEffect(s,one);
                e.accumulate(one);
            }
            f.doc.biog.accumulate(e);
            if(q+1<(int)biog.questions.size()){f.subCursor=q+1;f.cursor=0;}
            else{f.step=StepBonusStats;f.cursor=0;f.subCursor=0;
                 rollCharacter(f.doc,rng);}
        }else if(act==ActBack){
            if(q>0){f.subCursor=q-1;f.cursor=0;}
            else if(f.doc.career.custom){f.step=StepCustom;f.cursor=12;}
            else{f.step=StepClass;f.cursor=f.doc.classIndex<0?0:f.doc.classIndex;}
        }
        break;}
    case StepBonusStats:
        if(act==ActUp)f.cursor=(f.cursor+StatCount-1)%StatCount;
        else if(act==ActDown)f.cursor=(f.cursor+1)%StatCount;
        else if(act==ActRight){
            if(f.doc.bonusPool>0&&f.doc.attributes[f.cursor]<100){
                f.doc.attributes[f.cursor]++;f.doc.bonusPool--;finalizeCharacter(f.doc);}
        }else if(act==ActLeft){
            if(f.doc.attributes[f.cursor]>f.doc.career.attributes[f.cursor]){
                f.doc.attributes[f.cursor]--;f.doc.bonusPool++;finalizeCharacter(f.doc);}
        }else if(act==ActConfirm){
            if(f.doc.bonusPool==0){f.step=StepBonusSkills;f.cursor=0;}
            else f.message="You must distribute all bonus points.";
            if(!f.message.empty())f.step=before;
        }else if(act==ActBack){f.step=StepBiography;f.cursor=0;f.subCursor=0;}
        break;
    case StepBonusSkills:{
        // Cursor walks the 12 class skills; right spends that group's pool.
        int list[12],group[12],n=0;
        for(int i=0;i<3;i++)if(f.doc.career.primary[i]>=0){list[n]=f.doc.career.primary[i];group[n]=0;n++;}
        for(int i=0;i<3;i++)if(f.doc.career.major[i]>=0){list[n]=f.doc.career.major[i];group[n]=1;n++;}
        for(int i=0;i<6;i++)if(f.doc.career.minor[i]>=0){list[n]=f.doc.career.minor[i];group[n]=2;n++;}
        if(n<1){f.step=StepFace;f.cursor=0;break;}
        if(act==ActUp)f.cursor=(f.cursor+n-1)%n;
        else if(act==ActDown)f.cursor=(f.cursor+1)%n;
        else if(act==ActRight){
            int g=group[f.cursor];
            if(f.doc.skillPool[g]>0&&f.doc.skillValues[list[f.cursor]]<100){
                f.doc.skillValues[list[f.cursor]]++;f.doc.skillPool[g]--;
            }
        }else if(act==ActLeft){
            int g=group[f.cursor];
            if(f.doc.skillValues[list[f.cursor]]>0){
                f.doc.skillValues[list[f.cursor]]--;f.doc.skillPool[g]++;
            }
        }else if(act==ActConfirm){
            if(f.doc.skillPool[0]+f.doc.skillPool[1]+f.doc.skillPool[2]==0){f.step=StepFace;f.cursor=0;}
        }else if(act==ActBack){f.step=StepBonusStats;f.cursor=0;}
        break;}
    case StepFace:
        if(act==ActLeft)f.cursor=(f.cursor+9)%10;
        else if(act==ActRight)f.cursor=(f.cursor+1)%10;
        else if(act==ActConfirm){
            f.doc.faceIndex=f.cursor;f.step=StepName;f.cursor=0;
            f.nameList.clear();
            for(int i=0;i<8;i++)f.nameList.push_back(names.fullName(f.doc.race,f.doc.gender,rng));
            if(!f.nameList.empty())f.doc.name=f.nameList[0];
        }
        else if(act==ActBack){f.step=StepBonusSkills;f.cursor=0;}
        break;
    case StepName:
        if(act==ActUp||act==ActLeft)f.cursor=(f.cursor+7)%8;
        else if(act==ActDown||act==ActRight)f.cursor=(f.cursor+1)%8;
        else if(act==ActReset){
            f.nameList.clear();
            for(int i=0;i<8;i++)f.nameList.push_back(names.fullName(f.doc.race,f.doc.gender,rng));
        }
        else if(act==ActConfirm){
            if(f.cursor<(int)f.nameList.size())f.doc.name=f.nameList[(size_t)f.cursor];
            f.step=StepSummary;f.cursor=0;finalizeCharacter(f.doc);
        }
        else if(act==ActBack){f.step=StepFace;f.cursor=f.doc.faceIndex;}
        break;
    case StepSummary:
        // Fields: face, reflexes, done - the classic summary screen lets the
        // player revisit both before accepting the character.
        if(act==ActUp)f.cursor=(f.cursor+2)%3;
        else if(act==ActDown)f.cursor=(f.cursor+1)%3;
        else if(act==ActLeft||act==ActRight){
            int dir=act==ActRight?1:-1;
            if(f.cursor==0)f.doc.faceIndex=(f.doc.faceIndex+dir+10)%10;
            else if(f.cursor==1)f.doc.reflexes=(f.doc.reflexes+dir+5)%5;
        }
        else if(act==ActConfirm){
            if(f.cursor==2){f.step=StepDone;finalizeCharacter(f.doc);}
        }
        else if(act==ActBack){f.step=StepName;f.cursor=0;}
        break;
    default:break;
    }
    return f.step!=before;
}
void charGenConfirmStep(CharGenFlow& f,const std::vector<Career>& classes,
                        const BiogFile& biog,const NameGen& names,DfRandom& rng,const Vfs& vfs){
    charGenAdvance(f,ActConfirm,classes,biog,names,rng,vfs);
}
} // namespace dfpsp
