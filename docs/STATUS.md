# Vertical slice status

Target: PSP-1000 (32 MB) first; PSP-2000/3000 fallback.

Validated in this environment:
- Host core tests pass against `data/`.
- MAPS.BSA layout parse: *Ruins of Old Evelara's Farm*, region 16, exterior 18358,
  8 blocks, start `B0000001.RDB`.
- RDB/ARCH3D/TEXTURE load: `B0000001.RDB` -> 1168 triangles, 36 model instances,
  17 flats.
- Host software render produces `docs/slice.bmp`.
- PSP cross-compile produces `EBOOT.PBP` (~522 KB).

Not validated:
- PPSSPP / real PSP execution.
- Frame rate on hardware.
- Full collision robustness across all blocks.

Known gaps:
- Single block only; no block streaming/transitions.
- Doors/actions, enemies, combat, UI, audio, saves, quests, overworld.
- Data provenance limitations in `DATA.md`.
