#include "Texture.h"
#include "Stream.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace dfpsp {
bool loadPalette(const Vfs& vfs,const std::string& name,Palette& pal){
    std::vector<u8> b;if(!vfs.readFile(name,b))return false;
    size_t off=0;if(b.size()==776)off=8;else if(b.size()!=768)return false;
    // ART_PAL.COL stores 8-bit channels. Only MAP.PAL uses VGA 6-bit values.
    for(int i=0;i<256;i++)pal.c[i]=rgba(b[off+i*3],b[off+i*3+1],b[off+i*3+2]);
    return true;
}
// The low byte is the codec and the high byte holds record flags, so 0x0108 is an
// RLE record while the dungeon favourite 0x0900 is plain uncompressed.
static bool isRleCodec(u16 comp){ return (comp&0xff)==0x08||(comp&0xff)==0x02; }

static bool readRecordDirectory(const Vfs& vfs,const char* name,int record,u32& recPos){
    std::vector<u8> dir;
    if(!vfs.readFile(name,dir,0,26+(size_t)(record+1)*20))return false;
    if(dir.size()<26)return false;
    Reader r(dir);int count=r.i16_();
    if(record<0||record>=count||count>4096)return false;
    size_t entry=26+(size_t)record*20;
    if(entry+20>dir.size())return false;
    r.seek(entry);r.i16_();recPos=r.u32_();
    return true;
}

bool textureInfo(const Vfs& vfs,int archive,int record,TextureInfo& out){
    out=TextureInfo{};
    if(record<0)return false;
    if(archive==0||archive==1){out.w=out.h=32;out.frames=1;return true;}
    char name[32];snprintf(name,sizeof(name),"TEXTURE.%03d",archive);
    u32 recPos=0;if(!readRecordDirectory(vfs,name,record,recPos))return false;
    std::vector<u8> hdr;
    if(!vfs.readFile(name,hdr,recPos,28)||hdr.size()<28)return false;
    Reader r(hdr);
    r.i16_();r.i16_();out.w=r.i16_();out.h=r.i16_();r.u16_();r.u32_();r.u32_();r.i16_();
    int frames=r.u16_();out.frames=frames>0?frames:1;r.i16_();
    out.scaleX=r.i16_();out.scaleY=r.i16_();
    return out.w>0&&out.h>0&&out.w<=1024&&out.h<=1024;
}

bool loadTextureRecord(const Vfs& vfs,int archive,int record,TextureRecord& out,size_t maxBytes){
    out=TextureRecord{};out.archive=archive;out.record=record;
    if(record<0)return false;
    if(archive==0||archive==1){
        // Solid colour archives: one 32x32 flat colour per record, no file data.
        out.solid=(archive==1)?2:1;out.w=out.h=32;out.frames=1;out.valid=true;return true;
    }
    char name[32];snprintf(name,sizeof(name),"TEXTURE.%03d",archive);
    u32 recPos=0;if(!readRecordDirectory(vfs,name,record,recPos))return false;
    std::vector<u8> hdr;
    if(!vfs.readFile(name,hdr,recPos,28)||hdr.size()<28)return false;
    Reader r(hdr);
    out.offsetX=r.i16_();out.offsetY=r.i16_();out.w=r.i16_();out.h=r.i16_();
    out.compression=r.u16_();u32 recSize=r.u32_();out.dataOffset=r.u32_();r.i16_();
    int frames=r.u16_();out.frames=frames>0?frames:1;r.i16_();
    out.scaleX=r.i16_();out.scaleY=r.i16_();
    if(out.w<=0||out.h<=0||out.w>1024||out.h>1024)return false;
    // RecordSize is authoritative for RLE and animated records. Single frame
    // uncompressed records store rows with a 256 byte stride, which can run past
    // RecordSize, so ask for whichever extent is larger. maxBytes==(size_t)-1
    // reads everything from the record start to the end of the file.
    size_t want;
    if(maxBytes==(size_t)-1)want=0;
    else if(maxBytes)want=maxBytes;
    else{
        want=(size_t)std::max<u32>(recSize,28u);
        if(out.frames==1&&!isRleCodec(out.compression))
            want=std::max(want,(size_t)out.dataOffset+(size_t)(out.h-1)*256+(size_t)out.w);
    }
    if(!vfs.readFile(name,out.data,recPos,want))return false;
    if(out.data.size()<28)return false;
    out.valid=true;return true;
}

