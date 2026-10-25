// Offsets.h — every Sky 0.35.1.423272 RVA, struct field and native typedef
// used by the mod. Derived in CHAIN_ANALYSIS.md; keep that doc in sync.
#pragma once

#include <cstdint>

namespace piano {

// --- Native function signatures ----------------------------------------------

using GameUpdateFn = void (*)(void *);
using WriteRpcFn = void (*)(void *, void *, void *);
using GetInstrumentFn = void *(*)(void *, unsigned char);
using InstrumentNameFn = const char *(*)(unsigned char, unsigned char);
using PlaySoundFn = void (*)(void *, void *, unsigned, char, unsigned,  // ret,game,note,sustain,pid
                             unsigned char, unsigned, int, float, float);  // type,sub,a8,vol,vol2
using PlayAnimFn = void (*)(void *, int, void *, unsigned char, unsigned);  // (ret,note,ins,type,sub)
using NumSustainFn = unsigned (*)(void *);              // AvatarSound::NumSustainNotesPlaying(bank)
using StopSustainFn = void (*)(void *, unsigned, int);  // sub_14165E330(bank, note, flag)
using IsSustainPlayingFn = __int64 (*)(void *, unsigned);  // sub_14165E7F0(bank, note) -> ringing?
using NoteTimeFn = void (*)(void *, int);               // sub_1405F69B0(game, note)

// --- RVAs (0.35.1.423272 IDB) -------------------------------------------------

constexpr std::uintptr_t kRvaGameUpdate = 0x116EC90;      // Game::Update
constexpr std::uintptr_t kRvaWriteRpc = 0x1BC3F0;         // NetRpcClient::WriteRpc
constexpr std::uintptr_t kRvaGetInstrument = 0x1763B00;   // sub_141763B00(avatarMgr, bool)
constexpr std::uintptr_t kRvaInstrumentName = 0xEC55D0;   // sub_140EC55D0(type, note) -> name
constexpr std::uintptr_t kRvaPlaySound = 0x5F7730;        // PianoButton::PlaySound(ret,game,...)
constexpr std::uintptr_t kRvaPlayAnim = 0x5F8C00;         // PianoButton::PlayAnimation(ret,note,ins,type,sub)
constexpr std::uintptr_t kRvaNoteTimeFn = 0x5F69B0;       // note-time bookkeeping(game, note)
constexpr std::uintptr_t kRvaNumSustain = 0x165E860;      // NumSustainNotesPlaying(bank)
constexpr std::uintptr_t kRvaStopSustain = 0x165E330;     // StopSustainNote(bank, note, flag)
constexpr std::uintptr_t kRvaIsSustainPlaying = 0x165E7F0;  // IsSustainNotePlaying(bank, note)
constexpr std::uintptr_t kRvaPianoButtonRpcVtable = 0x1E6FCD0;
constexpr std::uintptr_t kRvaRpcChain = 0x1E3CDE0;        // rpc+0x18 global
constexpr std::uintptr_t kRvaMetaGlobal = 0x2548418;      // qword_142548418 (+56 = metaId)
constexpr std::uintptr_t kRvaInvalidPlayerId = 0x2548138;  // dword_142548138

// --- PianoButton array inside the instrument ----------------------------------
// instrument + 0x60 + 0x400*note.

constexpr std::size_t kSlotBase = 0x60;
constexpr std::size_t kSlotStride = 0x400;
constexpr std::size_t kBtnNote = 0x380;      // u32 note
constexpr std::size_t kBtnPlayerId = 0x384;  // u32 (InvalidPlayerId for a local tap)
constexpr std::size_t kBtnType = 0x388;      // u8 instrumentType (local)
constexpr std::size_t kBtnType2 = 0x389;     // u8 variant 2
constexpr std::size_t kBtnPitch = 0x39C;     // u32 pitch
constexpr std::size_t kBtnActive = 0x3B0;    // u8 active/sustain
constexpr std::size_t kBtnFlags = 0x3E8;     // u8 (Execute clears it)

// --- Instrument fields (CHAIN_ANALYSIS.md §1) ---------------------------------

constexpr std::size_t kInstrType = 76;             // u8: 2 -> 8 keys, 4 -> 4 keys, else 15
constexpr std::size_t kInstrType2 = 77;            // u8
constexpr std::size_t kInstrPitch = 78;            // u8
constexpr std::size_t kInstrSustain = 79;          // u8 == 1 for held/sustain instruments (piano = 0)
constexpr std::size_t kInstrBank = 80;             // AvatarSound* (sustain registry)
constexpr std::size_t kInstrPoseController = 0xA8; // pose controller (only while instrument UI is open)
constexpr std::size_t kInstrMuteGate = 47248;      // != 0 -> OnStart skips the RPC
constexpr std::size_t kInstrAvatarLoaded = 68108;  // signed char < 0 required by PlaySound's gate
constexpr std::size_t kInstrNumNotes = 75128;      // int 15/8/4 (refreshed by the menu)
constexpr std::size_t kInstrPlayerIdXor = 47200;   // *(u32*)(instr+47200) ^ (u32)(instr+47200)

// --- Game fields used by the sound+RPC path -----------------------------------

constexpr std::size_t kGameClockOffset = 0x1B0;     // -> clock obj; +0x28 float picks +0xE0/+0xF0 double
constexpr std::size_t kGameNetClientOffset = 0x1C8;  // -> client; *(client+0x38) = net module
constexpr std::size_t kGameSheetMgrOffset = 0x740;  // -> mgr; *(mgr+64)+75568 > 0 -> OnStart skips RPC

constexpr std::size_t kGameAvatarOffset = 0x318;    // Game -> avatar manager
constexpr std::size_t kGameReadableSize = 0x1000;
constexpr std::size_t kGameLuaStateOffset = 0x20;
constexpr std::size_t kInstrumentReadableSize = 0x12800;
constexpr std::size_t kAvatarMgrReadableSize = 68164;  // covers the +68156 flag read

// --- Note range ----------------------------------------------------------------

constexpr int kNoteMin = 0;
constexpr int kNoteMax = 14;  // melodic notes are 0..14 (sub_1405F88A0 skips >= 15)

constexpr unsigned kDefaultSustainMs = 500;

}  // namespace piano
