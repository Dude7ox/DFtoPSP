#pragma once
#include "Vfs.h"
namespace dfpsp {
// Original FONT000?.FNT bitmap fonts (DFU-compatible parse):
//   header  u16 fixedWidth, u16 fixedHeight
//   table   240 * (u16 dataOffset, u16 pixelWidth)
//   data    32 bytes per glyph: 16 rows of 16-bit bitfields where byte[y*2+1]
//           is the left half (pixels 0..7) of row y and byte[y*2] the right.
// Glyph index = ascii - 33 ('!' is glyph 0); space (32) is synthesized with
// width fixedWidth-1. Classic advance includes 1 px of glyph spacing.
class Font {
public:
    static constexpr int kGlyphCount=240, kCellSize=16, kAsciiStart=33;
    bool load(const Vfs& vfs,const std::string& name);
    bool loaded()const{return ok_;}
    int fixedWidth()const{return fw_;}
    int fixedHeight()const{return fh_;}
    int glyphWidth(char c)const;             // ink cell width in pixels
    int advance(char c)const{return glyphWidth(c)+1;}
    int textWidth(const std::string& s)const;
    const u16* rows(char c)const;            // 16 bitfield rows; nullptr for space/unsupported
    bool pixel(char c,int x,int y)const;
private:
    bool ok_=false;int fw_=0,fh_=0;
    u16 widths_[kGlyphCount]{};
    u16 rows_[kGlyphCount][kCellSize]{};
};
} // namespace
