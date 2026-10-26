// Instrument.cpp — equipped-instrument snapshot.
#include "Instrument.h"

#include "GameState.h"
#include "Sigs.h"

namespace piano {

InstrumentStatus GetInstrumentStatus() {
  InstrumentStatus st;
  if (g_game == nullptr || g_gameBase == nullptr) {
    return st;
  }
  if (!IsReadable(g_game, kGameReadableSize)) {
    return st;
  }
  void *avatarMgr = *reinterpret_cast<void **>(static_cast<char *>(g_game) + g_off->avatarMgr);
  if (!IsReadable(avatarMgr, kAvatarMgrReadableSize)) {
    return st;
  }
  if (g_funcs.getInstrument == nullptr) {
    return st;
  }
  auto getInstrument = reinterpret_cast<GetInstrumentFn>(g_funcs.getInstrument);
  void *instrument = getInstrument(avatarMgr, 1);
  // Newer builds only need the fields up to +0x11000 validated, since the
  // layout past that point is not trusted (see Offsets.h).
  const std::size_t need = g_off->instrLayoutVerified ? kInstrumentReadableSize : 0x11000;
  if (!IsReadable(instrument, need)) {
    return st;
  }
  const auto *ins = static_cast<const unsigned char *>(instrument);
  st.instrument = instrument;
  st.inWorld = static_cast<signed char>(ins[g_off->instrAvatarLoaded]) < 0;
  if (g_off->instrLayoutVerified) {
    st.type = ins[g_off->instrType] <= 4 ? ins[g_off->instrType] : 0;
    st.pitch = ins[g_off->instrPitch] <= kNoteMax ? ins[g_off->instrPitch] : 0;
    st.altType = ins[g_off->instrType2];
    st.sustain = ins[g_off->instrSustain] == 1;
    const int numNotes = *reinterpret_cast<const int *>(ins + g_off->instrNumNotes);
    st.numNotes = (numNotes > 0 && numNotes <= 16) ? numNotes : 15;
  } else {
    // This build's instrument fields are not mapped. Default to the piano voice
    // (0/1) — exactly what a manual piano key press broadcasts — and let
    // CaptureNativeVoice refresh it from a real key press.
    st.type = 0;
    st.pitch = 1;
    st.altType = 0;
    st.sustain = false;
    st.numNotes = 15;
  }

  st.playerId = *reinterpret_cast<const unsigned *>(ins + g_off->instrPlayerIdXor)
      ^ static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(instrument) + g_off->instrPlayerIdXor);
  st.valid = true;
  return st;
}

bool IsSustainingInstrument(int type, int sub) {
  return type == 1                                  // Contrabass/4thAnnivBass/Cello
      || type == 3                                  // Flute/Horn family
      || (type == 0 && (sub == 9 || sub == 11));    // MicAurora, Violin
}

const char *InstrumentName(unsigned char type, unsigned char sub) {
  if (g_gameBase == nullptr || g_funcs.instrumentName == nullptr) {
    return nullptr;
  }
  auto nameFn = reinterpret_cast<InstrumentNameFn>(g_funcs.instrumentName);
  return nameFn(type, sub);
}

}  // namespace piano
