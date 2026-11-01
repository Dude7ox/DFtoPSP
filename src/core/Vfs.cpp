#include "Vfs.h"
#ifdef __PSP__
#include <cstdlib>
#define DF_THROW(msg) std::abort()
#else
#define DF_THROW(msg) throw std::runtime_error(msg)
#endif
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <cctype>
namespace dfpsp {
static std::string upper(std::string s){for(auto&c:s)c=(char)std::toupper((unsigned char)c);return s;}
static bool fileExists(const std::string&p){FILE*f=fopen(p.c_str(),"rb");if(!f)return false;fclose(f);return true;}
bool Vfs::addRoot(const std::string& dir){
    std::string d=dir; while(!d.empty()&&d.back()=='/')d.pop_back();
    if(d.empty())return false;
    roots_.insert(roots_.begin(),d); return true;
}
bool Vfs::loadModList(const std::string& root){
    std::string p=root+"/mods/loadorder.txt"; FILE*f=fopen(p.c_str(),"rb"); if(!f)return false;
    char line[512]; std::vector<std::string> order;
    while(fgets(line,sizeof(line),f)){std::string s=line; while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'||s.back()==' '))s.pop_back(); if(!s.empty()&&s[0]!='#')order.push_back(s);}
    fclose(f);
    for(auto& m:order) addRoot(root+"/mods/"+m);
    return true;
}
std::string Vfs::resolve(const std::string& name) const {
    std::string up=upper(name);
    for(auto& r:roots_){
        std::string p=r+"/"+name; if(fileExists(p))return p;
        p=r+"/"+up; if(fileExists(p))return p;
        p=r+"/ARENA2/"+name; if(fileExists(p))return p;
        p=r+"/ARENA2/"+up; if(fileExists(p))return p;
        p=r+"/arena2/"+name; if(fileExists(p))return p;
        p=r+"/arena2/"+up; if(fileExists(p))return p;
    }
    return {};
}
bool Vfs::exists(const std::string& name)const{return !resolve(name).empty();}
bool Vfs::readFile(const std::string& name,std::vector<u8>& out)const{
    auto p=resolve(name); if(p.empty())return false;
    FILE*f=fopen(p.c_str(),"rb"); if(!f)return false;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<0){fclose(f);return false;} out.resize((size_t)n);
    size_t got=fread(out.data(),1,out.size(),f); fclose(f); out.resize(got); return got==(size_t)n;
}
std::vector<u8> Vfs::readRequired(const std::string& name)const{std::vector<u8> v; if(!readFile(name,v))DF_THROW("missing file: "+name); return v;}
} // namespace
