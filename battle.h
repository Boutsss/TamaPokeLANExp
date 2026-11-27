#pragma once
#include <Arduino.h>
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "items.h"

// Turn resolution. Deliberately free of any UI or hardware so the whole thing
// can be simulated headlessly -- see tools/emu.
//
// Integer maths only, like types.h: no floats anywhere on the MCU.

enum : uint8_t { SI_ATK = 0, SI_DEF, SI_SPA, SI_SPD, SI_SPE, SI_COUNT };

// A creature as it exists inside a battle. Built from the live pet or from a
// banked party member; nothing here is ever written back, which is what keeps
// ailments battle-only.
struct Combatant {
  int16_t dex = 0;
  uint8_t level = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t base[SI_COUNT] = { 1, 1, 1, 1, 1 };  // before stat stages
  uint16_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  int8_t stage[SI_COUNT] = { 0, 0, 0, 0, 0 };   // -6..+6
  uint8_t ailment = AIL_NONE;
  uint8_t ailTurns = 0;      // sleep/freeze countdown
  uint8_t confuseTurns = 0;  // confusion runs alongside a real ailment
  bool recharge = false;     // EF_RECHARGE spent this creature's next turn
  uint16_t charging = 0;     // EF_CHARGE move already wound up
  bool shiny = false;        // which sprite variant to stream
  uint8_t skin = 0;          // and which skin (skins.h); cosmetic only
  uint8_t held = ITEM_NONE;  // held item snapshot for this battle
  bool itemUsed = false;     // consumable already triggered this battle
  int8_t holder = -1;        // player's pet/party/box slot; -1 for opponents/LAN
  char name[12] = "";

  // ---- v3.37: what the second batch of moves needs. All battle-only, like
  // the ailments: nothing here is ever written back to the stored creature.
  uint8_t side = 0;          // 0 = the player, 1 = the opponent (battleOnEntry sets it)
  uint8_t bond = 0;          // RETURN / FRUSTRATION
  uint8_t hpType = 0;        // HIDDEN POWER's type, from the IVs
  int8_t accStage = 0, evaStage = 0;   // -6..+6
  uint8_t critStage = 0;     // FOCUS ENERGY
  bool laserFocus = false;   // the next hit is a critical one
  bool protecting = false;   // PROTECT / DETECT this turn
  uint8_t protectChain = 0;  // consecutive successful uses
  bool enduring = false;     // ENDURE this turn
  bool flinched = false;
  uint16_t subHp = 0;        // a SUBSTITUTE's HP, 0 = none
  bool seeded = false;       // LEECH SEED
  uint8_t bindTurns = 0;     // WRAP and friends
  bool trapped = false;      // MEAN LOOK, BLOCK, SPIDER WEB
  bool ingrained = false, aquaRing = false;
  uint8_t yawn = 0;          // 2 -> 1 -> asleep
  uint8_t tauntTurns = 0, encoreTurns = 0;
  uint16_t encoreMove = 0;
  uint16_t lastMove = 0;     // the last move it actually used
  uint8_t streak = 0;        // consecutive uses of lastMove (FURY CUTTER, PROTECT)
  bool lastFailed = false;   // STOMPING TANTRUM
  uint8_t toxicN = 0;        // > 0: BADLY poisoned, this many sixteenths next
  bool destinyBond = false;
  uint8_t perish = 0;        // turns left, 0 = none
  bool cursed = false;       // a Ghost's CURSE
  bool chargedUp = false;    // CHARGE: the next Electric move is doubled
  uint8_t turnsOut = 0;      // FAKE OUT works only on the first
  bool movedThisTurn = false;
  uint16_t hurtThisTurn = 0; // FOCUS PUNCH, REVENGE, ASSURANCE
  uint16_t lastHitDmg = 0;   // COUNTER, MIRROR COAT, METAL BURST
  uint8_t lastHitCat = 0xFF; // MC_PHYS / MC_SPEC of that hit
  uint16_t queuedMove = 0;   // what it chose this turn (SUCKER PUNCH)
  uint16_t lockMove = 0;     // THRASH, PETAL DANCE, OUTRAGE
  uint8_t lockTurns = 0;

