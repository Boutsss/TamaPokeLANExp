# Merge safety audit

This document records the storage and LAN invariants that must remain true while
porting the TamaPoke feature and trading forks into this branch.

## Chosen firmware foundation

Keep `reallyjustsomeguy/TamaPokeLANExp` as the firmware foundation. Do not
replace its save implementation, partition layout, installer, or board pin
configuration with files copied wholesale from either source fork.

## Existing-save invariants

- The base firmware stores the active pet and party in the NVS namespace
  `tamapoke`.
- Its save implementation uses alternating checkpoint keys `petA`, `petB`,
  `plyA`, and `plyB`. These keys must remain in the backup field table.
- The existing party box is an 18-slot NVS value named `box`. It is distinct
  from the larger PC storage introduced by the trading fork.
- Save import/export uses the `TKPS` format with CRC validation. Keep the base
  validation and checkpoint recovery behavior when adding new save fields.
- The base web installer writes bootloader, partition table, boot app, and app
  as separate address-specific parts. Never publish the merged
  `web/firmware/tamapoke.bin` as an update part at offset zero.
- `new_install_prompt_erase` must remain `true`; on this firmware's
  no-Improv install path, false triggers a full-chip erase without asking.
- Keep `tools/check_installer.py` in the release/build validation path.

## PC storage migration requirements

The trading fork adds a large FAT-backed PC store and migrates two older stores:
the older dex-indexed file and the base-style NVS box. Porting this requires an
explicit, tested migration that:

1. Copies every valid old record before clearing its source.
2. Does not clear the NVS box unless the new PC file has been fully written and
   verified.
3. Preserves ordering, nickname, level, IVs, training, moves, shiny state, and
   all fields present in the old `PartyMon` record.
4. Handles a full PC store or failed filesystem writes without deleting source
   records.
5. Is idempotent across reboot/power-loss at every migration step.
6. Does not format an existing filesystem as a side effect of a failed mount
   unless the user has explicitly chosen a destructive recovery path.

The base already has an 18-slot `party.box` NVS implementation, so this is a
migration between two stores, not a simple addition of a new empty box.

## Trade protocol requirements

The trading fork's LAN protocol must be integrated with the base's battle
protocol without changing existing battle packet meanings accidentally.
Validate protocol/build compatibility before accepting a trade.

The trade must follow a storage-first rule: a device must not send the final
"stored" acknowledgement until its received creature has actually been
persisted successfully. In the trading fork's current flow, the link layer
sets the trade phase and sends `LM_TDONE` before the UI's `tradePoll()`
calls `pcAdd()`. Since `pcAdd()` can fail, that ordering can let the peer
release its offered creature even though the local copy was not stored.
Refactor this into an explicit app-to-link storage confirmation.

Required trade tests include:
- lost, duplicated, delayed, and reordered frames;
- a changed offer invalidating earlier acceptance;
- power loss and link loss at each commit stage;
- full PC storage and failed filesystem writes;
- both devices retaining at least one copy of each creature after an interrupted
  exchange (duplication is acceptable; loss is not);
- compatibility rejection for peers with incompatible packet/build layouts.

## Feature-port requirements

Port feature modules in dependency groups rather than replacing the sketch:
- shared data and save fields first;
- gameplay logic and its unit tests next;
- UI screens and navigation after their dependencies;
- LAN trading after the shared protocol and persistent storage are stable.

Every new persisted key must be included in `SAVE_FIELDS` and covered by the
save round-trip test. Preserve existing checkpoint keys and the save format's
validation behavior.

## Validation before publishing an installer

A release candidate is not ready until all of the following are true:

- emulator regression suite passes;
- Arduino ESP32-S3 firmware compile passes;
- save export/import round-trip tests pass, including old-save migration;
- installer layout test proves NVS and FFat are outside every write range;
- installer erase prompt remains safe and defaults to no erase;
- PC migration and trade interruption tests pass;
- browser installer assets and firmware manifest hashes are regenerated from
  the exact tested binaries;
- attribution and non-commercial asset licensing requirements are respected.

No firmware should be flashed to the user's device as part of repository work.
