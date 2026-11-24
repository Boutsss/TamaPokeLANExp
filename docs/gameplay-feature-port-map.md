# Gameplay feature port map

This map compares the selected base `reallyjustsomeguy/TamaPokeLANExp`
with `vunguyen2712/TamaPoke` (feature source). It is a porting plan, not a
claim that these modules have been merged.

## Base remains authoritative

The base is the newer v3.28 line and has several files that the feature source
does not have: `inventory.cpp/.h`, `nvsinfo.cpp/.h`, and
`pedometer.cpp/.h`. Keep these base files. In particular, do not replace
base `inventory.cpp` with feature `items.cpp`, and do not replace base
`TamaPoke.ino`, `pet.cpp`, `party.cpp`, `save.cpp`, or `link.cpp`
with the feature-fork versions.

## Feature-only gameplay modules to evaluate and port

| Module | Purpose | Integration caution |
| --- | --- | --- |
| `game_events.cpp/.h` | Event bus for gameplay systems | Add sinks only after checking event ordering and duplicate delivery |
| `tasks.cpp/.h` | Daily and weekly tasks | Depends on event bus, Pet, Party, inventory/items, wild, tower, breed |
| `tower.cpp/.h` | Battle tower and rematches | Depends on trainer/battle APIs and reward inventory |
| `breed.cpp/.h` | Daycare and breeding | Depends on dex evolution tables, species art/region availability, items and party |
| `rank.cpp/.h` | Trainer rank, unlocks and themes | Depends on catch/challenge counters and UI/theme plumbing |
| `challenge.cpp/.h` | Weekly gym challenge | Depends on trainer tables, party, battle and rank |
| `persona.cpp/.h` | Creature personality modifiers | Requires a safe persistent trait field; base PartyMon has no feature-fork `spare` field |
| `friends.cpp/.h` | Two-board gifts and visits | Depends on link protocol, journal persistence, item transfer and peer compatibility |
| `habitat.h` | Habitat definitions/constants | Check dex, region and art availability before port |
| `ui_arcade.ino` | Four arcade minigames and prizes | Depends on graphics, touch, audio, inventory and new save keys |
| `ui_hub.ino` and feature screens | Hub navigation and feature menus | Port in small screen-specific slices; do not replace base home/navigation wholesale |

Feature-only UI files also include arcade, challenge, daycare, friends, habitat,
hub, persona, rank, tasks, tower, training and weather screens. Some concepts
may overlap with newer base screens; compare behavior before importing.

## Persistent state additions to the base save manifest

The feature save table adds these keys that are not in the base table:

- `tasks`, `twStrk`, `twBest`, `twSeed`, `rmWin`
- `dcare`
- `hitm`, `dxrw`, `dxgf`
- `wchain`
- `rkTop`, `rkCat`, `rkChW`, `rkChL`, `rkThm`, `rkAvG`
- `chal`, `prsn`
- `frId`, `frGday`, `frVday`, `frJrn`
- `arcHi`, `arcDay`, `arcRw`

Before enabling any module, add its keys to the BASE `SAVE_FIELDS` table and
verify every key's exact type, length, maximum size, and migration behavior.
Preserve the base checkpoint keys `petA`, `petB`, `plyA`, and `plyB`.
Do not copy the feature fork's `save.cpp`: its restore path clears the
namespace and its field table does not include those base checkpoint keys.

The feature source has no save fields for weather/habitat in this table; inspect
their actual runtime state before deciding whether they need persistent keys.

## Party record incompatibility

The feature fork's PartyMon includes held-item and personality bytes after
moves. The base PartyMon instead appends care-state fields after moves and has
a different record layout. Raw feature-fork `party` or `box` NVS blobs must
not be interpreted using the base PartyMon layout. The base's existing
short-record migration is for its own historical layouts, not a general
cross-fork schema converter.

If held items or personality on banked creatures are ported, define explicit
field-by-field conversion and append any new base fields at the end of the
base struct with versioned stride tests. Never copy feature structs wholesale.

## Port order

1. Preserve base save, PartyMon, NVS checkpoint and installer behavior.
2. Add a save-manifest-only change for one isolated feature and test backup /
   restore round-trips, including old base saves.
3. Port pure logic modules with host tests before touching UI.
4. Integrate gameplay event emission and inventory rewards.
5. Add UI screens and navigation after their logic tests pass.
6. Port friends/trading only after durable storage acknowledgements and
   protocol compatibility are fixed.
7. Build the ESP32-S3 firmware and regenerate installer hashes from the exact
   tested binaries.

No module is ready for release merely because it compiles: feature logic,
persistence, UI, installer, and existing-save regression tests must all pass.
