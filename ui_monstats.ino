// The STATS page for ANY creature: the one you raise (party slot 1), a banked
// party member, or one in the PC box. It used to be that only the creature on
// the main screen had a stats view; everything else showed one cramped line.
//
// Opened from the STATS button on a creature's sheet, or by tapping party slot
// 1. Any tap, a swipe down or a swipe sideways goes back to where it was
// opened from. The numbers come from the same Party::*Of() formulas the
// battles use, so what this page says is what a fight will use.

uint8_t monStats = MS_NONE;



// The button on the sheet. Geometry asked, never restated: the renderer, the
// taps and the tests all use this.
// On a BOX sheet the row holds two pills, STATS and MOVE; on a party sheet
// STATS alone, centred. monSheetPill() is the one answer for both.
void monSheetPill(uint8_t which, int *x, int *y, int *w, int *h) {
  bool box = boxOpen && boxDetail;
  *y = MSTATS_BTN_Y; *h = MSTATS_BTN_H;
  if (!box) { *x = MSTATS_BTN_X; *w = MSTATS_BTN_W; return; }
  *w = 132;
  *x = which ? CX + 6 : CX - 6 - 132;
}
static bool inPill(uint8_t which, int16_t px, int16_t py) {
  int x, y, w, h; monSheetPill(which, &x, &y, &w, &h);
  return px >= x && px <= x + w && py >= y && py <= y + h;
}
bool monSheetStatsAt(int16_t x, int16_t y) { return inPill(0, x, y); }
bool monSheetMoveAt(int16_t x, int16_t y) { return boxOpen && boxDetail && inPill(1, x, y); }
void monSheetStatsCentre(int *x, int *y) { int a, b, w, h; monSheetPill(0, &a, &b, &w, &h); if (x) *x = a + w / 2; if (y) *y = b + h / 2; }
void monSheetMoveCentre(int *x, int *y) { int a, b, w, h; monSheetPill(1, &a, &b, &w, &h); if (x) *x = a + w / 2; if (y) *y = b + h / 2; }

// The creature the page is about, or false if it has gone (released, swapped).
bool monStatsSubject(PartyMon *out) {
  switch (monStats) {
    case MS_LIVE:
      if (pet.awaitingStarter() || pet.isEgg()) return false;
      *out = pet.toPartyMon();
      return true;
    case MS_PARTY:
      if (partyDetail < 1 || partyDetail > PARTY_SLOTS) return false;
      *out = party.slots[partyDetail - 1];
      return !out->empty();
    case MS_BOX:
      if (boxDetail < 1 || boxDetail > BOX_SLOTS || !party.box) return false;
      *out = party.box[boxDetail - 1];
      return !out->empty();
  }
  return false;
}

// One row: label, a bar, the stat, its IV, and its training against the cap.
static void msRow(int y, const char *label, uint16_t val, uint16_t color, uint8_t iv, int tr) {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(66, y);
  gfx->print(label);
  const int bx = 122, bw = 104;
  int fw = (int)val * bw / 360;      // the same 360 scale as the card's battle page
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(bx, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(bx, y + 2, fw, 11, 3, color);
  char n[8];
  snprintf(n, sizeof(n), "%u", val);
  gfx->setCursor(234, y);
  gfx->print(n);
  char b[16];
  snprintf(b, sizeof(b), T(S_IV_FMT), iv);
  gfx->setTextSize(1);
  gfx->setTextColor(iv >= 31 ? UI_BAR_WARN : UI_INK_SOFT);   // a perfect IV is the lucky one
  gfx->setCursor(284, y + 4);
  gfx->print(b);
  if (tr >= 0) {
    snprintf(b, sizeof(b), T(S_MS_TR), (unsigned)tr, (unsigned)Pet::trMaxFor(iv));
    gfx->setTextColor(tr >= Pet::trMaxFor(iv) ? UI_BAR_OK : UI_INK_SOFT);
    gfx->setCursor(328, y + 4);
    gfx->print(b);
  }
}

static void msLine(int y, const char *s, uint16_t col) {
  gfx->setTextColor(col);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(s) * 3, y);
  gfx->print(s);
}

void renderMonStats() {
  PartyMon m;
  if (!monStatsSubject(&m)) {   // gone: back to what is underneath, drawn THIS frame
    monStats = MS_NONE;
    render();
    return;
  }
  const DexEntry &d = DEX_TBL[m.dex];
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[36];
  snprintf(head, sizeof(head), "%s%s Lv.%u", m.shiny ? "*" : "", m.nick[0] ? m.nick : d.name, (unsigned)m.level);
  gfx->setTextColor(d.accent);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(head) * 6, 34);
  gfx->print(head);
  // the type chips, centred under the name
  {
    int w1 = (int)strlen(typeName(d.type1)) * 6 + 10;
    int w2 = d.type2 == T_NONE ? 0 : (int)strlen(typeName(d.type2)) * 6 + 10 + 6;
    int x = CX - (w1 + w2) / 2;
    x += drawTypeChip(x, 56, d.type1) + 6;
    if (d.type2 != T_NONE) drawTypeChip(x, 56, d.type2);
  }

  msRow(86,  T(S_STAT_VIT), party.vitOf(m), UI_BAR_OK,   m.ivHp,  -1);
  msRow(114, T(S_STAT_ATK), party.atkOf(m), UI_BAR_BAD,  m.ivAtk, m.trAtk);
  msRow(142, T(S_STAT_DEF), party.defOf(m), 0x4C98,      m.ivDef, m.trDef);
  msRow(170, T(S_MS_SPA),   party.spaOf(m), 0xD31B,      m.ivAtk, m.trAtk);
  msRow(198, T(S_MS_SPD),   party.spdOf(m), 0x2D7F,      m.ivDef, m.trDef);
  msRow(226, T(S_STAT_SPE), party.speOf(m), UI_BAR_WARN, m.ivSpe, m.trSpe);

  char b[48];
  unsigned ivt = (unsigned)m.ivHp + m.ivAtk + m.ivDef + m.ivSpe;
  snprintf(b, sizeof(b), T(S_MS_IVTOT), ivt);
  msLine(258, b, ivt >= 100 ? UI_BAR_WARN : UI_INK);
  snprintf(b, sizeof(b), T(S_MS_HELD), m.held ? itemEntry(m.held).name : "-");
  msLine(276, b, UI_INK);
  const char *pn = personaName(m.persona);
  snprintf(b, sizeof(b), T(S_MS_NATURE), (pn && pn[0]) ? pn : "-");
  msLine(294, b, UI_INK);
  if (m.hasCareState() || monStats == MS_LIVE) {
    snprintf(b, sizeof(b), T(S_MS_BOND), (unsigned)m.bond, (unsigned)m.medals);
    msLine(312, b, UI_INK_SOFT);
  }
  // the sprite, small, so the page says who it is at a glance
  const uint8_t *th = thumbs.get(m.dex);
  if (th) drawThumb(th, CX - GAL_CELL / 2, 316, 1, false);
  msLine(410, T(S_BACK), UI_INK_SOFT);
  gfx->flush();
}
