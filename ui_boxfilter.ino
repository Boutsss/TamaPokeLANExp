// Finding creatures in the box (box_filter.cpp holds the rules) and marking
// FAVOURITES. The box's hub opens the FILTER sheet: one chip per filter --
// everything, favourites, shinies, holding an item, and each of the 18 types --
// and a SORT pill that cycles slot order, level, IV total, the four battle
// stats, dex number and name. Geometry is asked, never restated: bfChip(),
// bfSortBtn() and sheetFavBtn() answer for the renderer, the taps and the tests.


uint8_t boxFilter = BF_ALL;
uint8_t boxSort = BS_SLOT;
bool boxFilterOpen = false;

// The view the box screen shows right now. A creature waiting to come in from
// the party needs the EMPTY slots, so an armed swap always shows the plain box.
// Cached: sorting 1200 creatures once per drawn slot would be far too slow, so
// the list is rebuilt only when the filter, the sort, the armed swap or the
// box itself (Party::boxGen) has changed.
static uint16_t bfCache[BOX_SLOTS];
static uint16_t bfCacheN = 0;
static uint32_t bfCacheKey = 0xFFFFFFFFu, bfCacheGen = 0xFFFFFFFFu, bfCacheQ = 0xFFFFFFFFu;
// A creature waiting to come in from the party, or one being MOVED within the
// box, needs the empty slots: both show the plain box.
static bool boxForcePlain() { return boxSwapFrom || boxMoveFrom; }
static const uint16_t *boxViewCached(uint16_t *nOut) {
  uint8_t f = boxForcePlain() ? BF_ALL : boxFilter, s = boxForcePlain() ? BS_SLOT : boxSort;
  uint32_t key = ((uint32_t)f << 8) | s;
  if (key != bfCacheKey || party.boxGen != bfCacheGen || (f == BF_SEARCH && boxSearchGen() != bfCacheQ)) {
    bfCacheQ = boxSearchGen();
    bfCacheN = boxView(party, f, s, bfCache);
    bfCacheKey = key;
    bfCacheGen = party.boxGen;
  }
  *nOut = bfCacheN;
  return bfCache;
}
uint16_t boxViewNow(uint16_t *out) {
  uint16_t n;
  const uint16_t *v = boxViewCached(&n);
  if (out) memcpy(out, v, n * sizeof(uint16_t));
  return n;
}
uint16_t boxViewCount() { uint16_t n; boxViewCached(&n); return n; }
uint8_t boxPages() {
  uint16_t n = boxViewCount();
  return n ? (uint8_t)((n + BOX_PER_PAGE - 1) / BOX_PER_PAGE) : 1;
}
// The box index shown at grid cell `pos` of the current page, -1 if none.
int boxIndexAt(uint8_t pos) {
  uint16_t n;
  const uint16_t *v = boxViewCached(&n);
  uint32_t k = (uint32_t)boxPage * BOX_PER_PAGE + pos;
  return k < n ? v[k] : -1;
}
bool boxFiltered() { return !boxForcePlain() && !boxViewPlain(boxFilter, boxSort); }

const char *boxFilterName(uint8_t f) {
  switch (f) {
    case BF_ALL: return T(S_BF_ALL);
    case BF_FAV: return T(S_BF_FAV);
    case BF_SHINY: return T(S_BF_SHINY);
    case BF_HELD: return T(S_BF_HELD);
    case BF_SEARCH: {
      static char q[BF_QUERY_LEN + 4];
      if (!boxSearchQuery()[0]) return T(S_BF_SEARCH);
      snprintf(q, sizeof(q), "\"%s\"", boxSearchQuery());
      return q;
    }
    default: return typeName((uint8_t)(f - BF_TYPE0));
  }
}
const char *boxSortName(uint8_t s) {
  switch (s) {
    case BS_LEVEL: return T(S_BF_S_LEVEL);
    case BS_IV: return T(S_BF_S_IV);
    case BS_ATK: return T(S_BF_S_ATK);
    case BS_DEF: return T(S_BF_S_DEF);
    case BS_SPE: return T(S_BF_S_SPE);
    case BS_HP: return T(S_BF_S_HP);
    case BS_DEX: return T(S_BF_S_DEX);
    case BS_NAME: return T(S_BF_S_NAME);
    default: return T(S_BF_S_SLOT);
  }
}
const char *boxFilterWord() { return T(S_BF_FILTER); }
const char *boxFavLockLine() { return T(S_BF_FAVLOCK); }
void boxFoundLine(char *out, size_t n, uint16_t found) { snprintf(out, n, T(S_BF_FOUND_FMT), (unsigned)found); }
void boxSortLine(char *out, size_t n) { snprintf(out, n, T(S_BF_SORT_FMT), boxSortName(boxSort)); }

