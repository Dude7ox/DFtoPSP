#pragma once
#include "ImgFile.h"
#include "Inventory.h"
#include "SoftwareRenderer.h"
#include <vector>
namespace dfpsp {
// The classic character doll: race background, nude body with clothed halves,
// race head and every worn item blitted in draw order at its classic offset.
// Art comes straight from the original IMG/CIF files and TEXTURE archives.
class PaperDoll {
public:
    bool load(const Vfs& vfs,int race,int gender);
    int width()const{return 110;}
    int height()const{return 184;}
    void draw(Framebuffer& fb,int ox,int oy,const Inventory& inv,int faceIndex,const Palette& pal)const;
    // Classic click test: slot of the worn item under a point in doll space,
    // SlotNone when the point hits nothing.
    int equipIndexAt(int x,int y)const;
    int race()const{return race_;}
    int gender()const{return gender_;}
private:
    struct Drawn { int slot,x,y,w,h; int tex; };   // tex = index into texCache_
    ImgFile bg_,bodyNude_,bodyClothed_,heads_;
    const Vfs* vfs_=0;
    int race_=1,gender_=0;
    mutable std::vector<Texture> texCache_;
    mutable std::vector<u64> texKeys_;
    mutable std::vector<Drawn> layout_;
    int texture(int archive,int record,const Palette& pal)const;
};
} // namespace dfpsp
