#include "CharGen.h"
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
// --- flow ---
static const int kCustomHpMin=4,kCustomHpMax=30;
struct CustomField { int kind; int idx; };       // 0 hp, 1 skill slot, 2 advantage, 3 done
bool charGenAdvance(CharGenFlow& f,CharGenAction act,const std::vector<Career>& classes,
                    const BiogFile& biog,const NameGen& names,DfRandom& rng){
    int before=f.step;
    f.message.clear();
    switch(f.step){
    case StepRace:
        if(act==ActUp||act==ActLeft)f.cursor=(f.cursor+kRaceCount-1)%kRaceCount;
        else if(act==ActDown||act==ActRight)f.cursor=(f.cursor+1)%kRaceCount;
        else if(act==ActConfirm){f.doc.race=f.cursor+1;f.step=StepGender;f.cursor=0;}
        break;
    case StepGender:
        if(act==ActUp||act==ActDown)f.cursor^=1;
        else if(act==ActConfirm){f.doc.gender=f.cursor;f.step=StepClass;f.cursor=0;f.page=0;}
        else if(act==ActBack){f.step=StepRace;f.cursor=f.doc.race-1;}
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
        }else if(act==ActBack){f.step=StepGender;f.cursor=f.doc.gender;}
        break;}
    case StepCustom:{
        // Fields: hp/level, 12 skill slots, done.
        int slots=12;
        if(act==ActUp)f.cursor=(f.cursor+slots+1)%(slots+2);
        else if(act==ActDown)f.cursor=(f.cursor+1)%(slots+2);
        else if(act==ActLeft||act==ActRight){
            int dir=act==ActRight?1:-1;
            if(f.cursor==0){
                f.doc.career.hpPerLevel+=dir;
                if(f.doc.career.hpPerLevel<kCustomHpMin)f.doc.career.hpPerLevel=kCustomHpMin;
                if(f.doc.career.hpPerLevel>kCustomHpMax)f.doc.career.hpPerLevel=kCustomHpMax;
            }else if(f.cursor<=slots){
                int slot=f.cursor-1;
                int& target=slot<3?f.doc.career.primary[slot]
                    :slot<6?f.doc.career.major[slot-3]:f.doc.career.minor[slot-6];
                target+=dir;
                if(target< -1)target=34;
                if(target>34)target=-1;
            }
        }else if(act==ActConfirm){
            // The classic ships no biography file for custom careers, so a
            // custom class proceeds straight to the bonus rolls.
            if(f.cursor==slots+1){f.step=StepBonusStats;f.cursor=0;f.doc.biog.clear();
                rollCharacter(f.doc,rng);}
        }else if(act==ActBack){f.step=StepClass;f.cursor=(int)classes.size();}
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
        if(act==ActConfirm){f.step=StepDone;finalizeCharacter(f.doc);}
        else if(act==ActBack){f.step=StepName;f.cursor=0;}
        break;
    default:break;
    }
    return f.step!=before;
}
void charGenConfirmStep(CharGenFlow& f,const std::vector<Career>& classes,
                        const BiogFile& biog,const NameGen& names,DfRandom& rng){
    charGenAdvance(f,ActConfirm,classes,biog,names,rng);
}
} // namespace dfpsp