// A small five-pointed star, the favourite's mark.
void drawFavStar(int cx, int cy, int r, uint16_t col) {
  for (int k = 0; k < 5; k++) {
    float a = -1.5708f + k * 1.2566f;
    int tx = cx + (int)(r * cosf(a)), ty = cy + (int)(r * sinf(a));
    int lx = cx + (int)(r * 0.42f * cosf(a - 0.628f)), ly = cy + (int)(r * 0.42f * sinf(a - 0.628f));
    int rx = cx + (int)(r * 0.42f * cosf(a + 0.628f)), ry = cy + (int)(r * 0.42f * sinf(a + 0.628f));
    gfx->fillTriangle(cx, cy, tx, ty, lx, ly, col);
    gfx->fillTriangle(cx, cy, tx, ty, rx, ry, col);
  }
}

// ---- the FILTER sheet: 4 columns of chips, then the SORT pill
#define BF_CHIP_W 84
#define BF_CHIP_H 44   // UI_TAP_MIN: the floor, not the target, but 22 chips have to fit
void bfChip(uint8_t i, int *x, int *y, int *w, int *h) {
  *w = BF_CHIP_W; *h = BF_CHIP_H;
  *x = CX - 2 * (BF_CHIP_W + 4) + 2 + (i % 4) * (BF_CHIP_W + 4);
  *y = 78 + (i / 4) * (BF_CHIP_H + 4);
}
void bfSortBtn(int *x, int *y, int *w, int *h) { *w = 240; *h = 46; *x = CX - 120; *y = 370; }

void renderBoxFilter() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_BF_FILTER);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(t) * 6, 48);
  gfx->print(t);
  for (uint8_t i = 0; i < BF_COUNT; i++) {
    int x, y, w, h; bfChip(i, &x, &y, &w, &h);
    bool on = boxFilter == i;
    uint16_t fill = UI_WHITE, ink = UI_INK;
    if (i >= BF_TYPE0 && i < BF_SEARCH) {
      uint8_t ty = (uint8_t)(i - BF_TYPE0);
      fill = typeColor(ty);
      ink = typeColorIsLight(ty) ? UI_INK : UI_WHITE;
    } else if (i == BF_FAV) { fill = 0xFEA0; }
    else if (i == BF_SEARCH) { fill = 0xBF1F; }
    gfx->fillRoundRect(x, y, w, h, 10, fill);
    gfx->drawRoundRect(x, y, w, h, 10, UI_INK);
    if (on) { gfx->drawRoundRect(x - 2, y - 2, w + 4, h + 4, 12, UI_INK); gfx->drawRoundRect(x - 3, y - 3, w + 6, h + 6, 13, UI_INK); }
    const char *nm = boxFilterName(i);
    gfx->setTextColor(ink);
    gfx->setTextSize(1);
    gfx->setCursor(x + w / 2 - (int)strlen(nm) * 3, y + h / 2 - 4);
    gfx->print(nm);
    if (i == BF_FAV) drawFavStar(x + 10, y + 10, 7, UI_INK);
    if (i == BF_SEARCH) {                    // a magnifying glass
      gfx->drawCircle(x + 11, y + 11, 5, UI_INK);
      gfx->drawLine(x + 15, y + 15, x + 19, y + 19, UI_INK);
      gfx->drawLine(x + 16, y + 15, x + 20, y + 19, UI_INK);
    }
  }
  int x, y, w, h; bfSortBtn(&x, &y, &w, &h);
  char b[40]; boxSortLine(b, sizeof(b));
  gfx->fillRoundRect(x, y, w, h, 14, C565(0xff, 0xf1, 0xc4));
  gfx->drawRoundRect(x, y, w, h, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(b) * 6, y + h / 2 - 8);
  gfx->print(b);
  gfx->flush();
}

