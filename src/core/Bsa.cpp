#include "Bsa.h"
#include "Stream.h"
#include <cstdio>
#include <algorithm>
namespace dfpsp {
static std::string upper(std::string s){for(auto&c:s)c=(char)std::toupper((unsigned char)c);return s;}
bool Bsa::open(const Vfs& vfs,const std::string& name){
    path_=vfs.resolve(name); if(path_.empty())return false;
    FILE*f=fopen(path_.c_str(),"rb"); if(!f)return false;
    fseek(f,0,SEEK_END); long fsz=ftell(f); fseek(f,0,SEEK_SET);
    if(fsz<4){fclose(f);return false;}
    u8 hdr[4]; fread(hdr,1,4,f); i16 cnt=(i16)(hdr[0]|hdr[1]<<8); u16 type=hdr[2]|hdr[3]<<8;
    if(cnt<0|| (type!=0x0100&&type!=0x0200)){fclose(f);return false;}
    entries_.resize(cnt); u32 off=4;
    if(type==0x0100){
        long dir=(long)fsz-18L*cnt; if(dir<4){fclose(f);return false;} fseek(f,dir,SEEK_SET);
        for(int i=0;i<cnt;i++){char nm[15]={}; fread(nm,1,14,f); u8 sz[4];fread(sz,1,4,f); u32 s=sz[0]|sz[1]<<8|sz[2]<<16|u32(sz[3])<<24; entries_[i].name=nm; entries_[i].offset=off; entries_[i].size=s; off+=s; byName_[upper(nm)]=i;}
    } else {
        long dir=(long)fsz-8L*cnt; if(dir<4){fclose(f);return false;} fseek(f,dir,SEEK_SET);
        for(int i=0;i<cnt;i++){u8 b[8];fread(b,1,8,f); u32 id=b[0]|b[1]<<8|b[2]<<16|u32(b[3])<<24; u32 s=b[4]|b[5]<<8|b[6]<<16|u32(b[7])<<24; entries_[i].id=id; entries_[i].name=std::to_string(id); entries_[i].offset=off; entries_[i].size=s; off+=s; byId_[id]=i;}
    }
    fclose(f); return off<=(u32)fsz;
}
bool Bsa::read(u32 i,std::vector<u8>& out)const{
    if(i>=entries_.size())return false; auto&e=entries_[i]; if(e.size==0){out.clear();return true;}
    FILE*f=fopen(path_.c_str(),"rb"); if(!f)return false; fseek(f,e.offset,SEEK_SET); out.resize(e.size); size_t got=fread(out.data(),1,e.size,f); fclose(f); return got==e.size;
}
int Bsa::find(const std::string& name)const{auto it=byName_.find(upper(name));return it==byName_.end()?-1:it->second;}
int Bsa::findId(u32 id)const{auto it=byId_.find(id);return it==byId_.end()?-1:it->second;}
bool Bsa::read(const std::string& name,std::vector<u8>& out)const{int i=find(name);return i>=0&&read((u32)i,out);}
bool Bsa::readId(u32 id,std::vector<u8>& out)const{int i=findId(id);return i>=0&&read((u32)i,out);}
} // namespace
