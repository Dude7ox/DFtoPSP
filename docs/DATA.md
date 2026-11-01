# Data provenance

The repository ships an original-format ARENA2 subset so the vertical slice can run
without a mandatory conversion step.

- Primary cached source: `PetrasBartusis/Daggerfall` commit
  `9f18f5bd0b0c141667d826de7615f178eba9b8a4`.
- Hash reference: third-party candidate GOG manifest
  `vgrichina/berrry-wine` (`gog-hashes.json`). This is not a publisher signature.
- `data/MANIFEST.json` records size, SHA-256, and status for every shipped file.

Important limitations:
- `BLOCKS.BSA`, `MAPS.BSA`, and `FLATS.CFG` do not match the candidate GOG manifest.
  They are original-format classic files, but their publisher/version authenticity is
  unverified. The slice uses `B0000001.RDB` from this cached BLOCKS.BSA.
- Cached BLOCKS.BSA lacks `M0000999.RDB`, so Privateer's Hold cannot be fully loaded
  from it. The slice therefore uses *Ruins of Old Evelara's Farm* instead.
- `WOODS.WLD` was omitted from the repository to stay under the workspace snapshot
  budget. Restore it from a full ARENA2 before overworld work.
- 24 TEXTURE archives that mismatched the candidate manifest were not shipped.

For a full port, replace `data/ARENA2` with a complete, version-consistent ARENA2
from the original freeware release or a legal installed copy. The engine reads the
original files directly.
