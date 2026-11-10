#include "ItemData.h"
#include "ItemDefs.h"
#include <cmath>
#include <cstring>
namespace dfpsp {
const char* const kWeaponMaterialNames[10]={"Iron","Steel","Silver","Elven","Dwarven",
    "Mithril","Adamantium","Ebony","Orcish","Daedric"};
const char* const kArmorMaterialNames[13]={"Leather","Chain","Chain","Iron","Steel",
    "Silver","Elven","Dwarven","Mithril","Adamantium","Ebony","Orcish","Daedric"};
static const short kValueMult[10]={1,2,4,8,16,32,64,128,256,512};
static const short kWeightMult[10]={4,5,4,4,3,4,4,2,4,5};
static const short kConditionMult[10]={4,6,6,8,12,16,20,24,28,32};
int armorMaterialFromWeapon(int wm){
    if(wm<0||wm>9)return AMatIron;
    return AMatIron+wm;
}
int bodyMorphology(int race){
    switch(race){
    case 8:return 0;                 // Argonian
    case 4:case 5:case 6:return 1;   // elves
    case 7:return 3;                 // Khajiit
    default:return 2;                // humans
    }
}
const ItemTemplate& Item::tpl()const{return itemTemplate(templateIndex);}
int Item::group()const{return itemGroupOf(templateIndex);}
std::string Item::displayName()const{
    const ItemTemplate& t=tpl();
    int g=group();
    const char* mat=0;
    if(g==3&&material>=0&&material<10)mat=kWeaponMaterialNames[material];
    else if(g==2){
        if(material==AMatLeather)mat="Leather";
        else if(material==AMatChain||material==AMatChain2)mat="Chain";
        else if(material>=AMatIron)mat=kWeaponMaterialNames[material-0x200];
    }
    std::string n;
    if(mat)n=mat;
    if(!n.empty())n+=" ";
    n+=t.name;
    return n;
}
static int roundQ(float kg){return (int)std::floor(kg*4.0f+0.5f);}
Item makeItem(int templateIndex){
    Item it;
    it.templateIndex=templateIndex;
    const ItemTemplate& t=itemTemplate(templateIndex);
    it.value=t.basePrice;
    it.weightQ=roundQ(t.weightKg);
    it.maxCondition=t.hitPoints;
    it.condition=t.hitPoints;
    it.playerArchive=t.playerArchive;
    it.playerRecord=t.playerRecord;
    return it;
}
static void applyMaterialProperties(Item& it,int wm){
    it.value*=3*kValueMult[wm];
    it.weightQ=(int)std::floor((it.weightQ*kWeightMult[wm])/4.0f+0.5f);
    it.maxCondition=it.maxCondition*kConditionMult[wm]/4;
    it.condition=it.maxCondition;
}
Item makeWeapon(int weaponIdx,int wm,bool female){
    Item it=makeItem(weaponIdx);
    if(weaponIdx==ItemDef::Weapons_Arrow){
        it.material=0;it.maxCondition=0;it.condition=0;
        return it;
    }
    if(wm<0||wm>9)wm=0;
    it.material=wm;
    applyMaterialProperties(it,wm);
    if(female&&it.playerArchive>0)it.playerArchive-=1;   // 234 -> 233 etc.
    return it;
}
Item makeArmor(int armorIdx,int am,int gender,int race,int variant){
    Item it=makeItem(armorIdx);
    it.material=am;
    if(am==AMatLeather){
        it.weightQ=(it.weightQ+1)/2;                     // round(quarters/2)
    }else if(am==AMatChain){
        it.value*=2;
    }else if(am>=AMatIron){
        applyMaterialProperties(it,am-0x200);
    }
    it.playerArchive=(gender==1?245:249)+bodyMorphology(race);
    if(variant<0)variant=0;
    const ItemTemplate& t=itemTemplate(armorIdx);
    if(t.variants>0){
        if(armorIdx==ItemDef::Armor_Cuirass){
            if(am==AMatLeather)variant=0;
            else if(am==AMatChain||am==AMatChain2)variant=4;
        }
        it.variant=variant%t.variants;
    }
    it.playerRecord=t.playerRecord+it.variant;
    return it;
}
Item makeClothing(int clothingIdx,int gender,int race,int variant){
    (void)gender;
    Item it=makeItem(clothingIdx);
    const ItemTemplate& t=itemTemplate(clothingIdx);
    if(t.playerArchive>0)it.playerArchive=t.playerArchive+bodyMorphology(race);
    if(variant<0)variant=0;
    if(t.variants>0)it.variant=variant%t.variants;
    it.playerRecord=t.playerRecord+it.variant;
    return it;
}
bool isCloakIndex(int i){
    return i==ItemDef::MensClothing_Casual_cloak||i==ItemDef::MensClothing_Formal_cloak||
           i==ItemDef::WomensClothing_Casual_cloak||i==ItemDef::WomensClothing_Formal_cloak;
}
int naturalSlot(const Item& it){
    int g=it.group();
    int i=it.templateIndex;
    if(g==3)return SlotRightHand;                        // weapons (bows included)
    if(g==2){
        switch(i){
        case ItemDef::Armor_Cuirass:return SlotChestArmor;
        case ItemDef::Armor_Gauntlets:return SlotGloves;
        case ItemDef::Armor_Greaves:return SlotLegsArmor;
        case ItemDef::Armor_Left_Pauldron:return SlotLeftArm;
        case ItemDef::Armor_Right_Pauldron:return SlotRightArm;
        case ItemDef::Armor_Helm:return SlotHead;
        case ItemDef::Armor_Boots:return SlotFeet;
        default:return SlotLeftHand;                     // shields
        }
    }
    if(g==25){
        switch(i){
        case ItemDef::Jewellery_Amulet:case ItemDef::Jewellery_Torc:
        case ItemDef::Jewellery_Cloth_amulet:return SlotAmulet0;
        case ItemDef::Jewellery_Bracer:return SlotBracer0;
        case ItemDef::Jewellery_Ring:return SlotRing0;
        case ItemDef::Jewellery_Bracelet:return SlotBracelet0;
        case ItemDef::Jewellery_Mark:return SlotMark0;
        default:return SlotNone;
        }
    }
    if(g==6||g==12){
        // torso
        switch(i){
        case ItemDef::MensClothing_Straps:case ItemDef::MensClothing_Armbands:case ItemDef::MensClothing_Kimono:
        case ItemDef::MensClothing_Fancy_Armbands:case ItemDef::MensClothing_Sash:case ItemDef::MensClothing_Eodoric:
        case ItemDef::MensClothing_Dwynnen_surcoat:case ItemDef::MensClothing_Short_tunic:
        case ItemDef::MensClothing_Formal_tunic:case ItemDef::MensClothing_Toga:
        case ItemDef::MensClothing_Reversible_tunic:case ItemDef::MensClothing_Plain_robes:
        case ItemDef::MensClothing_Priest_robes:case ItemDef::MensClothing_Short_shirt:
        case ItemDef::MensClothing_Short_shirt_with_belt:case ItemDef::MensClothing_Long_shirt:
        case ItemDef::MensClothing_Long_shirt_with_belt:case ItemDef::MensClothing_Short_shirt_closed_top:
        case ItemDef::MensClothing_Long_shirt_closed_top:case ItemDef::MensClothing_Open_Tunic:
        case ItemDef::MensClothing_Vest:
        case ItemDef::WomensClothing_Brassier:case ItemDef::WomensClothing_Formal_brassier:
        case ItemDef::WomensClothing_Peasant_blouse:case ItemDef::WomensClothing_Eodoric:
        case ItemDef::WomensClothing_Formal_eodoric:case ItemDef::WomensClothing_Evening_gown:
        case ItemDef::WomensClothing_Day_gown:case ItemDef::WomensClothing_Casual_dress:
        case ItemDef::WomensClothing_Strapless_dress:case ItemDef::WomensClothing_Plain_robes:
        case ItemDef::WomensClothing_Priestess_robes:case ItemDef::WomensClothing_Short_shirt:
        case ItemDef::WomensClothing_Short_shirt_belt:case ItemDef::WomensClothing_Long_shirt:
        case ItemDef::WomensClothing_Long_shirt_belt:case ItemDef::WomensClothing_Short_shirt_closed:
        case ItemDef::WomensClothing_Short_shirt_closed_belt:case ItemDef::WomensClothing_Long_shirt_closed:
        case ItemDef::WomensClothing_Long_shirt_closed_belt:case ItemDef::WomensClothing_Open_tunic:
        case ItemDef::WomensClothing_Vest:
            return SlotChestClothes;
        case ItemDef::MensClothing_Shoes:case ItemDef::MensClothing_Tall_Boots:case ItemDef::MensClothing_Boots:
        case ItemDef::MensClothing_Sandals:
        case ItemDef::WomensClothing_Shoes:case ItemDef::WomensClothing_Tall_boots:
        case ItemDef::WomensClothing_Boots:case ItemDef::WomensClothing_Sandals:
            return SlotFeet;
        case ItemDef::MensClothing_Casual_pants:case ItemDef::MensClothing_Breeches:
        case ItemDef::MensClothing_Short_skirt:case ItemDef::MensClothing_Khajiit_suit:
        case ItemDef::MensClothing_Loincloth:case ItemDef::MensClothing_Wrap:case ItemDef::MensClothing_Long_Skirt:
        case ItemDef::WomensClothing_Casual_pants:case ItemDef::WomensClothing_Khajiit_suit:
        case ItemDef::WomensClothing_Loincloth:case ItemDef::WomensClothing_Wrap:
        case ItemDef::WomensClothing_Long_skirt:case ItemDef::WomensClothing_Tights:
            return SlotLegsClothes;
        default:
            if(isCloakIndex(i))return SlotCloak1;
            return SlotNone;
        }
    }
    return SlotNone;
}
bool isUpperClothingIndex(int i){
    Item probe=makeItem(i);
    int s=naturalSlot(probe);
    return s==SlotChestClothes||s==SlotChestArmor;
}
bool isLowerClothingIndex(int i){
    Item probe=makeItem(i);
    return naturalSlot(probe)==SlotLegsClothes;
}
bool isPaperDollDrawable(const Item& it){
    int g=it.group();
    if(g==2||g==3||g==6||g==12)return true;
    if(g==25&&it.slot>11)return true;                    // jewellery shows when worn
    return false;
}
void resolveItemArt(Item& it,int gender,int race){
    const ItemTemplate& t=it.tpl();
    int g=it.group();
    if(g==3){
        it.playerArchive=t.playerArchive>0?(t.playerArchive-(gender==1?1:0)):t.playerArchive;
        it.playerRecord=t.playerRecord+it.variant;
    }else if(g==2){
        it.playerArchive=(gender==1?245:249)+bodyMorphology(race);
        it.playerRecord=t.playerRecord+it.variant;
    }else if(g==6||g==12){
        it.playerArchive=t.playerArchive>0?t.playerArchive+bodyMorphology(race):t.playerArchive;
        it.playerRecord=t.playerRecord+it.variant;
    }else{
        it.playerArchive=t.playerArchive;
        it.playerRecord=t.playerRecord;
    }
}
int iconArchive(const Item& it){
    const ItemTemplate& t=it.tpl();
    if(t.playerArchive==0)return t.worldArchive;
    return it.playerArchive;
}
bool weaponHandsEither(int i){
    switch(i){
    case ItemDef::Weapons_Dagger:case ItemDef::Weapons_Tanto:
    case ItemDef::Weapons_Shortsword:case ItemDef::Weapons_Wakazashi:
    case ItemDef::Weapons_Broadsword:case ItemDef::Weapons_Saber:
    case ItemDef::Weapons_Longsword:case ItemDef::Weapons_Katana:
    case ItemDef::Weapons_Battle_Axe:case ItemDef::Weapons_Mace:
        return true;
    default:return false;
    }
}
int iconRecord(const Item& it,int equipSlotForHands){
    const ItemTemplate& t=it.tpl();
    int rec=t.playerArchive==0?t.worldRecord:t.playerRecord;
    if(t.variants>0)rec+=it.variant;
    if(it.group()==3){
        if(it.templateIndex==ItemDef::Weapons_Katana)rec+=1;
        else if(equipSlotForHands==SlotRightHand&&weaponHandsEither(it.templateIndex))rec+=1;
    }
    return rec;
}
} // namespace dfpsp
