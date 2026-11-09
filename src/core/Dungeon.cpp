#include "Dungeon.h"
#include <algorithm>
#include <cmath>
namespace dfpsp {
const DungeonBlockRef* Dungeon::refAt(int x,int z)const{for(auto& b:layout_.blocks)if(b.x==x&&b.z==z)return &b;return nullptr;}
const DungeonBlockRef* Dungeon::current()const{return refAt(currentX_,currentZ_);}
const Dungeon::Resident* Dungeon::residentAt(const Vec3& p)const{
    int x=(int)std::floor(p.x/BLOCK_SIDE),z=(int)std::floor(p.z/BLOCK_SIDE);
    for(auto& r:resident_)if(r.ref.x==x&&r.ref.z==z)return &r;
    return nullptr;
}
bool Dungeon::enter(const DungeonLayout& layout,int playerLevel){
    const DungeonBlockRef* start=nullptr;for(auto& b:layout.blocks)if(b.start){start=&b;break;}if(!start&&!layout.blocks.empty())start=&layout.blocks[0];if(!start)return false;
    Scene first;if(!builder_.loadBlock(start->name,first))return false;
    layout_=layout;resident_.clear();doorStates_.clear();currentX_=start->x;currentZ_=start->z;
    playerLevel_=playerLevel<1?1:playerLevel;
    enemies_.clear();deadKeys_.clear();spawnedKeys_.clear();deadSet_.clear();spawnedSet_.clear();
    // Classic seeds the encounter rolls with the dungeon location id and fills
    // both 256 entry pick lists before any marker is placed.
    rng_.seed((u32)layout_.interiorId);
    for(int i=0;i<256;i++)encounterLand_[i]=book_.rollEncounter(layout_.dungeonType,playerLevel_,rng_);
    for(int i=0;i<256;i++)encounterWater_[i]=book_.rollEncounter(19,playerLevel_,rng_);
    spawn_=first.spawn+Vec3{(float)start->x*BLOCK_SIDE,0,(float)start->z*BLOCK_SIDE};
    resident_.push_back({*start,std::move(first)});
    spawnBlockEnemies(resident_.back());
    currentX_=start->x+1000000;return update(spawn_);
}
void Dungeon::rememberDoors(){for(auto& r:resident_){std::vector<std::pair<float,float>> states;for(auto& d:r.scene.doors)states.push_back({d.amount,d.target});doorStates_[r.ref.name+":"+std::to_string(r.ref.x)+":"+std::to_string(r.ref.z)]=std::move(states);}}
void Dungeon::spawnBlockEnemies(const Resident& r){
    Vec3 offset{(float)r.ref.x*BLOCK_SIDE,0,(float)r.ref.z*BLOCK_SIDE};
    for(size_t i=0;i<r.scene.markers.size();i++){
        const SpawnMarker& m=r.scene.markers[i];
        std::string key=r.ref.name+":"+std::to_string(r.ref.x)+","+std::to_string(r.ref.z)+"#"+std::to_string(i);
        if(spawnedSet_.count(key))continue;
        spawnedSet_.insert(key);spawnedKeys_.push_back(key);
        int mobileId=-1;
        if(m.fixed){
            if(m.mobileType!=99&&findEnemyStatic(m.mobileType))mobileId=m.mobileType;
        }else{
            int slot=m.flags;
            if(slot<1||slot>255)slot=rng_.range(1,7);
            mobileId=encounterLand_[slot&255];
        }
        const EnemyStatic* st=findEnemyStatic(mobileId);
        if(!st)continue;
        Enemy e;
        e.mobileId=mobileId;e.key=key;
        e.pos=m.pos+offset;
        e.passive=m.passive;
        e.female=m.fixed&&(m.soundIndex==1);
        int archive=(e.female&&st->femaleArchive)?st->femaleArchive:st->archive;
        TextureInfo ti;
        if(textureInfo(builder_.vfs(),archive,0,ti)){
            e.bodyW=(float)(ti.w+(int)((float)ti.w*((float)ti.scaleX/256.f)));
            e.bodyH=(float)(ti.h+(int)((float)ti.h*((float)ti.scaleY/256.f)));
            if(e.bodyW<8)e.bodyW=8;if(e.bodyH<8)e.bodyH=8;
        }
        if(deadSet_.count(key)){
            // A corpse stays where it fell for the rest of the session.
            e.state=EnemyDead;e.hp=0;e.animState=AnimHurt;e.frame=0;
            enemies_.push_back(e);
            continue;
        }
        initEnemyStats(e,*st,book_.career(mobileId),playerLevel_,rng_);
        if(!e.flying){
            float f=floor(e.pos,e.pos.y+96.f);
            if(f>-10000.f)e.pos.y=f;
        }
        enemies_.push_back(e);
    }
}
bool Dungeon::update(const Vec3& p){
    int x=(int)std::floor(p.x/BLOCK_SIDE),z=(int)std::floor(p.z/BLOCK_SIDE);if(!refAt(x,z))return false;
    if(x==currentX_&&z==currentZ_&&!resident_.empty())return true;
    rememberDoors();currentX_=x;currentZ_=z;std::vector<Resident> next;
    std::vector<char> fresh;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
        auto* ref=refAt(x+dx,z+dz);if(!ref)continue;
        bool loaded=false;for(auto& old:resident_)if(old.ref.name==ref->name&&old.ref.x==ref->x&&old.ref.z==ref->z){next.push_back(std::move(old));loaded=true;break;}
        fresh.push_back(loaded?0:1);
        if(loaded)continue;Scene s;if(!builder_.loadBlock(ref->name,s))continue;
        Vec3 offset{(float)ref->x*BLOCK_SIDE,0,(float)ref->z*BLOCK_SIDE};
        for(auto& t:s.tris)for(auto& v:t.v)v.p=v.p+offset;
        for(auto& f:s.flats)f.pos=f.pos+offset;
        for(auto& d:s.doors)d.pos=d.pos+offset;   // door sight bounds stay block local
        s.spawn=s.spawn+offset;
        auto saved=doorStates_.find(ref->name+":"+std::to_string(ref->x)+":"+std::to_string(ref->z));if(saved!=doorStates_.end())for(size_t i=0;i<s.doors.size()&&i<saved->second.size();i++){s.doors[i].amount=saved->second[i].first;s.doors[i].target=saved->second[i].second;}
        for(auto& door:s.doors)refreshDoor(door);next.push_back({*ref,std::move(s)});
    }
    resident_=std::move(next);
    for(size_t i=0;i<fresh.size()&&i<resident_.size();i++)if(fresh[i])spawnBlockEnemies(resident_[i]);
    return !resident_.empty();
}
void Dungeon::tick(float dt){for(auto& r:resident_)updateDoors(r.scene,dt);}
static bool losTrampoline(void* ctx,const Vec3& a,const Vec3& b){return ((Dungeon*)ctx)->lineOfSight(a,b);}
static Vec3 moveTrampoline(void* ctx,const Vec3& feet,const Vec3& delta,float radius,float height){return ((Dungeon*)ctx)->move(feet,delta,radius,height);}
static float floorTrampoline(void* ctx,const Vec3& pos,float maxY){return ((Dungeon*)ctx)->floor(pos,maxY);}
int Dungeon::tickEnemies(float dt,const PlayerState& player,const Vec3& camForward){
    int damage=0;
    EnemyWorld w;
    w.player=player.pos;w.camForward=camForward;
    w.los=losTrampoline;w.move=moveTrampoline;w.floor=floorTrampoline;w.ctx=this;
    for(auto& e:enemies_){
        if(!residentAt(e.pos))continue;   // only simulate monsters near the player
        const EnemyStatic* st=findEnemyStatic(e.mobileId);
        if(!st)continue;
        damage+=updateEnemy(e,*st,w,rng_,dt);
        orientEnemy(e,player.pos,camForward);
    }
    return damage;
}
static void awardProgress(PlayerState& p,int enemyLevel,DfRandom& rng){
    p.xp+=enemyLevel*20;
    p.gold+=rng.range(enemyLevel*5,enemyLevel*25+1);
    // Simplified classic progression: a full bar levels the character up.
    while(p.xp>=p.level*100){
        p.xp-=p.level*100;
        p.level++;p.maxHp+=8;p.hp=p.maxHp;p.skill+=2;p.lockpick+=1;
    }
}
int Dungeon::playerAttack(PlayerState& player,const Vec3& camForward,float range){
    Vec3 eye=player.pos+Vec3{0,64.f,0};
    Enemy* best=nullptr;float bestScore=-1.f;
    for(auto& e:enemies_){
        if(e.state==EnemyDead||!residentAt(e.pos))continue;
        Vec3 to=(e.pos+Vec3{0,enemyEyeHeight(e)*0.7f,0})-eye;
        float dist=to.len();
        if(dist>range)continue;
        float score=to.norm().dot(camForward);
        if(score<0.6f)continue;
        if(!lineOfSight(eye,e.pos+Vec3{0,enemyEyeHeight(e),0}))continue;
        if(score>bestScore){bestScore=score;best=&e;}
    }
    if(!best)return 0;
    const EnemyStatic* st=findEnemyStatic(best->mobileId);
    if(!st)return 0;
    int dmg=playerSwingDamage(player.weaponMin,player.weaponMax,best->armor,rng_);
    best->hp-=dmg;
    best->aggro=true;best->passive=false;
    if(best->hp<=0){
        best->hp=0;best->state=EnemyDead;
        deadSet_.insert(best->key);deadKeys_.push_back(best->key);
        awardProgress(player,st->level,rng_);
    }else{
        best->state=EnemyHurt;best->hurtTimer=0.4f;best->animTimer=0.f;
    }
    return dmg;
}
bool Dungeon::lineOfSight(const Vec3& a,const Vec3& b)const{
    for(auto& r:resident_){
        Vec3 off{(float)r.ref.x*BLOCK_SIDE,0,(float)r.ref.z*BLOCK_SIDE};
        Vec3 la=a-off,lb=b-off;
        if(sceneBlocksSight(r.scene,la,lb))return false;
        for(auto& d:r.scene.doors)if(doorBlocksSegment(d,la,lb))return false;
    }
    return true;
}
void Dungeon::setEnemies(const std::vector<Enemy>& e,const std::vector<std::string>& dead,const std::vector<std::string>& spawned){
    enemies_=e;deadKeys_=dead;spawnedKeys_=spawned;
    deadSet_.clear();for(auto& k:deadKeys_)deadSet_.insert(k);
    spawnedSet_.clear();for(auto& k:spawnedKeys_)spawnedSet_.insert(k);
}
bool Dungeon::pickLock(PlayerState& player,Door& door){
    if(!door.locked)return true;
    float chance=0.10f+player.lockpick/200.f+player.level*0.01f;
    if(chance<0.05f)chance=0.05f;if(chance>0.8f)chance=0.8f;
    if(rng_.percent()<=(int)(chance*100.f)){door.locked=false;return true;}
    // A fumbled lock alerts monsters loitering near the door.
    for(auto& e:enemies_){
        if(e.state==EnemyDead)continue;
        if((e.pos-door.pos).len()<600.f){e.aggro=true;if(e.state==EnemyIdle)e.state=EnemyMove;}
    }
    return false;
}
Door* Dungeon::aimedDoor(const Vec3& eye,const Vec3& forward){Door* best=nullptr;float dist=1e9f;for(auto& r:resident_){Door* d=dfpsp::aimedDoor(r.scene,eye,forward);if(d&&(d->pos-eye).len()<dist){best=d;dist=(d->pos-eye).len();}}return best;}
Vec3 Dungeon::move(const Vec3& feet,const Vec3& delta,float radius,float height)const{
    Vec3 p=feet;int steps=std::max(1,(int)std::ceil(delta.len()/(radius*0.5f)));steps=std::min(steps,64);Vec3 step=delta*(1.f/steps);
    for(int i=0;i<steps;i++){Vec3 candidate=p+step;Vec3 result=candidate;
        for(auto& r:resident_)result=moveWithCollision(r.scene,result,Vec3{},radius,height);
        auto valid=[&](const Vec3& q){return refAt((int)std::floor(q.x/BLOCK_SIDE),(int)std::floor(q.z/BLOCK_SIDE))!=nullptr;};
        if(valid(result))p=result;
        else{
            Vec3 px{p.x+step.x,p.y,p.z};for(auto& r:resident_)px=moveWithCollision(r.scene,px,Vec3{},radius,height);
            Vec3 pz{p.x,p.y,p.z+step.z};for(auto& r:resident_)pz=moveWithCollision(r.scene,pz,Vec3{},radius,height);
            if(valid(px))p=px;else if(valid(pz))p=pz;
        }
    }
    return p;
}
float Dungeon::floor(const Vec3& p,float maxY)const{float y=-1e9f;for(auto& r:resident_)y=std::max(y,floorHeight(r.scene,p,maxY));return y;}
bool Dungeon::headroom(const Vec3& p,float h)const{for(auto& r:resident_)if(!hasHeadroom(r.scene,p,h))return false;return true;}
size_t Dungeon::triangleCount()const{size_t n=0;for(auto& r:resident_){n+=r.scene.tris.size();for(auto& d:r.scene.doors)n+=d.world.size();}return n;}
} // namespace dfpsp
