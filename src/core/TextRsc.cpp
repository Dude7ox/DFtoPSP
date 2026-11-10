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
static std::string trim(const std::string& s){
    size_t a=0,b=s.size();
    while(a<b&&(s[a]==' '||s[a]=='\t'))a++;
    while(b>a&&(s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'))b--;
    return s.substr(a,b-a);
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
std::vector<std::string> TextRsc::subLines(int id,int sub)const{
    // TEXT.RSC record 9000 (the class questions) stores its subrecords on
    // 0xfc-separated lines numbered "N." - Daggerfall Unity splits the whole
    // record on the \d+[.] pattern, so match that behaviour.
    std::vector<u8> rec;
    if(!recordBytes(id,rec))return std::vector<std::string>();
    std::vector<std::string> lines;
    std::string cur;
    for(size_t i=0;i<rec.size();i++){
        u8 b=rec[i];
        if(b==0xfe)break;
        if(b==0xfc||b==0xfd||b==0x00||b==0x0a||b==0x0d){lines.push_back(cur);cur.clear();}
        else if(b>=0x20&&b<=0x7f)cur+=(char)b;
    }
    if(!cur.empty())lines.push_back(cur);
    std::vector<std::string> out;
    int cursub=-1;
    for(size_t i=0;i<lines.size();i++){
        const std::string& l=lines[i];
        int marker=-1;
        for(size_t p=0;p+1<l.size();p++){
            if(l[p]>='0'&&l[p]<='9'){
                size_t q=p;
                while(q<l.size()&&l[q]>='0'&&l[q]<='9'&&q-p<2)q++;
                if(q<l.size()&&l[q]=='.'&&q>p){marker=std::atoi(l.substr(p,q-p).c_str());
                    cur=l.substr(q+1);break;}
            }
        }
        if(marker>=0){
            cursub=marker;
            if(cursub==sub&&!trim(cur).empty())out.push_back(trim(cur));
        }else if(cursub==sub)out.push_back(l);
    }
    while(!out.empty()&&trim(out.back()).empty())out.pop_back();
    return out;
}
std::string TextRsc::text(int id)const{
    std::vector<std::string> ls=lines(id);
    std::string s;
    for(size_t i=0;i<ls.size();i++){if(i)s+='\n';s+=ls[i];}
    return s;
}
} // namespace dfpsp
