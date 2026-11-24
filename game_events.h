#pragma once
#include <Arduino.h>

// Small publish/subscribe bus for gameplay events. Gameplay systems can emit
// events without knowing which optional feature systems are listening.
//
// Event IDs are append-only: task/event consumers may persist their numeric
// meaning, so never reorder or reuse an existing value.
enum GameEv : uint8_t {
  EV_NONE = 0,
  EV_HATCH,
  EV_EVOLVE,
  EV_FEED,
  EV_CLEAN,
  EV_PLAY,
  EV_TRAIN,
  EV_CARE_DAY,
  EV_GYM_WIN,
  EV_GYM_LOSS,
  EV_REMATCH_WIN,
  EV_WILD_SEEN,
  EV_WILD_WIN,
  EV_WILD_CATCH,
  EV_TOWER_WIN,
  EV_TOWER_LOSS,
  EV_BREED,
  EV_ITEM_USED,
  EV_DEX_REGISTER,
  EV_CHALLENGE_WIN,
  EV_RANK_UP,
  EV_SHINY_FOUND,
  EV_CHAIN,
  EV_GIFT_SENT,
  EV_GIFT_RECEIVED,
  EV_VISIT,
  EV_COUNT
};

typedef void (*GameEvSink)(GameEv ev, int16_t arg);

void gameEvent(GameEv ev, int16_t arg = 0);

#define GAME_EV_SINKS 10
bool gameEventSubscribe(GameEvSink sink);
void gameEventClearSinks();  // tests only
