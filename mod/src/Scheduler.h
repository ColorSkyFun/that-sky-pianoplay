// Scheduler.h — delayed note queue drained on the game thread.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Common.h"

namespace piano {

struct PendingNote {
  std::uint64_t due;
  int note;
  int typeOv = -1;
  int subOv = -1;
  int durMs = 0;
  bool noRpc = false;          // local sound only (black-midi walls)
  bool playLocal = false;      // piano.noteAfter: local SoundEmitter only
  std::string sound;
  int menuPitch = 0;
  double volume = 1.0;
};

struct PendingStop {
  std::uint64_t due;
  int note;
  bool noRpc = false;
};

// Queue a native note (piano.playAfter).
void QueueNote(int note, unsigned delayMs, int typeOv = -1, int subOv = -1, int durMs = 0,
               bool noRpc = false);

// Queue a note-off for a sustained voice.
void QueueStop(int note, unsigned delayMs, bool noRpc = false);

// Queue a local-sound event (piano.noteAfter).
void QueueNoteFull(int note, unsigned delayMs, const std::string &sound, int menuPitch,
                   double volume);

// Panic clear: drop everything pending.
void ClearQueue();

// Game-thread tick: fires due notes/stops. Called from the Game::Update hook.
void TickQueue();

}  // namespace piano
