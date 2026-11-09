#pragma once
#include "Bsa.h"
namespace dfpsp {
struct DungeonBlockRef { int x=0,z=0; std::string name; bool start=false; };
struct DungeonLayout {
    int region=0, exteriorId=0, interiorId=0;
    int dungeonType=255;   // DFRegion.DungeonTypes; seeds the encounter table
    std::string name; std::vector<DungeonBlockRef> blocks;
};
bool loadDungeonLayout(const Bsa& maps,int region,int exteriorId,DungeonLayout& out);
bool findDungeonByName(const Bsa& maps,const std::string& needle,DungeonLayout& out);
} // namespace