  // ---- v3.56: held items. Battle-only like everything above.
  uint8_t typeOver = 0xFF;   // a plate on ARCEUS: its one type for this fight (0xFF = its own)
  uint16_t choiceLock = 0;   // CHOICE items: the move it is locked into
  bool micle = false;        // MICLE BERRY: the next move is more accurate
  bool custap = false;       // CUSTAP BERRY: acts first this turn
  bool gemOn = false;        // a gem is powering THIS hit
  bool resistOn = false;     // a resist berry is softening THIS hit
  bool strongBind = false;   // bound by a BINDING BAND holder: 1/6 a turn

  bool fainted() const { return hp == 0; }
};

// The field: weather, terrain, TRICK ROOM, and each side's hazards, screens
// and timers. One battle at a time, so one global; battleFieldReset() at the
// start of every fight. Battle-only like everything above.
enum : uint8_t { W_NONE = 0, W_RAIN, W_SUN, W_SAND, W_HAIL };
enum : uint8_t { TR_NONE = 0, TR_ELECTRIC, TR_GRASSY, TR_MISTY, TR_PSYCHIC };
struct BattleSide {
  uint8_t rocks = 0, spikes = 0, tspikes = 0, web = 0;
  uint8_t reflect = 0, lscreen = 0, veil = 0, tailwind = 0, safeguard = 0, mist = 0;
  uint8_t wishTurns = 0; uint16_t wishHp = 0;
  uint8_t futureTurns = 0; uint16_t futureDmg = 0;
};
struct BattleField {
  uint8_t weather = W_NONE, weatherTurns = 0;
  uint8_t terrain = TR_NONE, terrainTurns = 0;
  uint8_t trickRoom = 0;
  BattleSide side[2];
};
extern BattleField gField;
void battleFieldReset();

void combatantFromPet(Combatant &c, const Pet &p);
void combatantFromParty(Combatant &c, const PartyMon &m);

// What one action did, so the UI can narrate it without recomputing anything.
struct TurnLog {
  uint16_t move = 0;
  uint16_t damage = 0;
  uint16_t effPct = 100;   // type effectiveness, percent
  uint8_t hits = 0;        // >1 for EF_MULTI
  uint8_t inflicted = AIL_NONE;
  int8_t stageDelta = 0;   // for EF_STAGE, so the UI can say "rose sharply"
  uint8_t stageMask = 0;
  bool missed = false;
  bool crit = false;
  bool immune = false;     // type chart says it does nothing
  bool skipped = false;    // asleep, frozen, fully paralysed or recharging
  bool hurtSelf = false;   // confusion
  bool charged = false;    // spent the turn winding up
  bool healed = false;
  bool targetFainted = false;
  bool userFainted = false;  // EXPLOSION, DESTINY BOND, recoil
  // v3.40: a switch the engine asks for and the battle screen performs, since
  // only the screen knows the teams. forceOut: the TARGET is dragged out for a
  // random healthy teammate (ROAR, DRAGON TAIL; a wild battle simply ends).
  // selfOut: the USER leaves after acting (U-TURN, VOLT SWITCH, PARTING SHOT),
  // the player choosing who comes in. baton: BATON PASS, see battleBatonPass().
  bool forceOut = false, selfOut = false, baton = false;
  // v3.56: held items that move a creature out. ejectOut: the TARGET's EJECT
  // BUTTON sends it back; userOut: its RED CARD sends the ATTACKER back. Both
  // bring on a random healthy teammate and, unlike ROAR, never end a wild fight.
  bool ejectOut = false, userOut = false;
  uint8_t atkItem = ITEM_NONE, defItem = ITEM_NONE;
  // v3.37: everything else that happened, in order, for the narration. Each is
  // a BN_ code and who it is about (0 = the one acting, 1 = its target).
  uint8_t note[6] = { 0 }, noteWho[6] = { 0 };
  uint8_t noteN = 0;
  void add(uint8_t bn, uint8_t who) { if (noteN < 6) { note[noteN] = bn; noteWho[noteN] = who; noteN++; } }
};