// Row offsets in an RLE record are relative to the record itself while the row
// table sits at dataOffset + height*frame*4 (DFU TextureFile.ReadRle parity).
static bool decodeRle(const TextureRecord& rec,int frame,std::vector<u8>& idx){
    const int w=rec.w,h=rec.h;
    const size_t n=rec.data.size();
    size_t table=(size_t)rec.dataOffset+(size_t)h*(size_t)frame*4;
    if(table+(size_t)h*4>n)return false;
    Reader r(rec.data);
    r.seek(table);
    std::vector<i16> off(h);std::vector<bool> rle(h);
    for(int y=0;y<h;y++){off[y]=r.i16_();rle[y]=(r.u16_()&0x8000)!=0;}
    idx.assign((size_t)w*h,0);
    size_t dst=0;
    for(int y=0;y<h;y++){
        if(off[y]<0||(size_t)off[y]>=n)return false;
        r.seek((size_t)off[y]);
        if(!rle[y]){
            if((size_t)off[y]+(size_t)w>n)return false;
            memcpy(&idx[dst],&rec.data[off[y]],(size_t)w);dst+=(size_t)w;
        }else{
            if((size_t)off[y]+2>n)return false;
            int rowWidth=r.u16_(),rowPos=0;if(rowWidth<0||rowWidth>w)return false;
            while(rowPos<rowWidth){
                if(r.left()<2)return false;
                int probe=r.i16_();
                if(probe<0){int cnt=-probe;if(r.left()<1)return false;u8 p=r.u8_();if(rowPos+cnt>rowWidth||dst+(size_t)cnt>idx.size())return false;for(int i=0;i<cnt;i++)idx[dst++]=p;rowPos+=cnt;}
                else if(probe>0){if(r.left()<(size_t)probe||rowPos+probe>rowWidth||dst+(size_t)probe>idx.size())return false;for(int i=0;i<probe;i++)idx[dst++]=r.u8_();rowPos+=probe;}
                else break;
            }
            if(rowPos!=rowWidth)return false;
            while(rowPos++<w){if(dst>=idx.size())return false;idx[dst++]=0;}
        }
    }
    return dst==idx.size();
}

static bool decodeUncompressed(const TextureRecord& rec,int frame,std::vector<u8>& idx){
    const int w=rec.w,h=rec.h;
    const size_t n=rec.data.size();
    size_t pos=(size_t)rec.dataOffset;
    if(rec.frames==1){
        if(pos+(size_t)(h-1)*256+(size_t)w>n)return false;
        idx.assign((size_t)w*h,0);
        for(int y=0;y<h;y++)memcpy(&idx[(size_t)y*w],&rec.data[pos+(size_t)y*256],(size_t)w);
        return true;
    }
    // Animated records start with a table of per frame offsets.
    if(pos+(size_t)rec.frames*4>n||frame>=rec.frames)return false;
    Reader r(rec.data);
    r.seek(pos);
    std::vector<u32> offs(rec.frames);
    for(auto& off:offs)off=r.u32_();
    size_t fp=pos+offs[frame];
    if(fp+4>n)return false;
    r.seek(fp);
    int cx=r.i16_(),cy=r.i16_();
    if(cx<=0||cy<=0||(size_t)cx*(size_t)cy>(size_t)w*h)return false;
    idx.assign((size_t)w*h,0);
    size_t dst=0;
    for(int y=0;y<cy;y++){
        int x=0;
        while(x<cx){
            if(r.left()<2)return false;
            int transparent=r.u8_(),opaque=r.u8_();
            if(x+transparent+opaque>cx||dst+(size_t)(transparent+opaque)>idx.size())return false;
            x+=transparent;dst+=(size_t)transparent;
            if(opaque>0){
                if(r.left()<(size_t)opaque)return false;
                memcpy(&idx[dst],&rec.data[r.p],(size_t)opaque);dst+=(size_t)opaque;r.skip((size_t)opaque);
            }
            x+=opaque;
        }
    }
    return dst<=idx.size();
}

bool decodeTextureFrame(const TextureRecord& rec,int frame,const Palette& pal,Texture& out){
    out=Texture{};
    if(!rec.valid||frame<0||frame>=rec.frames)return false;
    const int w=rec.w,h=rec.h;
    std::vector<u8> idx;
    if(rec.solid){
        idx.assign((size_t)w*h,(u8)(rec.solid==2?128+rec.record:rec.record));
    }else if(isRleCodec(rec.compression)){
        if(!decodeRle(rec,frame,idx))return false;
    }else{
        if(!decodeUncompressed(rec,frame,idx))return false;
    }
    if((int)idx.size()!=w*h)return false;
    out.w=w;out.h=h;out.frames=rec.frames;out.indices=std::move(idx);
    out.rgba.resize((size_t)w*h*4);out.transparent=false;
    for(int i=0;i<w*h;i++){
        u8 ci=out.indices[i];u32 c=pal.c[ci];u8 a=ci==0?0:255;
        if(!a)out.transparent=true;
        out.rgba[i*4]=getr(c);out.rgba[i*4+1]=getg(c);out.rgba[i*4+2]=getb(c);out.rgba[i*4+3]=a;
    }
    return true;
}

bool loadTexture(const Vfs& vfs,int archive,int record,const Palette& pal,Texture& out,int frame){
    TextureRecord rec;
    if(!loadTextureRecord(vfs,archive,record,rec))return false;
    if(decodeTextureFrame(rec,frame,pal,out))return true;
    // RecordSize underestimates a few records; retry with everything after the
    // record header before giving up.
    if(rec.solid||rec.data.empty())return false;
    TextureRecord big;
    if(!loadTextureRecord(vfs,archive,record,big,(size_t)-1))return false;
    if(big.data.size()<=rec.data.size())return false;
    return decodeTextureFrame(big,frame,pal,out);
}
} // namespace
