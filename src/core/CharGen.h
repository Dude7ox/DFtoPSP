#pragma once
#include "Vfs.h"
#include "Random.h"
#include <string>
#include <vector>
namespace dfpsp {
// Character creation, built on the original data files: CLASS??.CFG careers,
// BIOG??T0.TXT biographies, NAMEGEN.DAT names, TEXT.RSC descriptions and the
// race paper-doll art names. Nothing is converted - a mod that replaces any of
// those files changes what the port offers.
enum CharStat { StatStrength=0,StatIntelligence,StatWillpower,StatAgility,StatEndurance,
                StatPersonality,StatSpeed,StatLuck,StatCount=8 };
extern const char* const kStatNames[8];
extern const char* const kStatShort[8];
extern const char* const kSkillNames[35];       // classic DFCareer.Skills order
extern const char* const kGenderNames[2];
enum CharRace { RaceNone=0,RaceBreton=1,RaceRedguard=2,RaceNord=3,RaceDarkElf=4,
                RaceHighElf=5,RaceWoodElf=6,RaceKhajiit=7,RaceArgonian=8 };
// Classic EffectFlags bits, used by both careers and races.
enum CharEffect { EffParalysis=1,EffMagic=2,EffPoison=4,EffFire=8,EffFrost=16,EffShock=32,EffDisease=64 };
// Race templates (MIT Daggerfall Unity values) plus the art each race needs.
struct RaceInfo {
    int id;
    const char* name;
    int descriptionId;      // TEXT.RSC record
    int clipId;             // DAGGER.SND record played on selection
    const char* paperDollBackground;
    const char* bodyMaleNude,*bodyMaleClothed,*bodyFemaleNude,*bodyFemaleClothed;
    const char* headsMale,*headsFemale;         // CIF with the ten head records
    unsigned resistance,immunity,lowTolerance,criticalWeakness;
    unsigned specialAbilities;
};
extern const int kRaceCount;                    // 8 playable races
const RaceInfo* raceInfo(int id);
// A career as stored in CLASS??.CFG: the 74-byte classic definition.
struct Career {
    std::string name;
    int hpPerLevel=8;
    float advancement=1.0f;
    int attributes[8]={50,50,50,50,50,50,50,50};
    int primary[3]={-1,-1,-1},major[3]={-1,-1,-1},minor[6]={-1,-1,-1,-1,-1,-1};
    unsigned resistance=0,immunity=0,lowTolerance=0,criticalWeakness=0;
    unsigned specialAbilities=0;
    int rapidHealing=0,regeneration=0,spellAbsorption=0,attackModifier=0;
    unsigned forbiddenMaterials=0,weaponArmorShields=0;
    bool custom=false;
    int skillGroup(int skill)const;             // 0 primary, 1 major, 2 minor, -1 none
};
bool loadClasses(const Vfs& vfs,std::vector<Career>& out);   // CLASS00..CLASS17
bool parseCareer(const std::vector<u8>& data,Career& out);   // single CLASS??.CFG record
// Biography: twelve questions with answers that carry effect commands.
struct BiogAnswer { std::string text; std::vector<std::string> effects; };
struct BiogQuestion { std::vector<std::string> text; std::vector<BiogAnswer> answers; };
struct BiogFile {
    int classIndex=0,backstoryId=0;
    std::vector<BiogQuestion> questions;
    bool load(const Vfs& vfs,int classIndex,int bioIndex=0);
};
struct BiogEffects {
    int skillMod[35]={0};
    int goldMod=0;
    int socialRep[5]={0};
    int resistPoison=0,fatigue=0,reaction=0,resistDisease=0,resistMagic=0,avoidHit=0;
    struct ItemGrant { int group=-1,index=-1,material=-1; };
    std::vector<ItemGrant> items;
    void clear();
    void accumulate(const BiogEffects& o);
};
// Parses the classic effect commands: "<skill> <delta>", GP, IT, r/rf, RP, FT,
// RR, RD, MR, TH. Returns false for an unknown command (it is then ignored).
bool parseBiogEffect(const std::string& line,BiogEffects& out);
// NAMEGEN.DAT: eleven banks of six fixed-width part lists.
class NameGen {
public:
    bool open(const Vfs& vfs);
    bool loaded()const{return loaded_;}
    std::string firstName(int race,int gender,DfRandom& rng)const;
    std::string surname(int race,DfRandom& rng)const;
    std::string fullName(int race,int gender,DfRandom& rng)const;
private:
    struct Bank { std::vector<std::string> parts; };
    struct Group { Bank banks[6]; };
    Group groups_[11];
    bool loaded_=false;
};
// The classic stat/skill rollout: every attribute gains 0-10 on top of the
// career base plus a free pool of 6-14 points; skills start at 28/18/13 plus
// 0-3 and each group gets six free points.
struct CharRoll { int attributes[8]; int bonusPool; };
CharRoll rollStats(const Career& c,DfRandom& rng);
struct SkillRoll { int values[35]; int pool[3]; };
SkillRoll rollSkills(const Career& c,DfRandom& rng);
// The document being built, one step at a time.
struct CharacterDoc {
    int race=RaceBreton,gender=0;
    int classIndex=-1;              // -1 while custom
    Career career;
    int biographyIndex=0;
    int answers[12];                // chosen answer per question, -1 unanswered
    BiogEffects biog;
    int faceIndex=0;
    std::string name;
    int attributes[8]={0};
    int rolledAttributes[8]={0};
    int bonusPool=0;
    int skillValues[35]={0};
    int skillPool[3]={0,0,0};
    int level=1,xp=0,gold=0,maxHp=0,hp=0;
    bool rolled=false;
    CharacterDoc();
    int spellPoints()const;
    int maxEncumbrance()const;
    int damageMod()const;
    int toHitMod()const;
    int magicResist()const;
    int healingRate()const;
    int hpModifier()const;
    int fatigueModifier()const;
};
// Rolls attributes/skills for the current career and applies the biography.
void rollCharacter(CharacterDoc& d,DfRandom& rng);
void finalizeCharacter(CharacterDoc& d);
// Maximum health for a level-1 character of this career and endurance.
int rollMaxHealth(const Career& c,int endurance,int level=1);
// --- Creation flow state machine, shared by host and PSP ---
enum CharGenStep { StepRace=0,StepGender,StepClass,StepCustom,StepBiography,
                   StepBonusStats,StepBonusSkills,StepFace,StepName,StepSummary,StepDone };
struct CharGenFlow {
    int step=StepRace;
    int cursor=0;                   // highlighted entry of the current list
    int subCursor=0;                // inner position (answer, question...)
    int page=0;                     // first visible row for long lists
    CharacterDoc doc;
    std::vector<std::string> nameList;   // eight name suggestions
    std::string message;
};
// Actions the platform layer can send. Returns true when the step changed.
enum CharGenAction { ActUp,ActDown,ActLeft,ActRight,ActConfirm,ActBack,ActReset };
bool charGenAdvance(CharGenFlow& f,CharGenAction act,const std::vector<Career>& classes,
                    const BiogFile& biog,const NameGen& names,DfRandom& rng);
// Advances the flow to the next step with whatever is currently selected
// (used by scripted host runs and tests).
void charGenConfirmStep(CharGenFlow& f,const std::vector<Career>& classes,
                        const BiogFile& biog,const NameGen& names,DfRandom& rng);
} // namespace dfpsp
