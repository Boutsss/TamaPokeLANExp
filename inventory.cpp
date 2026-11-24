#include "inventory.h"
#include <string.h>
#include <stdlib.h>
#include "nvsinfo.h"
Inventory bag;
void Inventory::begin() {
  memset(counts, 0, sizeof(counts));
  if (!prefs.begin("tamapoke", false)) Serial.println("save: NVS would not open for the bag");
  size_t stored = prefs.getBytesLength("bag");
  if (stored) {
    if (stored <= sizeof(counts)) prefs.getBytes("bag", counts, stored);
    else {
      uint8_t *tmp = (uint8_t *)malloc(stored);
      if (tmp) { if (prefs.getBytes("bag", tmp, stored) == stored) memcpy(counts, tmp, sizeof(counts)); free(tmp); }
    }
  } else grantStarterKit();
  counts[ITEM_NONE] = 0;
  for (uint8_t k = 0; k < ITEM_COUNT; k++) if (counts[k] > BAG_STACK_MAX) counts[k] = BAG_STACK_MAX;
}
bool Inventory::save() {
  if (prefs.putBytes("bag", counts, sizeof(counts)) != sizeof(counts)) { logKeyFailure("bag", "bag"); return false; }
  return true;
}
void Inventory::clear() { memset(counts, 0, sizeof(counts)); save(); }
void Inventory::grantStarterKit() { counts[IT_POKEBALL] = 5; counts[IT_POTION] = 3; save(); }
uint8_t Inventory::count(ItemKey k) const { return itemValid(k) ? counts[k] : 0; }
uint8_t Inventory::add(ItemKey k, uint8_t n) {
  if (!itemValid(k) || !n) return 0;
  uint8_t room = (uint8_t)(BAG_STACK_MAX - counts[k]);
  uint8_t got = n < room ? n : room;
  if (!got) return 0;
  uint8_t old = counts[k]; counts[k] = (uint8_t)(counts[k] + got);
  if (!save()) { counts[k] = old; return 0; }
  return got;
}
bool Inventory::addChecked(ItemKey k, uint8_t n) {
  if (!itemValid(k) || !n || count(k) > BAG_STACK_MAX - n) return false;
  return add(k, n) == n;
}
bool Inventory::consume(ItemKey k, uint8_t n) {
  if (!itemValid(k) || !n || counts[k] < n) return false;
  uint8_t old = counts[k]; counts[k] = (uint8_t)(counts[k] - n);
  if (!save()) { counts[k] = old; return false; }
  return true;
}
uint8_t Inventory::distinctCount() const { uint8_t n=0; for(uint8_t k=1;k<ITEM_COUNT;k++) if(counts[k]) n++; return n; }
ItemKey Inventory::keyAt(uint8_t i) const {
  for(uint8_t k=1;k<ITEM_COUNT;k++){ if(!counts[k])continue; if(!i)return k; i--; }
  return ITEM_NONE;
}
static bool excluded(ItemKey k,const ItemKey *ex,uint8_t n){for(uint8_t i=0;i<n;i++)if(ex[i]==k)return true;return false;}
uint32_t Inventory::weightTotal(const ItemKey *exclude,uint8_t excludeCount) const {
  uint32_t total=0; for(uint8_t k=1;k<ITEM_COUNT;k++){if(excluded(k,exclude,excludeCount))continue;total+=ITEM_TBL[k].dropWeight;} return total;
}
ItemKey Inventory::weightedDrop(uint32_t roll,const ItemKey *exclude,uint8_t excludeCount) const {
  uint32_t total=weightTotal(exclude,excludeCount); if(!total)return ITEM_NONE;
  uint32_t at=roll%total; for(uint8_t k=1;k<ITEM_COUNT;k++){if(excluded(k,exclude,excludeCount))continue;uint32_t w=ITEM_TBL[k].dropWeight;if(at<w)return k;at-=w;} return ITEM_NONE;
}
