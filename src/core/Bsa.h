#pragma once
#include "Vfs.h"
#include <unordered_map>
namespace dfpsp {
struct BsaEntry { std::string name; u32 id=0; u32 offset=0,size=0; };
class Bsa {
public:
    bool open(const Vfs& vfs,const std::string& name);
    bool read(u32 index,std::vector<u8>& out)const;
    bool read(const std::string& name,std::vector<u8>& out)const;
    bool readId(u32 id,std::vector<u8>& out)const;
    int find(const std::string& name)const;
    int findId(u32 id)const;
    size_t count()const{return entries_.size();}
    const BsaEntry* entry(u32 i)const{return i<entries_.size()?&entries_[i]:nullptr;}
    const std::string& path()const{return path_;}
private:
    std::string path_; std::vector<BsaEntry> entries_;
    std::unordered_map<std::string,int> byName_; std::unordered_map<u32,int> byId_;
};
} // namespace
