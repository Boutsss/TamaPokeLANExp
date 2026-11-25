#pragma once
#include <Arduino.h>

// The Game Corner: a hidden casino behind the ARCADE button (hold it for
// CASINO_HOLD_MS). GAME LOGIC ONLY -- no gfx, no touch -- so it links into the
// headless tests beside pet.cpp. The screens live in ui_casino.ino.
//
// MONEY. Coins live on Pet (Pet::coins), in the PLAYER checkpoint beside the
// wallet they are bought with, so a coin purchase is ONE atomic write: the
// dollars leave and the coins arrive together or not at all. Capped at
// COIN_CAP like the real Coin Case. Coins cannot be sold back.
//
// PRIZES. Card backs, trainer avatars and trainer-card backgrounds, all
// CASINO_PRICE coins. What you own and what you have equipped are in NVS under
// "casino". Buying spends the coins FIRST and records the prize second, so a
// power cut between the two costs coins rather than handing out a free prize.
//
// EVERY RANDOM DRAW goes through the caller's random(), so a test seeds it.

#define COIN_CAP 9999
#define CASINO_PRICE 9999
#define COIN_DOLLARS 20         // Pokedollars per coin: the GBC price, 50 coins for $1000
#define CASINO_HOLD_MS 6400     // hold ARCADE this long to find the Game Corner
// The reveal: once a press is too long to be a tap, the ARCADE row starts to
// turn like a card, slowly at first, and lands on its GAME CORNER face at
// CASINO_REVEAL_MS; it glitters there until the door opens at CASINO_HOLD_MS.
#define CASINO_SPIN_START_MS 3000   // nothing shows for the first 3 s
#define CASINO_REVEAL_MS 5600
#define CASINO_SPIN_HALF_TURNS 5   // odd: it must finish face DOWN, i.e. on GAME CORNER

// ---- cards -----------------------------------------------------------------
// A card is 0..51: rank = c % 13 (0 = ACE .. 12 = KING), suit = c / 13.
inline uint8_t cardRank(uint8_t c) { return c % 13; }
inline uint8_t cardSuit(uint8_t c) { return c / 13; }
const char *cardRankName(uint8_t c);   // "A","2".."10","J","Q","K"

struct Deck {
  uint8_t c[52];
  uint8_t top = 52;            // 52 = empty, shuffle() refills
  void shuffle();
  uint8_t draw();              // reshuffles a fresh deck when empty
};

// ---- blackjack ---------------------------------------------------------------
// One deck, reshuffled every hand. Dealer stands on all 17s. A natural pays 3:2,
// a push returns the bet, DOUBLE takes exactly one more card at twice the bet.
enum BjState : uint8_t { BJ_IDLE = 0, BJ_PLAYER, BJ_DONE };
enum BjResult : uint8_t { BJR_NONE = 0, BJR_WIN, BJR_BLACKJACK, BJR_PUSH, BJR_LOSE, BJR_BUST };
#define BJ_MAX_CARDS 11

struct Blackjack {
  Deck deck;
  uint8_t you[BJ_MAX_CARDS], youN = 0;
  uint8_t dealer[BJ_MAX_CARDS], dealerN = 0;
  uint16_t bet = 0;
  uint8_t state = BJ_IDLE;
  uint8_t result = BJR_NONE;
  bool doubled = false;
  // Start a hand. The bet has already been taken from the coins by the caller.
  void deal(uint16_t betCoins);
  void hit();
  void stand();
  bool canDouble() const { return state == BJ_PLAYER && youN == 2 && !doubled; }
  void doubleDown();           // caller takes the second bet first
  // Coins returned to the player at the end of the hand (bet included): 0 on a
  // loss, bet on a push, 2*bet on a win, bet + bet*3/2 on a natural.
  uint16_t payout() const;
};
uint8_t bjTotal(const uint8_t *cards, uint8_t n, bool *soft = nullptr);

// ---- high / low --------------------------------------------------------------
// One card face up; guess whether the next is HIGHER or LOWER (ace low, king
// high). A tie is neither and loses -- the house edge. Each right guess doubles
// the pot; CASH OUT whenever.
enum HlState : uint8_t { HL_IDLE = 0, HL_GUESS, HL_LOST };
struct HighLow {
  Deck deck;
  uint8_t card = 0, next = 0;
  uint16_t pot = 0;            // coins riding
  uint8_t streak = 0;
  uint8_t state = HL_IDLE;
  void start(uint16_t betCoins);
  bool guess(bool higher);     // true: right, pot doubled (capped at COIN_CAP)
  uint16_t cashOut();          // the pot, and the round is over
};

