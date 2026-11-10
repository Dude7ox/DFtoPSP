# Audio

Native playback of the original Daggerfall sound data: HMI music from `MIDI.BSA`
and digital sound effects from `DAGGER.SND`. Nothing is converted, pre-rendered or
re-packaged — the engine reads the shipped files, so a mod that replaces either
archive (or drops loose `D1.HMI` / `SWING.WAV`-style files next to them) is picked
up automatically.

| piece | file | role |
| --- | --- | --- |
| `src/core/Midi.{h,cpp}` | HMI → note list | parses `MIDI.BSA` songs into a tick-sorted event list |
| `src/core/Synth.{h,cpp}` | software synthesizer | sequencer + 28-voice additive/PCM mixer at 22050 Hz stereo |
| `src/core/Sfx.{h,cpp}` | `DAGGER.SND` | sample bank with an eviction budget, plus the `SoundSink` interface |
| `src/core/Audio.{h,cpp}` | director | owns song choice, sfx routing and the WAV writer used for validation |
| `src/platform/psp/PspAudio.{h,cpp}` | output | audio thread feeding `sceAudioSRC*` with double buffering |
| `src/core/Dungeon.{h,cpp}` | gameplay cues | swings, hits, kills, monster barks/attacks, doors, lockpicks, level-ups |

## Original formats

### `MIDI.BSA` — HMI songs

`MIDI.BSA` is a BSA of type `0x0100` (named directory) holding **131 songs**
(1,232,240 bytes): `D1.HMI` … `D10.HMI` for the dungeons plus `FM_*` variants,
`DUNGEON.HMI`, `GDNGN*`, `TAVERN.HMI`, the weather themes (`SUNNYDAY`,
`OVERCAST`, `RAINING`, `SNOWING`), `MAGIC_2`, `SNEAKING` and `FOLK1`–`FOLK3`.

HMI (Human Machine Interfaces) is *not* standard MIDI, and it is not XMI either.
The layout implemented here matches ZMusic's `midisource_hmi.cpp` and
SpessaSynth's `parsers/hmi.c` (kode54), with the extra-field sizes that the
Daggerfall corpus actually uses:

```text
song header   "HMI-MIDISONG061595"
  0xD4  uint16  tempo source; PPQN = value << 2            -> 480
        uint32  Tempo = InitialTempo = 4000000 us/quarter  -> 120 ticks per second
  0xE4  uint32  track count
  0xE8  uint32  track table offset (song relative)
track header  "HMI-MIDITRACK" (13 bytes)
  +0x4B uint8   meta text length, text at +0x4C
  +0x57 uint32  event data offset (track relative)
  +0x99 8×uint16 device designations (0xA000 GM, 0xA001 MPU-401, 0xA002 OPL2,
        0xA004 MT-32, 0xA008 SBAWE32, 0xA009 OPL3, 0xA00A GUS)
```

Track data uses standard SMF variable-length quantities and running status. Two
HMI specifics matter:

* a note-on carries an explicit **duration** as a VLQ right after velocity, so the
  parser emits its own note-off;
* events `0xFE`/`0xFD` are HMI extras whose payload lengths differ per sub-command.
  ZMusic skips 6 bytes for `0x13`; on the Daggerfall files that desynchronises
  `TAVERN.HMI` and 27 other songs. **10 bytes** parses the whole corpus cleanly
  (`0x10`: 2 + extra + 4, `0x12`: 2, `0x13`: 10, `0x14`: 2 → loop start,
  `0x15`: 6 → loop end).

Validated over all 131 songs: every track reaches its end-of-track meta,
**104,827 notes**, durations 33.6 s … 348.5 s (mean 127.5 s). Examples:

| song | tracks | notes | events | length | loop |
| --- | --- | --- | --- | --- | --- |
| `D1.HMI` | 4 | 454 | 6060 | 224.4 s | 0 – 217.7 s |
| `DUNGEON.HMI` | 20 | 2115 | 11079 | 222.3 s | 0 – 213.6 s |
| `TAVERN.HMI` | 23 | 576 | 3860 | 34.0 s | 0 – 33.5 s |
| `FOLK1.HMI` | 5 | 437 | 3364 | 33.6 s | 0 – 33.3 s |

### `DAGGER.SND` — sound effects

A BSA of type `0x0200` (id/size directory at the end of the file) with **459
records** (7,661,766 bytes) of raw **8-bit unsigned mono PCM at 11025 Hz**,
contiguous from offset 4. Record ids are not contiguous, so the game (and DFU's
`SoundClips` enum) address sounds **by record index**, which is what `SfxId` in
`src/core/Sfx.h` mirrors:

