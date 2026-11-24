#pragma once
#include <Arduino.h>

// Trainer cards: swapped between two boards during a FRIENDS session and kept
// in a CARD BOOK on the player card. GAME LOGIC ONLY -- no gfx.
//
// On the wire a card is laid out BYTE BY BYTE (CARD_WIRE_LEN), never memcpy'd
// from the struct, so two builds with different padding still agree. Nothing
// that arrives is trusted: cardFromWire() terminates the name, keeps it to
// printable ASCII (the panel font has nothing else) and clamps every index
// before anything can draw with it.
//
// The book is NVS key "cards": [u8 version][u8 count][u16 stride] + records,
// one per friend (keyed by friend id, else name), most recent first, capped at
// CARD_BOOK_CAP. Records are APPEND-ONLY; the stride travels with them.

#define CARD_BOOK_CAP 12
#define CARD_NAME_LEN 12
#define CARD_WIRE_LEN 30

struct TrainerCard {
  char name[CARD_NAME_LEN] = "";
  uint8_t avatar = 0;       // AVATARS[] index
  uint8_t bg = 0;           // CARD_BGS[] index (casino_art.h)
  uint8_t rank = 1;
  uint8_t badges = 0;       // easy badges, every region
  uint16_t dex = 0;         // species registered
  uint16_t medals = 0;      // lifetime medals
  int16_t petDex = 0;       // their creature at the time (0 = an egg)
  uint8_t petShiny = 0;
  uint16_t bestStreak = 0;
  uint32_t fid = 0;         // their friend id (friends.h), 0 = unknown
};

class Pet;
// Our own card, from the player's state.
TrainerCard cardMine(const Pet &p, uint8_t avatar, uint8_t bg, uint8_t rank, uint32_t fid);
uint8_t cardToWire(const TrainerCard &c, uint8_t *out);           // returns CARD_WIRE_LEN
bool cardFromWire(const uint8_t *in, uint8_t n, TrainerCard &out); // false = not a card

void cardBookBegin();
uint8_t cardBookCount();
const TrainerCard &cardBookAt(uint8_t i);   // 0 = most recent
// File a received card: replaces that friend's older card and moves it to the
// front. Ignores our own card (same fid). True if it was stored.
bool cardBookAdd(const TrainerCard &c, uint32_t myFid);
