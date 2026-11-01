#include "Texture.h"
#include "Stream.h"
#include <cstdio>
#include <cstring>
namespace dfpsp {
bool loadPalette(const Vfs& vfs,const std::string& name,Palette& pal){
    std::vector<u8> b; if(!vfs.readFile(name,b))return false;
    size_t off=0; if(b.size()==776)off=8; else if(b.size()!=768)return false;
    for(int i=0;i<256;i++){u8 r=b[off+i*3],g=b[off+i*3+1],bl=b[off+i*3+2]; pal.c[i]=rgba(r*4,g*4,bl*4);}
    return true;
}
static bool decodeRle(Reader& r,size_t recPos,int w,int h,int frame,std::vector<u8>& idx){
    idx.assign(w*h,0); r.seek(recPos+(size_t)h*frame*4);
    struct Row{int off;bool rle;}; std::vector<Row> rows(h);
    for(int y=0;y<h;y++){rows[y].off=r.i16_(); rows[y].rle=(r.u16_()&0x8000)!=0;}
    size_t dst=0;
    for(int y=0;y<h;y++){
        r.seek(recPos+rows[y].off);
        if(!rows[y].rle){ for(int x=0;x<w;x++){ if(dst>=idx.size())return false; idx[dst++]=r.u8_(); } }
        else { int rowWidth=r.u16_(); int rowPos=0; while(rowPos<rowWidth){ int probe=r.i16_(); if(probe<0){int n=-probe;u8 p=r.u8_();for(int i=0;i<n;i++){if(dst>=idx.size())return false;idx[dst++]=p;}rowPos+=n;} else if(probe>0){for(int i=0;i<probe;i++){if(dst>=idx.size())return false;idx[dst++]=r.u8_();}rowPos+=probe;} else break; } }
    }
    return true;
}
bool loadTexture(const Vfs& vfs,int archive,int record,const Palette& pal,Texture& out,int frame){
    char name[32]; snprintf(name,sizeof(name),"TEXTURE.%03d",archive);
    std::vector<u8> b; if(!vfs.readFile(name,b))return false;
    Reader r(b); int count=r.i16_(); if(record<0||record>=count)return false;
    bool solidA=(archive==0), solidB=(archive==1);
    if(solidA||solidB){out.w=out.h=32;out.rgba.resize(32*32*4);u8 ci=(u8)(solidB?128+record:record);u32 c=pal.c[ci];for(int i=0;i<32*32;i++){out.rgba[i*4]=getr(c);out.rgba[i*4+1]=getg(c);out.rgba[i*4+2]=getb(c);out.rgba[i*4+3]=255;}return true;}
    // Record header table: 26-byte file header, then 20-byte entries.
    r.seek(26+(size_t)record*20); r.i16_(); u32 recPos=r.u32_(); r.skip(2+4+8);
    if(recPos+28>b.size())return false;
    r.seek(recPos); i16 ox=r.i16_(),oy=r.i16_(); int w=r.i16_(),h=r.i16_(); u16 comp=r.u16_(); u32 recSize=r.u32_(); u32 dataOff=r.u32_(); r.i16_(); int frames=r.u16_();
    (void)ox;(void)oy;(void)recSize;
    if(w<=0||h<=0||w>1024||h>1024||frames<=0||frame>=frames)return false;
    std::vector<u8> idx;
    if(comp==0x1108||comp==0x0108||comp==0x0002){ if(!decodeRle(r,recPos+dataOff,w,h,frame,idx))return false; }
    else { idx.assign(w*h,0); size_t pos=recPos+dataOff; if(frames==1){ for(int y=0;y<h;y++){ size_t src=pos+(size_t)y*256; if(src+w>b.size())return false; memcpy(&idx[y*w],&b[src],w);} }
      else { r.seek(pos); std::vector<u32> offs(frames); for(int i=0;i<frames;i++)offs[i]=r.u32_(); r.seek(pos+offs[frame]); int cx=r.i16_(),cy=r.i16_(); if(cx!=w||cy!=h)return false; size_t dst=0; for(int y=0;y<cy;y++){int x=0;while(x<cx){u8 t=r.u8_();for(int i=0;i<t&&x<cx;i++,x++){if(dst>=idx.size())return false;idx[dst++]=0;}u8 n=r.u8_();for(int i=0;i<n&&x<cx;i++,x++){if(dst>=idx.size())return false;idx[dst++]=r.u8_();}}} } }
    out.w=w;out.h=h;out.rgba.resize(w*h*4);out.transparent=false;
    for(int i=0;i<w*h;i++){u8 ci=idx[i];u32 c=pal.c[ci];u8 a=255; if(ci==0){a=0;out.transparent=true;} out.rgba[i*4]=getr(c);out.rgba[i*4+1]=getg(c);out.rgba[i*4+2]=getb(c);out.rgba[i*4+3]=a;}
    return true;
}
} // namespace
