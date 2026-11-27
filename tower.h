#pragma once
#include <Arduino.h>
#include "trainers.h"
#include "party.h"
#include "items.h"

// The Battle Tower after the Elite 4, and harder gym rematches.
//
// Ported from vunguyen2712/TamaPoke (features 4+6, v3.13); the rules and the
// numbers are theirs. Adapted to this base: speciesShowable() is the drawable
// test, items are this base's ItemKeys, and the screens are this base's (the
// TOWER button on the gym chooser, a REMATCH state on the ladder's pill).
//
// GAME LOGIC ONLY: no gfx, no SD, no touch -- links into every headless test
// binary next to pet.cpp. The fight itself is the sketch's ordinary trainer
// battle: both features hand it a TrainerMon team (btlTeam), and it never
// looks at TRAINER_SETS for replacements.
//
// Keys (all in SAVE_FIELDS): twStrk U16, twBest U16, twSeed U32, rmWin BYTES,
// twTeam BYTES (the run's team), twMid U8 (a fight was in progress).

class Pet;

// ---- the tower
#define TOWER_LEVEL 50          // both sides, for the fight only
#define TOWER_TEAM 3            // 3 vs 3
#define TOWER_REWARD_EVERY 7    // items into the bag at every 7th straight win
#define TOWER_SMART_FLOOR 7     // the AI reads the type chart from this floor on
#define TOWER_IV_FLOORS 14      // EASY_IV on floor 1, rising to HARD_IV on floor 15
#define TOWER_LEGEND_FLOOR 22   // no legendary before the third reward
// Opponents are drawn from a base-stat-total band that climbs with the floor:
// centre = START + STEP * (floor - 1), capped at TOP, +-HALF either side.
#define TOWER_BST_START 380
#define TOWER_BST_STEP 10
#define TOWER_BST_TOP 600
#define TOWER_BST_HALF 50
#define TOWER_STREAK_MAX 30000  // rides in an int16_t event arg
// Every run that clears this floor unlocks one legendary costume avatar, picked
// at random from those not owned yet (none once all are).
#define TOWER_AVATAR_FLOOR 100

// ---- held-item rewards (v3.56), by tier (items.h HT_*)
// Every floor cleared rolls TOWER_HELD_PCT for a held item (certain on every
// TOWER_REWARD_EVERY-th). Which tier it comes from climbs with the floor:
//   floors 1-10   C only
//   floors 11-49  B rising 20% -> 60%, the rest C
//   floors 50-99  S 3% -> 15%, A 10% -> 40%, B 45%, the rest C
// Floor 100 pays TOWER_TOP_ITEMS from A/S. Past it, ENDLESS: a MASTER BALL
// every floor, and one A/S item every TOWER_ENDLESS_EVERY floors.
#define TOWER_HELD_PCT 35
#define TOWER_TIER_B_FLOOR 11
#define TOWER_TIER_A_FLOOR 50
#define TOWER_TOP_FLOOR 100
#define TOWER_TOP_ITEMS 4
#define TOWER_ENDLESS_EVERY 5
#define TOWER_REWARD_MAX 6
// The tier a roll in 0..99 lands in on `floor`.
uint8_t towerTierFor(uint16_t floor, uint8_t roll);
// A pick from `tier`, weighted: a plate or a gem counts 1, anything else 4
// (seventeen plates for one creature must not crowd out everything else).
// `roll` is reduced into range here. ITEM_NONE if the tier is empty.
ItemKey towerItemOfTier(uint8_t tier, uint32_t roll);

// ---- gym rematches
#define REMATCH_IV 24           // between EASY_IV and HARD_IV
// level' = level * 5/4 + 10, capped at MAX_TRAINER_LEVEL
uint8_t rematchLevel(uint8_t lvl);

// Which creature towerUnlocked()/rematchUnlocked() ask about. The sketch binds
// its live pet in setup(); tests bind their own. Unbound = locked.
// It is a pointer rather than `extern Pet pet` because pet lives in the
// sketch, and every test binary links this file whether or not it has one.
bool towerBindPet(const Pet *p);

// Called once from setup(), after pet/party/bag have loaded. Also re-reads
// NVS, which is how the tests prove the run is on flash and not just in RAM.
void towerBegin();
void towerClearAll();           // tests: forget the run and every rematch

