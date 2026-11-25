#pragma once
#include <Arduino.h>

// The PC BOX's own storage: a FILE, not NVS. 1200 stored creatures are ~62 KB
// and the whole NVS partition is 20 KB -- and a full NVS fails every write, the
// live creature's included (CLAUDE.md "NVS can run out"). So, as in Tubiana's
// TamaPoke (feat/pc-boxes, MIT), the box lives on the board's own FAT
// partition (app3M_fat9M_16MB: 9 MB, unused until now), and in the emulator in
// a file beside the save. A missing or swapped microSD cannot touch it.
//
// Only OCCUPIED slots are written, each with its slot number, so a box that is
// mostly empty costs a few KB. The file is written whole beside the old one and
// renamed over it: a power cut leaves the old box or the new one, never half of
// each. The header carries the record size, so a later, longer PartyMon reads
// an old file by the front of each record, the rest at its defaults.
struct PartyMon;

bool boxStoreReady();                                     // the storage mounted
// Reads the file into `box` (BOX_SLOTS records, already cleared by the caller).
// False if there is no file yet (a fresh board, or one not yet migrated).
bool boxStoreLoad(PartyMon *box);
bool boxStoreSave(const PartyMon *box);                   // atomic; true if it landed
// Read the file back: is slot `slot` exactly `m` on flash? (A trade claims a
// creature is stored only once this says so.)
bool boxStoreSlotIs(uint16_t slot, const PartyMon &m);
void boxStoreWipe();                                      // WIPE and IMPORT start from empty
// Emulator: where the file lives (beside the save). The board ignores it.
void boxStoreSetDir(const char *dir);
