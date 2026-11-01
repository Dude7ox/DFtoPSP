# Mod support

DFtoPSP reads original Daggerfall files. Mods are installed as loose original-format
files or replacement archives; there is no mandatory converted/proprietary container.

Layout:

```text
mods/
  loadorder.txt
  MyMod/
    ARENA2/
      TEXTURE.205
      ARCH3D.BSA
      BLOCKS.BSA
      ...
```

`mods/loadorder.txt` lists one mod directory per line. Later lines override earlier
lines. Loose files override archive records. Example:

```text
# comments start with #
Fixes
TexturePack
```

The engine searches, in priority order:
1. mod roots from `loadorder.txt`
2. `data/ARENA2`
3. `data`

DFU `.dfmod` packages are not interpreted directly. Extract classic loose files or
original archives from a mod into the layout above.
