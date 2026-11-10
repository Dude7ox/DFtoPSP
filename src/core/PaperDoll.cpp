#include "PaperDoll.h"
#include "Texture.h"
#include <algorithm>
namespace dfpsp {
static const int kDollOriginX=200,kDollOriginY=8;   // classic paper-doll window origin
static const int kWaistHeight=40;
bool PaperDoll::load(const Vfs& vfs,int race,int gender){
    race_=race;gender_=gender;vfs_=&vfs;
    const RaceInfo* r=raceInfo(race);
    texCache_.clear();texKeys_.clear();layout_.clear();
    if(!bg_.open(vfs,r->paperDollBackground))return false;
    if(!bodyNude_.open(vfs,gender==1?r->bodyFemaleNude:r->bodyMaleNude))return false;
    if(!bodyClothed_.open(vfs,gender==1?r->bodyFemaleClothed:r->bodyMaleClothed))return false;
    if(!heads_.open(vfs,gender==1?r->headsFemale:r->headsMale))return false;
    return true;
}
int PaperDoll::texture(int a,int r,const Palette& pal)const{
    u64 key=u64(u32(a))<<32|u32(r);
    for(size_t i=0;i<texKeys_.size();i++)if(texKeys_[i]==key)return (int)i;
    Texture t;
    if(!loadTexture(*vfs_,a,r,pal,t))return -1;
    texKeys_.push_back(key);
    texCache_.push_back(t);
    return (int)texCache_.size()-1;
}
static void blitTexture(Framebuffer& fb,int dx,int dy,const Texture& t,int srcY0,int srcH){
    for(int y=0;y<srcH;y++){
        int fy=dy+y;
        if(fy<0||fy>=fb.h)continue;
        int sy=srcY0+y;
        if(sy<0||sy>=t.h)continue;
        for(int x=0;x<t.w;x++){
            int fx=dx+x;
            if(fx<0||fx>=fb.w)continue;
            u8 a=t.rgba[((size_t)sy*t.w+x)*4+3];
            if(!a)continue;
            fb.color[(size_t)fy*fb.w+fx]=rgba(t.rgba[((size_t)sy*t.w+x)*4],
                                              t.rgba[((size_t)sy*t.w+x)*4+1],
                                              t.rgba[((size_t)sy*t.w+x)*4+2]);
        }
    }
}
void PaperDoll::draw(Framebuffer& fb,int ox,int oy,const Inventory& inv,int faceIndex,const Palette& pal)const{
    layout_.clear();
    if(bg_.count()&&bg_.decode(0))
        imgBlit(fb.color.data(),fb.w,fb.h,ox-8,oy-7,bg_.header(0),pal,false);
    Texture nude;
    if(bodyNude_.count()&&bodyNude_.decode(0)){
        imgToTexture(bodyNude_.header(0),pal,nude,true);
        blitTexture(fb,ox+bodyNude_.header(0).xoff-kDollOriginX,
                    oy+bodyNude_.header(0).yoff-kDollOriginY,nude,0,nude.h);
    }
    Texture clothed;
    bool haveClothed=bodyClothed_.count()&&bodyClothed_.decode(0);
    if(haveClothed)imgToTexture(bodyClothed_.header(0),pal,clothed,true);
    if(haveClothed&&!inv.isUpperClothed())
        blitTexture(fb,ox+bodyClothed_.header(0).xoff-kDollOriginX,
                    oy+bodyClothed_.header(0).yoff-kDollOriginY,clothed,0,kWaistHeight);
    if(haveClothed&&!inv.isLowerClothed())
        blitTexture(fb,ox+bodyClothed_.header(0).xoff-kDollOriginX,
                    oy+bodyClothed_.header(0).yoff-kDollOriginY+kWaistHeight,
                    clothed,kWaistHeight,clothed.h-kWaistHeight);
    if(heads_.count()>(size_t)faceIndex&&heads_.decode((size_t)faceIndex)){
        Texture head;
        imgToTexture(heads_.header((size_t)faceIndex),pal,head,true);
        blitTexture(fb,ox+heads_.header((size_t)faceIndex).xoff-kDollOriginX,
                    oy+heads_.header((size_t)faceIndex).yoff-kDollOriginY,head,0,head.h);
    }
    // Cloak interiors: Cloak2 first, then Cloak1 (classic order).
    for(int s=SlotCloak2;s>=SlotCloak1;s--){
        const Item* cl=inv.atSlot(s);
        if(!cl)continue;
        TextureRecord rec;
        if(!loadTextureRecord(*vfs_,cl->playerArchive,cl->playerRecord+1,rec))continue;
        Texture t;
        if(!decodeTextureFrame(rec,0,pal,t))continue;
        blitTexture(fb,ox+rec.offsetX-kDollOriginX,oy+rec.offsetY-kDollOriginY,t,0,t.h);
    }
    // Worn items in classic draw order.
    std::vector<const Item*> worn;
    for(size_t i=0;i<inv.items.size();i++){
        const Item& it=inv.items[i];
        if(it.slot==SlotNone||!isPaperDollDrawable(it))continue;
        if(isCloakIndex(it.templateIndex))continue;
        worn.push_back(&it);
    }
    std::stable_sort(worn.begin(),worn.end(),
        [](const Item* a,const Item* b){return a->tpl().drawOrder<b->tpl().drawOrder;});
    for(size_t i=0;i<worn.size();i++){
        const Item* it=worn[i];
        TextureRecord rec;
        if(!loadTextureRecord(*vfs_,it->playerArchive,it->playerRecord,rec))continue;
        Texture t;
        if(!decodeTextureFrame(rec,0,pal,t))continue;
        int dx=ox+rec.offsetX-kDollOriginX,dy=oy+rec.offsetY-kDollOriginY;
        blitTexture(fb,dx,dy,t,0,t.h);
        Drawn d;
        d.slot=it->slot;d.x=dx;d.y=dy;d.w=t.w;d.h=t.h;d.tex=texture(it->playerArchive,it->playerRecord,pal);
        layout_.push_back(d);
    }
}
int PaperDoll::equipIndexAt(int x,int y)const{
    for(int i=(int)layout_.size()-1;i>=0;i--){
        const Drawn& d=layout_[(size_t)i];
        if(x<d.x||y<d.y||x>=d.x+d.w||y>=d.y+d.h)continue;
        if(d.tex<0)continue;
        const Texture& t=texCache_[(size_t)d.tex];
        int px=x-d.x,py=y-d.y;
        if(px<0||py<0||px>=t.w||py>=t.h)continue;
        if(t.rgba[((size_t)py*t.w+px)*4+3]!=0)return d.slot;
    }
    return SlotNone;
}
} // namespace dfpsp
