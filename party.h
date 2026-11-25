#pragma once
#include <Arduino.h>
#include <Preferences.h>

// The party: pets that finished their life and were kept, rather than being
// dissolved into a single Pokedex bit like every previous ending did.
//
// Only the two endings the player CHOOSES bank a pet -- farewell and release.
// A runaway does not: it is the game's one punishing outcome, and letting a
// neglected pet come back as a team member would take the sting out of it.
#define PARTY_SLOTS 6      // the STORED record count: never shrink a stored layout
// The party is the creature you are raising PLUS five banked: the live pet is
// party slot 1 on screen, and slots[0..4] are slots 2..6. slots[5] stays in the
// stored layout (shrinking it would reinterpret every save) and is kept EMPTY:
// Party::begin() moves anything found there into a free slot or the box.
#define PARTY_BANKED (PARTY_SLOTS - 1)
#define BOX_NAME_LEN 12   // 11 characters, the shared keyboard's limit
// The PC BOX: storage beyond the six that fight. 40 boxes of 30 -- 1200 slots,
// the Sword/Shield PC -- held in RAM (PSRAM on the board) and kept in a FILE
// on the board's own FAT partition (box_store.h), because 1200 records are
// three times the whole NVS partition.
//
// It used to be 18 slots under the NVS key "box". That key is still READ, once:
// Party::begin() moves those creatures into the file and only then removes the
// key, so a failure leaves a creature in two places, never in none. An old
// backup that carries "box" restores the same way.
#define BOX_PER_BOX 30
#define BOX_BOXES 40
#define BOX_SLOTS (BOX_PER_BOX * BOX_BOXES)
#define BOX_LEGACY_SLOTS 18     // the old NVS box, migrated at boot
// Anything that numbers "party then box" in one index (breed.h, held_items.h,
// trades) needs 16 bits now: 6 + 1200 does not fit a byte. CLAUDE.md trap 2.
static_assert(PARTY_SLOTS + BOX_SLOTS > 255, "if this ever fails, nothing is wrong -- but nobody narrowed an index back to a byte either");
#define MOVE_SLOTS 4    // the same four every trainer gets in the real games

// A stored creature -- one of the six that fight, or one in the box.
//
// It used to be frozen forever: banked at a level and never trained again. It
// still is WHILE STORED, but the record now carries the full care state as
// well, so swapping it in as the focused creature resumes exactly where it left
// off instead of handing back a blanked-out copy. Freezing is now a property of
// where the record lives, not of the record itself.
#define PM_EGG_PAID 0xE9
#define PM_FAV 0xF7      // PartyMon::fav: a magic value, for the same reason as PM_EGG_PAID
struct PartyMon {
  int16_t dex = 0;      // Pokedex number, 0 = empty slot
  uint16_t level = 1;   // cached from ageMinutes so lists need not recompute it
  uint16_t medals = 0;  // what it earned in life
  uint8_t ivAtk = 0, ivDef = 0, ivSpe = 0, ivHp = 0;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t shiny = 0;
  char nick[12] = "";
  // Moves travel with the creature. 0 = empty slot (MOVE_TBL[0] is the "-"
  // filler). Appended at the END of the struct on purpose -- Party::begin()
  // migrates older, shorter blobs by length, and that only works if nothing
  // that already exists moves. Everything below obeys the same rule.
  uint8_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };

  // ---- care state, appended in v2.10 --------------------------------------
  // 0 means "this record predates care state": its fields below are merely the
  // initialisers, not something the creature earned, so `level` is the only
  // truth about its age. The default is 0 rather than 1 precisely so that a
  // record the length migration only half-filled reports itself honestly --
  // the migration memcpy's oldStride bytes over a default-constructed record
  // and leaves the tail alone, so anything defaulting to "I have real data"
  // would be a lie the moment the struct grows again.
  uint8_t stateVersion = 0;
  uint8_t fullness = 80, joy = 80, energy = 80, hygiene = 100;
  uint8_t poops = 0, weight = 0, bond = 0;
  uint8_t berryKnown = 0, careMistakes = 0;
  uint8_t evoDeclinedLv = 0, lastLearnLevel = 0;
  // The real age. `level` stays as its cached mirror so every list, the battle
  // squad builder and the box screen keep reading one byte instead of dividing.
  uint32_t ageMinutes = 0;
  // Held item appended after every older field; zero means no item.
  uint8_t held = 0;
  // Personality (persona.h). Lives in what was padding after `held`, so the
  // record size did not change; 0 = none (banked before personalities).
  uint8_t persona = 0;
  // PM_EGG_PAID once this creature's ending has already produced an egg
  // (farewell, retire or release). Lives in what was padding after `persona`,
  // so the record size did not change. A MAGIC value rather than a bool:
  // padding in records banked before this field existed is not guaranteed to
  // be zero, and a stray 1 must not read as "already paid".
  uint8_t eggPaid = 0;
  // PM_FAV: a FAVOURITE, which cannot be released, replaced to make room or
  // traded away until it is unfavourited. The last byte of what was padding,
  // so the record is still 52 bytes (see the static_assert below).
  uint8_t fav = 0;

  bool empty() const { return dex < 1; }
  bool paidEgg() const { return eggPaid == PM_EGG_PAID; }
  bool isFav() const { return fav == PM_FAV; }
  bool hasCareState() const { return stateVersion >= 1; }
};

