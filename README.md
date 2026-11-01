# DFtoPSP

Native C++ vertical slice of an original-files Daggerfall engine for Sony PSP.
This is **not** Daggerfall Unity, not a Unity export, and not DOSBox.

## Status

Playable-engine scaffold, not a complete campaign port.

Implemented:
- Original-format readers: BSA, ARCH3D.BSA meshes, TEXTURE.* + ART_PAL.COL, BLOCKS.BSA RDB objects, MAPS.BSA dungeon layouts.
- Loose-file / mod-directory overrides on top of original archives.
- One dungeon block scene builder (`B0000001.RDB`, start block of *Ruins of Old Evelara's Farm*).
- Software renderer + host CLI for validation and screenshots.
- PSP GU renderer, analog movement, triangle collision, floor snapping.
- Cross-compiled `EBOOT.PBP` for PSP.

Not implemented yet:
- Full dungeon streaming / block transitions, doors/actions, combat, AI, inventory, quests, dialogue, saves, audio, overworld.
- Hardware/PPSSPP validation is still pending in this environment.

## Build host tools

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/test_core data
./build/dfpsp_host --root data --layout
./build/dfpsp_host --root data --block B0000001.RDB --out frame.bmp
```

## Build PSP EBOOT

Requires a PSPDEV toolchain (psp-gcc/psp-g++, PSPSDK, mksfoex, pack-pbp).

```sh
export PSPDEV=/path/to/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -f Makefile.psp
```

Copy to PSP:

```text
ms0:/PSP/GAME/DFtoPSP/EBOOT.PBP
ms0:/PSP/GAME/DFtoPSP/data/ARENA2/...original files...
```

## Data

`data/ARENA2` contains an original-format subset. See `data/MANIFEST.json` and `docs/DATA.md`.
`BLOCKS.BSA`, `MAPS.BSA`, and `FLATS.CFG` are present but marked `manifest-mismatch`
against the third-party candidate GOG hash list; they are used for the vertical slice
and should be replaced by a matching full ARENA2 for campaign work.

## Mods

See `docs/MODS.md`. Classic loose-file overrides are supported; no proprietary
converted asset container is required.
