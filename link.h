#pragma once
#include <stdint.h>
#include "battle.h"
#include "trainers.h"   // TRAINER_TEAM_MAX: a link team is the same size

// Peer-to-peer battles.
//
// ONE DEVICE IS AUTHORITATIVE. That is forced, not chosen: battle.cpp makes 11
// random() calls a turn -- crit, damage spread, accuracy, ailment procs,
// multi-hit count, confusion, thaw, wake, speed ties -- so two devices
// resolving independently desync inside a single turn. A shared seed only holds
// until the two builds differ by one call. The host runs battleAct(); the guest
// sends an action and renders the result it is handed, and therefore needs no
// game logic at all.
//
// THE RADIO DROPS FRAMES. ESP-NOW is best effort, so nothing here may assume a
// packet arrived. Three rules cover it:
//
//   1. Every exchange is stamped with a turn number. A repeat of a turn already
//      handled is ignored, so a resend can never be mistaken for a new choice.
//   2. Whatever we last said is resent until something supersedes it. That is
//      what makes a lost frame a delay rather than a permanent stall.
//   3. A LinkResult carries ABSOLUTE state, never deltas. A guest that misses a
//      turn entirely still lands on the right numbers from the next one -- the
//      loss costs narration, not correctness.
//
// And every wait has a deadline: a peer that goes out of range or powers off
// ends as LINK_LOST with a message, never as a hang. But liveness is judged
// from RECEIVED traffic alone, and a side with nothing queued to resend sends
// nothing while it waits -- so whichever side is currently just thinking about
// a move, not transmitting, was starving the OTHER side's clock. A human
// deciding a move routinely takes longer than the deadline, so this fired on
// ordinary play, not just on a bad radio. LM_PING is tick()'s answer: a
// payload-less idle heartbeat sent on the same jittered cadence whenever
// nothing else is pending, so liveness no longer depends on anyone acting.
//
// The transport is a function pointer rather than a direct ESP-NOW call so the
// protocol can be tested: two Links cross-wired in one process exercise the
// whole handshake without a radio, and a deliberately lossy transport exercises
// all of the above. Only the radio itself is unverifiable here.

#define LINK_PROTO 6        // bump on ANY wire change; a mismatch is refused
                            // (4: trading; 5: hello flags bit1 = friends mode, LM_FRIEND;
                            //  6: LM_CARD, the trainer-card exchange)
#define LINK_SQUAD_OWE 8    // see Link::squadOwe
#define LINK_FRIEND_MAX 32  // a friends-mode state frame (friends.cpp owns the layout)
#define LINK_CARD_MAX 48    // a trainer card on the wire (card.h owns the layout)
#define LINK_MAX_PAYLOAD 200
#define LINK_NAME_LEN 12

#define LINK_RESEND_MS 400
// Resends are jittered, and that is not decoration. A fixed interval can lock
// step with a periodic interferer -- a beacon, a microwave -- so that the same
// packet in the cycle is lost every single time and the resends never help.
// The lossy test reproduces exactly that: with one frame in three dropped and a
// fixed interval, one squad packet was destroyed on all ten attempts.
#define LINK_JITTER_MS 200
#define LINK_PAIR_TIMEOUT_MS 60000    // waiting for a peer that may not exist
#define LINK_BATTLE_TIMEOUT_MS 60000  // mid-fight, where one is known to be there

// Idle at LINK_READY, nothing queued to resend: the heartbeat only needs to be
// RECEIVED, not delivered fast, so it rides its own slower cadence rather than
// LINK_RESEND_MS -- that constant stays fast for traffic someone is actually
// waiting on (a move, a result, a squad packet during pairing).
#define LINK_IDLE_PING_MS 1000

// A minute with nothing REAL happening -- as opposed to LINK_BATTLE_TIMEOUT_MS,
// which the heartbeat above keeps from ever elapsing on its own. Different
// question ("has anyone done anything") from "is the peer still there at all",
// so it gets its own name even though it is the same number today.
#define LINK_IDLE_MS 60000

