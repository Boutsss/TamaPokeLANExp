// PRACTICE against remembered LAN rivals, through the real sketch: a LAN
// battle starting is what remembers the rival's team (startLinkBattle(), not
// the test calling ghostRemember), the LAN screen's PRACTICE row lists them,
// a tap fights their EXACT creatures against the AI, the rest of their team
// comes in as each one faints, the record is kept, and the fight returns to
// the list. Also: a rival is one entry however often you meet them, the store
// is capped and most-recent-first, and it all survives a reload.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "link.h"
#include "ghost.h"
#include "battle.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=91; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int); void battleTap(int16_t,int16_t);
extern Pet pet; extern Link lan;
extern bool battleOpen, btlLink, btlGhost, btlOver, btlWon, lanOpen, lanPractice;
extern Combatant btlFoe, btlFoeSquad[];
extern uint8_t btlFoeAt, btlFoeSquadN, btlMsgCount;
extern int8_t btlSwapWho;
extern uint16_t squadMask;
extern uint32_t btlWinUntil;
void startLinkBattle();
void lanLeave();
int lanRowCenterY(uint8_t i);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
void emuSetFakePeerMac(const uint8_t mac[6]);
int btlCellCenterX(int i); int btlCellCenterY(int i);
extern uint8_t btlMenu;
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static void capture(void*, const uint8_t*, uint8_t){}

static LinkMon mon(int16_t dex, uint8_t lvl, const char *nm){
  Pet p; p.dbgHatchAs(dex,false);
  p.ivAtk=p.ivDef=p.ivSpe=p.ivHp=25;
  p.ageMinutes=(uint32_t)(lvl-1)*MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  Combatant c; combatantFromPet(c,p);
  snprintf(c.name,sizeof(c.name),"%s",nm);
  LinkMon m; linkMonFrom(m,c); return m;
}
// a LAN battle against `name`, started through the real path, then left
static void meet(const uint8_t mac[6], const char *name, int16_t lead, uint8_t lvl) {
  emuSetFakePeerMac(mac);
  lan.begin(true,"ME");
  snprintf(lan.peerName,sizeof(lan.peerName),"%s",name);
  lan.addMon(mon(9,50,"SHELL"));
  lan.theirs[0]=mon(lead,lvl,"LEAD");
  lan.theirs[1]=mon(65,lvl,"SPOON");
  lan.theirsN=2;
  lan.state=LINK_READY;
  lan.send=capture;
  startLinkBattle();
  battleOpen=false; btlLink=false; lanLeave(); lanOpen=false;
}

int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(149, false);                     // a DRAGONITE...
  pet.ageMinutes = 99UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  squadMask = 1;                                  // ...fighting alone

  // ---- a LAN battle remembers the rival
  const uint8_t macA[6]={1,2,3,4,5,6}, macB[6]={9,9,9,9,9,9};
  ck(ghostCount()==0, "no rivals before any LAN battle");
  meet(macA, "BEA", 6, 40);
  ck(ghostCount()==1 && !strcmp(ghostAt(0).name,"BEA") && ghostAt(0).n==2, "starting a LAN battle remembers the rival's team");
  ck(ghostAt(0).mon[0].dex==6 && ghostAt(0).mon[0].level==40, "exactly as it arrived over the link");
  meet(macA, "BEA2", 3, 45);                       // same device, renamed, new team
  ck(ghostCount()==1 && !strcmp(ghostAt(0).name,"BEA2") && ghostAt(0).mon[0].dex==3,
     "the same device is one entry: renamed and re-teamed, not duplicated");
  meet(macB, "CAL", 25, 30);
  ck(ghostCount()==2 && !strcmp(ghostAt(0).name,"CAL"), "a new rival goes to the front");
  for (uint8_t k=0;k<4;k++){ uint8_t m[6]={7,7,7,7,7,(uint8_t)k}; char n[8]; snprintf(n,sizeof n,"R%u",k); meet(m,n,16,10+k); }
  ck(ghostCount()==GHOST_CAP && !strcmp(ghostAt(0).name,"R3"), "the store is capped, most recent first");

  // ---- PRACTICE on the LAN screen
  lanOpen=true; lanPractice=false; lan.state=LINK_OFF; render();
  onTap(233, lanRowCenterY(3)); render();
  ck(lanOpen && lanPractice && onScreen("lan"), "PRACTICE opens the list of rivals");
  onTap(233, lanRowCenterY(0)); render();          // R3: a PIDGEY (L13) and an ALAKAZAM (L13)
  ck(battleOpen && btlGhost && !btlLink, "tapping a rival starts a practice fight, no radio");
  ck(btlFoe.dex==16 && btlFoe.level==13 && btlFoeSquadN==2, "against their exact team");
  // fight it out: the dragon wins, and the second creature comes in on the way
  bool sawSecond=false;
  for (int t=0;t<200 && !btlOver;t++){
    if (btlMsgCount || btlSwapWho>=0) battleTap(233,233);
    else battleTap(btlCellCenterX(0), btlCellCenterY(0));   // FIGHT, then the first move
    if (btlFoe.dex==65) sawSecond=true;
  }
  ck(sawSecond, "their second creature comes in when the first faints");
  ck(btlOver && btlWon, "and the practice fight can be won");
  while (battleOpen) battleTap(233,233);
  ck(lanOpen && lanPractice, "afterwards it returns to the rival list");
  ck(ghostAt(0).wins==1 && ghostAt(0).losses==0, "with the win on that rival's practice record");
  ck(!strcmp(ghostAt(0).name,"R3"), "and practising does not reorder the list");

  // ---- down: list -> LAN screen -> out
  onSwipeV(1); render();
  ck(lanOpen && !lanPractice, "down: from the list back to the LAN screen");
  onSwipeV(1); render();
  ck(!lanOpen, "down again: out");

  // ---- reload
  ghostsBegin();
  ck(ghostCount()==GHOST_CAP && ghostAt(0).wins==1 && ghostAt(0).mon[1].dex==65, "rivals and records survive a reload");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