// Narration codes (TurnLog::note). TamaPoke.ino maps each to a string.
enum : uint8_t {
  BN_NONE = 0, BN_PROTECTED, BN_BRACED, BN_ENDURED, BN_FLINCHED, BN_FAILED,
  BN_RAIN, BN_SUN, BN_SAND, BN_HAIL, BN_WEATHER_END, BN_SAND_HURT, BN_HAIL_HURT,
  BN_ROCKS, BN_SPIKES, BN_TSPIKES, BN_WEB, BN_ROCKS_HURT, BN_SPIKES_HURT, BN_WEB_SLOW, BN_HAZARDS_GONE,
  BN_REFLECT, BN_LSCREEN, BN_VEIL, BN_SCREEN_BROKE, BN_SCREEN_END,
  BN_SUB_UP, BN_SUB_HIT, BN_SUB_GONE, BN_REST, BN_SEEDED, BN_SEED_DRAIN, BN_BOUND, BN_BIND_HURT,
  BN_TRAPPED, BN_DROWSY, BN_TAUNTED, BN_TAUNT_BLOCK, BN_ENCORED, BN_CURSED, BN_CURSE_HURT, BN_DRUM,
  BN_STAGES_RESET, BN_PSYCHUP, BN_PAINSPLIT, BN_TAILWIND, BN_TRICKROOM, BN_DESTINY, BN_DESTINY_TOOK,
  BN_PERISH, BN_AQUARING, BN_RING_HEAL, BN_WISH, BN_WISH_HEAL, BN_SAFEGUARD, BN_MIST, BN_PUMPED,
  BN_NOTHING, BN_CHARGED, BN_TERRAIN, BN_KNOCKED, BN_OHKO, BN_FUTURE, BN_FUTURE_HIT, BN_TIRED,
  BN_TOXIC, BN_FOCUS_LOST, BN_SELF_FAINT, BN_INVERTED, BN_ASLEEP, BN_WOKE, BN_CURED, BN_STAGE_UP,
  BN_COUNT
};

uint16_t stagedStat(uint16_t base, int8_t stage);
uint16_t battleDamage(const Combatant &atk, const Combatant &def, uint16_t mv,
                      bool crit, uint8_t roll);

// True if `a` using `ma` acts before `b` using `mb`. Priority first, then the
// staged speed, and paralysis halves it.
bool battleMovesFirst(const Combatant &a, uint16_t ma,
                      const Combatant &b, uint16_t mb, uint8_t *claw = nullptr);

// One creature's action. Applies damage, ailments, stat stages and recoil.
void battleAct(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log);

// Burn and poison chip damage, applied to one creature after both have acted.
// `other` (the creature facing it) receives LEECH SEED's drain; without one the
// seed still drains, into nobody.
void battleEndTurn(Combatant &c, TurnLog &log);
void battleEndTurn(Combatant &c, Combatant *other, TurnLog &log);
// Once per round, after both battleEndTurn calls: weather, terrain, TRICK
// ROOM and the side timers count down.
void battleEndRound(TurnLog &log);

// v3.37 turn bookkeeping.
// Before either acts: records what each chose (SUCKER PUNCH reads it) and
// applies the locks -- a creature under ENCORE or mid-THRASH uses that move
// whatever was picked. Both moves are passed by reference for that reason.
void battleTurnStart(Combatant &a, uint16_t &ma, Combatant &b, uint16_t &mb);
// A creature coming on to the field (the start of a fight, a switch, a
// replacement): sets its side, then the hazards on that side bite.
void battleOnEntry(Combatant &c, uint8_t side, TurnLog &log);
// Leaving: everything that does not survive a switch (stages, seed, binds,
// substitute, taunt...) is cleared, as in the games.
void battleOnSwitchOut(Combatant &c);
// MEAN LOOK, BLOCK, a bind or INGRAIN: false = it may not switch out.
bool battleCanSwitch(const Combatant &c);
// BATON PASS: what `to` (just sent in, after battleOnSwitchOut(from)) takes
// over from `from` (as it was before leaving): stat stages, the substitute,
// confusion, FOCUS ENERGY, LEECH SEED, AQUA RING / INGRAIN, CURSE, PERISH SONG.
void battleBatonPass(const Combatant &from, Combatant &to);
// The type a move hits with in THIS situation (HIDDEN POWER, WEATHER BALL).
uint8_t battleMoveType(const Combatant &atk, uint16_t mv);
// The creature's types as this fight sees them (a plate on ARCEUS changes it).
bool battleIsType(const Combatant &c, uint8_t t);
// Sets up what a held item decides before the fight starts (ARCEUS's plate).
// combatantFromPet/Party call it; anything building a Combatant by hand and
// giving it an item must too.
void battleHeldSetup(Combatant &c);

// Picks the attacker's move. `smart` is what separates hard mode from easy:
// easy picks uniformly at random, hard reads the type chart, accuracy, whether
// a move kills this turn, and whether a status or a stat boost is worth the
// turn it costs.
uint16_t aiChooseMove(const Combatant &self, const Combatant &foe, bool smart);
