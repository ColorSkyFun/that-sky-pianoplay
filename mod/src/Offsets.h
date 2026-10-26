// Offsets.h — every Sky RVA, struct field and native typedef used by the mod,
// per game build. A build profile is selected once in HTModOnInit (see
// DetectGameVersion) and every consumer reads through g_off, so one DLL
// supports both the 0.35.1 release build and the newer dump-based build.
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
using StopSustainFn = void (*)(void *, unsigned, int);  // (bank, note, flag)
using IsSustainPlayingFn = __int64 (*)(void *, unsigned);  // (bank, note) -> ringing?
using NoteTimeFn = void (*)(void *, int);               // (game, note)

enum class GameVersion { kUnknown = 0, kV1 = 1, kV2 = 2 };

// One entry per supported build. RVAs are relative to the Sky.exe module base.
struct Offsets {
  const char *name;
  GameVersion version;

  // --- entry points ---------------------------------------------------------
  std::uintptr_t rvaGameUpdate;
  std::uintptr_t rvaWriteRpc;
  std::uintptr_t rvaPlaySound;
  std::uintptr_t rvaPlayAnim;
  std::uintptr_t rvaNoteTime;
  std::uintptr_t rvaGetInstrument;
  std::uintptr_t rvaInstrumentName;  // 0 = unavailable
  std::uintptr_t rvaNumSustain;      // 0 = unavailable
  std::uintptr_t rvaStopSustain;
  std::uintptr_t rvaIsSustainPlaying;  // 0 = unavailable

  // --- RPC data -------------------------------------------------------------
  std::uintptr_t rvaRpcVtable;       // PianoButtonRpc vtable
  std::uintptr_t rvaRpcChain;        // rpc+0x18 chain object
  std::uintptr_t rvaMetaGlobal;      // holder of a pointer; +56 = metaId
  std::uintptr_t rvaLocalPlayerId;   // dword the game uses as "local player"

  // --- Game fields ----------------------------------------------------------
  std::size_t netClient;    // -> client; +0x38 = net module
  std::size_t gameClock;    // -> clock object
  std::size_t clockSlotA;   // double slot when clock+0x28 float == 0
  std::size_t clockSlotB;   // double slot otherwise
  std::size_t avatarMgr;    // -> avatar manager
  std::size_t sheetMgr;     // -> sheet manager; inner = *(mgr+sheetInner),
                            // +sheetGate > 0 means a music sheet is running
  std::size_t sheetInner;
  std::size_t sheetGate;

  // --- Instrument fields ----------------------------------------------------
  std::size_t instrType;          // u8
  std::size_t instrType2;         // u8
  std::size_t instrPitch;         // u8
  std::size_t instrSustain;       // u8 == 1 for held/sustain instruments
  std::size_t instrBank;          // AvatarSound* (sustain registry)
  std::size_t instrAvatarLoaded;  // signed char < 0 when in world
  std::size_t instrNumNotes;      // int 15/8/4
  std::size_t instrPlayerIdXor;   // *(u32*) ^ (u32)(instr + offset)
  std::size_t instrMuteGate;      // only read when muteGateEnabled
  std::size_t instrPose;          // pose controller (only while the UI is open)

  bool muteGateEnabled;  // read instrMuteGate before broadcasting
  bool animTakesGame;    // PlayAnimation(game,...) instead of (ret,...)
  // When false, instrSustain/instrNumNotes are treated as unknown (no sustain
  // flag, full 15-key range) instead of trusting a possibly misaligned read.
  bool instrLayoutVerified;
};

extern const Offsets *g_off;
extern GameVersion g_version;

// Reads SizeOfImage/TimeDateStamp from the loaded module's PE header. Defaults
// to kV1 when the header is unreadable or unknown (keeps old behaviour).
GameVersion DetectGameVersion(std::uintptr_t moduleBase);
void SelectOffsets(GameVersion v);

// --- Values shared by every build -------------------------------------------

constexpr std::size_t kGameReadableSize = 0x1000;
constexpr std::size_t kInstrumentReadableSize = 0x12800;
constexpr std::size_t kAvatarMgrReadableSize = 68164;  // covers the +68156 flag read
constexpr std::size_t kGameLuaStateOffset = 0x20;

constexpr int kNoteMin = 0;
constexpr int kNoteMax = 14;  // melodic notes are 0..14

constexpr unsigned kDefaultSustainMs = 500;

}  // namespace piano
