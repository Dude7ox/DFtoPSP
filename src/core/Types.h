#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>

namespace dfpsp {
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t; using u64=uint64_t; using i16=int16_t; using i32=int32_t;
struct Vec2 { float x=0,y=0; };
struct Vec3 {
    float x=0,y=0,z=0;
    Vec3 operator+(const Vec3&o)const{return {x+o.x,y+o.y,z+o.z};}
    Vec3 operator-(const Vec3&o)const{return {x-o.x,y-o.y,z-o.z};}
    Vec3 operator*(float s)const{return {x*s,y*s,z*s};}
    float dot(const Vec3&o)const{return x*o.x+y*o.y+z*o.z;}
    Vec3 cross(const Vec3&o)const{return {y*o.z-z*o.y,z*o.x-x*o.z,x*o.y-y*o.x};}
    float len()const{return std::sqrt(dot(*this));}
    Vec3 norm()const{float l=len();return l>1e-8f?(*this)*(1.f/l):Vec3{0,0,0};}
};
struct Vertex { Vec3 p; Vec2 uv; Vec3 n; u32 color=0xffffffff; };
struct Texture { int w=0,h=0,frames=1; std::vector<u8> rgba; std::vector<u8> indices; bool transparent=false; };
struct Mesh {
    std::vector<Vertex> verts; // triangles
    std::vector<int> texArchive, texRecord; // per triangle
    Vec3 min{1e9f,1e9f,1e9f}, max{-1e9f,-1e9f,-1e9f};
};
struct Palette { std::array<u32,256> c{}; };
static inline u32 rgba(u8 r,u8 g,u8 b,u8 a=255){return u32(r)|u32(g)<<8|u32(b)<<16|u32(a)<<24;}
static inline u8 getr(u32 c){return c&255;} static inline u8 getg(u32 c){return (c>>8)&255;}
static inline u8 getb(u32 c){return (c>>16)&255;} static inline u8 geta(u32 c){return (c>>24)&255;}
} // namespace
