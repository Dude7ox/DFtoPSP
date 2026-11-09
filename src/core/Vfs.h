#pragma once
#include "Types.h"
#include <memory>
#include <unordered_map>
namespace dfpsp {
class Bsa;
// Original-file VFS. Loose files override archives; later mod roots override earlier ones.
class Vfs {
public:
    bool addRoot(const std::string& dir);
    bool loadModList(const std::string& root);
    bool readFile(const std::string& name, std::vector<u8>& out) const;
    // Partial read: `size` bytes starting at `offset`, or to end of file when size==0.
    bool readFile(const std::string& name, std::vector<u8>& out, size_t offset, size_t size) const;
    bool exists(const std::string& name) const;
    std::string resolve(const std::string& name) const;
    std::vector<u8> readRequired(const std::string& name) const;
    const std::vector<std::string>& roots()const{return roots_;}
private:
    std::vector<std::string> roots_; // highest priority first
};
} // namespace
