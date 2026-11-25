#pragma once
#include <Arduino.h>
#include "party.h"

// The daycare: breeding an egg from two banked creatures.
//
// Ported from vunguyen2712/TamaPoke (breed.cpp, "feature 5"). The RULES are
// theirs, unchanged; the persistence and the hand-over to the live egg were
// rebuilt for this base's checkpointed save -- see "THE HAND-OVER" below.
//
// GAME LOGIC ONLY: no gfx, no SD, no touch. pet.cpp calls the hooks at the
// bottom, so this links into every headless test binary next to it. The screen
// is renderDaycare() in TamaPoke.ino.
//
// THE RULES (README § "Daycare" documents the same numbers):
//
//   Compatible   -- two DIFFERENT banked creatures (party or box) that share a
//                   type, or where exactly one of them is DITTO. Two DITTOs
//                   never; any creature whose evolution line holds a legendary
//                   never. There are no egg groups in the data, so the type
//                   rule is the whole compatibility table.
//   The egg      -- the BASE form of the non-Ditto parent's line (parent A's if
//                   both qualify). It must be something the egg pool itself
//                   could hatch: not evolution-only, and speciesShowable() --
//                   drawable, its region's pack on the card. Otherwise the pair
//                   is refused with a reason.
//   IVs          -- 3 of the 4 are inherited, each from a random parent; the
//                   4th is rolled at hatch like any egg. With the OVAL CHARM all
//                   4 are inherited, each the BETTER of the two parents. The
//                   legendary and shiny IV floors still apply on top.
//   Shiny        -- both parents shiny: a 1 in BREED_SHINY_BONUS chance, rolled
//                   once when the egg is made, of the egg being shiny on top of
//                   its own normal roll.
//   Where it goes -- QUEUED (one at a time) and becomes the NEXT egg: newEgg()
//                   takes it instead of rolling. If the live creature is an
//                   uncracked egg it can also be swapped in at once.
//   Cooldown     -- one breed per game day (Pet::today()).
//   Parents are NOT consumed, and nothing about them is changed.
//
// THE HAND-OVER. The queue lives under its own NVS key ("dcare"), the egg in the
// pet checkpoint, and those are two separate writes. vunguyen2712's version
// cleared the queue FIRST and saved the egg second, so a power cut between them
// lost the bred egg outright. Here it is the other way round:
//
//   1. newEgg() PEEKS at the queue and builds the egg from it,
//   2. saves the pet checkpoint,
//   3. and only if that landed calls breedCommitTaken(), which moves the queued
//      record to "live" (the IVs waiting for the hatch) and clears the queue.
//
// A cut between 2 and 3 leaves the queue holding the egg that is now also live.
// breedBegin() recognises exactly that state -- an uncracked live egg of the
// queued species -- and finishes step 3. The failure direction is "the same
// egg twice", never "no egg".
#define BREED_BANK (PARTY_SLOTS + BOX_SLOTS)   // banked index: party 0..5, box 6..1205 (16-bit)
#define BREED_INHERIT 3          // IVs inherited without the Oval Charm (of 4)
#define BREED_IV_ROLL 0xFF       // an IV the parents did not decide: rolled at hatch
#define BREED_SHINY_BONUS 16     // both parents shiny: 1 in this many
#define BREED_DITTO 132          // DITTO: breeds with anything but itself

class Pet;

// Why a pair can or cannot breed. The screen shows the reason.
enum BreedVerdict : uint8_t {
  BV_OK = 0,
  BV_EMPTY,       // a slot is empty
  BV_SAME,        // the same creature twice
  BV_TWO_DITTO,
  BV_LEGEND,      // a legendary line
  BV_NO_TYPE,     // no shared type and no Ditto
  BV_NO_EGG,      // the egg would be undrawable, evolution-only or its pack is missing
};

// What making an egg did.
enum BreedResult : uint8_t { BR_OK = 0, BR_BAD_PAIR, BR_QUEUE_FULL, BR_COOLDOWN, BR_NOT_READY, BR_SAVE_FAILED };

struct BredEgg {
  int16_t dex = 0;                  // 0 = none
  uint8_t iv[4] = { 0, 0, 0, 0 };   // atk, def, spe, hp; BREED_IV_ROLL = rolled at hatch
  uint8_t shiny = 0;                // the both-shiny bonus roll came up
  int16_t pa = 0, pb = 0;           // the parents' species, for the screen
};

// ---- the rule (pure; no state)
int16_t breedBaseOf(int16_t dex);              // first form of dex's line
bool breedLineLegendary(int16_t dex);          // any member of the line legendary
bool breedEggable(int16_t dex);                // could this hatch from an egg here
BreedVerdict breedCheck(const PartyMon &a, const PartyMon &b);
int16_t breedEggSpecies(const PartyMon &a, const PartyMon &b);   // 0 unless BV_OK
// Fills out[4] (atk, def, spe, hp). Uses random().
void breedInherit(const PartyMon &a, const PartyMon &b, bool ovalCharm, uint8_t out[4]);

// ---- the daycare's state (NVS key "dcare", in SAVE_FIELDS)
const PartyMon *breedBanked(uint16_t idx);      // nullptr if out of range
BreedVerdict breedCheckIdx(uint16_t ia, uint16_t ib);
bool breedOnCooldown(uint32_t day);            // true = already bred today
BreedResult breedMake(uint16_t ia, uint16_t ib, uint32_t day);   // emits EV_BREED
bool breedHasQueued();
bool breedPossible();                          // a compatible pair exists and the queue is free
const BredEgg &breedQueued();
const BredEgg &breedLive();                    // the IVs waiting for the egg on screen, if any
bool breedQueuedHatchable();                   // the queued species can still hatch here
bool breedCanSwapIn(const Pet &p);
bool breedSwapIn(Pet &p);                      // the uncracked live egg becomes the queued one

// ---- hooks for pet.cpp (only ever called for the begin()'d player Pet)
bool breedPeekQueued(int16_t &dex, bool &shiny);  // step 1: what the next egg should be
void breedCommitTaken();                          // step 3: the egg's save landed
void breedForgetLive();                           // an ordinary egg replaced whatever was live
// hatch(): the inherited IVs, if the egg cracking is the bred one. Consumes it
// only on a match -- a region switch can park a bred egg while another hatches.
bool breedTakeLive(int16_t dex, uint8_t out[4]);

// Called once from setup(), after pet/party/bag have loaded. Loads from NVS and
// finishes a hand-over a power cut interrupted (see THE HAND-OVER).
void breedBegin(const Pet *p = nullptr);
void breedReset();        // tests: forget RAM state (NVS untouched)
