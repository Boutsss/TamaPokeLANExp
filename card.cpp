#include "card.h"
#include "stars.h"   // STARS_MASK only: the header, no state
#include "pet.h"
#include "dex.h"
#include "avatar_names.h"
#include "casino_art.h"
#include "rank.h"
#include "trainers.h"
#include <Preferences.h>
#include <string.h>

static TrainerCard book[CARD_BOOK_CAP];
static uint8_t bookN = 0;
static TrainerCard none;

TrainerCard cardMine(const Pet &p, uint16_t avatar, uint8_t bg, uint8_t rank, uint32_t fid) {
  TrainerCard c;
  snprintf(c.name, sizeof(c.name), "%s", p.trainerName[0] ? p.trainerName : "TRAINER");
  c.setAvatar(avatar);
  c.bg = bg;
  c.rank = rank;
  uint8_t b = 0;
  for (uint8_t r = 0; r < GYM_REGIONS; r++) {
    uint16_t m = p.badgeMask(r, false);
    while (m) { b += m & 1; m >>= 1; }
  }
  c.badges = b;
  c.dex = p.registeredCount();
  c.medals = p.totalMedals;
  c.petDex = p.isEgg() ? 0 : p.speciesId;
  c.petShiny = (!p.isEgg() && p.shiny) ? 1 : 0;
  c.bestStreak = p.bestStreak;
  c.fid = fid;
  return c;
}

static void w16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static uint16_t r16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }

uint8_t cardToWire(const TrainerCard &c, uint8_t *o) {
  memset(o, 0, CARD_WIRE_LEN);
  memcpy(o, c.name, CARD_NAME_LEN);
  o[12] = c.avatar() < 256 ? (uint8_t)c.avatar() : 0;   // what a 30-byte reader sees
  o[13] = c.bg; o[14] = c.rank; o[15] = c.badges;
  w16(o + 16, c.dex); w16(o + 18, c.medals); w16(o + 20, (uint16_t)c.petDex);
  o[22] = c.petShiny; w16(o + 23, c.bestStreak);
  w16(o + 25, (uint16_t)c.fid); w16(o + 27, (uint16_t)(c.fid >> 16));
  o[29] = 1;   // layout version
  w16(o + 30, c.avatar());
  o[32] = c.stars & STARS_MASK;
  return CARD_WIRE_LEN;
}

bool cardFromWire(const uint8_t *i, uint8_t n, TrainerCard &c) {
  if (n < CARD_WIRE_MIN || i[29] < 1) return false;
  c = TrainerCard();
  memcpy(c.name, i, CARD_NAME_LEN);
  c.name[CARD_NAME_LEN - 1] = 0;
  for (char *p = c.name; *p; p++) if (*p < 32 || *p > 126) *p = '?';   // the font is ASCII only
  if (!c.name[0]) snprintf(c.name, sizeof(c.name), "TRAINER");
  uint16_t av = n >= 32 ? r16(i + 30) : i[12];
  // YOU is composed from THIS board's wardrobe, so another player's YOU would
  // draw in our clothes: it arrives as RED, the classic stand-in.
  c.setAvatar(av < AVATAR_COUNT && av != AVATAR_YOU ? av : 0);
  c.bg = i[13] < CARD_BG_COUNT ? i[13] : 0;
  c.rank = i[14] >= 1 && i[14] <= RANK_MAX ? i[14] : 1;
  c.badges = i[15] <= GYM_REGIONS * TRAINER_GYMS ? i[15] : 0;
  c.dex = r16(i + 16) <= DEX_COUNT ? r16(i + 16) : DEX_COUNT;
  c.medals = r16(i + 18);
  int16_t d = (int16_t)r16(i + 20);
  c.petDex = d >= 1 && d <= DEX_COUNT ? d : 0;
  c.petShiny = c.petDex && i[22] ? 1 : 0;
  c.bestStreak = r16(i + 23);
  c.fid = (uint32_t)r16(i + 25) | ((uint32_t)r16(i + 27) << 16);
  c.stars = n >= 33 ? (uint8_t)(i[32] & STARS_MASK) : 0;   // an older board sends none
  return true;
}

