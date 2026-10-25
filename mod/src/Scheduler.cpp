// Scheduler.cpp — pending-note queues + the game-thread drain.
#include "Scheduler.h"

#include "LuaRuntime.h"
#include "NotePlayer.h"

namespace piano {

namespace {
SRWLOCK g_queueLock = SRWLOCK_INIT;
std::vector<PendingNote> g_queue;
std::vector<PendingStop> g_stopQueue;
}  // namespace

void QueueNote(int note, unsigned delayMs, int typeOv, int subOv, int durMs, bool noRpc) {
  AcquireSRWLockExclusive(&g_queueLock);
  PendingNote item{};
  item.due = GetTickCount64() + delayMs;
  item.note = note;
  item.typeOv = typeOv;
  item.subOv = subOv;
  item.durMs = durMs;
  item.noRpc = noRpc;
  g_queue.push_back(std::move(item));
  ReleaseSRWLockExclusive(&g_queueLock);
}

void QueueStop(int note, unsigned delayMs, bool noRpc) {
  AcquireSRWLockExclusive(&g_queueLock);
  g_stopQueue.push_back(PendingStop{GetTickCount64() + delayMs, note, noRpc});
  ReleaseSRWLockExclusive(&g_queueLock);
}

void QueueNoteFull(int note, unsigned delayMs, const std::string &sound, int menuPitch,
                   double volume) {
  AcquireSRWLockExclusive(&g_queueLock);
  PendingNote item{};
  item.due = GetTickCount64() + delayMs;
  item.note = note;
  item.playLocal = true;
  item.sound = sound;
  item.menuPitch = menuPitch;
  item.volume = volume;
  g_queue.push_back(std::move(item));
  ReleaseSRWLockExclusive(&g_queueLock);
}

void ClearQueue() {
  AcquireSRWLockExclusive(&g_queueLock);
  g_queue.clear();
  g_stopQueue.clear();
  ReleaseSRWLockExclusive(&g_queueLock);
}

void TickQueue() {
  std::vector<PendingNote> due;
  std::vector<PendingStop> stopsDue;
  {
    AcquireSRWLockExclusive(&g_queueLock);
    const std::uint64_t now = GetTickCount64();
    if (!g_queue.empty()) {
      std::vector<PendingNote> keep;
      keep.reserve(g_queue.size());
      for (const auto &item : g_queue) {
        if (item.due <= now) {
          due.push_back(item);
        } else {
          keep.push_back(item);
        }
      }
      g_queue.swap(keep);
    }
    if (!g_stopQueue.empty()) {
      std::vector<PendingStop> keep;
      keep.reserve(g_stopQueue.size());
      for (const auto &item : g_stopQueue) {
        if (item.due <= now) {
          stopsDue.push_back(item);
        } else {
          keep.push_back(item);
        }
      }
      g_stopQueue.swap(keep);
    }
    ReleaseSRWLockExclusive(&g_queueLock);
  }
  for (const auto &stop : stopsDue) {
    StopNote(stop.note, stop.noRpc);
  }
  for (const auto &item : due) {
    if (item.playLocal) {
      EmitLocal(item.sound.c_str(), item.menuPitch, item.volume);
    }
    const int result = PlayNote(item.note, item.typeOv, item.subOv, item.durMs, item.noRpc);
    static int loggedFails = 0;
    if (result != kPlayOk && loggedFails < 20) {
      ++loggedFails;
      Log("[piano-rpc] queued note %d failed: %s", item.note, PlayResultName(result));
    }
  }
}

}  // namespace piano