// sizeof(PartyMon) is load-bearing in four places (CLAUDE.md section 5), and
// eggPaid and fav were placed in old padding precisely so it would not move.
// If this fires, read that section before touching anything else.
static_assert(sizeof(PartyMon) == 52, "PartyMon grew: party/box migration, MAX_VAL, the link tag and the pet tail all depend on it");

class Party {
public:
  PartyMon slots[PARTY_SLOTS];
  // BOX_SLOTS records, allocated by begin() (PSRAM on the board). Indexed like
  // an array; never touched before begin().
  PartyMon *box = nullptr;
  // Bumped by every boxSave() and load: anything that caches a view of the box
  // (the box screen's filtered, sorted list) recomputes when it moves.
  uint32_t boxGen = 0;

  // Each PC box's name, NUL-terminated; empty = the default ("BOX n"). NVS
  // "boxnm", so it travels in the backup like any other key.
  char boxNames[BOX_BOXES][BOX_NAME_LEN] = {};
  void boxNameOf(uint8_t b, char *out, size_t n) const;   // the custom name or "BOX n"
  bool setBoxName(uint8_t b, const char *name);           // "" restores the default

  void begin();                 // load the party (NVS) and the box (its file)
  void reshelveSixth();         // slots[5] -> a free slot or the box (see PARTY_BANKED)
  uint8_t count() const;
  bool isFull() const { return count() >= PARTY_BANKED; }
  int firstFree() const;        // index of the first empty slot, -1 if full
  bool add(const PartyMon &m);  // into the first free slot; false if full
  void replaceAt(uint8_t i, const PartyMon &m);
  void releaseAt(uint8_t i);    // free a slot again
  bool save();
  uint16_t boxCount() const;
  int boxFirstFree() const;
  bool boxAdd(const PartyMon &m);     // into the first free box slot
  // For a TRADE (and anything else that must not be lost): into the first free
  // party slot, else the first free box slot, written and READ BACK. True only
  // if the record is verifiably on flash; on any failure RAM is put back and
  // nothing is claimed. `where` = 0..5 party, 6.. box (the breed.h indices).
  bool storeDurably(const PartyMon &m, int16_t *where);
  // Takes out the record at `where` only if it is still exactly `expect` (the
  // slot may have changed since it was offered), WITHOUT returning its held
  // item to the bag -- a traded creature takes its item with it. Written and
  // read back; true only if the slot is verifiably empty on flash.
  bool takeOutExact(int16_t where, const PartyMon &expect);
  void boxReleaseAt(uint16_t i);
  bool boxSave();
  // Swaps a party slot with a box slot. Either may be empty, so this doubles as
  // deposit and withdraw rather than needing three separate operations.
  void swapPartyBox(uint8_t partyIdx, uint16_t boxIdx);
  void swapBoxBox(uint16_t a, uint16_t b);   // MOVE within the PC box: swap two slots (either may be empty)
  // WIPE / IMPORT: the box is emptied and its file removed.
  void boxWipe();

  // combat stats of a party member, same formula as the live pet's
  uint16_t atkOf(const PartyMon &m) const;
  uint16_t defOf(const PartyMon &m) const;
  uint16_t speOf(const PartyMon &m) const;
  uint16_t vitOf(const PartyMon &m) const;
  uint16_t spaOf(const PartyMon &m) const;
  uint16_t spdOf(const PartyMon &m) const;

private:
  Preferences prefs;
  void loadBox();
  void migrateLegacyBox();
  void loadBoxFileOnly();
};

extern Party party;
