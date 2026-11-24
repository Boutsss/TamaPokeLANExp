#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "items.h"
constexpr uint8_t BAG_STACK_MAX = 99;
class Inventory {
public:
  void begin();
  bool save();
  uint8_t count(ItemKey k) const;
  uint8_t add(ItemKey k, uint8_t n = 1);
  bool addChecked(ItemKey k, uint8_t n = 1);
  bool consume(ItemKey k, uint8_t n = 1);
  bool has(ItemKey k) const { return count(k) > 0; }
  uint8_t distinctCount() const;
  ItemKey keyAt(uint8_t i) const;
  void grantStarterKit();
  ItemKey weightedDrop(uint32_t roll, const ItemKey *exclude = nullptr, uint8_t excludeCount = 0) const;
  uint32_t weightTotal(const ItemKey *exclude = nullptr, uint8_t excludeCount = 0) const;
  void clear();
private:
  uint8_t counts[ITEM_COUNT] = { 0 };
  Preferences prefs;
};
extern Inventory bag;
