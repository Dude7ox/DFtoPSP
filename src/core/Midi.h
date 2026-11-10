#pragma once
#include "Types.h"
#include <string>
#include <vector>
namespace dfpsp {
// Daggerfall music lives in MIDI.BSA as HMI songs: the "HMI-MIDISONG061595"
// format Human Machine Interfaces wrote for the SOS audio middleware the
// original FALL.EXE used. Layout of one song record:
//   0x00  "HMI-MIDISONG061595" magic
//   0xD4  uint16 pulses per quarter note (the file also stores value<<2 at 0xD2)
//   0xE4  uint32 track count
//   0xE8  uint32 offset of the track table, trackCount uint32 song offsets
// Every track starts with "HMI-MIDITRACK"; the uint32 at track+0x57 is the
// offset of the event stream inside that track and track+0x99 holds eight
// device designations (0xA000 generic GM, 0xA002 OPL2, 0xA00A GUS). Daggerfall
// designates every track for GM, so all tracks are playable by this synth.
//
// The event stream is standard MIDI with SMF variable length deltas and running
// status, with two differences: a note-on carries its own duration instead of a
// matching note-off, and status 0xFE introduces HMI sub events (0x10 device
// data, 0x12/0x13 unknown, 0x14 loop start, 0x15 loop end). No tempo meta
// events are stored; SOS played HMI songs at a fixed four seconds per quarter
// note, so one tick is 1/(value at 0xD4) seconds, 120 Hz for Daggerfall.
enum MidiKind : u8 {
    MidiNote=0, MidiNoteOff=1, MidiProgram=2, MidiControl=3, MidiBend=4,
    MidiLoopStart=5, MidiLoopEnd=6
};
struct MidiEvent {
    u32 tick=0;                        // absolute ticks from the song start
    u32 dur=0;                         // note length in ticks (notes only)
    u8 kind=MidiNote, ch=0, d1=0, d2=0; // note: key/velocity, off: key
};
struct MidiSong {
    bool ok=false;
    std::string error;
    u32 ppqn=480;                      // pulses per quarter note (value<<2)
    u32 tempoUs=4000000;               // microseconds per quarter note
    u32 endTick=0, loopStartTick=0, loopEndTick=0;
    int tracks=0;
    u32 notes=0;
    std::vector<MidiEvent> events;     // merged, sorted by tick then by kind rank
    double secondsPerTick()const{return (tempoUs/1000000.0)/(double)ppqn;}
    double seconds()const{return endTick*secondsPerTick();}
    double ticksPerSecond()const{return ppqn>0?(1000000.0*ppqn)/tempoUs:120.0;}
};
// Parses one HMI song record (for example a MIDI.BSA entry). Note-offs are
// synthesised from the durations, so playback only walks `events` forward.
bool parseHmiSong(const u8* data,size_t len,MidiSong& out);
// Builds a minimal song in code, used by the tests to check synth pitch.
MidiSong makeTestSong(int key,int program,double seconds,double tempoUs=4000000.0,u32 ppqn=480);
} // namespace