// An action is a move slot, or a switch with the high bit set. One message
// rather than two, so turn matching and resend have a single path -- which is
// the place a second message type would most easily drift out of step.
//
// A move is stored as slot+1 so that 0 can keep meaning "nothing chosen yet".
// Storing the slot raw would make move slot 0 -- the first move every creature
// has -- indistinguishable from silence.
#define LINK_ACT_SWITCH 0x80
#define LINK_ACT_MOVE(i) ((uint8_t)((i) + 1))
#define LINK_ACT_SWITCH_TO(i) ((uint8_t)(LINK_ACT_SWITCH | ((i) & 0x0F)))
#define LINK_ACT_IS_SWITCH(a) (((a) & LINK_ACT_SWITCH) != 0)
#define LINK_ACT_SLOT(a) \
  ((uint8_t)(LINK_ACT_IS_SWITCH(a) ? ((a) & 0x0F) : ((a) - 1)))

enum LinkState : uint8_t {
  LINK_OFF = 0,
  LINK_LISTENING,    // waiting for a peer
  LINK_HANDSHAKE,    // hello sent, waiting for theirs
  LINK_SQUADS,       // teams being exchanged
  LINK_READY,        // both known; an action may be chosen
  LINK_WAITING,      // acted, waiting on the other side
  LINK_DONE,         // somebody won
  LINK_REFUSED,      // incompatible -- loudly, never a silent desync
  LINK_LOST,         // peer went quiet, or said goodbye
};

enum LinkMsg : uint8_t {
  LM_HELLO = 1,
  LM_SQUAD,
  LM_ACT,        // guest -> host: a move slot or a switch, stamped with a turn
  LM_RESULT,     // host -> guest: what happened, stamped with the same turn
  LM_END,
  LM_BYE,        // leaving on purpose, so the peer need not wait for a timeout
  LM_REMATCH,    // go again with the same squads
  LM_PING,       // idle heartbeat: liveness only, no payload, not `keep`d --
                 // tick() sends a fresh one each idle beat rather than
                 // resending one until "superseded" by nothing
  // ---- trading (ported from Tubiana/TamaPoke, storage-first fix applied) ----
  LM_TOFFER,     // [seq][PartyMon] what I put on the table; seq 0 = nothing
  LM_TACCEPT,    // [your seq][my seq] I take yours for mine
  LM_TDONE,      // [your seq][my seq] ...and yours is now DURABLY stored here
  // ---- gifts and visits (ported from vunguyen2712/TamaPoke, feature 7) ----
  LM_FRIEND,     // friends mode: one side's whole state (friends.cpp), resent
  LM_CARD,       // friends mode: my trainer card (card.h), sent unkept and
                 // repeated -- it must never take LM_FRIEND's resend slot
};

// What a pairing is FOR. Carried in the hello and refused on a mismatch, so a
// board on the FRIENDS screen can never pair into a battle (or the other way
// round) and read the other's packets as its own.
enum LinkMode : uint8_t { LINK_MODE_BATTLE = 0, LINK_MODE_FRIENDS = 1 };

// Why a link was refused, so a screen can say which of the loud refusals it is.
enum LinkWhy : uint8_t {
  LINK_WHY_NONE = 0,
  LINK_WHY_PROTO,     // a different protocol version
  LINK_WHY_BUILD,     // same protocol, different tables (linkBuildTag)
  LINK_WHY_SAMEID,    // two devices with the same id
  LINK_WHY_MODE,      // one is on FRIENDS, the other on LAN BATTLE
  LINK_WHY_RADIO,     // set by the UI: the radio did not come up
};

// A creature on the wire. Deliberately not `Combatant` itself: that carries
// live battle state (stages, ailments, timers) which only the host owns.
struct LinkMon {
  int16_t dex;
  uint8_t level;
  uint16_t maxHp;
  uint16_t base[SI_COUNT];
  uint8_t movesLo[MOVE_SLOTS];   // low byte of each move ID; movesHi below (v3.37)
  uint8_t shiny;
  char name[LINK_NAME_LEN];
  // Appended so a ghost (ghost.h) saved with the older, shorter record still
  // reads: its stride travels with it and the tail stays zero.
  uint8_t movesHi[MOVE_SLOTS];
  uint16_t move(uint8_t i) const { return (uint16_t)(movesLo[i] | ((uint16_t)movesHi[i] << 8)); }
  void setMove(uint8_t i, uint16_t m) { movesLo[i] = (uint8_t)m; movesHi[i] = (uint8_t)(m >> 8); }
};

