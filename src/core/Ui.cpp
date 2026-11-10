#include "Ui.h"
#include <cstdio>
namespace dfpsp {
void MsgLog::push(const std::string& s){
    lines_.push_back(s);
    if(lines_.size()>kMax)lines_.erase(lines_.begin());
}
std::vector<std::string> charSheetLines(const PlayerState& p,const std::string& place){
    char line[64];std::vector<std::string> out;
    snprintf(line,sizeof(line),"Level %d  XP %d/%d",p.level,p.xp,p.level*100);out.push_back(line);
    snprintf(line,sizeof(line),"HP %d/%d  Armor %d",p.hp>0?p.hp:0,p.maxHp,p.armor);out.push_back(line);
    snprintf(line,sizeof(line),"Weapon skill %d%%",p.skill);out.push_back(line);
    snprintf(line,sizeof(line),"Lockpicking %d%%",p.lockpick);out.push_back(line);
    snprintf(line,sizeof(line),"Gold %d  Kills %d",p.gold,p.kills);out.push_back(line);
    if(!place.empty())out.push_back(place);
    return out;
}
std::vector<std::string> helpLines(){
    return {
        "Stick move    DPad look",
        "X door/pick   Square attack",
        "O run         Triangle noclip",
        "L/R noclip up and down",
        "Start save    Select window",
        "Home exit",
    };
}
} // namespace
