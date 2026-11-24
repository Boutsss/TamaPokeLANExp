#pragma once
#include <Arduino.h>
#include "link.h"
#include "trainers.h"

// LAN opponents, remembered for PRACTICE. At the start of every LAN battle the
// rival's team is saved exactly as it arrived over the link (their LinkMons:
// species, level, stats, moves), keyed by their device (the MAC, like the
// head-to-head record) so a rival who renames keeps one slot. The LAN screen's
// PRACTICE row fights any of them later against the battle AI -- no radio, no
// rewards, the same creatures they really brought.
//
// GAME LOGIC ONLY: no gfx. NVS key "ghosts", one blob: [u8 version][u8 count]
// [u16 stride] then `count` records. A record is APPEND-ONLY; the stride
// travels with it so a later build that grows the record still reads this one.

#define GHOST_CAP 4

struct Ghost {
  uint8_t mac[6] = { 0 };       // all zero = keyed by name (no radio: emulator)
  char name[LINK_NAME_LEN] = "";
  uint8_t n = 0;                // creatures in the team
  uint8_t wins = 0, losses = 0; // practice record against them (capped at 255)
  uint8_t rsvd = 0;
  LinkMon mon[TRAINER_TEAM_MAX];
};

void ghostsBegin();
uint8_t ghostCount();
const Ghost &ghostAt(uint8_t i);   // 0 = most recent
// Remember (or refresh) this rival's team; moves them to the front. False if
// the team is empty or the write did not land.
bool ghostRemember(const uint8_t *mac, const char *name, const LinkMon *mons, uint8_t n);
// Record a practice result against ghost i (does not reorder).
void ghostResult(uint8_t i, bool won);
bool ghostForget(uint8_t i);