// What the host tells the guest happened. Everything the guest needs to render
// a turn, and nothing it could use to resolve one -- it has no say in the
// outcome, which is the whole point of one side being authoritative.
//
// Every field is ABSOLUTE, not a delta. That is what lets a guest which missed
// a turn recover from the next one instead of drifting.
#define LINK_RESULT_NOTES 12
struct LinkResult {
  uint16_t hostHp, guestHp;
  uint8_t hostAil, guestAil;
  uint16_t hostMove, guestMove;
  uint16_t hostDmg, guestDmg;
  uint8_t hostIdx, guestIdx;   // which squad member is out on each side
  uint8_t flags;               // bit0 crit, bit1 super, bit2 a faint happened
  // v3.40: what the guest needs to narrate the turn like the host did -- the
  // battle.h BN_ notes in order, each about the host's creature (side 0) or the
  // guest's (1) -- and the field, absolute like everything above.
  uint8_t weather, terrain, trickRoom;
  uint8_t noteN;
  uint8_t note[LINK_RESULT_NOTES], noteSide[LINK_RESULT_NOTES];
};
static_assert(sizeof(LinkResult) <= LINK_MAX_PAYLOAD, "a turn result must fit one packet");

#include "party.h"      // PartyMon: what a trade moves

struct Link {
  uint8_t state = LINK_OFF;
  bool isHost = false;
  uint8_t protoTheirs = 0;
  uint16_t buildTheirs = 0;
  uint16_t id = 0;             // breaks the tie when both sides offer to host
  uint16_t peerId = 0;
  char peerName[LINK_NAME_LEN] = "";   // write: onPacket() only. read: display only.
  char myName[LINK_NAME_LEN] = "";     // write: begin() only. read: sendHello() only.

  LinkMon mine[TRAINER_TEAM_MAX];
  uint8_t mineN = 0;
  LinkMon theirs[TRAINER_TEAM_MAX];
  uint8_t theirsN = 0;
  uint8_t theirsExpected = 0;  // from their hello, so a lost SQUAD is detectable
  uint16_t theirsGot = 0;      // which slots have actually arrived

  uint8_t turn = 0;            // the exchange both sides are on
  uint8_t pendingAct = 0;      // host: the guest's action for `turn`, 0 = none
  bool youWon = false;

  // guest: the last turn the host resolved. `resultNew` is cleared by whoever
  // renders it, so a dropped or duplicated frame cannot replay a turn twice.
  uint8_t result[LINK_MAX_PAYLOAD];
  uint8_t resultN = 0;
  bool resultNew = false;

  // what we last said, kept so it can be said again until superseded
  uint8_t tx[2 + LINK_MAX_PAYLOAD];
  uint8_t txN = 0;
  bool txLive = false;
  uint32_t txAt = 0;
  uint32_t lastRx = 0;
  uint8_t resendSeq = 0;       // varies the resend interval, see LINK_JITTER_MS
  bool sawRx = false;          // onPacket has no clock; tick() stamps it
  bool armed = false;

  // Same idea as lastRx/sawRx, but LM_PING never counts -- this is "did
  // anything REAL happen", which lastRx alone can no longer answer once a
  // heartbeat exists to keep it fresh forever on its own.
  uint32_t lastActivity = 0;
  bool sawActivity = false;

  // set by whoever owns the radio; ctx lets a test route two Links to each other
  void (*send)(void *ctx, const uint8_t *buf, uint8_t len) = nullptr;
  void *ctx = nullptr;

  void begin(bool host, const char *name);
  void addMon(const LinkMon &m);
  void start();                       // announce: hello, then squad
  void onPacket(const uint8_t *buf, uint8_t len);
  void tick(uint32_t now);            // resend + timeout; call once a frame

  void sendAct(uint8_t act);          // guest -> host: a move slot or a switch
  void sendResult(const uint8_t *blob, uint8_t len);   // host -> guest
  void sendEnd(bool hostWon);
  void sendBye();                     // leaving deliberately
  void sendRematch();
  void rearm();                       // same squads, fresh fight

  bool ready() const { return state == LINK_READY; }
  bool live() const {
    return state != LINK_OFF && state != LINK_REFUSED && state != LINK_LOST;
  }
  // the host's own action is its own business; this is the guest's, and it
  // only counts once it has actually arrived
  bool hasPeerAct() const { return isHost && pendingAct != 0; }

