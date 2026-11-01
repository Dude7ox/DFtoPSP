#include "Texture.h"
#include "Stream.h"
#include <cstdio>
#include <cstring>
namespace dfpsp {
bool loadPalette(const Vfs& vfs,const std::string& name,Palette& pal){
    std::vector<u8> b;if(!vfs.readFile(name,b))return false;
    size_t off=0;if(b.size()==776)off=8;else if(b.size()!=768)return false;
    // ART_PAL.COL stores 8-bit channels. Only MAP.PAL uses VGA 6-bit values.
    for(int i=0;i<256;i++)pal.c[i]=rgba(b[off+i*3],b[off+i*3+1],b[off+i*3+2]);
    return true;
}
static bool decodeRle(Reader& r,size_t recPos,int w,int h,int frame,std::vector<u8>& idx){
    idx.assign(w*h,0);r.seek(recPos+(size_t)h*frame*4);
    struct Row{int off;bool rle;};std::vector<Row> rows(h);
    for(int y=0;y<h;y++){rows[y].off=r.i16_();rows[y].rle=(r.u16_()&0x8000)!=0;}
    size_t dst=0;
    for(int y=0;y<h;y++){
        if(rows[y].off<0)return false;r.seek(recPos+rows[y].off);
        if(!rows[y].rle){for(int x=0;x<w;x++)idx[dst++]=r.u8_();}
        else{
            int rowWidth=r.u16_(),rowPos=0;if(rowWidth<0||rowWidth>w)return false;
            while(rowPos<rowWidth){int probe=r.i16_();if(probe<0){int n=-probe;u8 p=r.u8_();if(rowPos+n>rowWidth)return false;for(int i=0;i<n;i++)idx[dst++]=p;rowPos+=n;}else if(probe>0){if(rowPos+probe>rowWidth)return false;for(int i=0;i<probe;i++)idx[dst++]=r.u8_();rowPos+=probe;}else break;}
            if(rowPos!=rowWidth)return false;while(rowPos++<w)idx[dst++]=0;
        }
    }
    return dst==idx.size();
}
bool loadTexture(const Vfs& vfs,int archive,int record,const Palette& pal,Texture& out,int frame){
    char name[32];snprintf(name,sizeof(name),"TEXTURE.%03d",archive);
    std::vector<u8> b;if(!vfs.readFile(name,b)||b.size()<26)return false;
    Reader r(b);int count=r.i16_();if(record<0||record>=count||count>4096)return false;
    std::vector<u8> idx;int w,h;
    if(archive==0||archive==1){w=h=32;idx.assign(w*h,(u8)(archive==1?128+record:record));}
    else{
        r.seek(26+(size_t)record*20);r.i16_();u32 recPos=r.u32_();r.skip(14);
        if(recPos+28>b.size())return false;r.seek(recPos);
        r.i16_();r.i16_();w=r.i16_();h=r.i16_();u16 comp=r.u16_();u32 recSize=r.u32_();u32 dataOff=r.u32_();r.i16_();int frames=r.u16_();
        if(w<=0||h<=0||w>1024||h>1024||frames<=0||frame>=frames||recPos+dataOff>b.size())return false;
        size_t pos=recPos+dataOff;
        // The low byte is the codec; the high byte contains record flags.
        // Dungeon textures commonly carry 0x0900 and are uncompressed.
        if((comp&0xff)==0x08||(comp&0xff)==0x02){if(!decodeRle(r,pos,w,h,frame,idx))return false;}
        else if(frames==1){idx.assign(w*h,0);for(int y=0;y<h;y++){size_t src=pos+(size_t)y*256;if(src+w>b.size())return false;memcpy(&idx[y*w],&b[src],w);}}
        else{
            r.seek(pos);std::vector<u32> offs(frames);for(auto& off:offs)off=r.u32_();if(pos+offs[frame]>b.size())return false;r.seek(pos+offs[frame]);
            int cx=r.i16_(),cy=r.i16_();if(cx!=w||cy!=h)return false;idx.assign(w*h,0);size_t dst=0;
            for(int y=0;y<cy;y++){int x=0;while(x<cx){int transparent=r.u8_(),opaque=r.u8_();if(x+transparent+opaque>cx)return false;x+=transparent;dst+=transparent;for(int i=0;i<opaque;i++)idx[dst++]=r.u8_();x+=opaque;}}
        }
    }
    out.w=w;out.h=h;out.indices=std::move(idx);out.rgba.resize(w*h*4);out.transparent=false;
    for(int i=0;i<w*h;i++){u8 ci=out.indices[i];u32 c=pal.c[ci];u8 a=ci==0?0:255;if(!a)out.transparent=true;out.rgba[i*4]=getr(c);out.rgba[i*4+1]=getg(c);out.rgba[i*4+2]=getb(c);out.rgba[i*4+3]=a;}
    return true;
}
} // namespace
