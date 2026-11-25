#include "box_filter.h"
#include "dex.h"
#include <string.h>
#include <algorithm>

bool boxFilterMatch(const PartyMon &m, uint8_t f) {
  if (m.empty()) return false;
  switch (f) {
    case BF_ALL: return true;
    case BF_FAV: return m.isFav();
    case BF_SHINY: return m.shiny != 0;
    case BF_HELD: return m.held != 0;
    default: {
      if (f < BF_TYPE0 || f >= BF_COUNT || m.dex < 1 || m.dex > DEX_COUNT) return false;
      uint8_t t = (uint8_t)(f - BF_TYPE0);
      const DexEntry &d = DEX_TBL[m.dex];
      return d.type1 == t || (d.type2 != T_NONE && d.type2 == t);
    }
  }
}

static const char *nameOf(const PartyMon &m) { return m.nick[0] ? m.nick : DEX_TBL[m.dex].name; }

// Bigger is better for every numeric sort; NAME and DEX ascend.
static int32_t key(const Party &p, const PartyMon &m, uint8_t s) {
  switch (s) {
    case BS_LEVEL: return m.level;
    case BS_IV: return m.ivAtk + m.ivDef + m.ivSpe + m.ivHp;
    case BS_ATK: return p.atkOf(m);
    case BS_DEF: return p.defOf(m);
    case BS_SPE: return p.speOf(m);
    case BS_HP: return p.vitOf(m);
    case BS_DEX: return -(int32_t)m.dex;
    default: return 0;
  }
}

// true if box slot a should come before box slot b
static bool before(const Party &p, uint16_t a, uint16_t b, uint8_t s) {
  const PartyMon &x = p.box[a], &y = p.box[b];
  if (s == BS_NAME) {
    int c = strcmp(nameOf(x), nameOf(y));
    if (c) return c < 0;
  } else if (s != BS_SLOT) {
    int32_t kx = key(p, x, s), ky = key(p, y, s);
    if (kx != ky) return kx > ky;
  }
  return a < b;   // ties keep the box's own order
}

uint16_t boxView(const Party &p, uint8_t filter, uint8_t sort, uint16_t *out) {
  uint16_t n = 0;
  if (!p.box) return 0;
  if (boxViewPlain(filter, sort)) {
    for (uint16_t i = 0; i < BOX_SLOTS; i++) out[n++] = i;
    return n;
  }
  for (uint16_t i = 0; i < BOX_SLOTS; i++) if (boxFilterMatch(p.box[i], filter)) out[n++] = i;
  // up to 1200: O(n log n), and stable (the comparator breaks ties by slot anyway)
  std::stable_sort(out, out + n, [&](uint16_t a, uint16_t b) { return before(p, a, b, sort); });
  return n;
}
