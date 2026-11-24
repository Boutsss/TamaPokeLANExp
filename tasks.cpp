#include "tasks.h"
#include "pet.h"
#include "inventory.h"
#include <string.h>

const TaskDef DAILY_TASKS[] = {
  { TK_FEED, 5, IT_POKEBALL, 2 }, { TK_FAV_BERRY, 2, IT_POTION, 1 },
  { TK_CLEAN, 2, IT_POTION, 2 }, { TK_PLAY, 2, IT_POKEBALL, 2 },
  { TK_TRAIN, 2, IT_PROTEIN, 1 }, { TK_CARE_DAY, 1, IT_POTION, 2 },
  { TK_HATCH, 1, IT_GREATBALL, 1 }, { TK_EVOLVE, 1, IT_PROTEIN, 1 },
  { TK_GYM_WIN, 1, IT_GREATBALL, 2 }, { TK_REMATCH_WIN, 1, IT_HYPERPOTION, 1 },
  { TK_WILD_SEEN, 3, IT_POKEBALL, 3 }, { TK_WILD_WIN, 2, IT_POTION, 2 },
  { TK_WILD_CATCH, 1, IT_GREATBALL, 2 },
};
const uint8_t DAILY_TASK_COUNT = sizeof(DAILY_TASKS) / sizeof(DAILY_TASKS[0]);
const TaskDef WEEKLY_TASKS[] = {
  { TK_CARE_DAY, 5, IT_GREATBALL, 2 }, { TK_FEED, 25, IT_POTION, 4 },
  { TK_PLAY, 8, IT_POKEBALL, 5 }, { TK_TRAIN, 8, IT_PROTEIN, 2 },
  { TK_WILD_WIN, 8, IT_HYPERPOTION, 2 }, { TK_WILD_CATCH, 3, IT_GREATBALL, 4 },
  { TK_GYM_WIN, 2, IT_GREATBALL, 3 }, { TK_REMATCH_WIN, 3, IT_HYPERPOTION, 2 },
  { TK_EVOLVE, 1, IT_PROTEIN, 3 }, { TK_HATCH, 1, IT_GREATBALL, 3 },
};
const uint8_t WEEKLY_TASK_COUNT = sizeof(WEEKLY_TASKS) / sizeof(WEEKLY_TASKS[0]);

static Pet *gPet = nullptr;
static Preferences prefs;
static bool opened = false, have = false;
static uint32_t curDay = 0, curWeek = 0;
static uint8_t rowIdx[TASK_SLOTS] = {0}, prog[TASK_SLOTS] = {0};
static uint8_t claimed = 0;
static uint32_t noticeAt = 0;
static bool noticeSet = false;
static uint32_t lastTickAt = 0;

#define TASK_SAVE_VER 3
#define TASK_SAVE_V2_LEN (2 + TASK_SLOTS * 2 + 8)
#define TASK_SAVE_LEN (TASK_SAVE_V2_LEN + 5)
#define BONUS_SLOT 0xFF
#define BONUS_BIT (1u << TASK_SLOTS)

static bool pending = false;
static uint8_t pendingSlot = BONUS_SLOT, pendingItem = ITEM_NONE, pendingQty = 0, pendingBefore = 0;

