#include "Arduino.h"
#include "game_events.h"
#include <cstdio>

static int bad = 0;
static int calls[11] = {0};
static GameEv lastEv[11];
static int16_t lastArg[11];

static void record(uint8_t i, GameEv ev, int16_t arg) {
  calls[i]++;
  lastEv[i] = ev;
  lastArg[i] = arg;
}
#define SINK(N) static void sink##N(GameEv ev, int16_t arg) { record(N, ev, arg); }
SINK(0) SINK(1) SINK(2) SINK(3) SINK(4) SINK(5)
SINK(6) SINK(7) SINK(8) SINK(9) SINK(10)
#undef SINK

static void ck(bool ok, const char *what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  GameEvSink sinks[11] = {
    sink0, sink1, sink2, sink3, sink4, sink5,
    sink6, sink7, sink8, sink9, sink10
  };

  gameEventClearSinks();
  ck(!gameEventSubscribe(nullptr), "null subscribers are rejected");
  ck(gameEventSubscribe(sinks[0]), "first subscriber registers");
  ck(gameEventSubscribe(sinks[0]), "registering the same subscriber twice is harmless");
  gameEvent(EV_HATCH, 25);
  ck(calls[0] == 1 && lastEv[0] == EV_HATCH && lastArg[0] == 25,
     "one event reaches a subscriber exactly once with its argument");
  gameEvent(EV_NONE, 99);
  gameEvent(EV_COUNT, 99);
  ck(calls[0] == 1, "sentinel and out-of-range event IDs are ignored");

  ck(gameEventSubscribe(sinks[1]), "a second subscriber registers");
  gameEvent(EV_FEED, 1);
  ck(calls[0] == 2 && calls[1] == 1 &&
     lastEv[1] == EV_FEED && lastArg[1] == 1,
     "events are broadcast to all registered subscribers");

  for (int i = 2; i < GAME_EV_SINKS; i++)
    ck(gameEventSubscribe(sinks[i]), "subscriber slot registers");
  ck(!gameEventSubscribe(sinks[10]), "subscriber capacity is bounded");
  gameEvent(EV_TOWER_WIN, 7);
  bool all = true;
  for (int i = 0; i < GAME_EV_SINKS; i++)
    if (calls[i] == 0 || lastEv[i] != EV_TOWER_WIN || lastArg[i] != 7) all = false;
  ck(all, "all ten subscribers receive the same event");

  ck(gameEventUnsubscribe(sinks[4]), "one subscriber can be removed independently");
  int removedBefore = calls[4];
  int retainedBefore = calls[5];
  gameEvent(EV_CHAIN, 9);
  ck(calls[4] == removedBefore && calls[5] == retainedBefore + 1,
     "removing one listener leaves every other listener connected");
  ck(!gameEventUnsubscribe(sinks[4]), "removing an absent listener is harmless");

  int before[10];
  for (int i = 0; i < 10; i++) before[i] = calls[i];
  gameEventClearSinks();
  gameEvent(EV_VISIT, 42);
  bool quiet = true;
  for (int i = 0; i < 10; i++) if (calls[i] != before[i]) quiet = false;
  ck(quiet, "clearing subscribers prevents further delivery");

  std::printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