```text
  0 SpookyHighPitch      22 ArenaFootstep1     105 SwingLowPitch
  3 ArrowShoot           24 DungeonDoorClose   106 SwingHighPitch
  4 ArrowHit             25 DungeonDoorOpen    108-112 Hit1..Hit5
  6 AmbientCrickets      30 ArenaHitSound      115-117 Rat move/bark/attack
 15 BodyFall             32 ArenaFanfareLevelUp 118-120 Imp move/bark/attack
 19 ActivateGears        40 ActivateGrind      ...
 20 ActivateDoorClose    63 AmbientDripShort
 21 AmbientDripLong      65 AmbientWindMoan
 23 ArenaFootstep2       78 DrawWeapon         86-90 SpellImpact Magic/Fire/
 91 FallDamage  92 FallHard  93/94 NormalDoor Close/Open  96 LevelUp        Poison/Shock/Cold
```

Per-monster move/bark/attack triples come from DFU's MIT-licensed `EnemyBasics.cs`
and are baked into `src/core/EnemyData.cpp` by `tools/gen_enemy_data.py`
(`moveSound`, `barkSound`, `attackSound`; `-1` means silent).

## Synthesizer

`Synth` is a small float synthesizer written for a 333 MHz Allegrex: 28 voices
(22 music + 6 reserved for samples), 22050 Hz stereo, rendered in caller-sized
blocks (512 frames on the PSP, 512 in the host tool).

* **Sequencer** — walks the sorted event list by tick; 120 ticks/second, so a
  512-frame block advances `512/22050*120 ≈ 3.06` ticks. Loop start/end come from the HMI
  loop markers, so dungeon themes repeat like the original.
* **Instruments** — GM programs are folded into 8 families (piano, reed, plucked,
  bowed, brass, strings, pad, mallet) with 1–6 detuned sine partials, an
  attack/decay/sustain/release envelope, a lowpass-ish partial roll-off and a
  vibrato LFO. Percussion (channel 10) maps GM drum numbers to short noise/tonal
  hits.
* **Samples** — sfx voices resample the 11025 Hz unsigned bytes to 22050 Hz with
  linear interpolation, apply a per-cue volume and pan, and stop themselves when
  the bank evicts the record they are reading (`SfxEvicted` carries the old
  pointers, so no voice can read freed memory).
* **Mix** — sums voices into stereo with pan, saturates at ±32767 and counts
  cut notes for diagnostics. `kNoteGain = 0.45`, sfx gain `0.62`; measured mixes
  peak at 15k–27k with no clipping (see below).

Host mix cost, measured on this sandbox (Xeon @ 2.60 GHz, `-O2`, 60 s renders
including 4 sound effects):

| track | max voices | notes | wall time | realtime factor |
| --- | --- | --- | --- | --- |
| `D1.HMI` | 8 | 107 | 0.172 s | **348×** |
| `TAVERN.HMI` | 22 | 1058 | 0.231 s | **260×** |

Even if the Allegrex is an order of magnitude slower per thread, the densest
theme still leaves ~25× headroom, and the synth runs on its own thread so frame
hitches cannot starve it (the block queue absorbs ~1 s of jitter).

## PSP output

`PspAudio` reserves one hardware channel with `sceAudioSRCChReserve(512, 22050,
PSP_AUDIO_FORMAT_STEREO)` — sample count is per channel and must be a multiple of
64 — so the **hardware sample-rate converter** upsamples the 22050 Hz mix — the CPU never pays for 44.1 kHz. If SRC reservation fails it falls
back to `sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, …)` at 44100 Hz and resamples
in software (`usingSrcChannel()` reports which path is live).

* audio thread: priority `0x12`, stack 32 KB, `THREAD_ATTR_USER | THREAD_ATTR_VFPU`;
* double buffer `i16 g_pcm[2][1024]` — 512 stereo frames per block, ~23 ms,
  64-byte aligned, in `.bss`, no dynamic allocation on the hot path;
* the game thread never blocks: it pushes commands (`CMD_SFX`, `CMD_SONG`,
  `CMD_STOP`, `CMD_MUSIC_VOLUME`, `CMD_SFX_VOLUME`) and audio samples into a
  64-slot lock-free ring of `volatile` entries;
* `term()` signals the thread and polls its exit flag for up to 300 ms before
  releasing the channel (avoids `sceKernelWaitThreadEnd`, whose pspsdk signature
  varies between releases);
* diagnostics on the HUD/log: `blocksRendered()`, `lastError()`,
  `usingSrcChannel()`, `songCount()`, `sfxCount()`.

Requires `-lpspaudio`; `Makefile.psp` links it inside the existing
`--start-group`.

## Gameplay wiring

`Dungeon` gained a `SoundSink*` and two helpers:

* `cue(id, vol, pan)` — a plain sound;
* `cueAt(enemy, id, player, camForward)` — distance and stereo: volume
  `110 - 70·d/2200` clamped to `[25, 110]`, pan ±90° from the camera right vector
  `{forward.z, 0, -forward.x}`.

