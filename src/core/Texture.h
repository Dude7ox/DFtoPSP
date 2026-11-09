#pragma once
#include "Vfs.h"
namespace dfpsp {
bool loadPalette(const Vfs& vfs, const std::string& name, Palette& pal);
bool loadTexture(const Vfs& vfs, int archive, int record, const Palette& pal, Texture& out, int frame=0);

// One texture record with every animation frame still encoded. Animated sprites
// (monsters, torches, water) are read from disk once and decoded frame by frame,
// so a streaming renderer never re-reads the whole archive per frame.
struct TextureRecord {
    int archive=0, record=0;
    int w=0, h=0, frames=1;
    int solid=0;            // 1/2 for the solid colour archives TEXTURE.000/001
    u16 compression=0;
    u32 dataOffset=0;
    i16 offsetX=0, offsetY=0;
    i16 scaleX=0, scaleY=0; // billboard scale in 1/256 units: size + size*(scale/256)
    std::vector<u8> data;   // record bytes; index 0 is the start of the record header
    bool valid=false;
    // World size of a billboard drawn from this record (classic divides by 256).
    float displayWidth()const{return (float)(w+(int)((float)w*((float)scaleX/256.f)));}
    float displayHeight()const{return (float)(h+(int)((float)h*((float)scaleY/256.f)));}
};
// maxBytes==0 lets the loader decide how much of the record to read; pass a
// non-zero value to force reading that many bytes from the record start.
bool loadTextureRecord(const Vfs& vfs, int archive, int record, TextureRecord& out, size_t maxBytes=0);
bool decodeTextureFrame(const TextureRecord& rec, int frame, const Palette& pal, Texture& out);

// Header-only probe: dimensions, frame count and billboard scale without pixels.
struct TextureInfo { int w=0,h=0,frames=1; i16 scaleX=0,scaleY=0; };
bool textureInfo(const Vfs& vfs, int archive, int record, TextureInfo& out);
} // namespace
