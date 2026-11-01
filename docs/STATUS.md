# Port status

Target: PSP-1000 (32 MB) first; PSP-2000/3000 fallback.

## Validated in this environment

- Host core tests pass against `data/`:
  - palette/raw-index texture decoding,
  - TEXTURE record/frame bounds and 0x0900 uncompressed handling,
  - ARCH3D/RDB loading,
  - MAPS layout parse,
  - door toggle/update,
  - collision/floor/headroom helpers,
  - dungeon streaming resident-set limits.
- MAPS.BSA layout parse: *Ruins of Old Evelara's Farm*, region 16, exterior 18358,
  8 blocks, start `B0000001.RDB`.
- Single-block load: `B0000001.RDB` -> 1128 world triangles, 36 model instances,
  12 gameplay flats, 4 action doors.
- Dungeon streaming preview: 7 resident blocks, 17071 triangles at the starting area.
- Data manifest verification: `python3 tools/verify_data.py data/ARENA2` -> 455 checked, 0 bad.
- PSP cross-compile and PBP packaging succeed.
  - `EBOOT.PBP`: 1,328,900 bytes
  - SHA-256: `b07f09c64d7ef1914f29a3605047e6c1080a25852fb0b31e63969b9f1e6c1cd4`

Screenshots from the host software renderer:
- `docs/slice.png`: single-block view.
- `docs/dungeon.png`: streamed dungeon view.

## PSP implementation

- GU/GE hardware renderer with T8 indexed textures and a shared 256-entry CLUT8888.
- Fixed 48-slot 128×128 texture cache (~768 KB VRAM) plus double 512×272 5650 framebuffers
  and a 4444 depth buffer (~835 KB), fitting the PSP's 2 MB EDRAM budget.
- CPU texture uploads use `sceGeEdramGetAddr()`; GE commands use VRAM offsets.
- 3×3 resident dungeon blocks, streamed as the player crosses 2048-unit block boundaries.
- Door states are remembered per block instance while streaming.
- HUD is drawn through `pspDebugScreen` into the current GE draw buffer.
- Exit callback thread, analog/DPad controls, noclip, gravity/floor snap, fall recovery.

## Not validated

- Real PSP execution.
- PPSSPP execution.
- Frame rate on hardware.
- Long-play stability and memory fragmentation.
- Full collision robustness across all blocks.
- GU texture-coordinate convention on real hardware/emulator (implemented from SDK samples,
  but not visually confirmed here).

## Known gaps

- No enemies, combat, AI, inventory, character progression, quests, dialogue, saves, audio,
  spells, automap, overworld, travel, or full UI.
- Only the test dungeon layout is wired into the PSP startup path.
- Data provenance limitations remain; see `DATA.md`.
- The manually bootstrapped PSPSDK link emits `psp-fixup-imports` ordering warnings in this
  sandbox; the EBOOT packs successfully, but this is one more reason hardware validation is
  still required.
