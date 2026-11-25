#include "casino.h"
#include "casino_art.h"
#include <Preferences.h>
#include <string.h>

CasinoState casino;

// ---------------------------------------------------------------- cards

const char *cardRankName(uint8_t c) {
  static const char *const R[13] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
  return R[cardRank(c) % 13];
}

void Deck::shuffle() {
  for (uint8_t i = 0; i < 52; i++) c[i] = i;
  for (int i = 51; i > 0; i--) {             // Fisher-Yates
    int j = (int)random(i + 1);
    uint8_t t = c[i]; c[i] = c[j]; c[j] = t;
  }
  top = 0;
}

uint8_t Deck::draw() {
  if (top >= 52) shuffle();
  return c[top++];
}

// ---------------------------------------------------------------- blackjack

uint8_t bjTotal(const uint8_t *cards, uint8_t n, bool *soft) {
  uint8_t sum = 0, aces = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t r = cardRank(cards[i]);
    if (r == 0) { aces++; sum += 1; }
    else sum += r >= 9 ? 10 : r + 1;         // 10, J, Q, K are all ten
  }
  // One ace may count 11 if that does not bust; two never can.
  bool s = aces && sum + 10 <= 21;
  if (soft) *soft = s;
  return s ? sum + 10 : sum;
}

static bool natural(const uint8_t *c, uint8_t n) { return n == 2 && bjTotal(c, n) == 21; }

void Blackjack::deal(uint16_t betCoins) {
  deck.shuffle();
  bet = betCoins;
  doubled = false;
  youN = dealerN = 0;
  you[youN++] = deck.draw();
  dealer[dealerN++] = deck.draw();
  you[youN++] = deck.draw();
  dealer[dealerN++] = deck.draw();
  result = BJR_NONE;
  state = BJ_PLAYER;
  // A natural on either side ends the hand before anyone acts.
  bool yn = natural(you, youN), dn = natural(dealer, dealerN);
  if (yn || dn) {
    result = yn && dn ? BJR_PUSH : yn ? BJR_BLACKJACK : BJR_LOSE;
    state = BJ_DONE;
  }
}

void Blackjack::hit() {
  if (state != BJ_PLAYER || youN >= BJ_MAX_CARDS) return;
  you[youN++] = deck.draw();
  uint8_t t = bjTotal(you, youN);
  if (t > 21) { result = BJR_BUST; state = BJ_DONE; }
  else if (t == 21) stand();                 // nothing better to do
}

void Blackjack::stand() {
  if (state != BJ_PLAYER) return;
  while (bjTotal(dealer, dealerN) < 17 && dealerN < BJ_MAX_CARDS) dealer[dealerN++] = deck.draw();
  uint8_t y = bjTotal(you, youN), d = bjTotal(dealer, dealerN);
  if (d > 21 || y > d) result = BJR_WIN;
  else if (y == d) result = BJR_PUSH;
  else result = BJR_LOSE;
  state = BJ_DONE;
}

void Blackjack::doubleDown() {
  if (!canDouble()) return;
  doubled = true;
  bet *= 2;
  you[youN++] = deck.draw();
  if (bjTotal(you, youN) > 21) { result = BJR_BUST; state = BJ_DONE; return; }
  stand();
}

uint16_t Blackjack::payout() const {
  if (state != BJ_DONE) return 0;
  switch (result) {
    case BJR_BLACKJACK: return bet + bet * 3 / 2;
    case BJR_WIN: return bet * 2;
    case BJR_PUSH: return bet;
    default: return 0;
  }
}

// ---------------------------------------------------------------- high / low

void HighLow::start(uint16_t betCoins) {
  deck.shuffle();
  card = deck.draw();
  next = card;
  pot = betCoins;
  streak = 0;
  state = HL_GUESS;
}

