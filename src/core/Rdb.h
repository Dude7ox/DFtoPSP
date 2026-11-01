#pragma once
#include "Bsa.h"
#include <functional>
namespace dfpsp {
struct RdbModelRef { u32 id=0; std::string desc; };
struct RdbAction { int axis=0,duration=0,magnitude=0,next=-1,flags=0; };
struct RdbObject {
    int type=0; Vec3 pos; int modelIndex=-1; Vec3 rot; int trigger=0; RdbAction action;
    int texArchive=0,texRecord=0,flatFlags=0,magnitude=0,faction=0,flatAction=0;
    int lightRadius=0;
};
struct RdbBlock {
    int version=0,width=0,height=0;
    std::vector<RdbModelRef> refs;
    std::vector<RdbObject> objects;
};
bool loadRdb(const Bsa& blocks,const std::string& name,RdbBlock& out);
} // namespace