  // ---- friends mode (vunguyen2712's feature 7) ----
  uint8_t mode = LINK_MODE_BATTLE;   // set after begin(), before start()
  uint8_t refusedWhy = LINK_WHY_NONE;
  // the peer's latest state frame. Only the newest matters, so a single slot
  // is right -- a skipped frame is superseded by the one after it.
  uint8_t fx[LINK_FRIEND_MAX];
  uint8_t fxN = 0;
  bool fxNew = false;
  // the peer's trainer card, parked like fx: untrusted bytes until card.cpp
  // has checked them on the main loop.
  uint8_t cx[LINK_CARD_MAX];
  uint8_t cxN = 0;
  bool cxNew = false;
  // READY, but the peer is still saying hello: it is missing our squad. Our
  // answer to its hello always sits at the same position after that hello, so
  // a loss pattern that hits that position would eat it every time. This many
  // squad resends ride OUR timer instead; any non-hello traffic from the peer
  // proves it is READY and stops them. (vunguyen2712's lossy test found it.)
  uint8_t squadOwe = 0;
  uint8_t burst = 0;           // friends mode: alternates the resend burst length
  // friends mode: our whole state, KEPT and resent until the next one
  // supersedes it. Only once READY; ignored in a battle.
  void sendFriend(const uint8_t *frame, uint8_t len);
  // Our trainer card, once. NOT kept for resend (that slot is LM_FRIEND's):
  // the caller repeats it on its own timer.
  void sendCard(const uint8_t *card, uint8_t len);

  // ---- trading, one creature for one, on a READY link ----
  //
  // NEVER LOSE A CREATURE. A radio cannot promise both sides learn the outcome
  // (the two generals), so the order is chosen for the failure that does not
  // hurt: each side STORES the other's creature first, and gives up its own
  // only once the peer says it has stored ours. A link or a power cut between
  // the two leaves a creature on both devices -- a duplicate -- never on neither.
  //
  // THE FIX over the original (Tubiana's link sent DONE from tradeCommit(),
  // before the app had stored anything, so a failed store on one side still
  // told the other to release): the link only raises tArrived. The APP stores
  // tTheirs, verifies the write, and only then calls tradeStored(), which is
  // the one thing that sends LM_TDONE. A store that fails never says DONE, so
  // the peer never lets go of its creature.
  //
  // Offers carry a sequence number that only grows during a session, and every
  // accept names BOTH offers it is for. Changing an offer voids any accept of
  // the old one, and a resend of an old accept or done can never match a newer
  // deal.
  PartyMon tMine, tTheirs;
  uint8_t tMySeq = 0;          // my latest offer's number (never reused)
  bool tOffering = false;      // tMine is on the table
  uint8_t tTheirSeq = 0;       // theirs, 0 = nothing offered
  uint8_t tTheirDone = 0;      // their offer in the last deal: its resends are stale
  bool tIAccepted = false, tPeerAccepted = false, tPeerDone = false;
  uint8_t tPhase = 0;          // 0 dealing, 1 agreed (store theirs), 2 mine is gone too
  bool tArrived = false;       // agreed: the app must store tTheirs, then call tradeStored()
  bool tStored = false;        // the app stored tTheirs durably; LM_TDONE now says so

  void tradeOffer(const PartyMon &m);
  void tradeWithdraw();        // nothing on the table (also voids my accept)
  void tradeAccept();          // OK on the current pair of offers
  void tradeStored();          // the app's verified write of tTheirs landed
  void tradeReleased();        // the app removed tMine: this deal is finished here
  void tradeReset();           // a new deal; keeps the sequence counter
  bool tradeHasTheirs() const { return tTheirSeq != 0; }
  // Mine may go only when BOTH are true: theirs is safely here, and they told
  // us ours is safely there.
  bool tradeCanRelease() const { return tPhase == 1 && tStored && tPeerDone; }
};

// A creature that arrived over the radio: every field clamped to something
// that can index a table or be drawn, whatever the bytes were. False = refuse.
bool tradeSanitize(PartyMon &m);
// The species that evolve when traded in the games (no item needed): what it
// becomes on arrival, or 0. Never into something this card cannot draw.
int16_t tradeEvolution(int16_t dex);

// Everything that must match for two builds to narrate the same fight. NOT the
// firmware version: a build differing only in, say, a colour is perfectly
// compatible and refusing it would be noise. These are the things that would
// actually make one device read a packet differently from the other.
uint16_t linkBuildTag();

// Builds the wire form of a combatant, so both ends agree on what a creature is.
// linkMonTo VALIDATES: everything it produces is safe to index a table with,
// whatever actually arrived over the air.
void linkMonFrom(LinkMon &out, const Combatant &c);
void linkMonTo(Combatant &out, const LinkMon &m);
uint16_t linkSafeMove(uint16_t m);      // 0 unless it is a real move
