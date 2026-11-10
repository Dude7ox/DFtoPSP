#include "Font.h"
#include "Stream.h"
namespace dfpsp {
bool Font::load(const Vfs& vfs,const std::string& name){
    std::vector<u8> data;
    if(!vfs.readFile(name,data))return false;
    // Header plus the 240-entry table must fit before any glyph data is read.
    if(data.size()<4u+(size_t)kGlyphCount*4u)return false;
    Reader r(data);
    fw_=r.u16_();fh_=r.u16_();
    if(fw_<1||fw_>kCellSize||fh_<1||fh_>kCellSize)return false;
    u16 off[kGlyphCount];
    for(int i=0;i<kGlyphCount;i++){off[i]=r.u16_();widths_[i]=r.u16_();}
    for(int i=0;i<kGlyphCount;i++){
        size_t p=off[i];
        if(p+(size_t)kCellSize*2u>data.size())return false;
        if(widths_[i]>kCellSize)widths_[i]=kCellSize;
        for(int y=0;y<kCellSize;y++)rows_[i][y]=(u16)(data[p+(size_t)y*2]|((u16)data[p+(size_t)y*2+1]<<8));
    }
    ok_=true;return true;
}
int Font::glyphWidth(char c)const{
    if(!ok_)return 0;
    unsigned u=(unsigned char)c;
    if(u<(unsigned)kAsciiStart||u>=(unsigned)kAsciiStart+kGlyphCount)return fw_>1?fw_-1:0; // space and unsupported codes
    return widths_[u-kAsciiStart];
}
int Font::textWidth(const std::string& s)const{int w=0;for(char c:s)w+=advance(c);return w;}
const u16* Font::rows(char c)const{
    if(!ok_)return nullptr;
    unsigned u=(unsigned char)c;
    if(u<(unsigned)kAsciiStart||u>=(unsigned)kAsciiStart+kGlyphCount)return nullptr;
    return rows_[u-kAsciiStart];
}
bool Font::pixel(char c,int x,int y)const{
    const u16* r=rows(c);
    if(!r||x<0||x>=kCellSize||y<0||y>=fh_)return false;
    return (r[y]&(0x8000u>>x))!=0;
}
} // namespace