// A chip chooses and closes (the box shows the result at once); the SORT pill
// cycles and stays. Anywhere else closes.
void boxFilterTap(int16_t px, int16_t py) {
  for (uint8_t i = 0; i < BF_COUNT; i++) {
    int x, y, w, h; bfChip(i, &x, &y, &w, &h);
    if (px < x || px >= x + w || py < y || py >= y + h) continue;
    if (i == BF_SEARCH) {                    // type a name; the keyboard sits on top
      openKeyboardFor(KB_SEARCH);
      sfxPlay(SFX_TAP);
      return;
    }
    boxFilter = i;
    boxPage = 0;
    boxFilterOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
  int x, y, w, h; bfSortBtn(&x, &y, &w, &h);
  if (px >= x && px < x + w && py >= y && py < y + h) {
    boxSort = (uint8_t)((boxSort + 1) % BS_COUNT);
    boxPage = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  boxFilterOpen = false;
}

// ---- every box at a glance: tap the box's title. 16 a page (4 x 4), the rim
// pages, a tap jumps to that box. RENAME names the box you came from.

#define BL_COLS 4
#define BL_W 76
#define BL_H 62
#define BL_GAP 6
#define BL_X0 (CX - (BL_COLS * BL_W + (BL_COLS - 1) * BL_GAP) / 2)
#define BL_Y0 76
uint8_t boxListPages() { return (uint8_t)((BOX_BOXES + BOX_LIST_PER - 1) / BOX_LIST_PER); }
void boxListTile(uint8_t pos, int *x, int *y, int *w, int *h) {
  *w = BL_W; *h = BL_H;
  *x = BL_X0 + (pos % BL_COLS) * (BL_W + BL_GAP);
  *y = BL_Y0 + (pos / BL_COLS) * (BL_H + BL_GAP);
}
void boxListRenameBtn(int *x, int *y, int *w, int *h) { *w = 180; *h = UI_TAP_MIN; *x = CX - 90; *y = 356; }
static uint8_t boxCountIn(uint8_t b) {
  uint8_t n = 0;
  for (uint8_t k = 0; k < BOX_PER_BOX; k++) if (!party.box[b * BOX_PER_BOX + k].empty()) n++;
  return n;
}
void renderBoxList() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (boxListPage >= boxListPages()) boxListPage = 0;
  char b[24];
  snprintf(b, sizeof(b), T(S_BOX_FMT), (unsigned)party.boxCount(), (unsigned)BOX_SLOTS);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(b) * 6, 40);
  gfx->print(b);
  for (uint8_t pos = 0; pos < BOX_LIST_PER; pos++) {
    uint8_t bx = (uint8_t)(boxListPage * BOX_LIST_PER + pos);
    if (bx >= BOX_BOXES) break;
    int x, y, w, h; boxListTile(pos, &x, &y, &w, &h);
    uint8_t n = boxCountIn(bx);
    bool here = bx == boxPage;
    gfx->fillRoundRect(x, y, w, h, 10, here ? (uint16_t)0xFF98 : UI_WHITE);
    gfx->drawRoundRect(x, y, w, h, 10, UI_INK);
    if (here) gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 9, UI_INK);
    // its name if it has one, else its number
    char nm[16];
    party.boxNameOf(bx, nm, sizeof(nm));
    bool named = party.boxNames[bx][0] != 0;
    if (named) { nm[10] = 0; }
    else snprintf(nm, sizeof(nm), "%u", (unsigned)(bx + 1));
    int sz = named ? 1 : 2;
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(sz);
    gfx->setCursor(x + w / 2 - (int)strlen(nm) * 3 * sz, y + 10);
    gfx->print(nm);
    // how full: a bar and the count
    gfx->fillRoundRect(x + 8, y + 36, w - 16, 7, 3, UI_TRACK);
    if (n) gfx->fillRoundRect(x + 8, y + 36, (w - 16) * n / BOX_PER_BOX, 7, 3, n == BOX_PER_BOX ? UI_BAR_BAD : UI_BAR_OK);
    snprintf(b, sizeof(b), "%u/%u", (unsigned)n, (unsigned)BOX_PER_BOX);
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(1);
    gfx->setCursor(x + w / 2 - (int)strlen(b) * 3, y + 48);
    gfx->print(b);
  }
  int x, y, w, h; boxListRenameBtn(&x, &y, &w, &h);
  snprintf(b, sizeof(b), T(S_BOX_RENAME_FMT), (unsigned)(boxPage + 1));
  gfx->fillRoundRect(x, y, w, h, 14, 0xBF1F);
  gfx->drawRoundRect(x, y, w, h, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(b) * 6, y + h / 2 - 7);
  gfx->print(b);
  uiChrome();
  gfx->flush();
}
void boxListTap(int16_t px, int16_t py) {
  int x, y, w, h; boxListRenameBtn(&x, &y, &w, &h);
  if (px >= x && px <= x + w && py >= y && py <= y + h) {
    boxListOpen = false;
    openKeyboardFor(KB_BOX);
    sfxPlay(SFX_TAP);
    return;
  }
  for (uint8_t pos = 0; pos < BOX_LIST_PER; pos++) {
    uint8_t bx = (uint8_t)(boxListPage * BOX_LIST_PER + pos);
    if (bx >= BOX_BOXES) break;
    boxListTile(pos, &x, &y, &w, &h);
    if (px < x || px >= x + w || py < y || py >= y + h) continue;
    boxPage = bx;
    boxListOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
}
