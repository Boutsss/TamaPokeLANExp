// Cosmetic skins (skins.h) and the Game Corner's prize creature, the armored
// MEWTWO: the flag's storage in a PartyMon's old padding (a magic value, so stale
// padding never reads as a skin), the live pet's checkpoint byte and legacy key,
// every Pet <-> PartyMon transfer, the battle's Combatant, the one sprite-name
// rule, and the purchase -- coins first, into the PC BOX, refunded if the box
// write does not land, never sold to a full box.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "link.h"
#include "casino.h"
#include "box_store.h"
#include "persona.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=97; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static uint32_t wallet = 0; static int spends = 0, refunds = 0; static bool spendOk = true;
static bool spend(uint32_t n){ spends++; if (!spendOk || wallet < n) return false; wallet -= n; return true; }
static void refund(uint32_t n){ refunds++; wallet += n; }

static int boxArmored() {
  int n = 0;
  for (int i = 0; i < BOX_SLOTS; i++) if (party.box[i].dex == 150 && party.box[i].skin() == SKIN_ARMORED) n++;
  return n;
}

int main(){
  Pet pet; pet.begin(); party.begin();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // ---- the flag in a PartyMon
  {
    static_assert(sizeof(PartyMon) == 56, "the record did not grow");
    PartyMon m; m.dex = 150;
    m.setSkin(SKIN_ARMORED);
    ck(m.skinTag == (PM_SKIN_TAG | SKIN_ARMORED) && m.skin() == SKIN_ARMORED, "a MEWTWO record wears the armored skin");
    bool staleSafe = true;
    for (int v = 0; v < 256; v++) {
      m.skinTag = (uint8_t)v;
      bool expect = v == (PM_SKIN_TAG | SKIN_ARMORED);
      if ((m.skin() == SKIN_ARMORED) != expect) staleSafe = false;
    }
    ck(staleSafe, "of all 256 values old padding could hold, exactly one means armored");
    PartyMon p; p.dex = 25; p.setSkin(SKIN_ARMORED);
    ck(p.skinTag == 0 && p.skin() == SKIN_NONE, "no other species can be given the skin");
    p.skinTag = PM_SKIN_TAG | SKIN_ARMORED;
    ck(p.skin() == SKIN_NONE, "and a forged tag on another species draws nothing");
  }

  // ---- the one sprite-name rule
  {
    char a[20], b[20], c[20], d[20];
    sdSpriteName(a, sizeof(a), 150, false, SKIN_ARMORED);
    sdSpriteName(b, sizeof(b), 150, true, SKIN_ARMORED);
    sdSpriteName(c, sizeof(c), 25, false, SKIN_ARMORED);
    sdSpriteName(d, sizeof(d), 150, true, SKIN_NONE);
    ck(!strcmp(a, "/pa150.bin") && !strcmp(b, "/pa150.bin"), "the armored MEWTWO's file is pa150.bin, shiny or not");
    ck(!strcmp(c, "/p025.bin") && !strcmp(d, "/ps150.bin"), "anything else keeps its ordinary name");
  }

  // ---- the prize creature
  const PartyMon prize = casinoPrizeMon(0);
  {
    bool movesOk = prize.move(0) != 0;
    for (int k = 0; k < MOVE_SLOTS; k++) if (prize.move(k) >= MOVE_COUNT) movesOk = false;
    ck(prize.dex == 150 && prize.level == 1 && prize.skin() == SKIN_ARMORED, "the prize is a level 1 armored MEWTWO");
    ck(prize.ivAtk == 31 && prize.ivDef == 31 && prize.ivSpe == 31 && prize.ivHp == 31, "with every IV perfect");
    ck(movesOk && personaValid(prize.persona) && prize.stateVersion == 1 && prize.ageMinutes == 0,
       "and moves from its learnset, a personality and real care state, like a caught creature");
    ck(casinoPrizePrice(PK_MON, 0) == 1000000 && casinoPrizePrice(PK_AVATAR, 0) == CASINO_PRICE,
       "it costs 1,000,000 coins; the cosmetics keep their price");
    ck(casinoShopCount(PK_MON) == 1 && casinoShopAt(PK_MON, 0) == 0 && !strcmp(casinoPrizeName(PK_MON, 0), "MEWTWO"),
       "the counter lists it");
  }

  // ---- buying it
  {
    for (int i = 0; i < BOX_SLOTS; i++) party.box[i] = PartyMon();
    party.boxSave();
    uint32_t c = 0;
    ck(!casinoBuy(PK_MON, 0, c = 5000000, spend), "the cosmetic buy path refuses a creature");

    wallet = 999999; spends = 0;
    ck(casinoBuyMon(0, wallet, spend, refund, nullptr) == CMB_COINS && spends == 0 && boxArmored() == 0,
       "999,999 coins buy nothing, and nothing is spent");

    wallet = 1500000; spends = 0; refunds = 0;
    int16_t slot = -1;
    ck(casinoBuyMon(0, wallet, spend, refund, &slot) == CMB_OK && wallet == 500000 && spends == 1 && refunds == 0,
       "1,500,000 coins buy it: exactly 1,000,000 spent, once");
    ck(slot >= 0 && party.box[slot].dex == 150 && party.box[slot].skin() == SKIN_ARMORED && boxArmored() == 1,
       "and it is in the PC box, armored");
    party.begin();
    ck(boxArmored() == 1 && party.box[slot].ivAtk == 31, "the box file holds it after a reload");

    wallet = 1000000; spendOk = false; spends = 0;
    ck(casinoBuyMon(0, wallet, spend, refund, nullptr) == CMB_SPEND && wallet == 1000000 && boxArmored() == 1,
       "a spend that does not land delivers nothing");
    spendOk = true;

    // the box write fails: the price comes back
    boxStoreSetDir("/nonexistent/tamapoke-skin-test");
    wallet = 1000000; spends = refunds = 0;
    CasMonResult r = casinoBuyMon(0, wallet, spend, refund, nullptr);
    ck(r == CMB_STORE && spends == 1 && refunds == 1 && wallet == 1000000,
       "a box write that does not land refunds the 1,000,000");
    boxStoreSetDir(".");
    party.begin();
    ck(boxArmored() == 1, "and leaves no half-delivered creature behind");

    // a full box: refused before a coin moves
    for (int i = 0; i < BOX_SLOTS; i++) if (party.box[i].empty()) { party.box[i].dex = 19; party.box[i].level = 3; }
    party.boxSave();
    wallet = 2000000; spends = 0;
    ck(casinoBuyMon(0, wallet, spend, refund, nullptr) == CMB_BOX_FULL && spends == 0 && wallet == 2000000,
       "a full PC box is refused before anything is spent");
    for (int i = 0; i < BOX_SLOTS; i++) if (party.box[i].dex == 19) party.box[i] = PartyMon();
    party.boxSave();
  }

  // ---- the live pet carries it, and so does every transfer
  {
    ck(pet.switchTo(prize), "the armored MEWTWO becomes the live pet");
    ck(pet.skin == SKIN_ARMORED && pet.skinShown() == SKIN_ARMORED, "wearing its skin");
    PartyMon back = pet.toPartyMon();
    ck(back.skin() == SKIN_ARMORED, "and banking it keeps the skin");
    Combatant c; combatantFromPet(c, pet);
    Combatant d; combatantFromParty(d, back);
    PartyMon plain; plain.dex = 150; plain.level = 5;
    Combatant e; combatantFromParty(e, plain);
    ck(c.skin == SKIN_ARMORED && d.skin == SKIN_ARMORED && e.skin == SKIN_NONE,
       "a battle draws it armored, and an ordinary MEWTWO plain");
    // over LAN: the record a rival receives carries it (in old padding, so the
    // wire record is the same size an older board expects)
    LinkMon w; linkMonFrom(w, d);
    Combatant f; linkMonTo(f, w);
    LinkMon wp; linkMonFrom(wp, e);
    Combatant g; linkMonTo(g, wp);
    ck(f.skin == SKIN_ARMORED && g.skin == SKIN_NONE, "a LAN rival sees it armored, and an ordinary one plain");
    LinkMon forged = w; forged.dex = 25;
    Combatant h; linkMonTo(h, forged);
    LinkMon junk = wp; junk.skinTag = 0x01;   // an older build's stale padding, say
    Combatant k; linkMonTo(k, junk);
    ck(h.skin == SKIN_NONE && k.skin == SKIN_NONE, "a tag on another species, or stray padding, draws nothing");

    ck(pet.saveNow(), "saved");
    nvs()["skin"] = { 0 };                 // the legacy key says otherwise
    { Pet q; q.begin(); ck(q.skin == SKIN_ARMORED, "the checkpoint carries the skin (and wins over the legacy key)"); }
    // A save from BEFORE the checkpoints: only legacy keys. Since v3.50 the
    // firmware no longer writes them, so the fixture sets the ones it needs.
    nvs()["skin"] = { SKIN_ARMORED };
    nvs()["dexn"] = { 150, 0 };
    nvs().erase("petA"); nvs().erase("petB");
    { Pet q; q.begin(); ck(q.speciesId == 150 && q.skin == SKIN_ARMORED, "with no checkpoint, the legacy key carries it"); }
    nvs()["skin"] = { 0x7F };
    nvs().erase("petA"); nvs().erase("petB");
    { Pet q; q.begin(); ck(q.skin == SKIN_NONE, "an unknown skin value off the flash is dropped"); }
    nvs()["skin"] = { SKIN_ARMORED };

    pet.newEgg();
    ck(pet.skin == SKIN_NONE && pet.skinShown() == SKIN_NONE, "an egg never inherits it");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