// ---- slots -------------------------------------------------------------------
enum SlotSym : uint8_t { SL_SEVEN = 0, SL_ROCKET, SL_BAR, SL_BALL, SL_BOLT, SL_MOON, SL_CHERRY, SL_COUNT };
#define SLOT_REEL_LEN 16
extern const uint8_t SLOT_REELS[3][SLOT_REEL_LEN];
// Payout for a centre line, as a multiple of the bet (0 = nothing).
uint16_t slotPays(uint8_t a, uint8_t b, uint8_t c);
// Pick three stops. The outcome is decided here, at the STOP tap, never by
// where the animation happened to be -- a reel the player could time would
// let skill beat the house.
void slotSpin(uint8_t stop[3]);
// The exact long-run return of SLOT_REELS (all 16^3 stops), so the test can
// hold the house edge to a range without sampling.
float slotReturn();

// ---- roulette -------------------------------------------------------------
// A single-zero wheel, 0..36, in the real pocket order. Every bet pays what
// the real table pays, and 0 is the house's: it loses every outside bet. The
// long-run return of every bet is exactly 36/37 (roulette_test sums all 37).
// RB_GREEN is the zero, the house pocket, as a bet of its own: 35 to 1.
enum RouletteBet : uint8_t { RB_RED = 0, RB_BLACK, RB_ODD, RB_EVEN, RB_LOW, RB_HIGH, RB_DOZEN, RB_NUMBER, RB_GREEN, RB_COUNT };
#define ROULETTE_POCKETS 37
extern const uint8_t ROULETTE_WHEEL[ROULETTE_POCKETS];   // the pocket order round the wheel
bool rouletteRed(uint8_t n);                 // 0 is neither red nor black
// Coins returned per coin staked (stake included): 0 = lost. `arg` is the
// dozen (0..2) for RB_DOZEN or the number (0..36) for RB_NUMBER.
uint8_t roulettePays(uint8_t bet, uint8_t arg, uint8_t result);
uint8_t rouletteSpin();                      // 0..36, decided at SPIN
uint8_t rouletteWheelIndex(uint8_t n);       // where n sits on the wheel

// ---- the prize shop -------------------------------------------------------------
enum PrizeKind : uint8_t { PK_BACK = 0, PK_AVATAR, PK_CARDBG, PK_KINDS };
// How many prizes of each kind exist. Avatars count only those the casino
// sells (the free starting set is not for sale).
uint16_t casinoPrizeCount(uint8_t kind);
const char *casinoPrizeName(uint8_t kind, uint16_t i);

// What the player owns / has equipped. Index 0 of backs and card backgrounds is
// the free default and is always owned.
#define CASINO_BACKS_MAX 32
#define CASINO_BGS_MAX 64
// Avatar ownership is split three ways: the first 128 bits sit where they
// always have in the stored record, bits 128..255 were APPENDED at its end
// (v3.44) and bits 256..1023 after those (v3.45) -- the record is append-only,
// so growing an earlier array would move every field after it. Always go
// through casinoOwns()/the buy path, never index these.
#define CASINO_AVATARS_LO 128
#define CASINO_AVATARS_MID 256
#define CASINO_AVATARS_MAX 1024
#define CASINO_BLOB_MAX 192      // the packed record (152 today); its length byte caps it at 255
struct CasinoState {
  uint32_t backs = 1;                    // bit i = back i owned
  uint8_t bgs[CASINO_BGS_MAX / 8] = { 1 };
  uint8_t avatars[CASINO_AVATARS_LO / 8] = { 0 };
  uint8_t back = 0, bg = 0;              // equipped
  uint16_t hlBest = 0;                   // best High/Low streak
  uint16_t bjWins = 0;
  uint32_t slotBest = 0;                 // biggest single slot payout
  uint8_t avatarsHi[(CASINO_AVATARS_MID - CASINO_AVATARS_LO) / 8] = { 0 };
  uint8_t avatarsX[(CASINO_AVATARS_MAX - CASINO_AVATARS_MID) / 8] = { 0 };
};
extern CasinoState casino;

void casinoBegin();
bool casinoSave();
bool casinoOwns(uint8_t kind, uint16_t i);
bool casinoPrizeRetired(uint8_t kind, uint16_t i);   // kept in the index order, never sold
uint16_t casinoShopCount(uint8_t kind);               // what the shop LISTS
int casinoShopAt(uint8_t kind, uint16_t pos);         // list position -> prize index, -1 past the end
// Spends CASINO_PRICE from `coins` and records the prize. False (nothing
// changes) if it is owned already or the coins are short. `spend` is called
// to take the coins and must return whether it landed; only then is the prize
// written (coins first: a cut costs coins, never hands out a prize).
bool casinoBuy(uint8_t kind, uint16_t i, uint16_t &coins, bool (*spend)(uint16_t));
void casinoEquip(uint8_t kind, uint16_t i);
