#pragma once
#include "Bsa.h"
#include "EnemyData.h"
#include "Random.h"
#include "Scene.h"
namespace dfpsp {
// Career record decoded from the original MONSTER.BSA (ENEMY###.CFG, the 74 byte
// CLASS.CFG layout). Mods that replace MONSTER.BSA therefore change names, hit
// points per level, attributes and resistances.
struct EnemyCareer {
    bool valid=false;
    u32 flags=0;
    int ability=0, spellPoints=0;
    int rapidHealing=0, regeneration=0, spellAbsorption=0, attackModifier=0;
    int forbiddenMaterials=0;
    u32 weaponMask=0;
    int primary[3]={0,0,0}, major[3]={0,0,0}, minor[6]={0,0,0};
    char name[17]={0};
    int hitPointsPerLevel=0;
    u32 advancement=0;
    int attributes[8]={0,0,0,0,0,0,0,0};
};
class EnemyBook {
public:
    bool load(const Bsa& monsters);
    bool ready()const{return loaded_;}
    const EnemyCareer* career(int mobileId)const;
    std::string name(int mobileId)const;
    // Classic random encounter pick for a dungeon type and player level
    // (RDBLayout.AddRandomRDBEnemyClassic / ChooseRandomEnemyType parity).
    int rollEncounter(int dungeonType,int playerLevel,DfRandom& rng)const;
private:
    bool loaded_=false;
    std::vector<EnemyCareer> careers_;
};
enum EnemyState { EnemyIdle=0, EnemyMove=1, EnemyAttack=2, EnemyHurt=3, EnemyDead=4 };
struct Enemy {
    int mobileId=0;
    Vec3 pos;               // feet, native RDB world coordinates
    float yaw=0;            // degrees, forward = {sin,0,cos} like the camera
    bool female=false, passive=false, water=false, flying=false;
    EnemyState state=EnemyIdle;
    int level=1, maxHp=1, hp=1, armor=0;
    float speed=90;
    int animState=AnimMove, facing=0, dir=0;   // facing 0..7 relative to the camera
    int frame=0;
    float bodyW=64, bodyH=64;   // canonical billboard size in world units
    bool seen=false;
    float animTimer=0, stateTimer=0, losTimer=0, wanderTimer=0, hurtTimer=0;
    bool aggro=false, mirrored=false, grounded=true, struck=false;
    std::string key;        // persistence key: block name, marker position, index
};
// Geometry and animation queries shared by both renderers.
int enemyRecord(const Enemy& e);            // record inside the monster archive
void orientEnemy(Enemy& e,const Vec3& player,const Vec3& camForward);
float enemyWorldHeight(const Enemy& e);    // billboard height in world units
float enemyWorldWidth(const Enemy& e);
float enemyEyeHeight(const Enemy& e);
// Callbacks into the dungeon so the AI can reuse collision, floors and sight.
struct EnemyWorld {
    Vec3 player;            // player feet
    Vec3 camForward;        // camera forward, y ignored
    bool (*los)(void* ctx,const Vec3& a,const Vec3& b)=nullptr;
    Vec3 (*move)(void* ctx,const Vec3& feet,const Vec3& delta,float radius,float height)=nullptr;
    float (*floor)(void* ctx,const Vec3& pos,float maxY)=nullptr;
    void* ctx=nullptr;
};
void initEnemyStats(Enemy& e,const EnemyStatic& st,const EnemyCareer* career,int playerLevel,DfRandom& rng);
// Advances one enemy. Returns melee damage dealt to the player this tick.
int updateEnemy(Enemy& e,const EnemyStatic& st,const EnemyWorld& w,DfRandom& rng,float dt);
// Combat rolls, exposed for the player side and for tests.
bool enemySwingHits(int enemyLevel,int playerArmor,DfRandom& rng);
int enemySwingDamage(const EnemyStatic& st,DfRandom& rng);
bool playerSwingHits(int skill,int enemyLevel,DfRandom& rng);
int playerSwingDamage(int minDamage,int maxDamage,int enemyArmor,DfRandom& rng);
} // namespace dfpsp
