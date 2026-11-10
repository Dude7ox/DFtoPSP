#pragma once
#include "Vfs.h"
#include "Texture.h"
namespace dfpsp {
// Classic loose-image containers: single-image *.IMG, multi-record *.CIF and the
// fixed-size *.RCI layout (FACES.CIF is an RCI in disguise). All three hold 8-bit
// indices into ART_PAL.COL, except the handful of files that carry their own
// palette right after the pixels.
struct ImgRecord {
    int w=0,h=0;
    i16 xoff=0,yoff=0;      // classic draw offset; the paper doll shifts these back
    u16 compression=0;      // 0 raw, 2 RLE, 0x0108/0x1108 animated CIF (unsupported)
    u32 dataOffset=0;       // pixel data inside the file
    u32 dataLength=0;
    std::vector<u8> idx;    // decoded w*h indices; empty until decode()
    bool valid=false;
};
class ImgFile {
public:
    // Detects the container from the file name and length, then parses every
    // record header. Pixels stay compressed until decode() is called.
    bool open(const Vfs& vfs, const std::string& name);
    size_t count()const{return recs_.size();}
    bool has(size_t i)const{return i<recs_.size();}
    const ImgRecord& header(size_t i)const{return recs_[i];}
    // Decodes record i in place; returns false for unsupported encodings.
    bool decode(size_t i)const;
    const ImgRecord* decoded(size_t i)const;   // nullptr when not decoded yet
    const std::string& name()const{return name_;}
    size_t fileSize()const{return data_.size();}
    // True when the file carries its own palette after the pixels (already
    // scaled by 4, which is what the original values need to look right).
    bool embeddedPalette()const{return embeddedPal_;}
    const Palette& embedded()const{return pal_;}
    // Name of the palette the classic engine uses for this file.
    std::string paletteName()const;
    bool loadPalette(const Vfs& vfs, Palette& out)const;
private:
    bool parseImg();
    bool parseCif();
    bool parseRci(int w,int h);
    bool headerlessSize(int& w,int& h)const;
    std::string name_;
    std::vector<u8> data_;
    mutable std::vector<ImgRecord> recs_;
    bool embeddedPal_=false, rci_=false;
    Palette pal_;
};
// Flattens a record into a Texture. `transparent` makes index 0 see-through,
// which is what character/weapon/item art wants; full-screen backgrounds are
// drawn opaque instead.
bool imgToTexture(const ImgRecord& rec,const Palette& pal,Texture& out,bool transparent);
// Blits a record into an RGBA framebuffer (host screenshots, PSP staging).
void imgBlit(u32* dst,int dstW,int dstH,int dx,int dy,const ImgRecord& rec,const Palette& pal,bool transparent);
} // namespace dfpsp
