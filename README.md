# DFtoPSP

Native C++ engine for original-files *The Elder Scrolls II: Daggerfall* on Sony PSP.
This is **not** Daggerfall Unity, not a Unity export, and not DOSBox.

## Status

Early playable-engine prototype, not a complete campaign port.

Implemented:
- Original-format readers: BSA, ARCH3D meshes, TEXTURE.* + ART_PAL.COL, BLOCKS.BSA RDB objects, MAPS.BSA dungeon layouts.
- Loose-file / mod-directory overrides on top of original archives.
- 3×3 dungeon-block streaming around the player, with block transitions and door-state carry-over.
- Action doors: aim + X to open/close, animated swing, collision updates; locked doors can be
  picked with X (chance scales with the lockpick skill; failure alerts nearby monsters).
- Dungeon monsters: classic spawn markers, encounter tables seeded by location id, animated
  billboards, line-of-sight aggro, melee combat, corpses, player HP/death and binary save games.
- Character progression: kills award XP and gold, level-ups raise HP/skill/lockpick
  (saved in format v2; v1 saves still load).
- Robust boot: probes `data/`, `data/ARENA2/`, `ms0:`/`ef0:`/`host0:` game-directory variants
  and relative paths; if a file is still missing, the fatal screen prints on-device diagnostics
  (working directory, per-candidate directory probes, per-root file probes, stdio write/read test).
- Original `FONT000?.FNT` bitmap faces (DFU-compatible reader): HUD and screenshots
  render in the classic FONT0001 gothic face on host and PSP.
- PSP GU renderer: indexed textures, CLUT, triangle world, billboard flats, depth buffer, double-buffered HUD.
- PSP controls: analog move, DPad look, X door/lockpick, Square attack, O run, Triangle noclip, L/R noclip vertical, Start save, Select window (help/character/log), HOME exit.
- Gravity, floor snapping, radius/height collision, fall recovery.
- Host software renderer + CLI for validation and screenshots.
- Cross-compiled `EBOOT.PBP` included in the repository.

Not implemented yet:
- Inventory, quests, dialogue, audio, spells, automap, overworld/travel.
- Full campaign data coverage; the shipped data subset loads the test dungeon only.
- Real-PSP / PPSSPP execution validation is still pending in this environment.

## Install on PSP

Copy the repository's PSP files to:

```text
ms0:/PSP/GAME/DFtoPSP/EBOOT.PBP
ms0:/PSP/GAME/DFtoPSP/data/ARENA2/...original files...
ms0:/PSP/GAME/DFtoPSP/mods/loadorder.txt   (optional)
ms0:/PSP/GAME/DFtoPSP/mods/MyMod/ARENA2/... (optional)
```

The prebuilt EBOOT targets PSP-1000 first (32 MB) and should also run on PSP-2000/3000
with custom firmware capable of launching homebrew EBOOTs.

## Build host tools

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/test_core data
./build/dfpsp_host --root data --layout
./build/dfpsp_host --root data --block B0000001.RDB --out frame.bmp
./build/dfpsp_host --root data --dungeon --out dungeon.bmp
```

## Build PSP EBOOT

Requires a PSPDEV toolchain (psp-gcc/psp-g++, PSPSDK, pthread-embedded, mksfoex,
pack-pbp, psp-fixup-imports).

```sh
export PSPDEV=/path/to/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -f Makefile.psp
```

See `docs/TOOLCHAIN.md` for notes about the locally bootstrapped toolchain used here.

## Data

`data/ARENA2` contains an original-format subset. See `data/MANIFEST.json` and `docs/DATA.md`.
`BLOCKS.BSA`, `MAPS.BSA`, and `FLATS.CFG` are present but marked `manifest-mismatch`
against a third-party candidate GOG hash list; they are used for the prototype and
should be replaced by a matching full ARENA2 for campaign work.

## Mods

See `docs/MODS.md`. Classic loose-file overrides are supported; no proprietary
converted asset container is required.
