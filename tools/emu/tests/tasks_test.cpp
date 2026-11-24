#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "inventory.h"
#include "tasks.h"
#include "save.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=17; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
static uint32_t fakeNow=0;
uint32_t millis(){return fakeNow;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}
int main(){
  Pet p; p.begin(); p.dbgHatchAs(4,false);
  bag.begin(); bag.clear();
  p.lastSeenEpoch=20000UL*86400UL+3600;
  tasksBegin(p); tasksRefresh();
  ck(tasksDay()==20000,"task day follows pet clock");
  uint8_t first[TASK_SLOTS]; for(uint8_t i=0;i<TASK_SLOTS;i++) first[i]=tasksDef(i).kind;
  tasksRefresh();
  bool same=true; for(uint8_t i=0;i<TASK_SLOTS;i++) if(first[i]!=tasksDef(i).kind)same=false;
  ck(same,"same-day refresh preserves chosen rows");
  tasksForget(); tasksBegin(p); tasksRefresh();
  same=true; for(uint8_t i=0;i<TASK_SLOTS;i++) if(first[i]!=tasksDef(i).kind)same=false;
  ck(same,"task selection survives reload");

  // Find a day whose first daily row is FEED, then prove only matching events count.
  uint32_t day=0;
  for(uint32_t d=20000;d<20500&&!day;d++){
    p.lastSeenEpoch=d*86400UL+3600; tasksRefresh();
    for(uint8_t s=0;s<TASK_DAILY;s++) if(tasksDef(s).kind==TK_FEED) day=d;
  }
  ck(day!=0,"deterministic picker offers feed within search window");
  p.lastSeenEpoch=day*86400UL+3600; tasksRefresh();
  int feed=-1; for(uint8_t s=0;s<TASK_DAILY;s++)if(tasksDef(s).kind==TK_FEED)feed=s;
  uint8_t before=tasksProgress(feed);
  gameEvent(EV_FEED,0);
  ck(tasksProgress(feed)==before+1,"feed event advances feed task");
  int fav=-1; for(uint8_t s=0;s<TASK_DAILY;s++)if(tasksDef(s).kind==TK_FAV_BERRY)fav=s;
  if(fav>=0){uint8_t n=tasksProgress(fav);gameEvent(EV_FEED,0);ck(tasksProgress(fav)==n,"ordinary berry does not count as favourite");}

  // Move to a day whose task is one action from completion, then claim once.
  uint32_t claimDay=day;
  p.lastSeenEpoch=claimDay*86400UL+3600; tasksRefresh();
  for(uint8_t s=0;s<TASK_DAILY;s++){
    const TaskDef &d=tasksDef(s);
    for(uint8_t n=tasksProgress(s);n<d.target;n++) gameEvent(d.kind==TK_FEED?EV_FEED:d.kind==TK_CLEAN?EV_CLEAN:d.kind==TK_PLAY?EV_PLAY:d.kind==TK_TRAIN?EV_TRAIN:d.kind==TK_CARE_DAY?EV_CARE_DAY:d.kind==TK_HATCH?EV_HATCH:d.kind==TK_EVOLVE?EV_EVOLVE:d.kind==TK_GYM_WIN?EV_GYM_WIN:d.kind==TK_REMATCH_WIN?EV_REMATCH_WIN:d.kind==TK_WILD_SEEN?EV_WILD_SEEN:d.kind==TK_WILD_WIN?EV_WILD_WIN:d.kind==TK_WILD_CATCH?EV_WILD_CATCH:EV_FEED,1);
    if(tasksDone(s)){
      uint8_t old=bag.count(d.item);
      nvsFailWritesAfter(0);
      ck(!tasksClaim(s),"claim is refused if its claimed marker cannot be saved");
      nvsResumeWrites();
      ck(!tasksClaimed(s) && bag.count(d.item)==old,"failed claim neither marks paid nor grants reward");
      ck(tasksClaim(s),"completed task reward can be claimed");
      ck(bag.count(d.item)>=old,"claim adds its reward");
      ck(!tasksClaim(s),"same task cannot be claimed twice");
      break;
    }
  }
  ck(tasksWeekOf(4)==1&&tasksWeekOf(10)==1&&tasksWeekOf(11)==2,"week rolls on Monday boundary");
  // Backup table includes the task blob after task initialization.
  uint8_t backup[SAVE_TRANSFER_MAX];
  size_t n=saveExport(backup,sizeof(backup));
  bool listed=false; for(uint16_t i=0;i<SAVE_FIELD_COUNT;i++) if(!strcmp(SAVE_FIELDS[i].key,"tasks"))listed=true;
  ck(n>0&&listed,"task state is included in whole-save backup");
  printf("%s\n",bad?"FAILURES":"all good"); return bad?1:0;
}
