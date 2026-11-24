# Trade protocol source review

Reviewed against `tubiana/TamaPoke` on its `main` branch, particularly
`link.cpp`, `link.h`, and the `tradePoll()` implementation in
`TamaPoke.ino`.

## Storage acknowledgement currently precedes storage

The current flow is:

1. `Link::tradeCommit()` sets `tPhase = 1`, sets `tArrived = true`, and
   immediately sends `LM_TDONE`.
2. The main loop later calls `tradePoll()`.
3. `tradePoll()` clears `tArrived` and only then calls `pcAdd(m)`.
4. The sender treats `LM_TDONE` as proof that the receiver stored the offered
   creature. Its own `tradePoll()` can then release its creature.

The local release path checks `tradeStored`, which prevents the receiver that
failed its own `pcAdd()` from releasing its offer. That is not sufficient:
the peer has already received the premature `LM_TDONE` and may release its
original creature. Example: A fails to store B's creature, but tells B it is
stored; B stores A's creature and receives A's premature DONE, then releases
B's original. A keeps A because its own storage failed, while B has lost B.
This violates the protocol's stated no-loss guarantee.

## Required protocol change

Separate agreement from durable storage:

- `LM_TACCEPT` means only "I agree to this exact pair of offer sequence IDs".
- Once both accept, each app attempts to persist the peer's creature.
- The app reports storage success or failure back to the link state machine.
- Only after the app reports a successful, verified persistent write may the
  link send a distinct durable-storage acknowledgement (the current
  `LM_TDONE` meaning).
- A failed write must not send DONE. It must leave the local offered creature
  untouched and communicate a retry / storage-full state.
- The peer must never release its offer based on ACCEPT alone.
- Duplicate DONE packets must remain idempotent; sequence IDs must continue to
  bind each acknowledgement to the exact pair of offers.
- Persisting the received creature must be verified before sending DONE. If
  the filesystem can lose a write after close, add read-back validation or a
  journaled commit protocol.

## Test cases required before port

1. A's receive storage fails while B's succeeds.
2. B's receive storage fails while A's succeeds.
3. Both stores fail.
4. Both stores succeed, but either DONE packet is lost and retransmitted.
5. Duplicate ACCEPT and DONE packets.
6. Link loss after one store commits but before either DONE is observed.
7. Link loss after one side observes DONE but before its own offer is released.
8. PC full, filesystem mount failure, write failure, and read-back mismatch.
9. Changed or withdrawn offers after an earlier ACCEPT.
10. Power loss at every persistent-write and acknowledgement boundary.

For all interrupted cases, assert that each offered creature exists in at least
one verified persistent location across the two devices. A duplicate is
acceptable; losing both copies is not.

Do not port the current trade state machine unchanged. Its existing tests cover
packet delivery behavior, but they do not make the app's storage operation part
of the protocol's acknowledgement boundary.
