#include "Rdb.h"
#include "Stream.h"
#include <cstdlib>
#include <set>
namespace dfpsp {
bool loadRdb(const Bsa& bsa,const std::string& name,RdbBlock& out){
    std::vector<u8> b; if(!bsa.read(name,b))return false; if(b.size()<9532)return false;
    Reader r(b); out=RdbBlock{}; out.version=r.u32_(); out.width=r.u32_(); out.height=r.u32_(); u32 rootOff=r.u32_(); r.u32_();
    if(out.width<=0||out.height<=0||out.width>64||out.height>64)return false;
    r.seek(20); out.refs.resize(750);
    for(int i=0;i<750;i++){std::string id=r.str(5); std::string desc=r.str(3); out.refs[i].id=(u32)strtoul(id.c_str(),nullptr,10); out.refs[i].desc=desc;}
    r.seek(9532); std::vector<i32> roots(out.width*out.height); for(auto&x:roots)x=r.i32_();
    std::set<i32> visited;
    for(i32 root:roots){ i32 cur=root; while(cur>=0&&(size_t)cur+25<=b.size()&&visited.insert(cur).second){
        Reader o(b.data()+cur,25); i32 next=o.i32_(); o.i32_(); RdbObject obj; obj.pos={(float)o.i32_(),-(float)o.i32_(),(float)o.i32_()}; obj.type=o.u8_(); u32 ro=o.u32_();
        if(ro+4<=b.size()){
            Reader rr(b.data()+ro,b.size()-ro);
            if(obj.type==1){ obj.rot={rr.i32_()/5.68888888888889f,-rr.i32_()/5.68888888888889f,rr.i32_()/5.68888888888889f}; obj.modelIndex=rr.u16_(); obj.trigger=(int)rr.u32_(); rr.u8_(); i32 act=rr.i32_();
                if(act>0&&(size_t)act+10<=b.size()){Reader ar(b.data()+act,10);obj.action.axis=ar.u8_();obj.action.duration=ar.u16_();obj.action.magnitude=ar.u16_();obj.action.next=ar.i32_();obj.action.flags=ar.u8_();}
            } else if(obj.type==3){ u16 bits=rr.u16_();obj.texArchive=bits>>7;obj.texRecord=bits&127;obj.flatFlags=rr.u16_();obj.magnitude=rr.u8_();obj.faction=rr.u8_();rr.i32_();obj.flatAction=rr.u8_(); }
            else if(obj.type==2){ rr.u32_();rr.u32_();obj.lightRadius=rr.u16_(); }
        }
        out.objects.push_back(obj); cur=next;
    }}
    return true;
}
} // namespace
