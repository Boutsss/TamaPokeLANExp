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

## Event plumbing now integrated

The base firmware now publishes these gameplay events through the append-only
event bus: care-day registration, hatch, evolution, feed (including favourite
berry as argument 1), clean, play (score as argument), training (gain as
argument), first gym badge win, gym rematch win, wild encounter, wild battle
win, and successful capture. Event publication is deliberately additive and
does not itself change existing game behavior or require new NVS keys.

## Daily and weekly tasks: first gameplay slice ported

The adapted task engine now uses the base game's event bus, Pet clock,
`ItemKey` inventory, and NVS namespace. It intentionally offers only task
types that already have real base-game events: feeding, favourite berries,
cleaning, play, training, care days, hatching, evolution, gym wins, rematches,
wild encounters, wild wins, and catches. Breeding and tower tasks are not
offered until those systems exist in the base branch.

Daily and weekly task rows are selected deterministically from the supported
pool, with progress, claimed flags, and rotation dates persisted in a compact
versioned `tasks` blob. The task UI is connected to the existing menu and
includes daily/weekly pages, progress, reward claims, and a daily bonus.
Rewards are explicitly mapped to existing base inventory items; this does
not import the feature fork's item IDs. Emulator coverage exercises task
selection, progress, reload persistence, failed claim-marker writes, duplicate
claims, and save-backup inclusion. This is a ported slice, not the full
feature-fork task system.

## Persistent state additions to the base save manifest

At this point, the only feature-specific key added to the BASE `SAVE_FIELDS`
table is `tasks` (`SK_BYTES`). Its fixed compact payload is versioned
independently and bounded by the existing backup maximum. The base checkpoint
keys `petA`, `petB`, `plyA`, and `plyB` remain present.

The feature fork also has keys not yet ported, including tower/rematch,
daycare, held-item/dex rewards, rank, challenge, persona, friends, and arcade
state. Add each only alongside its actual module, after verifying exact type,
length, maximum size, and migration behavior. Do not bulk-reserve future keys:
oversized backup manifests can exhaust the export buffer and break existing
save backups. Do not copy the feature fork's `save.cpp`: its restore path
clears the namespace and its field table does not include the base checkpoint
keys.

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
