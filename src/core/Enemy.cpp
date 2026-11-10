#include "Enemy.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace dfpsp {
bool EnemyBook::load(const Bsa& monsters){
    careers_.assign(43,EnemyCareer{});
    loaded_=false;
    for(int id=0;id<=42;id++){
        char name[32];snprintf(name,sizeof(name),"ENEMY%03d.CFG",id);
        std::vector<u8> b;
        if(!monsters.read(name,b)||b.size()<74)continue;
        EnemyCareer& c=careers_[id];
        c.flags=b[0]|u32(b[1])<<8|u32(b[2])<<16|u32(b[3])<<24;
        int ability=b[4]|b[5]<<8;
        c.ability=ability&0xff;c.spellPoints=(ability>>8)&0xff;
        c.rapidHealing=b[6];c.regeneration=b[7];
        c.spellAbsorption=b[9];c.attackModifier=(signed char)b[10];
        c.forbiddenMaterials=b[11]|b[12]<<8;
        c.weaponMask=(u32(b[13])<<16)|(u32(b[15])<<8)|u32(b[14]);
        for(int i=0;i<3;i++)c.primary[i]=b[16+i];
        for(int i=0;i<3;i++)c.major[i]=b[19+i];
        for(int i=0;i<6;i++)c.minor[i]=b[22+i];
        memcpy(c.name,b.data()+28,16);c.name[16]=0;
        c.hitPointsPerLevel=b[52]|b[53]<<8;
        c.advancement=b[54]|u32(b[55])<<8|u32(b[56])<<16|u32(b[57])<<24;
        for(int i=0;i<8;i++)c.attributes[i]=b[58+i*2]|b[59+i*2]<<8;
        c.valid=true;
        loaded_=true;
    }
    return loaded_;
}
const EnemyCareer* EnemyBook::career(int mobileId)const{
    if(mobileId<0||mobileId>=43)return nullptr;
    return careers_[mobileId].valid?&careers_[mobileId]:nullptr;
}
std::string EnemyBook::name(int mobileId)const{
    const EnemyCareer* c=career(mobileId);
    if(c&&c->name[0])return std::string(c->name);
    return enemyFallbackName(mobileId);
}
int EnemyBook::rollEncounter(int dungeonType,int playerLevel,DfRandom& rng)const{
    int table=dungeonType;
    if(table<0||table>18)table=6;   // NaturalCave as a sane default
    int r=rng.rangeInclusive(1,100);
    int mn,mx;
    if(r>95&&playerLevel<=5){mn=0;mx=playerLevel+2;}
    else if(r>80){mn=0;mx=playerLevel+1;}
    else{mn=playerLevel-3;mx=playerLevel+3;}
    if(mn<0){mn=0;mx=5;}
    else if(mx>19){mn=14;mx=19;}
    int idx=rng.rangeInclusive(mn,mx);
    int id=kEncounterTables[table][idx];
    if(id<0)id=0;
    return id;
}
void initEnemyStats(Enemy& e,const EnemyStatic& st,const EnemyCareer* career,int playerLevel,DfRandom& rng){
    e.flying=(st.behaviour==BehaviourFlying);
    if(e.mobileId>=128){
        // Humanoids scale with the player. RollEnemyClassMaxHealth lives in the
        // original executable and is not reproduced by DFU, so approximate it
        // with level * HitPointsPerLevel from MONSTER.BSA.
        int perLevel=(career&&career->hitPointsPerLevel>0)?career->hitPointsPerLevel:8;
        e.level=playerLevel;
        if(e.mobileId==146)e.level+=rng.range(3,7);   // city watch patrol variance
        if(e.level<1)e.level=1;
        e.maxHp=e.level*perLevel;
    }else{
        e.level=st.level;
        e.maxHp=rng.range(st.minHealth,st.maxHealth+1);
    }
    if(e.maxHp<1)e.maxHp=1;
    e.hp=e.maxHp;
    e.armor=st.armorValue*5;
    e.speed=90.f+e.level*8.f;
    if(e.speed>240.f)e.speed=240.f;
    if(e.flying)e.speed*=1.15f;
}
int enemyRecord(const Enemy& e){
    int dir=e.dir;
    if(dir<0)dir=0;if(dir>4)dir=4;
    return e.animState*5+dir;
}
void orientEnemy(Enemy& e,const Vec3& player,const Vec3& camForward){
    Vec3 d=player-e.pos;d.y=0;
    if(d.len()<1e-3f)return;
    d=d.norm();
    Vec3 f=camForward;f.y=0;
    if(f.len()<1e-3f)return;
    f=f.norm();
    float cross=f.x*d.z-f.z*d.x;
    float dot=f.x*d.x+f.z*d.z;
    float ang=std::atan2(cross,dot)*180.f/3.14159265358979f;
    int ori=(int)std::lround(-ang/45.f);
    ori=((ori%8)+8)%8;
    e.facing=ori;
    e.dir=ori<=4?ori:8-ori;
    e.mirrored=ori>4;
}
float enemyWorldHeight(const Enemy& e){return e.bodyH;}
float enemyWorldWidth(const Enemy& e){return e.bodyW;}
float enemyEyeHeight(const Enemy& e){return e.bodyH*0.75f;}
bool enemySwingHits(int enemyLevel,int playerArmor,DfRandom& rng){
    float chance=0.45f+enemyLevel*0.02f-playerArmor*0.003f;
    if(chance<0.15f)chance=0.15f;if(chance>0.9f)chance=0.9f;
    return rng.percent()<=(int)(chance*100.f);
}
static bool seqValid(const signed char* seq){return seq&&seq[0]>=-1;}
int enemySwingDamage(const EnemyStatic& st,DfRandom& rng){
    int column=0;
    const signed char* seq=st.attack1;
    int r=rng.range(1,100);
    if(st.chance3&&seqValid(st.attack3)&&r<=(int)st.chance3){seq=st.attack3;column=2;}
    else if(st.chance2&&seqValid(st.attack2)&&r<=(int)st.chance2){seq=st.attack2;column=1;}
    (void)seq;
    int lo=st.minDamage[column],hi=st.maxDamage[column];
    if(hi<lo)hi=lo;
    int dmg=rng.rangeInclusive(lo,hi+1);
    return dmg<1?1:dmg;
}
bool playerSwingHits(int skill,int enemyLevel,DfRandom& rng){
    float chance=0.30f+(skill-enemyLevel*3)/200.f+0.25f;
    if(chance<0.15f)chance=0.15f;if(chance>0.95f)chance=0.95f;
    return rng.percent()<=(int)(chance*100.f);
}
int playerSwingDamage(int minDamage,int maxDamage,int enemyArmor,DfRandom& rng){
    int lo=minDamage,hi=maxDamage;
    if(hi<lo)hi=lo;
    int raw=rng.rangeInclusive(lo,hi+1);
    int mitigated=raw*100/(100+(enemyArmor>0?enemyArmor:0));
    return mitigated<1?1:mitigated;
}
int updateEnemy(Enemy& e,const EnemyStatic& st,const EnemyWorld& w,DfRandom& rng,float dt){
    if(e.state==EnemyDead){e.stateTimer+=dt;return 0;}
    int damage=0;
    Vec3 eyePos=e.pos+Vec3{0,enemyEyeHeight(e),0};
    Vec3 playerEye=w.player+Vec3{0,64.f,0};
    float dx=w.player.x-e.pos.x,dz=w.player.z-e.pos.z;
    float dist=std::sqrt(dx*dx+dz*dz);
    e.losTimer-=dt;
    if(e.losTimer<=0.f){
        e.losTimer=0.25f;
        e.seen=w.los?w.los(w.ctx,eyePos,playerEye):true;
    }
    e.stateTimer+=dt;
    float attackRange=60.f+e.bodyW*0.5f;
    if(!e.aggro){
        if(!e.passive&&e.seen&&dist<1400.f){e.aggro=true;e.state=EnemyMove;e.stateTimer=0.f;}
    }else if(dist>2600.f||(e.stateTimer>6.f&&dist>attackRange)){
        e.aggro=false;
    }
    if(e.state==EnemyHurt){
        e.hurtTimer-=dt;
        e.animState=AnimHurt;
        e.frame=(int)(e.animTimer*4.f)%5;
        e.animTimer+=dt;
        if(e.hurtTimer<=0.f){e.state=e.aggro?EnemyMove:EnemyIdle;e.animTimer=0.f;}
        return 0;
    }
    if(e.aggro&&dist<=attackRange&&e.seen){
        if(e.state!=EnemyAttack){e.state=EnemyAttack;e.stateTimer=0.f;e.animTimer=0.f;e.struck=false;e.frame=0;}
        e.yaw=std::atan2(dx,dz)*180.f/3.14159265358979f;
        e.animState=AnimPrimaryAttack;
        int step=(int)(e.animTimer*10.f);
        e.animTimer+=dt;
        if(step<10){
            signed char f=st.attack1[step];
            if(f==-1){
                if(!e.struck){
                    e.struck=true;
                    damage=enemySwingHits(e.level,0,rng)?enemySwingDamage(st,rng):0;
                }
            }else if(f>=0){e.frame=f;}
        }else{
            e.state=EnemyMove;e.struck=false;e.animTimer=0.f;e.stateTimer=0.f;
        }
        return damage;
    }
    if(e.aggro){
        e.state=EnemyMove;
        float want=std::atan2(dx,dz)*180.f/3.14159265358979f;
        float delta=want-e.yaw;
        while(delta>180.f)delta-=360.f;
        while(delta<-180.f)delta+=360.f;
        float turn=240.f*dt;
        e.yaw+=delta>turn?turn:(delta<-turn?-turn:delta);
    }else{
        e.wanderTimer-=dt;
        if(e.wanderTimer<=0.f){e.wanderTimer=(float)rng.range(2,6);e.yaw+=(float)rng.range(-120,120);}
        e.state=dist>attackRange?EnemyMove:EnemyIdle;
    }
    if(e.state==EnemyMove){
        e.animState=AnimMove;
        float fps=e.flying?10.f:6.f;
        e.frame=(int)(e.animTimer*fps)%5;
        e.animTimer+=dt;
        float spd=e.speed*(e.aggro?1.f:0.35f);
        Vec3 fwd{std::sin(e.yaw*3.14159265358979f/180.f),0,std::cos(e.yaw*3.14159265358979f/180.f)};
        Vec3 np=w.move?w.move(w.ctx,e.pos,fwd*(spd*dt),20.f,80.f):e.pos+fwd*(spd*dt);
        e.pos=np;
        if(!e.flying&&w.floor){
            float f=w.floor(w.ctx,e.pos,e.pos.y+96.f);
            if(f>-10000.f)e.pos.y=f;
        }
    }else if(e.state==EnemyIdle){
        // Not every monster has an idle animation; fall back to the move loop.
        e.animState=(st.flags&EnemyHasIdle)?AnimIdle:AnimMove;
        e.frame=(int)(e.animTimer*4.f)%5;
        e.animTimer+=dt;
    }
    return damage;
}
} // namespace dfpsp
