#pragma once
#include "Types.h"
#include <cstring>
#include <stdexcept>
#ifdef __PSP__
#include <cstdlib>
#define DF_THROW(msg) std::abort()
#else
#define DF_THROW(msg) throw std::runtime_error(msg)
#endif
namespace dfpsp {
struct Reader {
    const u8* d; size_t n, p=0;
    Reader(const u8* d_,size_t n_):d(d_),n(n_){}
    Reader(const std::vector<u8>&v):d(v.data()),n(v.size()){}
    void seek(size_t x){ if(x>n) DF_THROW("seek past end"); p=x; }
    void skip(size_t x){ seek(p+x); }
    size_t left()const{return n-p;}
    u8 u8_(){ if(p+1>n)DF_THROW("eof"); return d[p++]; }
    i16 i16_(){ u16 v=u16_(); return (i16)v; }
    u16 u16_(){ if(p+2>n)DF_THROW("eof"); u16 v=d[p]|d[p+1]<<8; p+=2; return v; }
    u32 u32_(){ if(p+4>n)DF_THROW("eof"); u32 v=d[p]|d[p+1]<<8|d[p+2]<<16|u32(d[p+3])<<24; p+=4; return v; }
    i32 i32_(){ return (i32)u32_(); }
    std::string str(size_t len){ if(p+len>n)DF_THROW("eof"); std::string s((const char*)d+p,len); p+=len; auto z=s.find('\0'); if(z!=std::string::npos)s.resize(z); return s; }
    std::vector<u8> bytes(size_t len){ if(p+len>n)DF_THROW("eof"); std::vector<u8> v(d+p,d+p+len); p+=len; return v; }
};
} // namespace
