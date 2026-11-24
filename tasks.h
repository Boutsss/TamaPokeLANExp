#pragma once
#include <Arduino.h>
#include "items.h"
#include "game_events.h"

class Pet;

#define TASK_DAILY 3
#define TASK_WEEKLY 2
#define TASK_SLOTS (TASK_DAILY + TASK_WEEKLY)
#define TASK_NOTICE_MS 3000UL

enum TaskKind : uint8_t {
  TK_NONE = 0,
  TK_FEED, TK_FAV_BERRY, TK_CLEAN, TK_PLAY, TK_TRAIN,
  TK_CARE_DAY, TK_HATCH, TK_EVOLVE, TK_GYM_WIN, TK_REMATCH_WIN,
  TK_WILD_SEEN, TK_WILD_WIN, TK_WILD_CATCH
};

struct TaskDef {
  uint8_t kind;
  uint8_t target;
  ItemKey item;
  uint8_t qty;
};

extern const TaskDef DAILY_TASKS[];
extern const uint8_t DAILY_TASK_COUNT;
extern const TaskDef WEEKLY_TASKS[];
extern const uint8_t WEEKLY_TASK_COUNT;

void tasksBegin(Pet &pet);
void tasksRefresh();
void tasksTick();
void tasksOnEvent(GameEv ev, int16_t arg);
const TaskDef &tasksDef(uint8_t slot);
uint8_t tasksProgress(uint8_t slot);
bool tasksDone(uint8_t slot);
bool tasksClaimed(uint8_t slot);
bool tasksClaim(uint8_t slot);
bool tasksBonusReady();
bool tasksBonusClaimed();
bool tasksClaimBonus();
ItemKey tasksBonusItem();
uint8_t tasksBonusQty();
uint8_t tasksClaimable();
uint32_t tasksDay();
uint32_t tasksWeek();
bool tasksNoticeActive();
void tasksForget();
uint32_t tasksWeekOf(uint32_t day);
