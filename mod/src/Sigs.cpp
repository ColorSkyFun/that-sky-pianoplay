// Sigs.cpp — build profiles + signature/RVA resolution.
//
// V1 (0.35.1.423272, live Steam build): every address comes from an E8 call-site
// signature generated with tools/siggen.py from the on-disk Sky.exe.
// V2 (newer dump build): the addresses below were recovered by mapping the
// PianoButton cluster; see docs/BUILD_VERSIONS.md for the evidence.
#include "Sigs.h"

#include <cstring>

#include "Common.h"
#include "GameState.h"

namespace piano {

// --- build profiles ---------------------------------------------------------

namespace {

const Offsets kOffsetsV1 = {
    /*name*/ "0.35.1.423272",
    /*version*/ GameVersion::kV1,
    /*rvaGameUpdate*/ 0x116EC90,
    /*rvaWriteRpc*/ 0x1BC3F0,
    /*rvaPlaySound*/ 0x5F7730,
    /*rvaPlayAnim*/ 0x5F8C00,
    /*rvaNoteTime*/ 0x5F69B0,
    /*rvaGetInstrument*/ 0x1763B00,
    /*rvaInstrumentName*/ 0xEC55D0,
    /*rvaNumSustain*/ 0x165E860,
    /*rvaStopSustain*/ 0x165E330,
    /*rvaIsSustainPlaying*/ 0x165E7F0,
    /*rvaRpcVtable*/ 0x1E6FCD0,
    /*rvaRpcChain*/ 0x1E3CDE0,
    /*rvaMetaGlobal*/ 0x2548418,
    /*rvaLocalPlayerId*/ 0x2548138,
    /*netClient*/ 0x1C8,
    /*gameClock*/ 0x1B0,
    /*clockSlotA*/ 0xE0,
    /*clockSlotB*/ 0xF0,
    /*avatarMgr*/ 0x318,
    /*sheetMgr*/ 0x740,
    /*sheetInner*/ 64,
    /*sheetGate*/ 75568,
    /*instrType*/ 76,
    /*instrType2*/ 77,
    /*instrPitch*/ 78,
    /*instrSustain*/ 79,
    /*instrBank*/ 80,
    /*instrAvatarLoaded*/ 68108,
    /*instrNumNotes*/ 75128,
    /*instrPlayerIdXor*/ 47200,
    /*instrMuteGate*/ 47248,
    /*instrPose*/ 0xA8,
    /*muteGateEnabled*/ true,
    /*animTakesGame*/ false,
    /*instrLayoutVerified*/ true,
};

// Recovered from Sky_dump.exe (TimeDateStamp 0x6ADB03C9, SizeOfImage 0x843D000):
//   OnStart 0x7C9A30 builds the RPC (vtable 0x22EC510, chain 0x22AFAE0,
//   meta global 0x436B100) and calls PlaySound 0x7C9620, NoteTime 0x7C88B0,
//   WriteRpc 0x2B8DD0, PlayAnimation 0x7CAC00.
//   Game::Update 0x14CE980 ([rcx+0x1C80] frame-counter cache).
//   GetInstrument 0x1B331D0, StopSustain 0x1A26C60, NumSustain 0x1A271A0,
//   IsSustainPlaying 0x1A27130, InstrumentName 0x11CD020.
//   Instrument bank +0x90, playerId XOR +0xB8E0, avatar-loaded +0x10AA0,
//   game netClient +0x1E8, clock +0x1D0 (doubles +0xE8/+0xF8), avatarMgr +0x330,
//   sheet manager +0x7C8 -> +0x40 -> +0x19FB0.
// Fields marked "unverified" keep the V1 values guarded by sanity checks in
// Instrument.cpp, because no code path in the dump pinned them down.
const Offsets kOffsetsV2 = {
    /*name*/ "new dump (0x6ADB03C9)",
    /*version*/ GameVersion::kV2,
    /*rvaGameUpdate*/ 0x14CE980,
    /*rvaWriteRpc*/ 0x2B8DD0,
    /*rvaPlaySound*/ 0x7C9620,
    /*rvaPlayAnim*/ 0x7CAC00,
    /*rvaNoteTime*/ 0x7C88B0,
    /*rvaGetInstrument*/ 0x1B331D0,
    /*rvaInstrumentName*/ 0x11CD020,
    /*rvaNumSustain*/ 0x1A271A0,
    /*rvaStopSustain*/ 0x1A26C60,
    /*rvaIsSustainPlaying*/ 0x1A27130,
    /*rvaRpcVtable*/ 0x22EC510,
    /*rvaRpcChain*/ 0x22AFAE0,
    /*rvaMetaGlobal*/ 0x436B100,
    /*rvaLocalPlayerId*/ 0x436ADA0,
    /*netClient*/ 0x1E8,
    /*gameClock*/ 0x1D0,
    /*clockSlotA*/ 0xE8,
    /*clockSlotB*/ 0xF8,
    /*avatarMgr*/ 0x330,
    /*sheetMgr*/ 0x7C8,
    /*sheetInner*/ 0x40,
    /*sheetGate*/ 0x19FB0,
    /*instrType*/ 76,           // unverified
    /*instrType2*/ 77,          // unverified
    /*instrPitch*/ 78,          // unverified
    /*instrSustain*/ 79,        // unverified
    /*instrBank*/ 0x90,
    /*instrAvatarLoaded*/ 0x10AA0,
    /*instrNumNotes*/ 75128,    // unverified
    /*instrPlayerIdXor*/ 0xB8E0,
    /*instrMuteGate*/ 0,        // not pinned down: gate disabled
    /*instrPose*/ 0xA8,         // unverified
    /*muteGateEnabled*/ false,
    /*animTakesGame*/ true,
    /*instrLayoutVerified*/ false,
};

constexpr std::uint32_t kSizeOfImageV1 = 0x3171000;
constexpr std::uint32_t kTimestampV1 = 0x6ACFD5EF;
constexpr std::uint32_t kSizeOfImageV2 = 0x843D000;
constexpr std::uint32_t kTimestampV2 = 0x6ADB03C9;

}  // namespace

GameFuncs g_funcs;
GameData g_data;

const Offsets *g_off = &kOffsetsV1;
GameVersion g_version = GameVersion::kV1;

void SelectOffsets(GameVersion v) {
  g_version = v;
  g_off = (v == GameVersion::kV2) ? &kOffsetsV2 : &kOffsetsV1;
  Log("[piano-rpc] build profile: %s", g_off->name);
}

GameVersion DetectGameVersion(std::uintptr_t moduleBase) {
  if (moduleBase == 0 || !IsReadable(reinterpret_cast<const void *>(moduleBase), 0x40)) {
    return GameVersion::kV1;
  }
  const auto *dos = reinterpret_cast<const unsigned char *>(moduleBase);
  const std::uint32_t lfanew = *reinterpret_cast<const std::uint32_t *>(dos + 0x3C);
  if (lfanew < 0x40 || lfanew > 0x1000) {
    return GameVersion::kV1;
  }
  const auto *nt = dos + lfanew;
  if (!IsReadable(nt, 0x60) || std::memcmp(nt, "PE\0\0", 4) != 0) {
    return GameVersion::kV1;
  }
  const std::uint32_t timestamp = *reinterpret_cast<const std::uint32_t *>(nt + 8);
  const std::uint32_t sizeOfImage = *reinterpret_cast<const std::uint32_t *>(nt + 24 + 56);
  Log("[piano-rpc] module PE: timestamp=0x%X sizeOfImage=0x%X", timestamp, sizeOfImage);
  if (sizeOfImage == kSizeOfImageV2 || timestamp == kTimestampV2) {
    return GameVersion::kV2;
  }
  if (sizeOfImage == kSizeOfImageV1 || timestamp == kTimestampV1) {
    return GameVersion::kV1;
  }
  Log("[piano-rpc] unknown build; assuming V1");
  return GameVersion::kV1;
}

// --- V1 signatures ----------------------------------------------------------

namespace {

// pattern (E8 concrete, rel32 wildcarded), scan type, offset of the E8 byte
const HTAsmSig kSigWriteRpc = {
    "48 8B 48 38 4C 8D 41 38 48 8D 55 30 E8 ? ? ? ? 48 8B 8F 08 05 00 00 48 89 DA",
    HTSigScanType_E8, 12};
const HTAsmSig kSigPlaySound = {
    "3F 41 80 E1 01 48 8D 4D E0 48 89 F2 E8 ? ? ? ? 0F B6 47 31 44 0F B6 4F 30 44",
    HTSigScanType_E8, 12};
const HTAsmSig kSigPlayAnim = {
    "44 24 20 44 89 FA 4D 89 E0 41 89 F9 E8 ? ? ? ? 44 39 EB 0F 85 41 02 00 00 48",
    HTSigScanType_E8, 12};
const HTAsmSig kSigNoteTime = {
    "C5 16 01 49 89 C6 8B 57 2C 48 89 F1 E8 ? ? ? ? 48 89 F1 48 89 DA E8 3F F9 35",
    HTSigScanType_E8, 12};
const HTAsmSig kSigGetInstrument = {
    "C9 74 5C 48 8B 8F 18 03 00 00 B2 01 E8 ? ? ? ? 48 8B 40 18 C5 F8 28 00 C5 F8",
    HTSigScanType_E8, 12};
const HTAsmSig kSigInstrumentName = {
    "3D 28 25 A3 01 45 84 E4 4C 0F 45 F8 E8 ? ? ? ? 48 8D 15 19 B9 9F 01 4C 8D 75",
    HTSigScanType_E8, 12};
const HTAsmSig kSigNumSustain = {
    "75 06 80 7F 40 01 75 1D 48 8B 4B 50 E8 ? ? ? ? 83 F8 04 0F 87 6A FE FF FF 44",
    HTSigScanType_E8, 12};
const HTAsmSig kSigStopSustain = {
    "75 34 48 8B 4B 50 8B 57 2C 41 B0 01 E8 ? ? ? ? 48 8B 8E A8 07 00 00 44 8B 47",
    HTSigScanType_E8, 12};
const HTAsmSig kSigIsSustainPlaying = {
    "00 00 48 8B 4B 50 8B 96 80 03 00 00 E8 ? ? ? ? 84 C0 0F 84 81 01 00 00 48 8B",
    HTSigScanType_E8, 12};

// data addresses: signature the referencing instruction, then decode
const HTAsmSig kSigRpcVtable = {
    "06 F5 01 8B 40 38 89 45 F0 48 8D 05 ? ? ? ? 48 89 45 E8 C6 45 1B 00",
    HTSigScanType_Direct, 0};
const HTAsmSig kSigRpcChain = {
    "E8 D5 C3 CB 00 48 8D 45 48 48 8D 0D ? ? ? ? 48 89 4D 48 66 C7 45 50",
    HTSigScanType_Direct, 0};
const HTAsmSig kSigMetaGlobal = {
    "C7 45 08 00 01 48 89 45 F8 48 8B 05 ? ? ? ? 8B 40 38 89 45 F0 48 8D",
    HTSigScanType_Direct, 0};
const HTAsmSig kSigLocalPlayerId = {
    "89 CE 44 8B BD 90 00 00 00 44 8B 2D ? ? ? ? 48 8B 8A 18 03 00 00 45",
    HTSigScanType_Direct, 0};

constexpr std::size_t kDataDispOffset = 12;

// Scan one signature; verify the hit matches the expected RVA (a shifted
// signature would otherwise hand us a garbage pointer).
void *ResolveOne(const HTAsmSig &sig, const char *name, std::uintptr_t expectRva) {
  void *hit = HTSigScan(&sig);
  if (hit == nullptr) {
    Log("[piano-rpc] sig %s: no match", name);
    return nullptr;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(g_gameBase);
  const auto expect = base + expectRva;
  if (reinterpret_cast<std::uintptr_t>(hit) != expect) {
    Log("[piano-rpc] sig %s: hit %p != expected %p (%+lld), trusting the signature",
        name, hit, reinterpret_cast<void *>(expect),
        static_cast<long long>(reinterpret_cast<std::uintptr_t>(hit)) -
            static_cast<long long>(expect));
  }
  return hit;
}

void *ResolveDataRef(const HTAsmSig &sig, const char *name, std::uintptr_t expectRva) {
  void *site = HTSigScan(&sig);
  if (site == nullptr) {
    Log("[piano-rpc] data sig %s: no match", name);
    return nullptr;
  }
  const auto at = reinterpret_cast<std::uintptr_t>(site);
  if (!IsReadable(reinterpret_cast<const void *>(at + kDataDispOffset), 4)) {
    Log("[piano-rpc] data sig %s: site not readable", name);
    return nullptr;
  }
  const int disp = *reinterpret_cast<const int *>(at + kDataDispOffset);
  const auto target = at + kDataDispOffset + 4 + static_cast<std::intptr_t>(disp);
  const auto expect = reinterpret_cast<std::uintptr_t>(g_gameBase) + expectRva;
  if (target != expect) {
    Log("[piano-rpc] data sig %s: resolves to %p, expected %p (using the signature)",
        name, reinterpret_cast<void *>(target), reinterpret_cast<void *>(expect));
  }
  return reinterpret_cast<void *>(target);
}

}  // namespace

int ResolveGameData() {
  const auto base = reinterpret_cast<std::uintptr_t>(g_gameBase);
  void *vtable = nullptr;
  void *chain = nullptr;
  void *meta = nullptr;
  void *pid = nullptr;
  int bySig = 0;
  if (g_version != GameVersion::kV2) {
    vtable = ResolveDataRef(kSigRpcVtable, "RpcVtable", g_off->rvaRpcVtable);
    chain = ResolveDataRef(kSigRpcChain, "RpcChain", g_off->rvaRpcChain);
    meta = ResolveDataRef(kSigMetaGlobal, "MetaGlobal", g_off->rvaMetaGlobal);
    pid = ResolveDataRef(kSigLocalPlayerId, "LocalPlayerId", g_off->rvaLocalPlayerId);
    for (void *p : {vtable, chain, meta, pid}) {
      if (p != nullptr) {
        ++bySig;
      }
    }
  }

  g_data.rpcVtable = vtable ? vtable : reinterpret_cast<void *>(base + g_off->rvaRpcVtable);
  g_data.rpcChain = chain ? chain : reinterpret_cast<void *>(base + g_off->rvaRpcChain);
  g_data.metaGlobal = reinterpret_cast<void **>(
      meta ? meta : reinterpret_cast<void *>(base + g_off->rvaMetaGlobal));
  g_data.localPlayerId = reinterpret_cast<const unsigned *>(
      pid ? pid : reinterpret_cast<void *>(base + g_off->rvaLocalPlayerId));
  Log("[piano-rpc] data addresses: %d/4 by signature (rest by rva)", bySig);
  return bySig;
}

int ResolveGameFunctions() {
  struct Entry {
    void **slot;
    const HTAsmSig *sig;
    const char *name;
    std::uintptr_t rva;
  };
  const Entry entries[] = {
      {&g_funcs.writeRpc, &kSigWriteRpc, "WriteRpc", g_off->rvaWriteRpc},
      {&g_funcs.playSound, &kSigPlaySound, "PlaySound", g_off->rvaPlaySound},
      {&g_funcs.playAnim, &kSigPlayAnim, "PlayAnimation", g_off->rvaPlayAnim},
      {&g_funcs.noteTime, &kSigNoteTime, "NoteTime", g_off->rvaNoteTime},
      {&g_funcs.getInstrument, &kSigGetInstrument, "GetInstrument", g_off->rvaGetInstrument},
      {&g_funcs.instrumentName, &kSigInstrumentName, "InstrumentName", g_off->rvaInstrumentName},
      {&g_funcs.numSustain, &kSigNumSustain, "NumSustainNotesPlaying", g_off->rvaNumSustain},
      {&g_funcs.stopSustain, &kSigStopSustain, "StopSustainNote", g_off->rvaStopSustain},
      {&g_funcs.isSustainPlaying, &kSigIsSustainPlaying, "IsSustainNotePlaying",
       g_off->rvaIsSustainPlaying},
  };

  const auto base = reinterpret_cast<std::uintptr_t>(g_gameBase);
  const bool useSigs = (g_version != GameVersion::kV2);
  int bySig = 0;
  for (const auto &e : entries) {
    void *fn = useSigs ? ResolveOne(*e.sig, e.name, e.rva) : nullptr;
    if (fn != nullptr) {
      *e.slot = fn;
      ++bySig;
    } else if (e.rva != 0) {
      *e.slot = reinterpret_cast<void *>(base + e.rva);
    } else {
      *e.slot = nullptr;
      Log("[piano-rpc] %s: not available on this build; callers skip it", e.name);
    }
  }
  Log("[piano-rpc] function addresses: %d by signature, all resolved from %s",
      bySig, g_off->name);
  return bySig;
}

}  // namespace piano
