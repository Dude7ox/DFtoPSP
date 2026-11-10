#pragma once
#include "Types.h"
#include <string>
#include <vector>
namespace dfpsp {
// Classic item template: the definitions shipped inside the original
// executable (prices, weights, icon records). Generated into ItemDataGen.cpp
// by tools/gen_item_data.py from the original table.
struct ItemTemplate {
    const char* name;
    float weightKg;
    int hitPoints,capacity,basePrice,enchantPoints,rarity,variants,drawOrder,flags;
    int worldArchive,worldRecord,playerArchive,playerRecord;
    bool blunt()const{return (flags&1)!=0;}
    bool liquid()const{return (flags&2)!=0;}
    bool oneHanded()const{return (flags&4)!=0;}
    bool ingredient()const{return (flags&8)!=0;}
};
extern const int kTemplateCount;
constexpr int kGroupCount=29;
const ItemTemplate& itemTemplate(int i);
int itemGroupOf(int idx);                 // ItemGroups value, -1 when ungrouped
int itemOrdinalInGroup(int idx);          // position inside the group's enum
int itemTemplateOfOrdinal(int group,int ordinal);
const char* itemGroupName(int g);
// Materials, classic enum values.
enum WeaponMaterial { WMatIron=0,WMatSteel,WMatSilver,WMatElven,WMatDwarven,
                      WMatMithril,WMatAdamantium,WMatEbony,WMatOrcish,WMatDaedric };
enum ArmorMaterial { AMatLeather=0,AMatChain=0x100,AMatChain2=0x103,AMatIron=0x200,
                     AMatSteel=0x201,AMatSilver=0x202,AMatElven=0x203,AMatDwarven=0x204,
                     AMatMithril=0x205,AMatAdamantium=0x206,AMatEbony=0x207,
                     AMatOrcish=0x208,AMatDaedric=0x209 };
extern const char* const kWeaponMaterialNames[10];
extern const char* const kArmorMaterialNames[13];
int armorMaterialFromWeapon(int wm);      // biography IT treats both alike
// Classic EquipSlots.
enum EquipSlot { SlotNone=-1,SlotAmulet0=0,SlotAmulet1,SlotBracelet0,SlotBracelet1,
    SlotRing0,SlotRing1,SlotBracer0,SlotBracer1,SlotMark0,SlotMark1,SlotCrystal0,
    SlotCrystal1,SlotHead,SlotRightArm,SlotCloak1,SlotLeftArm,SlotCloak2,
    SlotChestClothes,SlotChestArmor,SlotRightHand,SlotGloves,SlotLeftHand,
    SlotUnknown1,SlotLegsArmor,SlotLegsClothes,SlotUnknown2,SlotFeet };
// A concrete item instance (inventory entry).
struct Item {
    int templateIndex=-1;
    int stack=1;
    int value=0;                          // gold per unit
    int weightQ=0;                        // quarter kilograms
    int material=-1;                      // WeaponMaterial / ArmorMaterial value
    int maxCondition=0,condition=0;
    int variant=0,dye=0;
    int slot=SlotNone;                    // equipped slot, SlotNone when carried
    int playerArchive=0,playerRecord=0;   // resolved paper-doll / icon art
    const ItemTemplate& tpl()const;
    std::string displayName()const;       // "Steel Broadsword" etc.
    int group()const;
};
// Builders following the classic rules (value/weight/condition by material).
Item makeItem(int templateIndex);
Item makeWeapon(int weaponIdx,int weaponMaterial,bool female);
Item makeArmor(int armorIdx,int armorMaterial,int gender,int race,int variant);
Item makeClothing(int clothingIdx,int gender,int race,int variant);
// Slot and art resolution.
int naturalSlot(const Item& it);
bool isUpperClothingIndex(int templateIndex);    // covers the torso
bool isLowerClothingIndex(int templateIndex);    // covers the legs
bool isPaperDollDrawable(const Item& it);        // drawn over the body
bool isCloakIndex(int templateIndex);
void resolveItemArt(Item& it,int gender,int race);
// Inventory icon: TEXTURE archive/record, variant and handedness applied.
int iconArchive(const Item& it);
int iconRecord(const Item& it,int equipSlotForHands);
// Body morphology archive offset per race (Argonian 0, Elf 1, Human 2, Khajiit 3).
int bodyMorphology(int race);
bool weaponHandsEither(int templateIndex);   // classic ItemHands.Either set
} // namespace dfpsp