bool HighLow::guess(bool higher) {
  if (state != HL_GUESS) return false;
  next = deck.draw();
  uint8_t a = cardRank(card), b = cardRank(next);
  bool right = higher ? b > a : b < a;     // a tie is never right
  card = next;
  if (!right) { pot = 0; state = HL_LOST; return false; }
  uint32_t p = (uint32_t)pot * 2;
  pot = p > COIN_CAP ? COIN_CAP : (uint16_t)p;
  streak++;
  if (streak > casino.hlBest) casino.hlBest = streak;
  return true;
}

uint16_t HighLow::cashOut() {
  if (state != HL_GUESS) return 0;
  uint16_t p = pot;
  pot = 0;
  state = HL_IDLE;
  return p;
}

// ---------------------------------------------------------------- slots
// Three identical-length strips, hand-weighted: SEVEN and ROCKET are rare,
// CHERRY common. slotReturn() reports what they actually pay over every stop,
// and slots_test holds it to the house's range.
const uint8_t SLOT_REELS[3][SLOT_REEL_LEN] = {
  { SL_SEVEN, SL_CHERRY, SL_BALL, SL_MOON, SL_BAR, SL_CHERRY, SL_BOLT, SL_BALL,
    SL_ROCKET, SL_MOON, SL_CHERRY, SL_BOLT, SL_BALL, SL_BAR, SL_MOON, SL_BOLT },
  { SL_BOLT, SL_SEVEN, SL_MOON, SL_BALL, SL_CHERRY, SL_BAR, SL_BOLT, SL_MOON,
    SL_BALL, SL_ROCKET, SL_CHERRY, SL_MOON, SL_BOLT, SL_BALL, SL_BAR, SL_MOON },
  { SL_MOON, SL_BALL, SL_SEVEN, SL_BOLT, SL_CHERRY, SL_MOON, SL_BAR, SL_BALL,
    SL_BOLT, SL_MOON, SL_ROCKET, SL_BALL, SL_CHERRY, SL_BOLT, SL_MOON, SL_BAR },
};

uint16_t slotPays(uint8_t a, uint8_t b, uint8_t c) {
  if (a == b && b == c) {
    switch (a) {
      case SL_SEVEN: return 300;
      case SL_ROCKET: return 100;
      case SL_BAR: return 50;
      case SL_BALL: return 15;
      case SL_BOLT: return 15;
      case SL_MOON: return 8;
      case SL_CHERRY: return 8;
      default: return 0;
    }
  }
  if (a == SL_CHERRY && b == SL_CHERRY) return 4;
  if (a == SL_CHERRY) return 2;
  return 0;
}

void slotSpin(uint8_t stop[3]) {
  for (int r = 0; r < 3; r++) stop[r] = (uint8_t)random(SLOT_REEL_LEN);
}

float slotReturn() {
  uint32_t paid = 0;
  for (int i = 0; i < SLOT_REEL_LEN; i++)
    for (int j = 0; j < SLOT_REEL_LEN; j++)
      for (int k = 0; k < SLOT_REEL_LEN; k++)
        paid += slotPays(SLOT_REELS[0][i], SLOT_REELS[1][j], SLOT_REELS[2][k]);
  return (float)paid / (SLOT_REEL_LEN * SLOT_REEL_LEN * SLOT_REEL_LEN);
}

// ---------------------------------------------------------------- roulette
const uint8_t ROULETTE_WHEEL[ROULETTE_POCKETS] = {
  0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10,
  5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26 };

bool rouletteRed(uint8_t n) {
  static const uint8_t R[18] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };
  for (uint8_t r : R) if (r == n) return true;
  return false;
}

