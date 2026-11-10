#pragma once
#include "ItemData.h"
#include "CharGen.h"
#include "Random.h"
#include <vector>
namespace dfpsp {
// Carried and worn items. Equip slots follow the classic ItemEquipTable so a
// save from the port maps one-to-one onto the original slot layout.
class Inventory {
public:
    std::vector<Item> items;
    int gold=0;
    int add(const Item& it);              // stacks arrows, returns item index
    bool equip(int itemIndex);            // swaps whatever occupies the slot
    void unequip(int itemIndex);
    Item* atSlot(int slot);
    const Item* atSlot(int slot)const;
    bool isUpperClothed()const;           // torso covered (clothes or cuirass)
    bool isLowerClothed()const;           // legs covered by clothing
    int weightQ()const;                   // carried weight, quarter kilograms
    int count()const{return (int)items.size();}
};
// Classic starting kit: spellbook, gender clothing (equipped) and the class
// weapon, plus the archer's axe and arrows (DFU ItemHelper.AssignStartingGear).
void assignStartingGear(Inventory& inv,int classIndex,bool custom,int gender,int race,DfRandom& rng);
// Biography IT grants.
void grantBiogItems(Inventory& inv,const BiogEffects& b,int gender,int race,DfRandom& rng);
// Gender clothing of a fresh character (short shirt + casual pants).
Item clothingStarter(int gender,int race,bool shirt,DfRandom& rng);
} // namespace dfpsp
