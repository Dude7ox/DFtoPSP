#pragma once
#include "Vfs.h"
namespace dfpsp {
bool loadPalette(const Vfs& vfs, const std::string& name, Palette& pal);
bool loadTexture(const Vfs& vfs, int archive, int record, const Palette& pal, Texture& out, int frame=0);
} // namespace