uint8_t roulettePays(uint8_t bet, uint8_t arg, uint8_t n) {
  if (n >= ROULETTE_POCKETS) return 0;
  if (bet == RB_NUMBER) return arg == n ? 36 : 0;      // 35 to 1, plus the stake
  if (bet == RB_GREEN) return n == 0 ? 36 : 0;          // the zero: 35 to 1
  if (n == 0) return 0;                                 // the house's pocket
  switch (bet) {
    case RB_RED:   return rouletteRed(n) ? 2 : 0;
    case RB_BLACK: return rouletteRed(n) ? 0 : 2;
    case RB_ODD:   return (n & 1) ? 2 : 0;
    case RB_EVEN:  return (n & 1) ? 0 : 2;
    case RB_LOW:   return n <= 18 ? 2 : 0;
    case RB_HIGH:  return n >= 19 ? 2 : 0;
    case RB_DOZEN: return (uint8_t)((n - 1) / 12) == arg ? 3 : 0;   // 2 to 1
    default: return 0;
  }
}

uint8_t rouletteSpin() { return (uint8_t)random(ROULETTE_POCKETS); }

uint8_t rouletteWheelIndex(uint8_t n) {
  for (uint8_t i = 0; i < ROULETTE_POCKETS; i++) if (ROULETTE_WHEEL[i] == n) return i;
  return 0;
}

// ---------------------------------------------------------------- the shop

uint8_t casinoPrizeCount(uint8_t kind) {
  switch (kind) {
    case PK_BACK: return CARD_BACK_COUNT;
    case PK_AVATAR: return SHOP_AVATAR_COUNT;
    case PK_CARDBG: return CARD_BG_COUNT;
    default: return 0;
  }
}

// Not sold: kept in the frozen index order (ownership is a bit per index) but
// never listed or bought. Today that is the shop avatar that duplicates a rank
// avatar -- rank avatars are earned, not bought.
bool casinoPrizeRetired(uint8_t kind, uint8_t i) {
  if (kind != PK_AVATAR) return false;
  for (uint8_t k = 0; k < SHOP_AVATAR_RETIRED_COUNT; k++) if (SHOP_AVATAR_RETIRED[k] == i) return true;
  return false;
}
// The shop's list: position -> prize index, retired ones skipped. -1 past the end.
uint8_t casinoShopCount(uint8_t kind) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < casinoPrizeCount(kind); i++) if (!casinoPrizeRetired(kind, i)) n++;
  return n;
}
int casinoShopAt(uint8_t kind, uint8_t pos) {
  for (uint8_t i = 0; i < casinoPrizeCount(kind); i++) {
    if (casinoPrizeRetired(kind, i)) continue;
    if (pos-- == 0) return i;
  }
  return -1;
}

const char *casinoPrizeName(uint8_t kind, uint8_t i) {
  if (i >= casinoPrizeCount(kind)) return "?";
  switch (kind) {
    case PK_BACK: return CARD_BACKS[i].name;
    case PK_AVATAR: return SHOP_AVATAR_NAMES[i];
    case PK_CARDBG: return CARD_BGS[i].name;
    default: return "?";
  }
}

static_assert(CARD_BACK_COUNT <= CASINO_BACKS_MAX, "raise CASINO_BACKS_MAX (append-only bitmask)");
static_assert(CARD_BG_COUNT <= CASINO_BGS_MAX, "raise CASINO_BGS_MAX (append-only bitmask)");
static_assert(SHOP_AVATAR_COUNT <= CASINO_AVATARS_MAX, "raise CASINO_AVATARS_MAX");

bool casinoOwns(uint8_t kind, uint8_t i) {
  if (i >= casinoPrizeCount(kind)) return false;
  switch (kind) {
    case PK_BACK: return i == 0 || (casino.backs >> i) & 1;
    case PK_CARDBG: return i == 0 || (casino.bgs[i / 8] >> (i % 8)) & 1;
    case PK_AVATAR: return (casino.avatars[i / 8] >> (i % 8)) & 1;
    default: return false;
  }
}

static void own(uint8_t kind, uint8_t i) {
  switch (kind) {
    case PK_BACK: casino.backs |= (1UL << i); break;
    case PK_CARDBG: casino.bgs[i / 8] |= (uint8_t)(1u << (i % 8)); break;
    case PK_AVATAR: casino.avatars[i / 8] |= (uint8_t)(1u << (i % 8)); break;
  }
}

