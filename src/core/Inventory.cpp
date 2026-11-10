#include "Inventory.h"
#include "ItemDefs.h"
namespace dfpsp {
int Inventory::add(const Item& it){
    if(it.templateIndex==ItemDef::Weapons_Arrow){
        for(size_t i=0;i<items.size();i++){
            if(items[i].templateIndex==it.templateIndex&&items[i].material==it.material&&items[i].stack<99){
                items[i].stack+=it.stack;
                return (int)i;
            }
        }
    }
    items.push_back(it);
    return (int)items.size()-1;
}
static int twinSlots(int slot,int& a,int& b){
    switch(slot){
    case SlotAmulet0:a=SlotAmulet0;b=SlotAmulet1;return 2;
    case SlotBracelet0:a=SlotBracelet0;b=SlotBracelet1;return 2;
    case SlotRing0:a=SlotRing0;b=SlotRing1;return 2;
    case SlotBracer0:a=SlotBracer0;b=SlotBracer1;return 2;
    case SlotMark0:a=SlotMark0;b=SlotMark1;return 2;
    case SlotCrystal0:a=SlotCrystal0;b=SlotCrystal1;return 2;
    case SlotCloak1:a=SlotCloak2;b=SlotCloak1;return 2;   // classic fills Cloak2 first
    default:a=slot;b=slot;return 1;
    }
}
bool Inventory::equip(int itemIndex){
    if(itemIndex<0||itemIndex>=(int)items.size())return false;
    Item& it=items[(size_t)itemIndex];
    int base=naturalSlot(it);
    if(base==SlotNone)return false;
    int a,b,n=twinSlots(base,a,b);
    if(base==SlotRightHand){
        // either-hand weapons try the right hand first, two-handed only right
        if(weaponHandsEither(it.templateIndex)){a=SlotRightHand;b=SlotLeftHand;n=2;}
        else{a=SlotRightHand;b=SlotRightHand;n=1;}
    }
    int target=SlotNone;
    for(int i=0;i<n;i++){
        int s=i==0?a:b;
        if(!atSlot(s)){target=s;break;}
    }
    if(target==SlotNone)target=a;             // swap out the occupant
    if(Item* occ=atSlot(target))occ->slot=SlotNone;
    it.slot=target;
    return true;
}
void Inventory::unequip(int itemIndex){
    if(itemIndex>=0&&itemIndex<(int)items.size())items[(size_t)itemIndex].slot=SlotNone;
}
Item* Inventory::atSlot(int slot){
    for(size_t i=0;i<items.size();i++)if(items[i].slot==slot)return &items[i];
    return 0;
}
const Item* Inventory::atSlot(int slot)const{
    for(size_t i=0;i<items.size();i++)if(items[i].slot==slot)return &items[i];
    return 0;
}
bool Inventory::isUpperClothed()const{return atSlot(SlotChestClothes)||atSlot(SlotChestArmor);}
bool Inventory::isLowerClothed()const{return atSlot(SlotLegsClothes)!=0;}
int Inventory::weightQ()const{
    int w=0;
    for(size_t i=0;i<items.size();i++)w+=items[i].weightQ*items[i].stack;
    return w;
}
void assignStartingGear(Inventory& inv,int classIndex,bool custom,int gender,int race,DfRandom& rng){
    inv.add(makeItem(ItemDef::MiscItems_Spellbook));
    Item shirt=clothingStarter(gender,race,true,rng);
    Item pants=clothingStarter(gender,race,false,rng);
    inv.add(shirt);inv.add(pants);
    inv.equip((int)inv.items.size()-2);
    inv.equip((int)inv.items.size()-1);
    static const int weaponByClass[18]={
        ItemDef::Weapons_Shortsword,ItemDef::Weapons_Saber,ItemDef::Weapons_Saber,
        ItemDef::Weapons_Shortsword,ItemDef::Weapons_Mace,ItemDef::Weapons_Shortsword,
        ItemDef::Weapons_Shortsword,ItemDef::Weapons_Tanto,ItemDef::Weapons_Saber,
        ItemDef::Weapons_Shortsword,ItemDef::Weapons_Shortsword,ItemDef::Weapons_Longsword,
        ItemDef::Weapons_Staff,ItemDef::Weapons_Long_Bow,ItemDef::Weapons_Battle_Axe,
        ItemDef::Weapons_Warhammer,ItemDef::Weapons_Broadsword,ItemDef::Weapons_Longsword};
    static const int materialByClass[18]={1,0,0,1,0,0,0,1,0,0,0,0,1,0,1,0,0,0};
    if(custom){
        inv.add(makeWeapon(ItemDef::Weapons_Longsword,WMatIron,gender==1));
        return;
    }
    int ci=classIndex<0?0:(classIndex>17?17:classIndex);
    inv.add(makeWeapon(weaponByClass[ci],materialByClass[ci],gender==1));
    if(ci==13){
        inv.add(makeWeapon(ItemDef::Weapons_Battle_Axe,WMatSteel,gender==1));
        Item arrows=makeWeapon(ItemDef::Weapons_Arrow,WMatIron,gender==1);
        arrows.stack=24;
        inv.add(arrows);
    }
}
Item clothingStarter(int gender,int race,bool shirt,DfRandom& rng){
    int idx;
    if(gender==1)idx=shirt?ItemDef::WomensClothing_Short_shirt_closed:ItemDef::WomensClothing_Casual_pants;
    else idx=shirt?ItemDef::MensClothing_Short_shirt:ItemDef::MensClothing_Casual_pants;
    int variant=shirt?0:rng.range(8);
    return makeClothing(idx,gender,race,variant);
}
void grantBiogItems(Inventory& inv,const BiogEffects& b,int gender,int race,DfRandom& rng){
    (void)rng;
    for(size_t i=0;i<b.items.size();i++){
        const BiogEffects::ItemGrant& g=b.items[i];
        if(g.group==3){
            int t=itemTemplateOfOrdinal(3,g.index);
            if(t>=0)inv.add(makeWeapon(t,g.material,gender==1));
        }else if(g.group==2){
            int t=itemTemplateOfOrdinal(2,g.index);
            if(t>=0)inv.add(makeArmor(t,armorMaterialFromWeapon(g.material),gender,race,0));
        }else if(g.group==7){
            inv.add(makeItem(277));
        }else{
            int t=itemTemplateOfOrdinal(g.group,g.index);
            if(t>=0)inv.add(makeItem(t));
        }
    }
}
} // namespace dfpsp
