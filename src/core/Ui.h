#pragma once
#include "Player.h"
#include <string>
#include <vector>
namespace dfpsp {
// Ring buffer of short gameplay messages, classic message-log style.
class MsgLog {
public:
    void push(const std::string& s);
    const std::vector<std::string>& lines()const{return lines_;}
    size_t size()const{return lines_.size();}
    void clear(){lines_.clear();}
private:
    std::vector<std::string> lines_;
    static constexpr size_t kMax=32;
};
// Window content shared by the host blitter and the PSP GE renderer so both
// platforms show identical panels. Titles use FONT0001, bodies FONT0003.
std::vector<std::string> charSheetLines(const PlayerState& p,const std::string& place);
std::vector<std::string> helpLines();
} // namespace
