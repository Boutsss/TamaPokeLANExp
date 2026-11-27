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
// 32 since v3.45: bytes 30-31 carry the full 16-bit avatar. A 30-byte card
// from an older build is still accepted (CARD_WIRE_MIN); its byte 12 is then
// the whole avatar. We still put a downgrade-safe low byte at 12 for them.
// 33 since v3.57: byte 32 is the sender's stars (stars.h). Anything shorter is
// read as "no stars" -- an older board -- and an older board reading ours just
// stops at 32, as it always has.
#define CARD_WIRE_LEN 33
#define CARD_WIRE_MIN 30
#define CARD_BOOK_HDR 4

struct TrainerCard {
  char name[CARD_NAME_LEN] = "";
  uint8_t avatarLo = 0;     // low byte of the AVATARS[] index -- use avatar()
  uint8_t bg = 0;           // CARD_BGS[] index (casino_art.h)
  uint8_t rank = 1;
  uint8_t badges = 0;       // easy badges, every region
  uint16_t dex = 0;         // species registered
  uint16_t medals = 0;      // lifetime medals
  int16_t petDex = 0;       // their creature at the time (0 = an egg)
  uint8_t petShiny = 0;
  uint16_t bestStreak = 0;
  uint32_t fid = 0;         // their friend id (friends.h), 0 = unknown
  // APPENDED (v3.45): the book stores this struct raw with its stride, so the
  // high byte goes at the end, never beside avatarLo. An older record reads
  // back with it 0, which is exactly what its avatar was.
  uint8_t avatarHi = 0;
  // APPENDED (v3.57). `stars` travels on the wire (byte 32). `starsNew` never
  // does: it is THIS board's note of which of their stars it has not shown
  // the player yet -- earned since the previous card from the same friend --
  // and they flash in the card book until a tap acknowledges them.
  uint8_t stars = 0;
  uint8_t starsNew = 0;
  uint16_t avatar() const { return (uint16_t)(avatarLo | (avatarHi << 8)); }
  void setAvatar(uint16_t a) { avatarLo = (uint8_t)a; avatarHi = (uint8_t)(a >> 8); }
};

// The largest "cards" blob, header included (save.cpp sizes the backup by it).
#define CARD_BOOK_BLOB_MAX (CARD_BOOK_HDR + CARD_BOOK_CAP * sizeof(TrainerCard))

class Pet;
// Our own card, from the player's state.
TrainerCard cardMine(const Pet &p, uint16_t avatar, uint8_t bg, uint8_t rank, uint32_t fid);
uint8_t cardToWire(const TrainerCard &c, uint8_t *out);           // returns CARD_WIRE_LEN
bool cardFromWire(const uint8_t *in, uint8_t n, TrainerCard &out); // false = not a card

void cardBookBegin();
uint8_t cardBookCount();
const TrainerCard &cardBookAt(uint8_t i);   // 0 = most recent
// File a received card: replaces that friend's older card and moves it to the
// front. Ignores our own card (same fid). True if it was stored.
bool cardBookAdd(const TrainerCard &c, uint32_t myFid);
// The player has seen card i's new stars: clear them, and save. True if any.
bool cardBookAck(uint8_t i);
