#pragma once
#include "Vfs.h"
#include <string>
#include <utility>
#include <vector>
namespace dfpsp {
// TEXT.RSC is the classic string bank: 1408 sparse records of ASCII plus a few
// formatting bytes. Character creation reads race descriptions (2000..2003),
// class descriptions (2100 + class index), skill blurbs (1360+) and the
// biography backstories (4116 + class index) from here, so a mod that replaces
// TEXT.RSC changes the text the port shows.
class TextRsc {
public:
    bool open(const Vfs& vfs,const std::string& name="TEXT.RSC");
    size_t count()const{return dir_.size();}
    bool has(int id)const{return find(id)>=0;}
    int find(int id)const;                  // directory index, -1 when absent
    int idAt(size_t i)const{return dir_[i].first;}
    u32 offsetAt(size_t i)const{return dir_[i].second;}
    // Record text with the formatting bytes resolved: 0xfd/0x00 start a new
    // line, 0x01 keeps the line and adds a space, 0x02 pulls the next line up,
    // 0xf9/0xfb skip their argument byte, 0xf6 ends the page and 0xfe the record.
    std::vector<std::string> lines(int id)const;
    std::string text(int id)const;          // lines joined with '\n'
    bool recordBytes(int id,std::vector<u8>& out)const;
private:
    std::vector<u8> data_;
    std::vector<std::pair<int,u32>> dir_;   // id, byte offset
};
} // namespace dfpsp
