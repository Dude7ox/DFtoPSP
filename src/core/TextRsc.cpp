#include "TextRsc.h"
#include "Stream.h"
#include <algorithm>
namespace dfpsp {
bool TextRsc::open(const Vfs& vfs,const std::string& name){
    data_.clear();dir_.clear();
    if(!vfs.readFile(name,data_)||data_.size()<8)return false;
    Reader rd(data_);
    u16 headerLen=rd.u16_();
    if(headerLen<6||(size_t)headerLen>data_.size())return false;
    int n=headerLen/6-1;
    if(n<=0||(size_t)2+(size_t)n*6>data_.size())return false;
    dir_.reserve((size_t)n);
    for(int i=0;i<n;i++){
        int id=rd.u16_();
        u32 off=rd.u32_();
        if(off<data_.size())dir_.push_back(std::make_pair(id,off));
    }
    return !dir_.empty();
}
int TextRsc::find(int id)const{
    for(size_t i=0;i<dir_.size();i++)if(dir_[i].first==id)return (int)i;
    return -1;
}
bool TextRsc::recordBytes(int id,std::vector<u8>& out)const{
    int at=find(id);
    if(at<0)return false;
    u32 off=dir_[(size_t)at].second;
    // A record runs to its end-of-record byte, or to the next record in the
    // file when that byte is missing (truncated entries do occur).
    u32 limit=(u32)data_.size();
    for(size_t i=0;i<dir_.size();i++){
        u32 o=dir_[i].second;
        if(o>off&&o<limit)limit=o;
    }
    out.clear();
    for(u32 p=off;p<limit;p++){
        u8 b=data_[p];
        if(b==0xfe)break;                 // end of record
        out.push_back(b);
    }
    return true;
}
std::vector<std::string> TextRsc::lines(int id)const{
    std::vector<std::string> res;
    std::vector<u8> rec;
    if(!recordBytes(id,rec))return res;
    std::string cur;
    for(size_t i=0;i<rec.size();i++){
        u8 b=rec[i];
        if(b==0xf6)break;                 // end of page
        if(b==0xfd||b==0x00){res.push_back(cur);cur.clear();}
        else if(b==0x01){if(!cur.empty())cur+=' ';}
        else if(b==0x02){/* pull the following text onto this line */}
        else if(b==0xf9||b==0xfb){i++;}   // font / position prefix: skip argument
        else if(b>=0x20&&b<=0x7f)cur+=(char)b;
    }
    if(!cur.empty())res.push_back(cur);
    // Trailing empty lines carry no information in a fixed-height panel.
    while(!res.empty()&&res.back().empty())res.pop_back();
    return res;
}
std::string TextRsc::text(int id)const{
    std::vector<std::string> ls=lines(id);
    std::string s;
    for(size_t i=0;i<ls.size();i++){if(i)s+='\n';s+=ls[i];}
    return s;
}
} // namespace dfpsp