static const TaskDef &rowOf(uint8_t slot) {
  if (slot < TASK_DAILY) return DAILY_TASKS[rowIdx[slot] % DAILY_TASK_COUNT];
  return WEEKLY_TASKS[rowIdx[slot] % WEEKLY_TASK_COUNT];
}
static void put32(uint8_t *p, uint32_t v) { for(uint8_t i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t v=0; for(uint8_t i=0;i<4;i++) v|=(uint32_t)p[i]<<(8*i); return v; }

static bool save() {
  if (!opened || !have) return false;
  uint8_t b[TASK_SAVE_LEN] = {0};
  b[0]=TASK_SAVE_VER; b[1]=claimed;
  for(uint8_t i=0;i<TASK_SLOTS;i++){b[2+i]=rowIdx[i];b[2+TASK_SLOTS+i]=prog[i];}
  put32(b+2+TASK_SLOTS*2,curDay);
  put32(b+6+TASK_SLOTS*2,curWeek);
  b[TASK_SAVE_V2_LEN]=pending?1:0;
  b[TASK_SAVE_V2_LEN+1]=pendingSlot;
  b[TASK_SAVE_V2_LEN+2]=pendingItem;
  b[TASK_SAVE_V2_LEN+3]=pendingQty;
  b[TASK_SAVE_V2_LEN+4]=pendingBefore;
  if(prefs.putBytes("tasks",b,sizeof(b))!=sizeof(b)){Serial.println("tasks: NVS write failed");return false;}
  return true;
}
static void load() {
  have=false; claimed=0; pending=false; pendingSlot=BONUS_SLOT; pendingItem=ITEM_NONE; pendingQty=pendingBefore=0;
  memset(rowIdx,0,sizeof(rowIdx)); memset(prog,0,sizeof(prog));
  size_t len=prefs.getBytesLength("tasks");
  if(len!=TASK_SAVE_V2_LEN&&len!=TASK_SAVE_LEN)return;
  uint8_t b[TASK_SAVE_LEN]={0};
  if(prefs.getBytes("tasks",b,len)!=len)return;
  if((len==TASK_SAVE_V2_LEN&&b[0]!=2)||(len==TASK_SAVE_LEN&&b[0]!=TASK_SAVE_VER))return;
  for(uint8_t i=0;i<TASK_SLOTS;i++){uint8_t n=i<TASK_DAILY?DAILY_TASK_COUNT:WEEKLY_TASK_COUNT;if(b[2+i]>=n)return;}
  claimed=b[1]&((1u<<(TASK_SLOTS+1))-1);
  for(uint8_t i=0;i<TASK_SLOTS;i++)rowIdx[i]=b[2+i];
  for(uint8_t i=0;i<TASK_SLOTS;i++){uint8_t target=rowOf(i).target;prog[i]=b[2+TASK_SLOTS+i]>target?target:b[2+TASK_SLOTS+i];}
  curDay=get32(b+2+TASK_SLOTS*2); curWeek=get32(b+6+TASK_SLOTS*2);
  if(len==TASK_SAVE_LEN&&b[TASK_SAVE_V2_LEN]){
    pending=true; pendingSlot=b[TASK_SAVE_V2_LEN+1]; pendingItem=b[TASK_SAVE_V2_LEN+2];
    pendingQty=b[TASK_SAVE_V2_LEN+3]; pendingBefore=b[TASK_SAVE_V2_LEN+4];
    if((pendingSlot!=BONUS_SLOT&&pendingSlot>=TASK_SLOTS)||!itemValid(pendingItem)||!pendingQty||pendingQty>BAG_STACK_MAX||pendingBefore>BAG_STACK_MAX-pendingQty)return;
  }
  have=true;
}
uint32_t tasksWeekOf(uint32_t day){return(day+3)/7;}
static uint32_t mix(uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;return x;}
static void pick(uint32_t seed,bool weekly,uint8_t *out){
  const uint8_t n=weekly?WEEKLY_TASK_COUNT:DAILY_TASK_COUNT,want=weekly?TASK_WEEKLY:TASK_DAILY;
  uint8_t cand[32];for(uint8_t i=0;i<n;i++)cand[i]=i;
  uint32_t s=mix(seed*2+(weekly?1:0)+0x7A5C);
  for(uint8_t k=0;k<want;k++){uint8_t j=k+(uint8_t)(s%(n-k));uint8_t t=cand[k];cand[k]=cand[j];cand[j]=t;out[k]=cand[k];s=mix(s+0x9E3779B9U);}
}
static void rollTo(uint32_t day){
  uint32_t week=tasksWeekOf(day);bool newDay=!have||day!=curDay,newWeek=!have||week!=curWeek;
  if(!newDay&&!newWeek)return;
  // Only ever roll FORWARD: a clock set back a day must not re-issue rewards.
  if(have&&day<curDay)return;
  if(pending)return;
  if(newDay){pick(day,false,rowIdx);for(uint8_t i=0;i<TASK_DAILY;i++){prog[i]=0;claimed&=~(1u<<i);}claimed&=~BONUS_BIT;curDay=day;}
  if(newWeek){pick(week,true,rowIdx+TASK_DAILY);for(uint8_t i=TASK_DAILY;i<TASK_SLOTS;i++){prog[i]=0;claimed&=~(1u<<i);}curWeek=week;}
  have=true;save();
}
static bool reconcilePending(){
  if(!pending)return true;
  uint8_t count=bag.count(pendingItem);
  uint8_t expected=(uint8_t)(pendingBefore+pendingQty);
  if(count==pendingBefore){
    if(!bag.addChecked(pendingItem,pendingQty))return false;
    count=bag.count(pendingItem);
  }
  if(count!=expected)return false;
  uint8_t oldClaimed=claimed,slot=pendingSlot,item=pendingItem,qty=pendingQty,before=pendingBefore;
  if(slot==BONUS_SLOT)claimed|=BONUS_BIT;else claimed|=(uint8_t)(1u<<slot);
  pending=false;pendingSlot=BONUS_SLOT;pendingItem=ITEM_NONE;pendingQty=pendingBefore=0;
  if(save())return true;
  claimed=oldClaimed;pending=true;pendingSlot=slot;pendingItem=item;pendingQty=qty;pendingBefore=before;
  return false;
}
static bool beginClaim(uint8_t slot,ItemKey item,uint8_t qty){
  if(pending&&!reconcilePending())return false;
  uint8_t before=bag.count(item);
  if(!itemValid(item)||!qty||before>BAG_STACK_MAX-qty)return false;
  pending=true;pendingSlot=slot;pendingItem=item;pendingQty=qty;pendingBefore=before;
  if(!save()){pending=false;pendingSlot=BONUS_SLOT;pendingItem=ITEM_NONE;pendingQty=pendingBefore=0;return false;}
  return reconcilePending();
}
static bool matches(uint8_t kind,GameEv ev,int16_t arg){
  switch(kind){
    case TK_FEED:return ev==EV_FEED;case TK_FAV_BERRY:return ev==EV_FEED&&arg==1;
    case TK_CLEAN:return ev==EV_CLEAN;case TK_PLAY:return ev==EV_PLAY&&arg>0;
    case TK_TRAIN:return ev==EV_TRAIN;case TK_CARE_DAY:return ev==EV_CARE_DAY;
    case TK_HATCH:return ev==EV_HATCH;case TK_EVOLVE:return ev==EV_EVOLVE;
    case TK_GYM_WIN:return ev==EV_GYM_WIN;case TK_REMATCH_WIN:return ev==EV_REMATCH_WIN;
    case TK_WILD_SEEN:return ev==EV_WILD_SEEN;case TK_WILD_WIN:return ev==EV_WILD_WIN;
    case TK_WILD_CATCH:return ev==EV_WILD_CATCH;default:return false;
  }
}
void tasksBegin(Pet &p){
  gPet=&p;opened=prefs.begin("tamapoke",false);
  if(!opened){Serial.println("tasks: NVS would not open");return;}
  load();lastTickAt=millis();noticeSet=false;
  gameEventSubscribe(tasksOnEvent);
  if(pending)reconcilePending();
}
void tasksRefresh(){if(opened&&gPet)rollTo(gPet->lastSeenEpoch/86400UL);}
void tasksForget(){have=false;opened=false;claimed=0;noticeSet=false;pending=false;pendingSlot=BONUS_SLOT;pendingItem=ITEM_NONE;pendingQty=pendingBefore=0;memset(prog,0,sizeof(prog));memset(rowIdx,0,sizeof(rowIdx));gameEventUnsubscribe(tasksOnEvent);}
void tasksOnEvent(GameEv ev,int16_t arg){
  if(!opened)return;tasksRefresh();bool changed=false;
  for(uint8_t i=0;i<TASK_SLOTS;i++){const TaskDef &d=rowOf(i);if(prog[i]>=d.target||!matches(d.kind,ev,arg))continue;prog[i]++;changed=true;if(prog[i]>=d.target){noticeAt=millis();noticeSet=true;}}
  if(changed)save();
}
void tasksTick(){if(!opened||!gPet)return;uint32_t now=millis();if(now-lastTickAt<60000UL)return;lastTickAt=now;tasksRefresh();}
const TaskDef &tasksDef(uint8_t s){static const TaskDef none={TK_NONE,1,IT_POTION,0};return s<TASK_SLOTS?rowOf(s):none;}
uint8_t tasksProgress(uint8_t s){return s<TASK_SLOTS?prog[s]:0;}
bool tasksDone(uint8_t s){return s<TASK_SLOTS&&prog[s]>=rowOf(s).target;}
bool tasksClaimed(uint8_t s){return s<TASK_SLOTS&&(claimed&(1u<<s));}
bool tasksClaim(uint8_t s){
  if(!opened)return false;tasksRefresh();if(pending&&!reconcilePending())return false;tasksRefresh();
  if(s>=TASK_SLOTS||!tasksDone(s)||tasksClaimed(s))return false;
  const TaskDef &d=rowOf(s);return beginClaim(s,d.item,d.qty);
}
bool tasksBonusClaimed(){return(claimed&BONUS_BIT)!=0;}
bool tasksBonusReady(){if(!have||tasksBonusClaimed())return false;for(uint8_t i=0;i<TASK_DAILY;i++)if(!(claimed&(1u<<i)))return false;return true;}
ItemKey tasksBonusItem(){static const ItemKey items[]={IT_GREATBALL,IT_HYPERPOTION,IT_PROTEIN,IT_POKEBALL};return items[curDay%4];}
uint8_t tasksBonusQty(){return tasksBonusItem()==IT_GREATBALL?2:1;}
bool tasksClaimBonus(){if(!opened)return false;tasksRefresh();if(pending&&!reconcilePending())return false;tasksRefresh();if(!tasksBonusReady())return false;return beginClaim(BONUS_SLOT,tasksBonusItem(),tasksBonusQty());}
uint8_t tasksClaimable(){if(!have)return 0;uint8_t n=0;for(uint8_t i=0;i<TASK_SLOTS;i++)if(tasksDone(i)&&!tasksClaimed(i))n++;if(tasksBonusReady())n++;return n;}
uint32_t tasksDay(){return curDay;}
uint32_t tasksWeek(){return curWeek;}
bool tasksNoticeActive(){if(!noticeSet)return false;if(millis()-noticeAt<TASK_NOTICE_MS)return true;noticeSet=false;return false;}