bool casinoBuy(uint8_t kind, uint8_t i, uint16_t &coins, bool (*spend)(uint16_t)) {
  if (i >= casinoPrizeCount(kind) || casinoOwns(kind, i) || casinoPrizeRetired(kind, i)) return false;
  if (coins < CASINO_PRICE) return false;
  if (!spend || !spend(CASINO_PRICE)) return false;   // coins first
  own(kind, i);
  casinoSave();
  return true;
}

void casinoEquip(uint8_t kind, uint8_t i) {
  if (!casinoOwns(kind, i)) return;
  if (kind == PK_BACK) casino.back = i;
  else if (kind == PK_CARDBG) casino.bg = i;
  casinoSave();
}

// ---------------------------------------------------------------- NVS
// One blob, append-only: [u8 version][u8 len] then the fields in order. A
// shorter (older) record leaves the newer fields at their initialisers.
#define CASINO_VER 1
#define CASINO_KEY "casino"

static size_t pack(uint8_t *b) {   // <= 96 bytes: 2+4+8+16+2+2+2+4+4 = 44
  size_t n = 2;
  memcpy(b + n, &casino.backs, 4); n += 4;
  memcpy(b + n, casino.bgs, sizeof(casino.bgs)); n += sizeof(casino.bgs);
  memcpy(b + n, casino.avatars, sizeof(casino.avatars)); n += sizeof(casino.avatars);
  b[n++] = casino.back;
  b[n++] = casino.bg;
  memcpy(b + n, &casino.hlBest, 2); n += 2;
  memcpy(b + n, &casino.bjWins, 2); n += 2;
  memcpy(b + n, &casino.slotBest, 4); n += 4;
  memcpy(b + n, &casino.pinBest, 4); n += 4;
  b[0] = CASINO_VER;
  b[1] = (uint8_t)n;
  return n;
}

void casinoBegin() {
  casino = CasinoState();
  Preferences p;
  if (!p.begin("tamapoke", true)) return;
  uint8_t b[96] = { 0 };
  size_t n = p.getBytesLength(CASINO_KEY);
  if (n >= 2 && n <= sizeof(b) && p.getBytes(CASINO_KEY, b, n) == n && b[1] <= n) {
    // Pack the DEFAULTS in this build's layout, then lay the stored prefix over
    // them: whatever the stored record is too old to have keeps its default.
    uint8_t cur[96] = { 0 };
    size_t full = pack(cur);
    size_t take = b[1] < full ? b[1] : full;
    memcpy(cur + 2, b + 2, take > 2 ? take - 2 : 0);
    size_t at = 2;
    memcpy(&casino.backs, cur + at, 4); at += 4;
    memcpy(casino.bgs, cur + at, sizeof(casino.bgs)); at += sizeof(casino.bgs);
    memcpy(casino.avatars, cur + at, sizeof(casino.avatars)); at += sizeof(casino.avatars);
    casino.back = cur[at++];
    casino.bg = cur[at++];
    memcpy(&casino.hlBest, cur + at, 2); at += 2;
    memcpy(&casino.bjWins, cur + at, 2); at += 2;
    memcpy(&casino.slotBest, cur + at, 4); at += 4;
    memcpy(&casino.pinBest, cur + at, 4); at += 4;
  }
  casino.backs |= 1;                          // the defaults are always owned
  casino.bgs[0] |= 1;
  if (!casinoOwns(PK_BACK, casino.back)) casino.back = 0;
  if (!casinoOwns(PK_CARDBG, casino.bg)) casino.bg = 0;
  p.end();
}

bool casinoSave() {
  uint8_t b[96];
  size_t n = pack(b);
  Preferences p;
  if (!p.begin("tamapoke", false)) return false;
  bool ok = p.putBytes(CASINO_KEY, b, n) == n;
  p.end();
  return ok;
}
