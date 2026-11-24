# PC storage source review

Reviewed against `tubiana/TamaPoke` on its `main` branch, files
`pcbox.cpp` and `pcbox.h`. This is a source review, not a claim that the
trading fork has been modified or tested on hardware.

## Do not port pcbox.cpp unchanged

Several paths can lose existing creatures or user data.

### 1. Migration clears sources even when the destination is incomplete

In `pcMigrate()`, records are appended only while `n < PC_MAX`. After the
loops, the code stores only when `n != was`. If the destination is already
full (`n == was == PC_MAX`), or only some source records fit, it still reaches
the cleanup code:

- `if (fromFile) remove(OLD_DEXBOX)`
- `if (fromBox) party.boxClear()`

The source is therefore deleted even though it was not fully copied. The same
problem applies if the old dex-box file is truncated: `fromFile` is set merely
because fopen succeeded, and the old file can be removed after a partial read.

**Required fix:** migration must track exactly which source records were
validated and committed. It may clear a source only after every record from
that source has been copied and the destination has been reopened and verified.
If capacity is insufficient, retain the source intact and report a partial /
blocked migration. Never treat a partial migration as complete.

### 2. The replacement write is not power-loss safe

`store()` writes `TMP`, closes it, removes `PATH`, and then renames `TMP`
to `PATH`. A power loss or rename failure after `remove(PATH)` leaves no
current PC file. This contradicts the comments claiming the old file remains
available until the new one is swapped in.

**Required fix:** use a filesystem-supported atomic replacement where available,
or a two-slot generation scheme with checksums and a commit marker. Do not
delete the last verified copy before a new copy has been verified and committed.

### 3. Mount failure can trigger automatic formatting

On ESP32, `mountFs()` calls `FFat.begin(true)`. The `true` argument enables
format-on-failure. A mount failure can therefore format the FAT partition,
which may contain the only copy of the PC store.

**Required fix:** mount without auto-format. If mounting fails, expose a clear
storage error and leave the partition untouched. Formatting must be a separate,
explicit recovery action after backup / confirmation.

### 4. Migration needs a durable completion record

The current migration infers work from whether source files or box entries are
present. A reboot during migration can cause duplicate copies, while clearing
the sources too early can lose creatures. Duplicates are preferable to loss,
but the migration needs a durable generation / completion marker and a
repeatable per-source commit strategy.

## Porting gate

Do not merge the PC storage module into the base branch until the above paths
are fixed and tests cover:

- PC store already full before migration;
- enough room for only part of the old box or dex-box;
- truncated or invalid legacy files;
- failure writing the temporary destination;
- power loss before and after the destination commit;
- mount failure with existing PC data;
- reboot at every migration phase;
- verification that every source creature remains present in either the source
  or verified destination after each simulated interruption.

The base branch's NVS box and save checkpoints remain authoritative until this
migration passes. Do not clear or repurpose them as part of an unverified port.
