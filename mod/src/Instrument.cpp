// Instrument.cpp — equipped-instrument snapshot.
#include "Instrument.h"

#include "GameState.h"

namespace piano {

InstrumentStatus GetInstrumentStatus() {
  InstrumentStatus st;
  if (g_game == nullptr || g_gameBase == nullptr) {
    return st;
  }
  if (!IsReadable(g_game, kGameReadableSize)) {
    return st;
  }
  void *avatarMgr = *reinterpret_cast<void **>(static_cast<char *>(g_game) + kGameAvatarOffset);
  if (!IsReadable(avatarMgr, kAvatarMgrReadableSize)) {
    return st;
  }
  auto getInstrument = reinterpret_cast<GetInstrumentFn>(
      reinterpret_cast<std::uintptr_t>(g_gameBase) + kRvaGetInstrument);
  void *instrument = getInstrument(avatarMgr, 1);
  if (!IsReadable(instrument, kInstrumentReadableSize)) {
    return st;
  }
  const auto *ins = static_cast<const unsigned char *>(instrument);
  st.instrument = instrument;
  st.type = ins[kInstrType];
  st.pitch = ins[kInstrPitch];
  st.sustain = ins[kInstrSustain] == 1;
  st.inWorld = static_cast<signed char>(ins[kInstrAvatarLoaded]) < 0;
  const int numNotes = *reinterpret_cast<const int *>(ins + kInstrNumNotes);
  st.numNotes = (numNotes > 0 && numNotes <= 16) ? numNotes : 15;
  st.playerId = *reinterpret_cast<const unsigned *>(ins + kInstrPlayerIdXor)
      ^ static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(instrument) + kInstrPlayerIdXor);
  st.valid = true;
  return st;
}

bool IsSustainingInstrument(int type, int sub) {
  return type == 1                                  // Contrabass/4thAnnivBass/Cello
      || type == 3                                  // Flute/Horn family
      || (type == 0 && (sub == 9 || sub == 11));    // MicAurora, Violin
}

const char *InstrumentName(unsigned char type, unsigned char sub) {
  if (g_gameBase == nullptr) {
    return nullptr;
  }
  auto nameFn = reinterpret_cast<InstrumentNameFn>(
      reinterpret_cast<std::uintptr_t>(g_gameBase) + kRvaInstrumentName);
  return nameFn(type, sub);
}

}  // namespace piano
