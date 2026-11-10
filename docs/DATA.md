# Data provenance

The repository ships an original-format ARENA2 subset so the vertical slice can run
without a mandatory conversion step.

- Primary cached source: `PetrasBartusis/Daggerfall` commit
  `9f18f5bd0b0c141667d826de7615f178eba9b8a4`.
- Hash reference: third-party candidate GOG manifest
  `vgrichina/berrry-wine` (`gog-hashes.json`). This is not a publisher signature.
- `data/MANIFEST.json` records size, SHA-256, and status for every shipped file.
- `FONT0000.FNT`..`FONT0004.FNT` (classic bitmap UI faces) were taken from the
  daggerfall-unity repository (`Assets/StreamingAssets/Fonts`, MIT-licensed project
  redistributing the original files) and verified byte-identical against the copy in
  `PetrasBartusis/Daggerfall`; the manifest marks them `dual-source-match`.

Important limitations:
- `BLOCKS.BSA`, `MAPS.BSA`, and `FLATS.CFG` do not match the candidate GOG manifest.
  They are original-format classic files, but their publisher/version authenticity is
  unverified. The slice uses `B0000001.RDB` from this cached BLOCKS.BSA.
- Cached BLOCKS.BSA lacks `M0000999.RDB`, so Privateer's Hold cannot be fully loaded
  from it. The slice therefore uses *Ruins of Old Evelara's Farm* instead.
- `WOODS.WLD` and `ITEMS.BSA` are **not present** in that cached source (re-checked
  against `contents/arena2` for this increment), so overworld terrain and inventory
  stay blocked until a full ARENA2 is supplied.
- 24 TEXTURE archives that mismatched the candidate manifest were not shipped.

Audio data (added for the audio increment, see `docs/AUDIO.md`):

| file | size | SHA-256 | contents | status |
| --- | --- | --- | --- | --- |
| `MIDI.BSA` | 1,232,240 | `bfb1366f8bc65b05aeab06aae8d767f6f9f534152a03102a6c090ec3777d9997` | 131 HMI songs (`D1`-`D10`, `DUNGEON`, `GDNGN*`, `TAVERN`, weather, `MAGIC_2`, `SNEAKING`, `FOLK*`) | `cached-source` |
| `DAGGER.SND` | 7,661,766 | `fbb6195ba72f9b2787b659e38afd9ed92d95ca185916d8bdd28ba947b986f3e2` | 459 raw 8-bit unsigned mono 11025 Hz records | `cached-source` |

Both come from the same cached `PetrasBartusis/Daggerfall` source. The candidate GOG
manifest repository returned HTTP 404 when these were fetched, so they are marked
`cached-source` instead of `manifest-match`: original-format classic files whose
publisher authenticity is unverified. `python3 tools/verify_data.py data/ARENA2`
checks all 462 shipped files.

For a full port, replace `data/ARENA2` with a complete, version-consistent ARENA2
from the original freeware release or a legal installed copy. The engine reads the
original files directly.
