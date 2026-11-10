#include "Sfx.h"
#include <algorithm>
namespace dfpsp {
bool SfxBank::open(const Vfs& vfs,const std::string& name){
    ready_=false; count_=0; cached_=0; order_.clear(); data_.clear();
    if(!bsa_.open(vfs,name))return false;
    count_=bsa_.count();
    if(!count_)return false;
    data_.resize(count_);
    ready_=true;
    return true;
}
int SfxBank::recordSize(int index)const{
    if(!ready_||index<0||(size_t)index>=count_)return -1;
    const BsaEntry* e=bsa_.entry((u32)index);
    return e?(int)e->size:-1;
}
bool SfxBank::extract(int index,std::vector<u8>& out)const{
    out.clear();
    if(!ready_||index<0||(size_t)index>=count_)return false;
    return bsa_.read((u32)index,out);
}
bool SfxBank::clip(int index,SfxClip& out,SfxEvicted* evicted){
    out=SfxClip();
    if(!ready_||index<0||(size_t)index>=count_)return false;
    size_t i=(size_t)index;
    if(data_[i].empty()){
        std::vector<u8> buf;
        if(!bsa_.read((u32)i,buf)||buf.empty())return false;
        if(buf.size()>budget_)return false;              // never cacheable
        while(cached_+buf.size()>budget_&&!order_.empty()){
            int victim=order_.front();
            order_.erase(order_.begin());
            if(victim==index)continue;
            size_t v=(size_t)victim;
            if(evicted&&data_[v].data())evicted->add(data_[v].data());
            cached_-=data_[v].size();
            std::vector<u8>().swap(data_[v]);
        }
        if(cached_+buf.size()>budget_)return false;
        data_[i].swap(buf);
        cached_+=data_[i].size();
        order_.push_back(index);
    }else{
        for(size_t k=0;k<order_.size();k++){
            if(order_[k]==index){
                order_.erase(order_.begin()+(long)k);
                order_.push_back(index);
                break;
            }
        }
    }
    if(data_[i].empty())return false;
    out.pcm=data_[i].data(); out.len=data_[i].size(); out.rate=SFX_SAMPLE_RATE;
    return true;
}
void SfxBank::clearCache(){
    for(size_t i=0;i<data_.size();i++)std::vector<u8>().swap(data_[i]);
    cached_=0; order_.clear();
}
} // namespace