// ---------------------------------------------------------------- the book
#define BOOK_VER 1
#define BOOK_KEY "cards"
#define BOOK_HDR CARD_BOOK_HDR

static bool bookSave() {
  uint8_t buf[BOOK_HDR + CARD_BOOK_CAP * sizeof(TrainerCard)];
  buf[0] = BOOK_VER;
  buf[1] = bookN;
  uint16_t stride = sizeof(TrainerCard);
  memcpy(buf + 2, &stride, 2);
  for (uint8_t i = 0; i < bookN; i++) memcpy(buf + BOOK_HDR + i * sizeof(TrainerCard), &book[i], sizeof(TrainerCard));
  size_t n = BOOK_HDR + bookN * sizeof(TrainerCard);
  Preferences p;
  if (!p.begin("tamapoke", false)) return false;
  bool ok = p.putBytes(BOOK_KEY, buf, n) == n;
  p.end();
  return ok;
}

void cardBookBegin() {
  bookN = 0;
  for (auto &c : book) c = TrainerCard();
  Preferences p;
  if (!p.begin("tamapoke", true)) return;
  size_t n = p.getBytesLength(BOOK_KEY);
  static uint8_t buf[BOOK_HDR + CARD_BOOK_CAP * 2 * sizeof(TrainerCard)];
  if (n >= BOOK_HDR && n <= sizeof(buf) && p.getBytes(BOOK_KEY, buf, n) == n) {
    uint16_t stride;
    memcpy(&stride, buf + 2, 2);
    uint8_t cnt = buf[1];
    if (stride && cnt <= CARD_BOOK_CAP && BOOK_HDR + (size_t)cnt * stride <= n) {
      for (uint8_t i = 0; i < cnt; i++) {
        memcpy(&book[i], buf + BOOK_HDR + i * stride, stride < sizeof(TrainerCard) ? stride : sizeof(TrainerCard));
        // re-check what was stored, exactly as if it had just arrived -- all
        // but starsNew, which is this board's own note and never on the wire
        uint8_t keepNew = book[i].starsNew;
        uint8_t w[CARD_WIRE_LEN];
        cardToWire(book[i], w);
        cardFromWire(w, CARD_WIRE_LEN, book[i]);
        book[i].starsNew = keepNew & book[i].stars;
      }
      bookN = cnt;
    }
  }
  p.end();
}

uint8_t cardBookCount() { return bookN; }
bool cardBookAck(uint8_t i) {
  if (i >= bookN || !book[i].starsNew) return false;
  book[i].starsNew = 0;
  return bookSave();
}
const TrainerCard &cardBookAt(uint8_t i) { return i < bookN ? book[i] : none; }

bool cardBookAdd(const TrainerCard &c, uint32_t myFid) {
  if (c.fid && c.fid == myFid) return false;        // our own card, echoed
  int at = -1;
  for (uint8_t i = 0; i < bookN; i++) {
    bool same = c.fid ? book[i].fid == c.fid : (!book[i].fid && !strcmp(book[i].name, c.name));
    if (same) { at = i; break; }
  }
  // Stars they have earned since their last card here are NEW to this player,
  // and any still unacknowledged from before stay new. A first card has no
  // "since last time", so nothing on it flashes.
  TrainerCard in = c;
  in.starsNew = at >= 0 ? (uint8_t)(((c.stars & ~book[at].stars) | book[at].starsNew) & c.stars) : 0;
  int from = at >= 0 ? at : (bookN < CARD_BOOK_CAP ? bookN : CARD_BOOK_CAP - 1);
  for (int i = from; i > 0; i--) book[i] = book[i - 1];
  book[0] = in;
  if (at < 0 && bookN < CARD_BOOK_CAP) bookN++;
  return bookSave();
}