Cues fire from `tickEnemies` (aggro bark, attack, hurt, death `BodyFall`,
`ArenaHitSound` when the player is struck), `playerAttack` (swing whoosh —
pitch alternates via a dedicated cosmetic RNG so **classic combat rolls stay
byte-identical** — impact `Hit1..Hit5`, kill, `ArenaFanfareLevelUp` on level-up)
and `pickLock` (`ActivateGrind`). On the PSP, doors, footsteps (alternating
`ArenaFootstep1/2`), ~25 s ambience (`AmbientDripShort` / `AmbientWindMoan`),
player death (`FallHard`) and travel music restarts are driven from `main.cpp`.
Music selection is `D{1 + exteriorId % 10}.HMI`, falling back to `DUNGEON.HMI`.

## Host validation

```sh
./build/dfhost --root data --list-songs                     # index/name/bytes/tracks/notes/seconds/loop
./build/dfhost --root data --music D1.HMI --audio-secs 30 --audio-out d1.wav
./build/dfhost --root data --music TAVERN.HMI --audio-skip 20 --audio-secs 10 --audio-out tavern.wav
./build/dfhost --root data --music DUNGEON.HMI \
    --sfx 106@1.0,108@1.35,116@1.4,22@4.0,25@6.5,30@9.5,15@12.5 \
    --audio-secs 16 --audio-out docs/audio.wav              # staged cues ("id@seconds")
./build/dfhost --root data --sfx-dump 106 --sfx-out swing.wav   # one raw record as WAV
./build/dfhost --root data --audio-rate 44100 --music D1.HMI --audio-secs 5 --audio-out d1-44k.wav
```

`--sfx-dump` prints the record length and writes canonical 8-bit→16-bit mono WAV.

Measured 60 s renders at 22050 Hz stereo (music + 4 staged sound effects):

| render | peak | rms | max voices | notes | cut |
| --- | --- | --- | --- | --- | --- |
| `D1.HMI` | 15450 | 2220 | 8 | 107 | 0 |
| `DUNGEON.HMI` | 17592 | 3277 | 8 | 202 | 0 |
| `TAVERN.HMI` | 25044 | 5461 | 22 | 1058 | 8 |
| `MAGIC_2.HMI` | 26113 | 4766 | 20 | 591 | 0 |

No render clipped (`peak < 32768`). `TAVERN.HMI` is the densest theme in the
archive and exhausts the 22 music voices for a handful of simultaneous notes,
which is reported as `cut 8`; raising the voice budget is a one-line change if a
device has the headroom.

**Pitch proof.** A synthetic A4 (key 69, program 0, `makeTestSong`) rendered at
22050 Hz was analysed with a Goertzel filter over an 8192-sample window: the
dominant bin is **440 Hz**, the second partial at 880 Hz is present but weaker,
and both neighbouring semitones (415.3 / 466.2 Hz) are more than 5× below it.
The same check runs inside `tests/test_core`, together with a real-song check:
`D1.HMI` at t ≈ 24 s holds a single note (key 36, channel 4, program 88) whose
dominant component measures **65.41 Hz** — C2, exactly what the score says
(2nd harmonic 130.8 Hz, ±1 semitone ≤ 281).

`docs/audio.wav` is a 16 s excerpt produced by the host renderer: `DUNGEON.HMI`
with a swing, an impact, a rat bark, footsteps, a door, a player hit and a body
fall staged on a timeline. Peak 17923, rms 2933.

### `tests/test_core`

The audio block of the core test suite covers: both archive layouts and counts;
`D1.HMI` field-by-field (tracks, PPQN, tempo, notes, end tick, seconds, loop);
every note carrying a duration and a matching note-off in sorted order; the
**whole 131-song corpus** parsing with >100,000 notes; truncated/bogus records
being refused; synth silence before the first note, the 440 Hz pitch check,
bit-identical renders from two independent synths, real-song level/polyphony
bounds and loop rewind; `DAGGER.SND` record sizes and byte-exact extraction
(records start on the unsigned silence level 128); budget eviction reporting the
old pointer; music+sfx mixing without clipping; the WAV header round trip; and
the gameplay cue wiring (swing, monster bark, `Hit1..Hit5` on impact,
`ActivateGrind` while picking a lock, and silence again after `setSoundSink(nullptr)`).

```text
$ ./build/test_core data
core tests passed
```

## Known limits

* No 3D reverb/EQ, no positional attenuation model beyond the linear volume/pan
  above, and no OPL3 emulation — the General-MIDI-style rendering is the
  synthesizer's own approximation of the original AdLib/Sound Canvas mixes.
* Voice stealing cuts notes on the densest themes (8 cuts in 60 s of `TAVERN.HMI`).
* Music selection is dungeon-only for now; weather and town themes exist in the
  archive and can be routed once the overworld data (`WOODS.WLD`) is available.
* Not yet validated on real hardware: SRC channel behaviour, thread priority
  interaction with the GU renderer, and audible output.
