#include "ImgFile.h"
#include <cstring>
#include "Stream.h"
namespace dfpsp {
namespace {
// Files that store their own palette directly after the pixels. The stored RGB
// values are four times darker than ART_PAL.COL entries, so they get scaled.
bool hasOwnPalette(const std::string& n){
    return n=="CHGN00I0.IMG"||n=="DIE_00I0.IMG"||n=="PICK02I0.IMG"||n=="PICK03I0.IMG"
        ||n=="PRIS00I0.IMG"||n=="TITL00I0.IMG";
}
// Some IMG files are really single RCI records: no header at all, just w*h
// bytes. The classic engine recognises them by file length.
bool headerlessDims(size_t len,int& w,int& h){
    switch(len){
    case 44:     w=22;  h=22;  return true;
    case 289:    w=17;  h=17;  return true;
    case 441:    w=49;  h=9;   return true;
    case 512:    w=32;  h=16;  return true;
    case 720:    w=9;   h=80;  return true;
    case 990:    w=45;  h=22;  return true;
    case 1720:   w=43;  h=40;  return true;
    case 2140:   w=107; h=20;  return true;
    case 2916:   w=81;  h=36;  return true;
    case 3200:   w=40;  h=80;  return true;
    case 3938:   w=179; h=22;  return true;
    case 4280:   w=107; h=40;  return true;
    case 4508:   w=322; h=14;  return true;
    case 20480:  w=320; h=64;  return true;
    case 26496:  w=184; h=144; return true;
    case 64000:  w=320; h=200; return true;
    case 64768:  w=320; h=200; return true;   // + 768 bytes of palette
    case 68800:  w=320; h=215; return true;
    case 112128: w=512; h=219; return true;
    default: return false;
    }
}
// RLE used by compression type 2: a code byte above 127 repeats the next byte
// (code-127) times, otherwise the next code+1 bytes are literal.
bool rleDecode(const std::vector<u8>& src,size_t pos,size_t len,std::vector<u8>& dst){
    size_t end=pos+len;int out=0,total=(int)dst.size();
    while(out<total&&pos<end){
        u8 code=src[pos++];
        if(code>127){
            if(pos>=end)return false;
            u8 px=src[pos++];
            int n=code-127;
            for(int k=0;k<n&&out<total;k++)dst[out++]=px;
        }else{
            int n=code+1;
            for(int k=0;k<n&&out<total&&pos<end;k++)dst[out++]=src[pos++];
        }
    }
    return out==total;
}
bool saneRecord(const ImgRecord& r,size_t fileSize){
    if(r.w<=0||r.h<=0||r.w>1024||r.h>1024)return false;
    if((size_t)r.w*(size_t)r.h>fileSize)return false;
    if(r.dataOffset>fileSize)return false;
    if(r.compression==0&&(size_t)r.dataOffset+(size_t)r.w*r.h>fileSize)return false;
    if(r.compression!=0&&r.compression!=2&&(size_t)r.dataOffset+r.dataLength>fileSize)return false;
    if(r.compression==2&&(size_t)r.dataOffset+r.dataLength>fileSize)return false;
    return true;
}
} // namespace
bool ImgFile::headerlessSize(int& w,int& h)const{return headerlessDims(data_.size(),w,h);}
bool ImgFile::parseImg(){
    recs_.clear();
    int w=0,h=0;
    ImgRecord r;
    if(headerlessSize(w,h)){
        // Headerless: pixels start at 0, never compressed.
        r.w=w;r.h=h;r.compression=0;r.dataOffset=0;r.dataLength=(u32)(w*h);
    }else{
        if(data_.size()<12)return false;
        Reader rd(data_);
        r.xoff=rd.i16_();r.yoff=rd.i16_();r.w=rd.i16_();r.h=rd.i16_();
        r.compression=rd.u16_();r.dataLength=rd.u16_();
        r.dataOffset=12;
        if(r.compression==0&&r.dataLength<(u32)(r.w*r.h))r.dataLength=(u32)(r.w*r.h);
    }
    if(!saneRecord(r,data_.size()))return false;
    recs_.push_back(r);
    if(hasOwnPalette(name_)){
        size_t palOff=(size_t)r.dataOffset+(size_t)r.w*r.h;
        if(palOff+768<=data_.size()){
            embeddedPal_=true;
            for(int i=0;i<256;i++){
                int cr=data_[palOff+i*3]*4, cg=data_[palOff+i*3+1]*4, cb=data_[palOff+i*3+2]*4;
                if(cr>255)cr=255;if(cg>255)cg=255;if(cb>255)cb=255;
                pal_.c[i]=rgba((u8)cr,(u8)cg,(u8)cb);
            }
        }
    }
    return true;
}
bool ImgFile::parseCif(){
    recs_.clear();
    size_t pos=0;
    while(pos+12<=data_.size()){
        ImgRecord r;
        r.xoff=(i16)(data_[pos]|data_[pos+1]<<8);
        r.yoff=(i16)(data_[pos+2]|data_[pos+3]<<8);
        r.w=(i16)(data_[pos+4]|data_[pos+5]<<8);
        r.h=(i16)(data_[pos+6]|data_[pos+7]<<8);
        r.compression=(u16)(data_[pos+8]|data_[pos+9]<<8);
        r.dataLength=(u16)(data_[pos+10]|data_[pos+11]<<8);
        r.dataOffset=(u32)(pos+12);
        if(r.compression==0&&r.dataLength<(u32)(r.w*r.h))r.dataLength=(u32)(r.w*r.h);
        if(!saneRecord(r,data_.size()))return !recs_.empty();
        recs_.push_back(r);
        pos=(size_t)r.dataOffset+r.dataLength;
        if(r.dataLength==0)break;      // avoid an infinite loop on a bad header
    }
    return !recs_.empty();
}
bool ImgFile::parseRci(int w,int h){
    recs_.clear();
    rci_=true;
    size_t sz=(size_t)w*h;
    if(sz==0||data_.size()<sz)return false;
    size_t n=data_.size()/sz;
    for(size_t i=0;i<n;i++){
        ImgRecord r;
        r.w=w;r.h=h;r.compression=0;r.dataOffset=(u32)(i*sz);r.dataLength=(u32)sz;
        recs_.push_back(r);
    }
    return true;
}
bool ImgFile::open(const Vfs& vfs,const std::string& name){
    name_=name;recs_.clear();embeddedPal_=false;rci_=false;pal_=Palette();
    if(!vfs.readFile(name,data_)||data_.empty())return false;
    // FACES.CIF and the *.RCI files are headerless grids of fixed-size records.
    if(name=="FACES.CIF"||name=="CHLD00I0.RCI"||name=="TFAC00I0.RCI")return parseRci(64,64);
    if(name=="BUTTONS.RCI")return parseRci(32,16);
    if(name=="MPOP.RCI")return parseRci(17,17);
    if(name=="NOTE.RCI")return parseRci(44,9);
    if(name=="SPOP.RCI")return parseRci(22,22);
    size_t dot=name.find_last_of('.');
    std::string ext=dot==std::string::npos?std::string():name.substr(dot+1);
    if(ext=="CIF")return parseCif();
    if(ext=="RCI"){int w,h;if(headerlessSize(w,h))return parseRci(w,h);return false;}
    return parseImg();
}
bool ImgFile::decode(size_t i)const{
    if(i>=recs_.size())return false;
    ImgRecord& r=recs_[i];
    if(!r.idx.empty())return r.valid;
    size_t px=(size_t)r.w*r.h;
    r.idx.assign(px,0);
    if(r.compression==0){
        if((size_t)r.dataOffset+px>data_.size()){r.idx.clear();r.valid=false;return false;}
        std::memcpy(r.idx.data(),data_.data()+r.dataOffset,px);
        r.valid=true;
    }else if(r.compression==2){
        r.valid=rleDecode(data_,r.dataOffset,r.dataLength,r.idx);
        if(!r.valid)r.idx.clear();
    }else{
        // 0x0108/0x1108 are the animated weapon CIF encodings; this port only
        // needs the static character, face, item and interface art.
        r.idx.clear();r.valid=false;
    }
    return r.valid;
}
const ImgRecord* ImgFile::decoded(size_t i)const{
    if(i>=recs_.size())return nullptr;
    if(recs_[i].idx.empty()&&!decode(i))return nullptr;
    return &recs_[i];
}
std::string ImgFile::paletteName()const{
    if(embeddedPal_)return std::string();
    if(name_=="DANK02I0.IMG")return "DANKBMAP.COL";
    if(name_.size()>=4&&name_.compare(0,4,"FMAP")==0)return "FMAP_PAL.COL";
    if(name_.size()>=4&&name_.compare(0,4,"NITE")==0)return "NIGHTSKY.COL";
    return "ART_PAL.COL";
}
bool ImgFile::loadPalette(const Vfs& vfs,Palette& out)const{
    if(embeddedPal_){out=pal_;return true;}
    return dfpsp::loadPalette(vfs,paletteName(),out);
}
bool imgToTexture(const ImgRecord& rec,const Palette& pal,Texture& out,bool transparent){
    if(rec.idx.empty()||rec.w<=0||rec.h<=0)return false;
    out.w=rec.w;out.h=rec.h;out.frames=1;out.transparent=transparent;
    out.indices=rec.idx;
    out.rgba.assign((size_t)rec.w*rec.h*4,0);
    for(size_t i=0;i<rec.idx.size();i++){
        u8 ix=rec.idx[i];
        u32 c=pal.c[ix];
        if(transparent&&ix==0)c=rgba(getr(c),getg(c),getb(c),0);
        out.rgba[i*4]=getr(c);out.rgba[i*4+1]=getg(c);out.rgba[i*4+2]=getb(c);out.rgba[i*4+3]=geta(c);
    }
    return true;
}
void imgBlit(u32* dst,int dstW,int dstH,int dx,int dy,const ImgRecord& rec,const Palette& pal,bool transparent){
    if(!dst||rec.idx.empty())return;
    for(int y=0;y<rec.h;y++){
        int ty=dy+y;if(ty<0||ty>=dstH)continue;
        for(int x=0;x<rec.w;x++){
            int tx=dx+x;if(tx<0||tx>=dstW)continue;
            u8 ix=rec.idx[(size_t)y*rec.w+x];
            if(transparent&&ix==0)continue;
            dst[(size_t)ty*dstW+tx]=pal.c[ix]|0xff000000u;
        }
    }
}
} // namespace dfpsp