// Open once ANY region's champion (index TRAINER_COUNT-1) has been beaten, on
// either difficulty.
bool towerUnlockedFor(const Pet &p);
bool towerUnlocked();

uint16_t towerStreak();         // wins in a row in the current run
uint16_t towerBest();           // best streak ever
inline uint16_t towerFloor() { return towerStreak() + 1; }   // the next fight
bool towerRunActive();          // a run under way (a win, or a team locked in): CONTINUE
uint32_t towerSeed();
void towerStartRun(uint32_t seed);   // START: streak 0, a fresh set of floors

struct TowerTeam {
  TrainerMon mon[TOWER_TEAM];
  uint8_t count;
  uint8_t iv;
  bool smart;                   // hard-mode AI
};

// The species pool: drawable (art + its region's pack), and optionally no
// legendaries. A legendary is judged by the ROOT of its line, so Cosmoem
// counts even though only Cosmog carries the rarity.
bool towerDrawable(int16_t dex);
bool speciesLegendary(int16_t dex);
uint16_t speciesBst(int16_t dex);
// Follows the line while the level allows and the next stage is drawable.
int16_t evolveForLevel(int16_t dex, uint8_t lvl);

void towerBand(uint16_t floor, uint16_t &lo, uint16_t &hi);
uint8_t towerIvFor(uint16_t floor);
bool towerSmartFor(uint16_t floor);
const char *towerFoeName(uint16_t floor);
// The team on `floor` of the run seeded `seed`. Deterministic, so the screen can
// show who is next and the fight is exactly that team. False if the pool is
// too small to field anyone (no pack at all).
bool towerTeamFor(uint16_t floor, uint32_t seed, TowerTeam &out);

struct TowerReward {
  uint8_t item[TOWER_REWARD_MAX];
  uint8_t qty[TOWER_REWARD_MAX];
  uint8_t n;
  int16_t avatar;               // towerWin only: the AVATARS[] index just unlocked, else -1
};
// Unlocks a random legendary costume not owned yet. Returns its AVATARS[]
// index, or -1 when every one is owned.
int16_t towerGrantAvatar();
// What reaching `streak` is worth. True only at a multiple of TOWER_REWARD_EVERY.
bool towerRewardFor(uint16_t streak, TowerReward &r);
// A floor cleared: streak +1, best kept, saved, the reward (if any) bagged
// once, EV_TOWER_WIN emitted. Returns the new streak.
uint16_t towerWin(TowerReward *got);
// A loss, or END RUN: EV_TOWER_LOSS with the streak that ended, and the run is over.
void towerLoss();

// ---- the run's team and its checkpoint (v3.56)
// The team is copied when a run starts and every floor is fought with that
// copy, so the party can be changed, traded or released mid-climb without
// touching the run. Stored records, not references: the copy is the team.
uint8_t towerTeam(PartyMon *out);          // how many (0 = none locked in)
bool towerSetTeam(const PartyMon *m, uint8_t n);
// A tower fight starting / ending. If the board restarts in between, the run
// comes back at the same floor with a fresh fight -- the checkpoint -- and
// towerResumed() says so until the next fight starts.
void towerFightBegin();
void towerSuspend();                       // left mid-fight: same floor, later
bool towerResumed();

// ---- rematches: open per region once its champion fell on EASY
bool rematchUnlockedFor(const Pet &p, uint8_t region);
bool rematchUnlocked(uint8_t region);
uint16_t rematchMask(uint8_t region);
bool rematchWon(uint8_t region, uint8_t idx);
// Sequential like the ladder: the first is open, each next once the previous fell.
bool rematchOpenFor(const Pet &p, uint8_t region, uint8_t idx);
// The stronger team: levels raised, members evolved where the level allows, and
// filled to six with drawable creatures of the leader's type. Returns the count.
uint8_t rematchTeam(uint8_t region, uint8_t idx, TrainerMon *out);
uint8_t rematchTop(uint8_t region, uint8_t idx, uint8_t *count = nullptr);
bool rematchRewardFor(uint8_t idx, bool first, TowerReward &r);
// Records the win in the rematch mask (never a badge), bags the reward,
// emits EV_REMATCH_WIN. Returns true if it was the first rematch win there.
bool rematchWin(uint8_t region, uint8_t idx, TowerReward *got);
