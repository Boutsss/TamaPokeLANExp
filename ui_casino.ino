// GAME CORNER -- the hidden casino. Hold the ARCADE row on the training
// menu for CASINO_HOLD_MS (five seconds) and a Team Rocket grunt lets you in.
//
// Rules, coins and the prize shop live in casino.cpp (headless, tested by
// casino_test). This file is the screens: the lobby with the dealer, SLOTS,
// HIGH/LOW, BLACKJACK, the coin counter and the PRIZE shop.
//
// Every screen asks casLobbyBtn()/casBtn() for its own geometry, and the test
// asks the same functions -- never a restated pixel.
//
// The animated trainer-card backgrounds (LIVE FIELD .. AURORA) are drawn here
// too, procedurally, in the spirit of Catch 151's walking zones (MIT,
// github.com/FedorSosnin/Catch151).



// ---------------------------------------------------------------- state
bool casinoOpen = false;
// APPEND-ONLY: casinoView() == CV_SHOP (5) is how the sketch tells the shop apart.
enum : uint8_t { CV_LOBBY = 0, CV_SLOTS, CV_HL, CV_BJ, CV_COINS, CV_SHOP, CV_ROULETTE };
static uint8_t casView = CV_LOBBY;
uint8_t casShopKind = PK_BACK;
uint8_t casShopPage = 0;
static int16_t casArmed = -1;          // a shop tile tapped once: the next tap buys it
// Bets in COINS. The slots take 1, 2 or 3 like the GBC machines; the tables,
// which the old Game Corner never had, take 5, 10 or 50.
static const uint16_t SLOT_BETS[3] = { 1, 2, 3 };
static const uint16_t TABLE_BETS[3] = { 5, 10, 50 };
static uint8_t casBetI = 0;
static uint16_t casBet() { return (casView == CV_SLOTS ? SLOT_BETS : TABLE_BETS)[casBetI % 3]; }
static uint32_t casOpenedAt = 0;      // the secret door opens from here

// the dealer's mood drives his animation and his line
enum : uint8_t { DM_IDLE = 0, DM_DEAL, DM_LAUGH, DM_GRUMBLE };
static uint8_t casMood = DM_IDLE;
static uint32_t casMoodAt = 0;
// The marquee flashes when the PLAYER wins -- which is exactly when the dealer
// scowls, so DM_GRUMBLE is the one hook and no game needs its own call.
// A separate flag rather than "0 = never": a stamp nudged off zero (millis() | 1)
// lands a millisecond in the FUTURE half the time, and now - stamp then wraps
// to four billion -- the flash simply never showed. casino_lights_ui_test.
static uint32_t casWinFlashAt = 0;
static bool casWinFlashed = false;
#define CAS_FLASH_MS 1500
static void casSetMood(uint8_t m) {
  casMood = m; casMoodAt = millis();
  if (m == DM_GRUMBLE) { casWinFlashAt = millis(); casWinFlashed = true; }
}
static bool casFlashingAt(uint32_t now) { return casWinFlashed && now - casWinFlashAt < CAS_FLASH_MS; }
bool casLightsFlashing() { return casFlashingAt(millis()); }

// the hidden door: a finger held on the ARCADE row
static uint32_t casHoldAt = 0;
static bool casHoldArmed = false;

// games
static uint8_t slStop[3] = { 0, 0, 0 };
static uint8_t slShown = 3;            // reels stopped (3 = idle)
static uint32_t slSpinAt = 0;
static uint32_t slWon = 0;
static HighLow casHl;
static Blackjack casBj;
static uint32_t casDealtAt = 0;        // newest card's slide-in starts here
static uint32_t casLastWin = 0;
static uint8_t casResultShown = 0;     // a finished round's banner is up

// ---------------------------------------------------------------- coins
static bool casSpendCoins(uint32_t n) { return pet.spendCoins(n); }
static void casPay(uint32_t n) { if (n) pet.addCoins(n); }
static bool casTakeBet(uint16_t n) {
  if (pet.coins < n) { sfxPlay(SFX_DENY); casSetMood(DM_LAUGH); return false; }
  return pet.spendCoins(n);
}

// ---------------------------------------------------------------- geometry
#define CAS_BTN_H 58
// The lobby: six buttons, two columns of three under the dealer.
#define CAS_LOBBY_BTNS 6
void casLobbyBtn(uint8_t i, int *x, int *y, int *w, int *h) {
  *h = 56; *w = 150;
  *x = (i % 2) ? CX + 6 : CX - 156;
  *y = 176 + (i / 2) * 66;
}
// A game screen's bottom row of up to three buttons.
void casBtn(uint8_t i, uint8_t n, int *x, int *y, int *w, int *h) {
  *h = CAS_BTN_H;
  *y = 318;
  int total = 360, gap = 8;
  *w = (total - gap * (n - 1)) / n;
  *x = CX - total / 2 + i * (*w + gap);
}
// The BET chip, top of every game screen.
void casBetBtn(int *x, int *y, int *w, int *h) { *w = 150; *h = 38; *x = CX - 75; *y = 272; }
// The shop: tabs and a 2x3 grid.
#define CAS_SHOP_PER 6
// casShopPage is a uint8_t (the rim's page pointer), so the longest list must
// fit 255 pages: 1530 prizes at six a page, comfortably past CASINO_AVATARS_MAX.
static_assert((CASINO_AVATARS_MAX + CAS_SHOP_PER - 1) / CAS_SHOP_PER <= 255, "casShopPage would wrap");
// One tab per PrizeKind, sized from PK_KINDS so a new kind gets its tab here.
void casTab(uint8_t i, int *x, int *y, int *w, int *h) {
  const int total = 364, gap = 4;
  *w = (total - gap * (PK_KINDS - 1)) / PK_KINDS; *h = 40; *y = 64;
  *x = CX - total / 2 + i * (*w + gap);
}
void casTile(uint8_t i, int *x, int *y, int *w, int *h) {
  *w = 132; *h = 78;
  *x = (i % 2) ? CX + 4 : CX - 136;
  *y = 112 + (i / 2) * 86;
}
static bool inBox(int16_t px, int16_t py, int x, int y, int w, int h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}
uint8_t casShopPages() {
  uint16_t n = casinoShopCount(casShopKind);
  return n ? (uint8_t)((n + CAS_SHOP_PER - 1) / CAS_SHOP_PER) : 1;
}

// ---------------------------------------------------------------- drawing helpers
static uint16_t mix565(uint16_t a, uint16_t b, uint8_t t) {   // t/255 of b
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((ar + (br - ar) * t / 255) << 11) | ((ag + (bg - ag) * t / 255) << 5) |
                    (ab + (bb - ab) * t / 255));
}
static void casText(int cx, int y, const char *t, uint16_t col, int size) {
  gfx->setTextColor(col);
  gfx->setTextSize(size);
  gfx->setCursor(cx - (int)strlen(t) * 3 * size, y);
  gfx->print(t);
}
static void casButton(int x, int y, int w, int h, const char *label, uint16_t fill, bool dim) {
  gfx->fillRoundRect(x, y, w, h, 14, dim ? UI_TRACK : fill);
  gfx->drawRoundRect(x, y, w, h, 14, UI_INK);
  casText(x + w / 2, y + h / 2 - 8, label, dim ? 0x8410 : UI_INK, 2);
}
static const uint16_t CAS_RED = 0xE8E4, CAS_GOLD = 0xFEA0, CAS_FELT = 0x2C6A, CAS_FELT_D = 0x1B47;
static const uint16_t CAS_PURPLE = 0x895F;   // the armored skin's tag

// Small integer sine: s(i) for i in 0..255 is round(127*sin(2*pi*i/256)).
static int8_t isin8(uint8_t i) {
  static const int8_t Q[65] = { 0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49,
    51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88, 90, 92, 94, 96, 98, 100, 102,
    104, 106, 107, 109, 111, 112, 113, 115, 116, 117, 118, 120, 121, 122, 122, 123, 124, 125,
    125, 126, 126, 126, 127, 127, 127, 127 };
  if (i < 64) return Q[i];
  if (i < 128) return Q[128 - i];
  if (i < 192) return (int8_t)-Q[i - 128];
  return (int8_t)-Q[256 - i];
}
static uint32_t hash2(int a, int b) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// ---- a motif, centred on (cx, cy), about `s` px across -----------------------
static void casMotif(uint8_t m, int cx, int cy, int s, uint16_t col, uint16_t ink) {
  int r = s / 2;
  switch (m) {
    case CM_BALL:   // white below, `col` above, a dark band and button
      gfx->fillCircle(cx, cy, r, UI_WHITE);
      for (int yy = -r; yy < 0; yy++) {
        int hw = (int)sqrtf((float)(r * r - yy * yy));
        gfx->drawFastHLine(cx - hw, cy + yy, hw * 2 + 1, col);
      }
      gfx->fillRect(cx - r, cy - 1 - r / 12, 2 * r + 1, 2 + r / 6, ink);
      gfx->fillCircle(cx, cy, r / 3 + 1, ink);
      gfx->fillCircle(cx, cy, r / 3 - 1, UI_WHITE);
      gfx->drawCircle(cx, cy, r, ink);
      break;
    case CM_ROCKET: {
      int w = s * 2 / 3, h = s;
      int x0 = cx - w / 2, y0 = cy - h / 2, t = s / 6 + 1;
      gfx->fillRect(x0, y0, t, h, col);                         // stem
      gfx->fillRect(x0, y0, w - t, t, col);                     // top
      gfx->fillRect(x0, cy - t / 2, w - t, t, col);             // middle
      gfx->fillRect(x0 + w - t, y0 + t / 2, t, h / 2 - t / 2, col);   // bowl
      for (int k = 0; k < t; k++) gfx->drawLine(cx - t / 2 + k, cy, x0 + w - t + k, y0 + h - 1, col);  // leg
      break;
    }
    case CM_FLAME:
      gfx->fillCircle(cx, cy + r / 3, r * 2 / 3, col);
      gfx->fillTriangle(cx - r * 2 / 3, cy + r / 3, cx + r * 2 / 3, cy + r / 3, cx, cy - r, col);
      gfx->fillCircle(cx, cy + r / 2, r / 3, ink == UI_WHITE ? CAS_GOLD : 0xFFE0);
      break;
    case CM_DROP:
      gfx->fillCircle(cx, cy + r / 3, r * 2 / 3, col);
      gfx->fillTriangle(cx - r * 2 / 3 + 1, cy + r / 4, cx + r * 2 / 3 - 1, cy + r / 4, cx, cy - r, col);
      gfx->fillCircle(cx - r / 4, cy + r / 4, r / 6 + 1, UI_WHITE);
      break;
    case CM_LEAF:
      gfx->fillCircle(cx - r / 4, cy, r * 2 / 3, col);
      gfx->fillCircle(cx + r / 4, cy, r * 2 / 3, col);
      gfx->fillTriangle(cx - r, cy, cx + r, cy, cx, cy - r, col);
      gfx->drawLine(cx - r + 2, cy + r / 2, cx + r - 2, cy - r / 2, ink);
      break;
    case CM_BOLT:
      gfx->fillTriangle(cx + r / 3, cy - r, cx - r / 2, cy + r / 6, cx + r / 8, cy + r / 6, col);
      gfx->fillTriangle(cx - r / 3, cy + r, cx + r / 2, cy - r / 6, cx - r / 8, cy - r / 6, col);
      break;
    case CM_SNOW:
      for (int k = 0; k < 3; k++) {
        int dx = (int)(r * cosf(k * 1.047f)), dy = (int)(r * sinf(k * 1.047f));
        gfx->drawLine(cx - dx, cy - dy, cx + dx, cy + dy, col);
        gfx->drawLine(cx - dx + 1, cy - dy, cx + dx + 1, cy + dy, col);
      }
      gfx->fillCircle(cx, cy, r / 5 + 1, col);
      break;
    case CM_FIST:
      gfx->fillRoundRect(cx - r * 2 / 3, cy - r / 2, r * 4 / 3, r, r / 3, col);
      for (int k = 0; k < 3; k++) gfx->drawFastVLine(cx - r / 3 + k * r / 3, cy - r / 2, r / 2, ink);
      gfx->fillRoundRect(cx - r / 2, cy + r / 2 - 1, r, r / 2, 3, col);
      break;
    case CM_SKULL:
      gfx->fillCircle(cx, cy - r / 6, r * 2 / 3, col);
      gfx->fillRect(cx - r / 3, cy + r / 3, r * 2 / 3, r / 3, col);
      gfx->fillCircle(cx - r / 4, cy - r / 6, r / 6 + 1, ink);
      gfx->fillCircle(cx + r / 4, cy - r / 6, r / 6 + 1, ink);
      break;
    case CM_MOUND:
      gfx->fillTriangle(cx - r, cy + r / 2, cx + r, cy + r / 2, cx - r / 6, cy - r * 2 / 3, col);
      gfx->fillTriangle(cx - r / 4, cy + r / 2, cx + r, cy + r / 2, cx + r / 2, cy - r / 4, mix565(col, ink, 60));
      break;
    case CM_WING:
      for (int k = 0; k < 3; k++)
        gfx->fillRoundRect(cx - r + k * r / 4, cy - r / 2 + k * r / 3, r * 2 - k * r / 2, r / 3, r / 6, col);
      break;
    case CM_EYE:
      gfx->fillCircle(cx, cy, r * 2 / 3, col);
      gfx->fillCircle(cx, cy, r / 3, ink);
      gfx->fillCircle(cx + r / 8, cy - r / 8, r / 8 + 1, UI_WHITE);
      break;
    case CM_BUG:
      gfx->fillCircle(cx, cy + r / 6, r * 2 / 3, col);
      gfx->drawFastVLine(cx, cy - r / 2, r * 4 / 3, ink);
      gfx->fillCircle(cx, cy - r * 2 / 3, r / 4, ink);
      gfx->fillCircle(cx - r / 3, cy + r / 6, r / 8 + 1, ink);
      gfx->fillCircle(cx + r / 3, cy + r / 3, r / 8 + 1, ink);
      break;
    case CM_ROCK:
      gfx->fillTriangle(cx - r, cy + r / 2, cx - r / 2, cy - r / 2, cx + r / 3, cy + r / 2, col);
      gfx->fillTriangle(cx - r / 3, cy + r / 2, cx + r / 3, cy - r * 2 / 3, cx + r, cy + r / 2, mix565(col, ink, 50));
      break;
    case CM_GHOST:
      gfx->fillCircle(cx, cy - r / 4, r * 2 / 3, col);
      gfx->fillRect(cx - r * 2 / 3, cy - r / 4, r * 4 / 3 + 1, r * 2 / 3, col);
      for (int k = 0; k < 3; k++) gfx->fillCircle(cx - r / 2 + k * r / 2, cy + r * 5 / 12, r / 5 + 1, col);
      gfx->fillCircle(cx - r / 4, cy - r / 4, r / 7 + 1, ink);
      gfx->fillCircle(cx + r / 4, cy - r / 4, r / 7 + 1, ink);
      break;
    case CM_DRAGON:
      gfx->fillTriangle(cx - r, cy + r / 2, cx, cy - r, cx + r, cy + r / 2, col);
      gfx->fillTriangle(cx - r / 2, cy + r / 2, cx, cy - r / 6, cx + r / 2, cy + r / 2, ink);
      gfx->fillCircle(cx, cy + r / 4, r / 6, col);
      break;
    case CM_MOON:
      gfx->fillCircle(cx, cy, r * 2 / 3, col);
      gfx->fillCircle(cx + r / 3, cy - r / 4, r * 2 / 3, ink);
      break;
    case CM_GEAR:
      for (int k = 0; k < 8; k++) {
        float a = k * 0.785f;
        gfx->fillCircle(cx + (int)(r * 0.75f * cosf(a)), cy + (int)(r * 0.75f * sinf(a)), r / 5 + 1, col);
      }
      gfx->fillCircle(cx, cy, r * 2 / 3, col);
      gfx->fillCircle(cx, cy, r / 4, ink);
      break;
    case CM_STAR:
      for (int k = 0; k < 5; k++) {
        float a = -1.5708f + k * 1.2566f, b = a + 2.513f;
        gfx->fillTriangle(cx, cy, cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)),
                          cx + (int)(r * 0.4f * cosf(a + 0.628f)), cy + (int)(r * 0.4f * sinf(a + 0.628f)), col);
        gfx->fillTriangle(cx, cy, cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)),
                          cx + (int)(r * 0.4f * cosf(a - 0.628f)), cy + (int)(r * 0.4f * sinf(a - 0.628f)), col);
        (void)b;
      }
      break;
    case CM_RING:
      gfx->fillCircle(cx, cy, r * 2 / 3, col);
      gfx->fillCircle(cx, cy, r / 3, ink);
      break;
    case CM_M:
      casMotif(CM_BALL, cx, cy, s, col, ink);
      gfx->setTextColor(UI_WHITE);
      gfx->setTextSize(s >= 40 ? 2 : 1);
      gfx->setCursor(cx - (s >= 40 ? 6 : 3), cy - r / 2 - (s >= 40 ? 6 : 2));
      gfx->print("M");
      break;
    case CM_STRIPES:
      casMotif(CM_BALL, cx, cy, s, col, ink);
      gfx->fillRect(cx - r / 2, cy - r * 3 / 4, r / 4, r / 2, ink == UI_WHITE ? CAS_RED : ink);
      gfx->fillRect(cx + r / 4, cy - r * 3 / 4, r / 4, r / 2, ink == UI_WHITE ? CAS_RED : ink);
      break;
    case CM_COIN:
      gfx->fillCircle(cx, cy, r * 3 / 4, col);
      gfx->drawCircle(cx, cy, r * 3 / 4, ink);
      gfx->drawCircle(cx, cy, r / 2, ink);
      casText(cx, cy - 4, "$", ink, 1);
      break;
    default: {   // CM_LATTICE: no centre motif
      break;
    }
  }
}

// ---- a card back ------------------------------------------------------------
void casDrawBack(int x, int y, int w, int h, uint8_t back) {
  const CardBackDef &d = CARD_BACKS[back < CARD_BACK_COUNT ? back : 0];
  gfx->fillRoundRect(x, y, w, h, 6, UI_WHITE);
  gfx->fillRoundRect(x + 3, y + 3, w - 6, h - 6, 4, d.base);
  // a lattice of fine diagonals behind every motif: the casino look
  uint16_t lat = mix565(d.base, d.accent, 70);
  for (int k = -h; k < w; k += 8) {
    int x0 = x + 4 + k, y0 = y + 4, x1 = x0 + (h - 8), y1 = y + h - 4;
    // clip the two diagonals to the inner rectangle
    for (int t = 0; t < h - 8; t += 1) {
      int px = x0 + t, py = y0 + t;
      if (px >= x + 4 && px < x + w - 4) gfx->fillRect(px, py, 1, 1, lat);
      int qx = x + w - 5 - (k + t), qy = y0 + t;
      if (qx >= x + 4 && qx < x + w - 4) gfx->fillRect(qx, qy, 1, 1, lat);
    }
    (void)x1; (void)y1;
  }
  gfx->drawRoundRect(x + 6, y + 6, w - 12, h - 12, 3, d.accent);
  if (d.motif != CM_LATTICE) {
    int s = (w < h ? w : h) * 3 / 5;
    casMotif(d.motif, x + w / 2, y + h / 2, s, d.accent, d.ink);
  }
}

// ---- a card face ------------------------------------------------------------
static void casSuit(uint8_t suit, int cx, int cy, int r) {
  uint16_t c = (suit == 1 || suit == 2) ? CAS_RED : UI_INK;   // hearts, diamonds red
  switch (suit) {
    case 0:   // spade
      gfx->fillCircle(cx - r / 2, cy + r / 4, r / 2, c);
      gfx->fillCircle(cx + r / 2, cy + r / 4, r / 2, c);
      gfx->fillTriangle(cx - r, cy + r / 6, cx + r, cy + r / 6, cx, cy - r, c);
      gfx->fillTriangle(cx, cy + r / 3, cx - r / 3, cy + r, cx + r / 3, cy + r, c);
      break;
    case 1:   // heart
      gfx->fillCircle(cx - r / 2, cy - r / 4, r / 2, c);
      gfx->fillCircle(cx + r / 2, cy - r / 4, r / 2, c);
      gfx->fillTriangle(cx - r, cy - r / 6, cx + r, cy - r / 6, cx, cy + r, c);
      break;
    case 2:   // diamond
      gfx->fillTriangle(cx - r * 3 / 4, cy, cx + r * 3 / 4, cy, cx, cy - r, c);
      gfx->fillTriangle(cx - r * 3 / 4, cy, cx + r * 3 / 4, cy, cx, cy + r, c);
      break;
    default:  // club
      gfx->fillCircle(cx, cy - r / 2, r / 2, c);
      gfx->fillCircle(cx - r / 2, cy + r / 6, r / 2, c);
      gfx->fillCircle(cx + r / 2, cy + r / 6, r / 2, c);
      gfx->fillTriangle(cx, cy, cx - r / 3, cy + r, cx + r / 3, cy + r, c);
      break;
  }
}
#define CAS_CARD_W 58
#define CAS_CARD_H 84
static void casDrawCard(int x, int y, uint8_t card) {
  gfx->fillRoundRect(x, y, CAS_CARD_W, CAS_CARD_H, 6, UI_WHITE);
  gfx->drawRoundRect(x, y, CAS_CARD_W, CAS_CARD_H, 6, UI_INK);
  uint8_t suit = cardSuit(card);
  uint16_t c = (suit == 1 || suit == 2) ? CAS_RED : UI_INK;
  const char *r = cardRankName(card);
  gfx->setTextColor(c);
  gfx->setTextSize(2);
  gfx->setCursor(x + 5, y + 5);
  gfx->print(r);
  casSuit(suit, x + CAS_CARD_W / 2, y + CAS_CARD_H / 2 + 8, 13);
  // face cards wear a little crown, the ace a ring
  uint8_t rk = cardRank(card);
  if (rk >= 10) {
    int cx = x + CAS_CARD_W - 14, cy = y + 12;
    gfx->fillTriangle(cx - 7, cy + 5, cx + 7, cy + 5, cx - 7, cy - 4, CAS_GOLD);
    gfx->fillTriangle(cx - 7, cy + 5, cx + 7, cy + 5, cx + 7, cy - 4, CAS_GOLD);
    gfx->fillTriangle(cx - 4, cy + 5, cx + 4, cy + 5, cx, cy - 6, CAS_GOLD);
  } else if (rk == 0) {
    gfx->drawCircle(x + CAS_CARD_W / 2, y + CAS_CARD_H / 2 + 8, 19, CAS_GOLD);
  }
}
// A card that is still sliding in from the shoe (top right) to (x, y).
static void casDealCard(int x, int y, uint8_t card, bool faceUp, uint32_t since) {
  uint32_t t = millis() - since;
  const uint32_t SLIDE = 260;
  if (t < SLIDE) {
    int sx = 360, sy = 60;
    x = sx + (int)((x - sx) * (int32_t)t / (int32_t)SLIDE);
    y = sy + (int)((y - sy) * (int32_t)t / (int32_t)SLIDE);
    faceUp = false;   // it flips as it lands
  }
  if (faceUp) casDrawCard(x, y, card);
  else casDrawBack(x, y, CAS_CARD_W, CAS_CARD_H, casino.back);
}

// ---- the dealer: the Team Rocket grunt, animated ------------------------------
void casDrawDealer(int cx, int top, int scale) {
  uint32_t now = millis(), dt = now - casMoodAt;
  if (casMood != DM_IDLE && dt > 1800) casMood = DM_IDLE;
  uint8_t frame = 0;
  int dy = 0, dx = 0;
  switch (casMood) {
    case DM_DEAL:    frame = (dt / 120) % 2 ? 2 : 5; break;            // turned to the shoe, dealing
    case DM_LAUGH:   frame = (dt / 140) % 2 ? 0 : 3; dy = -(int)((dt / 90) % 2) * 6; break;   // hopping
    case DM_GRUMBLE: frame = 1; dx = ((dt / 60) % 2) ? 3 : -3; break;  // back turned, shaking
    default:         frame = ((now / 600) % 2) ? 0 : 3; dy = ((now / 600) % 2) ? -2 : 0; break;
  }
  int x0 = cx - AVATAR_PX * scale / 2 + dx, y0 = top + dy;
  // a shadow under his feet
  gfx->fillRoundRect(cx - 7 * scale, top + 15 * scale, 14 * scale, 2 * scale, scale, mix565(CAS_FELT, 0, 120));
  const uint8_t *idx = AV_DEALER_IDX + frame * AVATAR_PX * AVATAR_PX;
  bool flip = (casMood == DM_DEAL) && (dt / 120) % 2;   // the frames face left; flip one for right
  for (int yy = 0; yy < AVATAR_PX; yy++)
    for (int xx = 0; xx < AVATAR_PX; xx++) {
      uint8_t v = idx[yy * AVATAR_PX + (flip ? AVATAR_PX - 1 - xx : xx)];
      if (v == 0xFF) continue;
      gfx->fillRect(x0 + xx * scale, y0 + yy * scale, scale, scale, AV_DEALER_PAL[v]);
    }
  // the line he says, in a bubble beside him
  const char *line = T(S_CAS_HELLO);
  if (casMood == DM_LAUGH) line = T(S_CAS_HEH);
  else if (casMood == DM_GRUMBLE) line = T(S_CAS_LUCKY);
  else if ((now / 4000) % 3 == 2) line = T(S_CAS_SECRET);
  int bw = (int)strlen(line) * 6 + 16, bx = cx + 8 * scale, by = top - 2;
  if (bx + bw > 440) bx = 440 - bw;
  gfx->fillRoundRect(bx, by, bw, 22, 8, UI_WHITE);
  gfx->drawRoundRect(bx, by, bw, 22, 8, UI_INK);
  gfx->fillTriangle(bx + 6, by + 22, bx + 16, by + 22, bx + 2, by + 30, UI_WHITE);
  casText(bx + bw / 2, by + 7, line, UI_INK, 1);
}

// The felt table every casino screen sits on, with the coin counter.
// ---------------------------------------------------------------- the marquee
// A ring of bulbs on a dark band round the felt, like the frame of a casino
// sign. Kept to a slow chase -- one bulb in four lit, a step every 140 ms --
// so it reads as glamour, not a strobe; a WIN lights them all, blinking, for
// CAS_FLASH_MS. Every bulb is drawn dim first, so the ring is always visible.
#define CAS_BULBS 40
#define CAS_BULB_R 221           // the ring's radius: inside the 233 glass
#define CAS_FELT_R 210           // the felt stops short of it
static const uint16_t CAS_BAND = 0x1800;                          // near-black maroon
static const uint16_t CAS_BULB_ON[3]  = { 0xFF00, 0xFC60, 0xF8A3 };   // yellow, orange, red
static const uint16_t CAS_BULB_DIM[3] = { 0x52A0, 0x5180, 0x4821 };
uint8_t casBulbLit(uint8_t i, uint32_t now) {   // 0 dim, 1 lit -- also what the tests ask
  if (casFlashingAt(now)) return ((now / 120) % 2) == 0;
  return ((i + now / 140) % 4) == 0;
}
void casBulbPos(uint8_t i, int *x, int *y) {
  float a = (float)i * 6.2831853f / CAS_BULBS - 1.5707963f;
  *x = CX + (int)lroundf(CAS_BULB_R * cosf(a));
  *y = CY + (int)lroundf(CAS_BULB_R * sinf(a));
}
static void casMarquee() {
  const uint32_t now = millis();
  gfx->fillCircle(CX, CY, 231, CAS_BAND);
  for (uint8_t i = 0; i < CAS_BULBS; i++) {
    int x, y; casBulbPos(i, &x, &y);
    uint8_t c = i % 3;
    if (casBulbLit(i, now)) {
      gfx->fillCircle(x, y, 7, CAS_BULB_DIM[c]);    // the glow
      gfx->fillCircle(x, y, 5, CAS_BULB_ON[c]);
      gfx->fillCircle(x - 1, y - 1, 2, 0xFFF6);     // the hot centre
    } else {
      gfx->fillCircle(x, y, 4, CAS_BULB_DIM[c]);
    }
  }
}
// A neon title: an orange glow under a yellow core.
static void casNeon(int x, int y, const char *t) {
  static const int8_t D[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
  for (auto &d : D) casText(x + d[0], y + d[1], t, 0xFC60, 2);
  casText(x, y, t, 0xFF00, 2);
}

static void casTable(const char *title) {
  gfx->fillScreen(RGB565_BLACK);
  casMarquee();
  gfx->fillCircle(CX, CY, CAS_FELT_R, CAS_FELT);
  gfx->drawCircle(CX, CY, CAS_FELT_R, CAS_GOLD);
  // the R in the felt, faint
  casMotif(CM_ROCKET, CX, CY + 20, 180, CAS_FELT_D, CAS_FELT_D);
  casNeon(CX, 26, title);
  char b[32];
  snprintf(b, sizeof(b), T(S_CAS_COINS_FMT), (unsigned)pet.coins);
  gfx->fillRoundRect(CX - 70, 46, 140, 20, 8, CAS_FELT_D);
  casText(CX, 52, b, UI_WHITE, 1);
}
static void casBetChip(bool dim) {
  int x, y, w, h; casBetBtn(&x, &y, &w, &h);
  char b[24]; snprintf(b, sizeof(b), T(S_CAS_BET_FMT), (unsigned)casBet());
  gfx->fillRoundRect(x, y, w, h, 20, dim ? CAS_FELT_D : CAS_RED);
  gfx->drawRoundRect(x, y, w, h, 20, UI_WHITE);
  casText(CX, y + h / 2 - 8, b, UI_WHITE, 2);
}
static void casBanner(const char *t, uint16_t col) {
  gfx->fillRoundRect(CX - 130, 226, 260, 32, 12, UI_INK);
  casText(CX, 234, t, col, 2);
}

// ---------------------------------------------------------------- lobby
static void renderCasLobby() {
  casTable(T(S_CAS_TITLE));
  casDrawDealer(CX - 32, 82, 4);
  const char *lab[CAS_LOBBY_BTNS] = { T(S_CAS_SLOTS), T(S_CAS_HL), T(S_CAS_BJ), T(S_CAS_ROULETTE), T(S_CAS_BUY), T(S_CAS_PRIZES) };
  for (uint8_t i = 0; i < CAS_LOBBY_BTNS; i++) {
    int x, y, w, h; casLobbyBtn(i, &x, &y, &w, &h);
    casButton(x, y, w, h, lab[i], i == 5 ? CAS_GOLD : UI_BG_DAY, false);
  }
}

// ---------------------------------------------------------------- slots
static void casDrawSym(uint8_t s, int cx, int cy) {
  switch (s) {
    case SL_SEVEN:  casText(cx, cy - 14, "7", CAS_RED, 4); break;
    case SL_ROCKET: gfx->fillCircle(cx, cy, 20, UI_INK); casMotif(CM_ROCKET, cx, cy, 26, CAS_RED, UI_INK); break;
    case SL_BAR:    gfx->fillRoundRect(cx - 24, cy - 10, 48, 20, 4, UI_INK); casText(cx, cy - 4, "BAR", UI_WHITE, 1); break;
    case SL_BALL:   casMotif(CM_BALL, cx, cy, 36, CAS_RED, UI_INK); break;
    case SL_BOLT:   casMotif(CM_BOLT, cx, cy, 38, 0xFE60, UI_INK); break;
    case SL_MOON:   casMotif(CM_MOON, cx, cy, 40, 0xFEA0, UI_WHITE); break;
    default:        // cherries
      gfx->drawLine(cx - 8, cy + 4, cx + 4, cy - 14, 0x3D66);
      gfx->drawLine(cx + 8, cy + 4, cx + 4, cy - 14, 0x3D66);
      gfx->fillCircle(cx - 8, cy + 8, 8, CAS_RED);
      gfx->fillCircle(cx + 8, cy + 8, 8, CAS_RED);
      break;
  }
}
#define SL_X0 (CX - 150)
#define SL_RW 96
#define SL_Y0 96
#define SL_H 156
static void renderCasSlots() {
  casTable(T(S_CAS_SLOTS));
  uint32_t now = millis();
  // the cabinet
  gfx->fillRoundRect(SL_X0 - 8, SL_Y0 - 8, 3 * SL_RW + 30, SL_H + 16, 14, CAS_RED);
  for (int r = 0; r < 3; r++) {
    int x = SL_X0 + r * (SL_RW + 7);
    gfx->fillRect(x, SL_Y0, SL_RW, SL_H, UI_WHITE);
    bool spinning = r >= slShown;
    // a stopped reel shows its stop in the middle; a spinning one scrolls
    int base = spinning ? (int)((now - slSpinAt) / 40) : (int)slStop[r] * 52;
    int pos = spinning ? base : base;
    for (int k = -1; k <= 1; k++) {
      int cell = spinning ? ((pos / 52) + k) : (slStop[r] + k);
      int off = spinning ? (pos % 52) : 0;
      uint8_t sym = SLOT_REELS[r][((cell % SLOT_REEL_LEN) + SLOT_REEL_LEN) % SLOT_REEL_LEN];
      int cy = SL_Y0 + SL_H / 2 + k * 52 + off;
      if (cy < SL_Y0 + 18 || cy > SL_Y0 + SL_H - 18) continue;
      casDrawSym(sym, x + SL_RW / 2, cy);
    }
  }
  gfx->drawFastHLine(SL_X0 - 6, SL_Y0 + SL_H / 2, 3 * SL_RW + 26, CAS_GOLD);   // the pay line
  if (slShown == 3 && slWon) {
    char b[24]; snprintf(b, sizeof(b), T(S_CAS_WIN_FMT), (unsigned)slWon);
    if ((now / 200) % 2) casBanner(b, CAS_GOLD);
  }
  casBetChip(slShown < 3);
  int x, y, w, h; casBtn(0, 1, &x, &y, &w, &h);
  casButton(x, y, w, h, slShown < 3 ? T(S_CAS_STOP) : T(S_CAS_SPIN), CAS_GOLD, false);
}
static void casSlotsSettle() {
  uint16_t m = slotPays(SLOT_REELS[0][slStop[0]], SLOT_REELS[1][slStop[1]], SLOT_REELS[2][slStop[2]]);
  slWon = (uint32_t)m * casBet();   // addCoins clamps at COIN_CAP
  casPay(slWon);
  if (slWon > casino.slotBest) { casino.slotBest = slWon; casinoSave(); }
  casSetMood(slWon > casBet() ? DM_GRUMBLE : slWon ? DM_IDLE : DM_LAUGH);
  sfxPlay(slWon ? SFX_TAP : SFX_DENY);
}
// called every frame: reels stop on their own if the player does not
static void casSlotsAuto() {
  if (slShown >= 3) return;
  uint32_t due = slSpinAt + 1200 + slShown * 500;
  if (millis() >= due) { slShown++; if (slShown == 3) casSlotsSettle(); }
}
static void casSlotsTap(int16_t px, int16_t py) {
  int x, y, w, h;
  casBetBtn(&x, &y, &w, &h);
  if (slShown == 3 && inBox(px, py, x, y, w, h)) { casBetI = (casBetI + 1) % 3; sfxPlay(SFX_TAP); return; }
  casBtn(0, 1, &x, &y, &w, &h);
  bool onReels = px > SL_X0 && px < SL_X0 + 3 * SL_RW + 14 && py > SL_Y0 && py < SL_Y0 + SL_H;
  if (!(inBox(px, py, x, y, w, h) || onReels)) return;
  if (slShown < 3) {                     // STOP the next reel (its stop was fixed at SPIN)
    slShown++;
    sfxPlay(SFX_TAP);
    if (slShown == 3) casSlotsSettle();
    return;
  }
  if (!casTakeBet(casBet())) return;
  slotSpin(slStop);                      // the outcome is decided NOW
  slShown = 0;
  slWon = 0;
  slSpinAt = millis();
  sfxPlay(SFX_TAP);
}

// ---------------------------------------------------------------- high / low
static void renderCasHl() {
  casTable(T(S_CAS_HL));
  casDrawDealer(CX - 120, 70, 3);
  char b[40];
  if (casHl.state == HL_IDLE) {
    casDrawBack(CX - 29, 120, CAS_CARD_W, CAS_CARD_H, casino.back);
    if (casLastWin) { snprintf(b, sizeof(b), T(S_CAS_WIN_FMT), (unsigned)casLastWin); casBanner(b, CAS_GOLD); }
    casBetChip(false);
    int x, y, w, h; casBtn(0, 1, &x, &y, &w, &h);
    casButton(x, y, w, h, T(S_CAS_DEAL), CAS_GOLD, false);
    return;
  }
  casDealCard(CX - 70, 120, casHl.card, true, casDealtAt);
  casDrawBack(CX + 12, 120, CAS_CARD_W, CAS_CARD_H, casino.back);
  snprintf(b, sizeof(b), T(S_CAS_POT_FMT), (unsigned)casHl.pot, (unsigned)casHl.streak);
  casText(CX, 216, b, UI_WHITE, 1);
  if (casHl.state == HL_LOST) {
    casBanner(T(S_CAS_LOSE), CAS_RED);
    casBetChip(false);
    int x, y, w, h; casBtn(0, 1, &x, &y, &w, &h);
    casButton(x, y, w, h, T(S_CAS_DEAL), CAS_GOLD, false);
    return;
  }
  const char *lab[3] = { T(S_CAS_HIGHER), T(S_CAS_LOWER), T(S_CAS_CASHOUT) };
  for (uint8_t i = 0; i < 3; i++) {
    int x, y, w, h; casBtn(i, 3, &x, &y, &w, &h);
    casButton(x, y, w, h, lab[i], i == 2 ? CAS_GOLD : UI_BG_DAY, false);
  }
}
static void casHlTap(int16_t px, int16_t py) {
  int x, y, w, h;
  if (casHl.state != HL_GUESS) {
    casBetBtn(&x, &y, &w, &h);
    if (inBox(px, py, x, y, w, h)) { casBetI = (casBetI + 1) % 3; sfxPlay(SFX_TAP); return; }
    casBtn(0, 1, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h) || !casTakeBet(casBet())) return;
    casHl.start(casBet());
    casLastWin = 0;
    casDealtAt = millis();
    casSetMood(DM_DEAL);
    sfxPlay(SFX_TAP);
    return;
  }
  for (uint8_t i = 0; i < 3; i++) {
    casBtn(i, 3, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h)) continue;
    if (i == 2) {
      casLastWin = casHl.cashOut();
      casPay(casLastWin);
      casinoSave();
      casSetMood(casLastWin > casBet() ? DM_GRUMBLE : DM_IDLE);
      sfxPlay(SFX_TAP);
      return;
    }
    bool right = casHl.guess(i == 0);
    casDealtAt = millis();
    casSetMood(right ? DM_GRUMBLE : DM_LAUGH);
    sfxPlay(right ? SFX_TAP : SFX_DENY);
    if (right) casinoSave();   // a new best streak
    return;
  }
}

// ---------------------------------------------------------------- blackjack
static void renderCasBj() {
  casTable(T(S_CAS_BJ));
  casDrawDealer(CX - 158, 76, 3);
  char b[32];
  bool live = casBj.state == BJ_PLAYER;
  if (casBj.state != BJ_IDLE) {
    // the dealer's hand: the hole card stays down until the hand is over
    for (uint8_t i = 0; i < casBj.dealerN && i < 6; i++)
      casDealCard(CX - 80 + i * 30, 62, casBj.dealer[i], !(live && i == 1), casDealtAt + i * 90);
    for (uint8_t i = 0; i < casBj.youN && i < 7; i++)
      casDealCard(CX - 100 + i * 30, 150, casBj.you[i], true, casDealtAt + i * 90);
    snprintf(b, sizeof(b), "%u", (unsigned)bjTotal(casBj.you, casBj.youN));
    gfx->fillCircle(CX + 150, 192, 18, UI_INK);
    casText(CX + 150, 185, b, UI_WHITE, 2);
    if (!live) {
      snprintf(b, sizeof(b), "%u", (unsigned)bjTotal(casBj.dealer, casBj.dealerN));
      gfx->fillCircle(CX + 150, 104, 18, UI_INK);
      casText(CX + 150, 97, b, UI_WHITE, 2);
    }
  }
  if (live) {
    const char *lab[3] = { T(S_CAS_HIT), T(S_CAS_STAND), T(S_CAS_DOUBLE) };
    for (uint8_t i = 0; i < 3; i++) {
      int x, y, w, h; casBtn(i, 3, &x, &y, &w, &h);
      bool dim = i == 2 && (!casBj.canDouble() || pet.coins < casBj.bet);
      casButton(x, y, w, h, lab[i], UI_BG_DAY, dim);
    }
    return;
  }
  if (casBj.state == BJ_DONE) {
    const char *r = T(S_CAS_LOSE); uint16_t col = CAS_RED;
    if (casBj.result == BJR_BLACKJACK) { r = T(S_CAS_BJWIN); col = CAS_GOLD; }
    else if (casBj.result == BJR_WIN) { snprintf(b, sizeof(b), T(S_CAS_WIN_FMT), (unsigned)casBj.payout()); r = b; col = CAS_GOLD; }
    else if (casBj.result == BJR_PUSH) { r = T(S_CAS_PUSH); col = UI_WHITE; }
    else if (casBj.result == BJR_BUST) r = T(S_CAS_BUST);
    gfx->fillRoundRect(CX - 130, 238, 260, 30, 12, UI_INK);   // under your hand, above the chip
    casText(CX, 246, r, col, 2);
  }
  casBetChip(false);
  int x, y, w, h; casBtn(0, 1, &x, &y, &w, &h);
  casButton(x, y, w, h, T(S_CAS_DEAL), CAS_GOLD, false);
}
static void casBjFinish() {
  uint16_t p = casBj.payout();
  casPay(p);
  if (casBj.result == BJR_WIN || casBj.result == BJR_BLACKJACK) { casino.bjWins++; casinoSave(); }
  casSetMood(p > casBj.bet ? DM_GRUMBLE : p == casBj.bet ? DM_IDLE : DM_LAUGH);
}
static void casBjTap(int16_t px, int16_t py) {
  int x, y, w, h;
  if (casBj.state != BJ_PLAYER) {
    casBetBtn(&x, &y, &w, &h);
    if (inBox(px, py, x, y, w, h)) { casBetI = (casBetI + 1) % 3; sfxPlay(SFX_TAP); return; }
    casBtn(0, 1, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h) || !casTakeBet(casBet())) return;
    casBj.deal(casBet());
    casDealtAt = millis();
    casSetMood(DM_DEAL);
    sfxPlay(SFX_TAP);
    if (casBj.state == BJ_DONE) casBjFinish();   // a natural on the deal
    return;
  }
  for (uint8_t i = 0; i < 3; i++) {
    casBtn(i, 3, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h)) continue;
    if (i == 0) { casBj.hit(); casDealtAt = millis() - (casBj.youN - 1) * 90; casSetMood(DM_DEAL); }
    else if (i == 1) casBj.stand();
    else {
      if (!casBj.canDouble() || !casTakeBet(casBj.bet)) { sfxPlay(SFX_DENY); return; }
      casBj.doubleDown();
    }
    sfxPlay(SFX_TAP);
    if (casBj.state == BJ_DONE) casBjFinish();
    return;
  }
}

// ---------------------------------------------------------------- buy coins
// The GBC counter's two offers: 50 coins for $1000, 500 for $10,000.
#define CAS_PACK_N 2
static const uint16_t CAS_PACKS[CAS_PACK_N] = { 50, 500 };
static void renderCasCoins() {
  casTable(T(S_CAS_BUY));
  casDrawDealer(CX - 40, 84, 4);
  char b[40];
  snprintf(b, sizeof(b), T(S_CAS_WALLET_FMT), (unsigned long)pet.wallet);
  casText(CX, 172, b, CAS_GOLD, 2);
  for (uint8_t i = 0; i < CAS_PACK_N; i++) {
    int x = CX - 150, y = 200 + i * 66, w = 300, h = CAS_BTN_H;
    snprintf(b, sizeof(b), T(S_CAS_PACK_FMT), (unsigned)CAS_PACKS[i], (unsigned)(CAS_PACKS[i] * COIN_DOLLARS));
    bool can = pet.wallet >= (uint32_t)CAS_PACKS[i] * COIN_DOLLARS && pet.coins < COIN_CAP;
    casButton(x, y, w, h, b, UI_BG_DAY, !can);
  }
}
void casCoinsBtn(uint8_t i, int *x, int *y, int *w, int *h) { *x = CX - 150; *y = 200 + i * 66; *w = 300; *h = CAS_BTN_H; }
static void casCoinsTap(int16_t px, int16_t py) {
  for (uint8_t i = 0; i < CAS_PACK_N; i++) {
    int x, y, w, h; casCoinsBtn(i, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h)) continue;
    bool ok = pet.buyCoins(CAS_PACKS[i], COIN_DOLLARS);
    sfxPlay(ok ? SFX_TAP : SFX_DENY);
    if (ok) casSetMood(DM_DEAL); else casSetMood(DM_LAUGH);
    return;
  }
}

// ---------------------------------------------------------------- roulette
// A single-zero wheel (casino.cpp has the rules and their exact 36/37). The
// result is drawn at SPIN; the wheel and the ball only travel to it.
#define RL_CY 148
#define RL_R 74
#define RL_SPIN_MS 3600
static uint8_t rlBet = RB_RED, rlArg = 0;     // the dozen 0..2, or the number 0..36
static uint8_t rlResult = 0;
static bool rlSpinning = false, rlHasResult = false;
static uint32_t rlSpinAt = 0;
static uint16_t rlStake = 0, rlWon = 0;
static float rlRest = -90.0f;                 // wheel angle at rest (deg)
static const float RL_STEP = 360.0f / ROULETTE_POCKETS;

// The bet board: the three colours and odd/even on top (5 cells), the
// ranges, a dozen and a number below (4 cells), then BET and SPIN. RL_ORDER
// is which bet each cell is; cell i always means RL_ORDER[i].
#define RL_CELLS 9
static const uint8_t RL_ORDER[RL_CELLS] = { RB_RED, RB_BLACK, RB_GREEN, RB_ODD, RB_EVEN, RB_LOW, RB_HIGH, RB_DOZEN, RB_NUMBER };
void casRlCell(uint8_t i, int *x, int *y, int *w, int *h) {
  *h = 40;
  if (i < 5) { *w = 70; *x = CX - 182 + i * 73; *y = 238; }
  else { *w = 86; *x = CX - 178 + (i - 5) * 90; *y = 284; }
}
uint8_t casRlCellBet(uint8_t i) { return i < RL_CELLS ? RL_ORDER[i] : RB_RED; }
void casRlBtn(uint8_t i, int *x, int *y, int *w, int *h) { *w = 140; *h = 50; *x = i ? CX + 8 : CX - 148; *y = 336; }

static float rlFinal() { return -90.0f - rouletteWheelIndex(rlResult) * RL_STEP; }
static float rlWheelAngle() {
  if (!rlSpinning) return rlRest;
  float t = (float)(millis() - rlSpinAt) / RL_SPIN_MS;
  if (t > 1) t = 1;
  float d = fmodf(rlRest - rlFinal(), 360.0f);
  if (d < 0) d += 360.0f;
  float e = 1 - (1 - t) * (1 - t) * (1 - t);
  return rlRest - (d + 720.0f) * e;             // spins one way and slows to a stop
}
static void rlPoint(float deg, int r, int *x, int *y) {
  float a = deg * 0.01745329f;
  *x = CX + (int)(r * cosf(a));
  *y = RL_CY + (int)(r * sinf(a));
}
#define RL_GREEN 0x0480
static uint16_t rlColor(uint8_t n) { return n == 0 ? RL_GREEN : rouletteRed(n) ? CAS_RED : 0x18C3; }

static void rlDrawWheel() {
  gfx->fillCircle(CX, RL_CY, RL_R + 6, 0x8A00);             // the wooden bowl
  gfx->drawCircle(CX, RL_CY, RL_R + 6, CAS_GOLD);
  float w = rlWheelAngle();
  for (uint8_t i = 0; i < ROULETTE_POCKETS; i++) {
    float c = w + i * RL_STEP;
    int x0, y0, x1, y1;
    rlPoint(c - RL_STEP / 2, RL_R, &x0, &y0);
    rlPoint(c + RL_STEP / 2, RL_R, &x1, &y1);
    gfx->fillTriangle(CX, RL_CY, x0, y0, x1, y1, rlColor(ROULETTE_WHEEL[i]));
    int fx, fy; rlPoint(c - RL_STEP / 2, RL_R, &fx, &fy);
    gfx->drawLine(CX, RL_CY, fx, fy, CAS_GOLD);              // the frets
  }
  gfx->fillCircle(CX, RL_CY, RL_R - 24, 0x6180);              // the cone
  gfx->drawCircle(CX, RL_CY, RL_R - 24, CAS_GOLD);
  casMotif(CM_BALL, CX, RL_CY, 52, CAS_RED, UI_INK);           // a Poke Ball hub
  // the pointer, fixed at the top
  gfx->fillTriangle(CX - 8, RL_CY - RL_R - 14, CX + 8, RL_CY - RL_R - 14, CX, RL_CY - RL_R + 2, CAS_GOLD);
  // the ball: round the rim the other way, slowing, dropping into the pocket under the pointer
  if (rlSpinning || rlHasResult) {
    float t = rlSpinning ? (float)(millis() - rlSpinAt) / RL_SPIN_MS : 1.0f;
    if (t > 1) t = 1;
    float ang = -90.0f + 1440.0f * (1 - (1 - t) * (1 - t));
    int r = t < 0.75f ? RL_R - 8 : RL_R - 8 - (int)((t - 0.75f) * 4 * 6);
    int bx, by; rlPoint(ang, r, &bx, &by);
    gfx->fillCircle(bx, by, 5, UI_WHITE);
    gfx->drawCircle(bx, by, 5, UI_INK);
  }
}

static void rlCellLabel(uint8_t i, char *b, size_t n) {
  switch (i) {
    case RB_RED: snprintf(b, n, "%s", T(S_CAS_RED)); break;
    case RB_BLACK: snprintf(b, n, "%s", T(S_CAS_BLACK)); break;
    case RB_ODD: snprintf(b, n, "%s", T(S_CAS_ODD)); break;
    case RB_EVEN: snprintf(b, n, "%s", T(S_CAS_EVEN)); break;
    case RB_GREEN: snprintf(b, n, "%s", T(S_CAS_GREEN)); break;
    case RB_LOW: snprintf(b, n, "1-18"); break;
    case RB_HIGH: snprintf(b, n, "19-36"); break;
    case RB_DOZEN: snprintf(b, n, "%u-%u", (unsigned)(rlBet == RB_DOZEN ? rlArg : 0) * 12 + 1, (unsigned)((rlBet == RB_DOZEN ? rlArg : 0) + 1) * 12); break;
    default: snprintf(b, n, "<%u>", (unsigned)(rlBet == RB_NUMBER ? rlArg : 17)); break;
  }
}

static void renderCasRoulette() {
  casTable(T(S_CAS_ROULETTE));
  rlDrawWheel();
  char b[32];
  if (rlHasResult && !rlSpinning) {
    snprintf(b, sizeof(b), "%u", (unsigned)rlResult);
    gfx->fillCircle(CX + RL_R + 34, RL_CY - 40, 20, rlColor(rlResult));
    gfx->drawCircle(CX + RL_R + 34, RL_CY - 40, 20, CAS_GOLD);
    casText(CX + RL_R + 34, RL_CY - 48, b, UI_WHITE, 2);
    if (rlWon) snprintf(b, sizeof(b), T(S_CAS_WIN_FMT), (unsigned)rlWon);
    else snprintf(b, sizeof(b), "%s", T(S_CAS_LOSE));
    casText(CX, 222, b, rlWon ? CAS_GOLD : UI_WHITE, 1);
  }
  casDrawDealer(CX - 178, 76, 3);
  for (uint8_t c = 0; c < RL_CELLS; c++) {
    uint8_t i = RL_ORDER[c];
    int x, y, w, h; casRlCell(c, &x, &y, &w, &h);
    bool coloured = i == RB_RED || i == RB_BLACK || i == RB_GREEN;
    uint16_t fill = i == RB_RED ? CAS_RED : i == RB_BLACK ? 0x18C3 : i == RB_GREEN ? RL_GREEN : UI_BG_DAY;
    uint16_t ink = coloured ? UI_WHITE : UI_INK;
    gfx->fillRoundRect(x, y, w, h, 10, fill);
    gfx->drawRoundRect(x, y, w, h, 10, UI_INK);
    if (i == rlBet) { gfx->drawRoundRect(x - 2, y - 2, w + 4, h + 4, 12, CAS_GOLD); gfx->drawRoundRect(x - 3, y - 3, w + 6, h + 6, 13, CAS_GOLD); }
    rlCellLabel(i, b, sizeof(b));
    casText(x + w / 2, y + h / 2 - 4, b, ink, 1);
  }
  int x, y, w, h;
  casRlBtn(0, &x, &y, &w, &h);
  snprintf(b, sizeof(b), T(S_CAS_BET_FMT), (unsigned)casBet());
  gfx->fillRoundRect(x, y, w, h, 20, rlSpinning ? CAS_FELT_D : CAS_RED);
  gfx->drawRoundRect(x, y, w, h, 20, UI_WHITE);
  casText(x + w / 2, y + h / 2 - 8, b, UI_WHITE, 2);
  casRlBtn(1, &x, &y, &w, &h);
  casButton(x, y, w, h, T(S_CAS_SPIN), CAS_GOLD, rlSpinning);
}

static void casRouletteSettle() {
  rlSpinning = false;
  rlHasResult = true;
  rlRest = rlFinal();
  rlWon = (uint16_t)(rlStake * roulettePays(rlBet, rlArg, rlResult));
  casPay(rlWon);
  casSetMood(rlWon > rlStake ? DM_GRUMBLE : rlWon ? DM_IDLE : DM_LAUGH);
  sfxPlay(rlWon ? SFX_MEDAL : SFX_DENY);
}
static void casRouletteAuto() {
  if (rlSpinning && millis() - rlSpinAt >= RL_SPIN_MS) casRouletteSettle();
}
static void casRouletteSettleNow() { if (rlSpinning) casRouletteSettle(); }

static void casRouletteTap(int16_t px, int16_t py) {
  if (rlSpinning) return;                       // no more bets
  int x, y, w, h;
  for (uint8_t c = 0; c < RL_CELLS; c++) {
    uint8_t i = RL_ORDER[c];
    casRlCell(c, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h)) continue;
    sfxPlay(SFX_TAP);
    if (i == RB_DOZEN && rlBet == RB_DOZEN) { rlArg = (uint8_t)((rlArg + 1) % 3); return; }
    if (i == RB_NUMBER && rlBet == RB_NUMBER) {  // the left half steps down, the right half up
      rlArg = (uint8_t)(px < x + w / 2 ? (rlArg + ROULETTE_POCKETS - 1) % ROULETTE_POCKETS : (rlArg + 1) % ROULETTE_POCKETS);
      return;
    }
    if (i != rlBet) rlArg = i == RB_NUMBER ? 17 : 0;
    rlBet = i;
    return;
  }
  casRlBtn(0, &x, &y, &w, &h);
  if (inBox(px, py, x, y, w, h)) { casBetI = (casBetI + 1) % 3; sfxPlay(SFX_TAP); return; }
  casRlBtn(1, &x, &y, &w, &h);
  if (!inBox(px, py, x, y, w, h) || !casTakeBet(casBet())) return;
  rlStake = casBet();
  rlResult = rouletteSpin();                    // decided NOW; the wheel only travels to it
  rlSpinning = true;
  rlHasResult = false;
  rlWon = 0;
  rlSpinAt = millis();
  casSetMood(DM_DEAL);
  sfxPlay(SFX_TAP);
}

// ---------------------------------------------------------------- the prize shop
static void casDrawBgSwatch(int x, int y, int w, int h, uint8_t bg);
// A creature purchase's outcome, shown for a moment over the grid.
uint16_t casMonNote = STR_COUNT;     // STR_COUNT = nothing to say
uint32_t casMonNoteUntil = 0;
// A prize creature's tile picture: the still frame baked into the firmware
// (armored_art.h), so the counter shows it even with no SD card.
static void casDrawPrizeMon(int x, int y, uint16_t i) {
  if (i >= CASINO_MON_COUNT || CASINO_MONS[i].skin != SKIN_ARMORED) return;
  for (int r = 0; r < ARMORED_H; r++)
    for (int c = 0; c < ARMORED_W; c++) {
      uint8_t v = ARMORED_IDX[r * ARMORED_W + c];
      if (v == 0xFF) continue;
      gfx->fillRect(x + c * 2, y + r * 2, 2, 2, ARMORED_PAL[v]);
    }
}
static void renderCasShop() {
  casTable(T(S_CAS_PRIZES));
  const char *tabs[PK_KINDS] = { T(S_CAS_BACKS), T(S_CAS_AVATARS), T(S_CAS_CARDS), T(S_CAS_MONS) };
  for (uint8_t i = 0; i < PK_KINDS; i++) {
    int x, y, w, h; casTab(i, &x, &y, &w, &h);
    gfx->fillRoundRect(x, y, w, h, 12, i == casShopKind ? CAS_GOLD : CAS_FELT_D);
    gfx->drawRoundRect(x, y, w, h, 12, UI_WHITE);
    casText(x + w / 2, y + h / 2 - 4, tabs[i], i == casShopKind ? UI_INK : UI_WHITE, 1);
  }
  if (casShopPage >= casShopPages()) casShopPage = 0;
  for (uint8_t t = 0; t < CAS_SHOP_PER; t++) {
    int li = casinoShopAt(casShopKind, (uint16_t)(casShopPage * CAS_SHOP_PER + t));
    if (li < 0) break;
    uint16_t i = (uint16_t)li;
    int x, y, w, h; casTile(t, &x, &y, &w, &h);
    bool own = casinoOwns(casShopKind, i);
    bool eq = (casShopKind == PK_BACK && casino.back == i) || (casShopKind == PK_CARDBG && casino.bg == i) ||
              (casShopKind == PK_AVATAR && pet.avatar == SHOP_AVATAR_FIRST + i);
    bool armed = casArmed == (int16_t)i;
    gfx->fillRoundRect(x, y, w, h, 10, armed ? CAS_GOLD : UI_BG_DAY);
    gfx->drawRoundRect(x, y, w, h, 10, eq ? CAS_RED : UI_INK);
    if (eq) gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 9, CAS_RED);
    // the preview, left
    if (casShopKind == PK_BACK) casDrawBack(x + 6, y + 6, 44, 64, i);
    else if (casShopKind == PK_AVATAR) drawAvatar(SHOP_AVATAR_FIRST + i, x + 6, y + 10, 3);
    else if (casShopKind == PK_MON) casDrawPrizeMon(x + 4, y + 10, i);
    else casDrawBgSwatch(x + 6, y + 8, 50, 62, i);
    // the name and the price / state, right
    const char *nm = casinoPrizeName(casShopKind, i);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(x + 60, y + 14);
    gfx->print(nm);
    if (casShopKind == PK_MON && CASINO_MONS[i].skin == SKIN_ARMORED) {
      gfx->setTextColor(CAS_PURPLE);
      gfx->setCursor(x + 60, y + 28);
      gfx->print(T(S_CAS_ARMORED));
    }
    const char *st = eq ? T(S_CAS_EQUIPPED) : own ? T(S_CAS_OWNED) : armed ? T(S_CAS_AGAIN) : nullptr;
    gfx->setCursor(x + 60, y + 48);
    if (st) { gfx->setTextColor(eq ? CAS_RED : 0x3D66); gfx->print(st); }
    else {
      const uint32_t price = casinoPrizePrice(casShopKind, i);
      gfx->setTextColor(pet.coins >= price ? UI_INK : CAS_RED);
      gfx->setTextSize(price > 99999 ? 1 : 2);   // a seven-digit price at size 2 leaves the tile
      char pr[12]; snprintf(pr, sizeof(pr), "%lu", (unsigned long)price);
      gfx->print(pr);
    }
  }
  if (casMonNote < STR_COUNT && (int32_t)(casMonNoteUntil - millis()) > 0)
    casBanner(T((StrId)casMonNote), casMonNote == S_CAS_TO_BOX ? CAS_GOLD : CAS_RED);
}
static void casShopTap(int16_t px, int16_t py) {
  for (uint8_t i = 0; i < PK_KINDS; i++) {
    int x, y, w, h; casTab(i, &x, &y, &w, &h);
    if (inBox(px, py, x, y, w, h)) { casShopKind = i; casShopPage = 0; casArmed = -1; sfxPlay(SFX_TAP); return; }
  }
  for (uint8_t t = 0; t < CAS_SHOP_PER; t++) {
    int li = casinoShopAt(casShopKind, (uint16_t)(casShopPage * CAS_SHOP_PER + t));
    if (li < 0) break;
    uint16_t i = (uint16_t)li;
    int x, y, w, h; casTile(t, &x, &y, &w, &h);
    if (!inBox(px, py, x, y, w, h)) continue;
    if (casinoOwns(casShopKind, i)) {    // owned: equip it
      if (casShopKind == PK_AVATAR) { pet.avatar = SHOP_AVATAR_FIRST + i; pet.saveNow(); }
      else casinoEquip(casShopKind, i);
      casArmed = -1;
      sfxPlay(SFX_TAP);
      return;
    }
    if (casArmed != (int16_t)i) { casArmed = i; sfxPlay(SFX_TAP); return; }   // 9999 coins: tap twice
    if (casShopKind == PK_MON) {        // a creature: to the PC box, never "owned"
      CasMonResult r = casinoBuyMon(i, pet.coins, casSpendCoins, casPay, nullptr);
      casArmed = -1;
      casMonNote = r == CMB_OK ? S_CAS_TO_BOX : r == CMB_BOX_FULL ? S_CAS_BOX_FULL : STR_COUNT;
      casMonNoteUntil = millis() + 2500;
      casSetMood(r == CMB_OK ? DM_GRUMBLE : DM_LAUGH);
      sfxPlay(r == CMB_OK ? SFX_TAP : SFX_DENY);
      return;
    }
    uint32_t coins = pet.coins;
    bool ok = casinoBuy(casShopKind, i, coins, casSpendCoins);
    casArmed = -1;
    if (ok) {
      if (casShopKind == PK_AVATAR) { pet.avatar = SHOP_AVATAR_FIRST + i; pet.saveNow(); }
      else casinoEquip(casShopKind, i);
      casSetMood(DM_GRUMBLE);
      sfxPlay(SFX_TAP);
    } else {
      casSetMood(DM_LAUGH);
      sfxPlay(SFX_DENY);
    }
    return;
  }
}

// ---------------------------------------------------------------- trainer-card backgrounds
// One function answers "what colour is this pixel" for every pattern, so the
// shop's swatch, the trainer card and the test all draw from the same rule.
uint16_t cardBgPixel(uint8_t bg, int x, int y, uint32_t t) {
  const CardBgDef &d = CARD_BGS[bg < CARD_BG_COUNT ? bg : 0];
  switch (d.pattern) {
    case BG_PLAIN: return d.c0;
    case BG_GRADIENT: return mix565(d.c0, d.c1, (uint8_t)(y * 255 / 466));
    case BG_STRIPES: return (((x + y) / 26) & 1) ? d.c0 : d.c1;
    case BG_DOTS: {
      int ox = (y / 40) & 1 ? 20 : 0;
      int dx = ((x + ox) % 40) - 20, dy = (y % 40) - 20;
      return dx * dx + dy * dy < 81 ? d.c1 : d.c0;
    }
    case BG_CHECKER: return ((x / 33 + y / 33) & 1) ? d.c0 : d.c1;
    case BG_WAVES: {
      int v = y + isin8((uint8_t)(x * 2 + t / 12)) / 12;
      int m = v % 36;
      return m < 3 ? d.c2 : m < 16 ? d.c1 : d.c0;
    }
    case BG_STARS: {
      uint16_t sky = mix565(d.c1, d.c0, (uint8_t)(y * 255 / 466));
      uint32_t h = hash2(x / 6, y / 6);
      if ((h % 61) == 0 && (x % 6) == 3 && (y % 6) == 3) return ((h >> 8) + t / 300) % 4 ? d.c2 : sky;
      return sky;
    }
    case BG_FLAMES: {
      int hgt = 110 + isin8((uint8_t)(x * 3)) / 3 + isin8((uint8_t)(x * 7 + 40)) / 5;
      if (y > 466 - hgt) return y > 466 - hgt / 2 ? d.c2 : d.c1;
      return d.c0;
    }
    case BG_LEAVES: {
      int tx = x / 48, ty = y / 48;
      int cx = tx * 48 + 24 + (int)(hash2(tx, ty) % 11) - 5, cy = ty * 48 + 24;
      int u = (x - cx) + (y - cy), v = (x - cx) - (y - cy);
      if (u * u / 4 + v * v < 120) return u == v || u == v + 1 ? d.c2 : d.c1;
      return d.c0;
    }
    case BG_BOLTS: {
      int col = x % 80, zz = ((y / 20) & 1) ? (y % 20) : 20 - (y % 20);
      int c = 30 + zz;
      return (col > c - 5 && col < c + 5) ? d.c1 : (col > c - 7 && col < c + 7) ? d.c2 : d.c0;
    }
    case BG_SNOW: {
      uint16_t sky = mix565(d.c0, d.c1, (uint8_t)(y * 255 / 466));
      int tx = x / 30, ty = y / 30;
      uint32_t h = hash2(tx, ty);
      int cx = tx * 30 + (int)(h % 20) + 5, cy = ty * 30 + (int)((h >> 5) % 20) + 5;
      int dx = x - cx, dy = y - cy;
      if ((dx == 0 && dy > -4 && dy < 4) || (dy == 0 && dx > -4 && dx < 4) || (dx == dy && dx > -3 && dx < 3)) return d.c2;
      return sky;
    }
    case BG_BUBBLES: {
      int tx = x / 44, ty = y / 44;
      uint32_t h = hash2(tx, ty);
      int r = 6 + (int)(h % 12);
      int cx = tx * 44 + 22, cy = ty * 44 + 22;
      int dd = (x - cx) * (x - cx) + (y - cy) * (y - cy);
      if (dd < r * r && dd > (r - 3) * (r - 3)) return d.c2;
      return mix565(d.c0, d.c1, (uint8_t)(y * 255 / 466));
    }
    case BG_HEX: {
      int row = y / 30, ox = (row & 1) ? 18 : 0;
      int dx = ((x + ox) % 36) - 18, dy = (y % 30) - 15;
      int dd = dx * dx + dy * dy;
      return dd < 150 ? d.c1 : dd < 200 ? d.c2 : d.c0;
    }
    case BG_ROCKET: {
      // a 5x7 R tiled every 60 px, 6 px a dot
      static const uint8_t R[7] = { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
      int tx = (x % 60) - 15, ty = (y % 60) - 9;
      bool alt = ((x / 60 + y / 60) & 1);
      if (tx >= 0 && tx < 30 && ty >= 0 && ty < 42 && (R[ty / 6] >> (4 - tx / 6)) & 1) return d.c2;
      return alt ? d.c0 : d.c1;
    }
    case BG_BALLS: {
      int cx = (x % 56) - 28, cy = (y % 56) - 28;
      int dd = cx * cx + cy * cy;
      if (dd > 400) return d.c0;
      if (dd < 25) return UI_WHITE;
      if (dd < 49 || (cy > -2 && cy < 2)) return d.c2;
      return cy < 0 ? d.c1 : UI_WHITE;
    }
    case BG_HILLS: {
      int h1 = 300 + isin8((uint8_t)(x * 2)) / 5, h2 = 360 + isin8((uint8_t)(x * 3 + 90)) / 6;
      if (y > h2) return d.c1;
      if (y > h1) return mix565(d.c1, d.c2, 90);
      return mix565(d.c0, d.c2, (uint8_t)(y * 120 / 466));
    }
    case BG_SUNSET: {
      int dx = x - 233, dy = y - 300;
      if (dx * dx + dy * dy < 70 * 70 && y < 300) return d.c2;
      if (y > 300) return mix565(d.c1, 0, (uint8_t)((y - 300) * 160 / 166));
      return mix565(d.c0, d.c1, (uint8_t)(y * 255 / 300));
    }
    case BG_GRID: return (x % 16 == 0 || y % 16 == 0) ? d.c1 : d.c0;

    // ---- animated: t moves them
    case BG_ANIM_FIELD: {
      int cloudX = (int)((t / 40) % 600) - 100;
      int dx = x - cloudX, dy = y - 110;
      if (dx * dx / 4 + dy * dy < 400 || (dx - 30) * (dx - 30) / 4 + (dy + 8) * (dy + 8) < 300) return UI_WHITE;
      int sh = (int)(t / 60);
      int h1 = 290 + isin8((uint8_t)(x + sh)) / 5, h2 = 350 + isin8((uint8_t)(x * 2 + sh * 2 + 70)) / 6;
      if (y > h2) return d.c1;
      if (y > h1) return mix565(d.c1, UI_WHITE, 70);
      return mix565(d.c0, UI_WHITE, (uint8_t)(y * 100 / 300));
    }
    case BG_ANIM_FOREST: {
      int sh = (int)(t / 30);
      int tx = (x + sh) % 70, ti = (x + sh) / 70;
      int top = 180 + (int)(hash2(ti, 1) % 60);
      int half = (y - top) / 3;
      if (y > top && abs(tx - 35) < half) return ((y / 20) & 1) ? d.c1 : mix565(d.c1, d.c2, 60);
      int tx2 = (x + sh / 2) % 50, ti2 = (x + sh / 2) / 50;
      int top2 = 130 + (int)(hash2(ti2, 2) % 50);
      if (y > top2 && abs(tx2 - 25) < (y - top2) / 3) return mix565(d.c1, d.c0, 140);
      return mix565(d.c2, UI_WHITE, (uint8_t)(150 - y * 150 / 466));
    }
    case BG_ANIM_BEACH: {
      int sea = 250 + isin8((uint8_t)(x + t / 8)) / 16;
      int shore = 340 + isin8((uint8_t)(x / 2 + t / 25)) / 10;
      if (y > shore) return d.c1;
      if (y > shore - 6) return UI_WHITE;
      if (y > sea) return (((y + isin8((uint8_t)(x * 3 + t / 6)) / 20) % 18) < 3) ? mix565(d.c0, UI_WHITE, 140) : d.c0;
      int dx = x - 330, dy = y - 110;
      if (dx * dx + dy * dy < 900) return 0xFFE0;
      return mix565(0x5D7F, UI_WHITE, (uint8_t)(y * 200 / 250));
    }
    case BG_ANIM_CAVE: {
      int top = 60 + isin8((uint8_t)(x * 5)) / 4 + ((x % 37) < 6 ? 50 - (x % 37) * 8 : 0);
      if (y < top) return 0x0841;
      uint32_t h = hash2(x / 5, y / 5);
      if ((h % 97) == 0 && ((h >> 9) + t / 200) % 3 == 0) return d.c2;   // glinting crystals
      int floor = 400 + isin8((uint8_t)(x * 2)) / 8;
      if (y > floor) return d.c1;
      // a torch's light swaying across the walls
      int torch = 233 + isin8((uint8_t)(t / 24)) / 2;
      int dd = abs(x - torch);
      return mix565(d.c0, 0, (uint8_t)(dd > 233 ? 230 : dd * 230 / 233));
    }
    case BG_ANIM_MANSION: {
      int dx = x - 120, dy = y - 100;
      if (dx * dx + dy * dy < 1600) return 0xFFDF;               // the moon
      // the house: a block with a roof and windows that flicker
      if (y > 220 && x > 120 && x < 350) {
        int wx = (x - 140) % 50, wy = (y - 250) % 60;
        if (x > 140 && x < 340 && y > 250 && y < 430 && wx < 24 && wy < 30) {
          uint32_t h = hash2((x - 140) / 50, (y - 250) / 60);
          return ((h + t / 700) % 5) ? d.c2 : 0x2104;
        }
        return d.c1;
      }
      if (y > 150 && y <= 220 && abs(x - 235) < (y - 150) * 165 / 70) return mix565(d.c1, 0, 80);
      // a bat or two crossing the sky
      int bx = (int)((t / 25) % 520) - 30, by = 160 + isin8((uint8_t)(t / 10)) / 6;
      if (abs(x - bx) < 9 && abs(y - by) < 3 - abs(x - bx) / 4 + 2) return 0x0000;
      return mix565(d.c0, 0, (uint8_t)(y * 100 / 466));
    }
    case BG_ANIM_AURORA: {
      int w1 = 140 + isin8((uint8_t)(x + t / 15)) / 3, w2 = 200 + isin8((uint8_t)(x * 2 - t / 20)) / 4;
      if (y > w1 && y < w1 + 70) return mix565(d.c1, d.c0, (uint8_t)((y - w1) * 255 / 70));
      if (y > w2 && y < w2 + 50) return mix565(d.c2, d.c0, (uint8_t)((y - w2) * 255 / 50));
      uint32_t h = hash2(x / 4, y / 4);
      if ((h % 211) == 0 && (x % 4) == 1 && (y % 4) == 1) return UI_WHITE;
      return d.c0;
    }
  }
  return d.c0;
}

// The shop's swatch: the same function, sampled into a small box.
static void casDrawBgSwatch(int x, int y, int w, int h, uint8_t bg) {
  uint32_t t = millis();
  for (int yy = 0; yy < h; yy += 2)
    for (int xx = 0; xx < w; xx += 2) {
      // sample the panel at the matching point of a 466 x 466 card
      uint16_t c = cardBgPixel(bg, xx * 466 / w, yy * 466 / h, t);
      gfx->fillRect(x + xx, y + yy, 2, 2, c);
    }
  gfx->drawRect(x, y, w, h, UI_INK);
}

// The trainer card: the background at full strength in the rim, softened
// toward the card's cream inside so every page stays readable on all of them.
#define CARD_RIM_IN 198
void drawCardBackground(uint8_t bg, uint32_t t) {
  if (bg == 0 || bg >= CARD_BG_COUNT) { gfx->fillCircle(CX, CY, 231, UI_BG_DAY); return; }
  const int R = 231, RI = CARD_RIM_IN;
  for (int y = CY - R; y <= CY + R; y += 2) {
    int dy = y - CY;
    int ho = (int)sqrtf((float)(R * R - dy * dy));
    int hi = abs(dy) < RI ? (int)sqrtf((float)(RI * RI - dy * dy)) : -1;
    int runX = CX - ho, runC = -1;
    for (int x = CX - ho; x <= CX + ho + 1; x += 2) {
      int c = -1;
      if (x <= CX + ho) {
        uint16_t p = cardBgPixel(bg, x, y, t);
        bool inner = hi >= 0 && x >= CX - hi && x <= CX + hi;
        c = inner ? mix565(p, UI_BG_DAY, 190) : p;
      }
      if (c != runC) {
        if (runC >= 0) gfx->fillRect(runX, y, x - runX, 2, (uint16_t)runC);
        runX = x;
        runC = c;
      }
    }
  }
  gfx->drawCircle(CX, CY, RI, mix565(CARD_BGS[bg].c1, UI_INK, 80));
}

// ---------------------------------------------------------------- the door, and the frame loop
static int8_t casSpinHalf = -1;   // half-turns completed, for the rising chime

// How long the finger has been on ARCADE (0 = not held).
uint32_t casinoHeldMs() { return casHoldArmed ? millis() - casHoldAt : 0; }
// The card's turn at a given hold time, in radians. Nothing until the press
// can no longer be a tap, then an ease-in -- a slow start that builds -- to an
// ODD number of half-turns, so it comes to rest showing GAME CORNER.
float casinoSpinAngle(uint32_t held) {
  if (held <= CASINO_SPIN_START_MS) return 0;
  const float END = CASINO_SPIN_HALF_TURNS * 3.14159265f;
  if (held >= CASINO_REVEAL_MS) return END;
  float t = (float)(held - CASINO_SPIN_START_MS) / (float)(CASINO_REVEAL_MS - CASINO_SPIN_START_MS);
  return END * t * t;
}
bool casinoSpinning() { return casinoHeldMs() > CASINO_SPIN_START_MS; }
// Which face is toward the player right now: true = GAME CORNER.
bool casinoSpinShowsCorner() { return cosf(casinoSpinAngle(casinoHeldMs())) < 0; }

void casinoTouchPeek(bool pressed, int16_t x, int16_t y, bool onArcadeRow) {
  if (!pressed || !onArcadeRow) { casHoldArmed = false; casSpinHalf = -1; return; }
  if (!casHoldArmed) { casHoldArmed = true; casHoldAt = millis(); casSpinHalf = -1; return; }
  uint32_t held = millis() - casHoldAt;
  // a rising note each half-turn: the suspense
  int8_t half = (int8_t)(casinoSpinAngle(held) / 3.14159265f);
  if (held > CASINO_SPIN_START_MS && half > casSpinHalf) {
    casSpinHalf = half;
    uint8_t step = (uint8_t)(half * (SFX_RISE_STEPS - 1) / CASINO_SPIN_HALF_TURNS);
    sfxRise(step);
  }
  if (held >= CASINO_HOLD_MS) {
    casHoldArmed = false;
    casSpinHalf = -1;
    casinoOpenNow();
  }
  (void)x; (void)y;
}
bool casinoHolding() { return casHoldArmed; }

// The ARCADE row, turning. Drawn by renderTrain() in place of the plain row
// for as long as casinoSpinning(). The card narrows to its edge and widens
// again (|cos| of the angle) and lifts a little as it turns.
void casinoDrawSpinRow(int x, int y, int w, int h, const char *arcadeLabel) {
  uint32_t held = casinoHeldMs();
  float a = casinoSpinAngle(held);
  float c = cosf(a);
  int cw = (int)(w * fabsf(c));
  if (cw < 4) cw = 4;
  int lift = (int)(6 * fabsf(sinf(a)));
  int cx = x + w / 2, rx = cx - cw / 2, ry = y - lift;
  bool corner = c < 0;
  // its shadow stays on the panel while the card lifts
  gfx->fillRoundRect(cx - cw / 2 + 3, y + 4, cw, h, 12, 0xC618);
  if (!corner) {
    gfx->fillRoundRect(rx, ry, cw, h, 12, C565(0xff, 0xf1, 0xc4));
    gfx->drawRoundRect(rx, ry, cw, h, 12, UI_INK);
    if (cw > (int)strlen(arcadeLabel) * 12 + 16) casText(cx, ry + h / 2 - 8, arcadeLabel, UI_INK, 2);
  } else {
    // Team Rocket's side: black, a red R, and the name of what is behind it.
    gfx->fillRoundRect(rx, ry, cw, h, 12, 0x0841);
    for (int sx = rx + 10; sx + 7 < rx + cw - 10; sx += 14)   // hazard stripes along the bottom
      gfx->fillRect(sx, ry + h - 8, 7, 3, CAS_RED);
    gfx->drawRoundRect(rx, ry, cw, h, 12, CAS_RED);
    if (cw > 12) gfx->drawRoundRect(rx + 2, ry + 2, cw - 4, h - 4, 10, 0x5000);
    if (cw > 44) {
      int ex = cw > w - 8 ? rx + 30 : cx;                     // the R, alone while it is narrow
      gfx->fillCircle(ex, ry + h / 2, 21, UI_WHITE);
      gfx->drawCircle(ex, ry + h / 2, 21, CAS_RED);
      casMotif(CM_ROCKET, ex, ry + h / 2, 30, CAS_RED, CAS_RED);
    }
    if (cw > w - 8) {                                         // fully round: the name
      casText(cx + 22, ry + 12, T(S_CAS_TITLE), CAS_GOLD, 2);
      casText(cx + 22, ry + 34, T(S_CAS_ROCKET), CAS_RED, 1);
    }
  }
  // revealed: it glitters until the door opens
  if (held >= CASINO_REVEAL_MS) {
    uint32_t t = millis();
    for (uint8_t k = 0; k < 8; k++) {
      uint32_t hsh = hash2(k, (int)(t / 120));
      int sx = x + (int)(hsh % (uint32_t)w), sy = y - 10 + (int)((hsh >> 8) % (uint32_t)(h + 20));
      if (sy > y && sy < y + h && sx > x + 8 && sx < x + w - 8) continue;   // around it, not on it
      gfx->drawFastHLine(sx - 3, sy, 7, CAS_GOLD);
      gfx->drawFastVLine(sx, sy - 3, 7, CAS_GOLD);
    }
  }
}

void casinoOpenNow() {
  casinoOpen = true;
  // The training menu it was found behind closes: every handler that checks
  // trainOpen before casinoOpen would otherwise answer for the casino's
  // gestures (down closed the menu instead of leaving a game). Leaving the
  // lobby reopens it.
  trainOpen = false;
  casView = CV_LOBBY;
  casArmed = -1;
  casSetMood(DM_LAUGH);
  casOpenedAt = millis();
  sfxPlay(SFX_MEDAL);
}
void casinoClose() {
  casinoOpen = false;
  casView = CV_LOBBY;
  slShown = 3;
  // A hand in progress is forfeit -- the bet is already in the house's pocket,
  // exactly as walking away from a real table. High/Low banks its pot.
  if (casHl.state == HL_GUESS) { casPay(casHl.cashOut()); }
  casBj.state = BJ_IDLE;
  casRouletteSettleNow();
}

// The way in: Team Rocket's hideout doors, shut with the R across them for a
// beat, then sliding apart onto the lobby. Drawn over the lobby for
// CAS_DOOR_MS after the door opens; taps wait until it has.
#define CAS_DOOR_HOLD_MS 350
#define CAS_DOOR_MS 1300
bool casinoDoorsShowing() { return casView == CV_LOBBY && millis() - casOpenedAt < CAS_DOOR_MS; }
static void casDrawDoors() {
  uint32_t t = millis() - casOpenedAt;
  float p = t <= CAS_DOOR_HOLD_MS ? 0 : (float)(t - CAS_DOOR_HOLD_MS) / (CAS_DOOR_MS - CAS_DOOR_HOLD_MS);
  if (p > 1) p = 1;
  p = p * p * (3 - 2 * p);                  // ease in and out
  int gap = (int)(p * 233);
  const uint16_t DOOR = 0x18C3, EDGE = 0x5000;
  if (CX - gap > 0) {
    gfx->fillRect(0, 0, CX - gap, 466, DOOR);
    for (int y = 40; y < 466; y += 56) gfx->fillRect(0, y, CX - gap, 6, EDGE);   // riveted bands
    gfx->fillRect(CX - gap - 6 > 0 ? CX - gap - 6 : 0, 0, 6, 466, CAS_RED);
  }
  if (CX + gap < 466) {
    gfx->fillRect(CX + gap, 0, 466 - (CX + gap), 466, DOOR);
    for (int y = 40; y < 466; y += 56) gfx->fillRect(CX + gap, y, 466 - (CX + gap), 6, EDGE);
    gfx->fillRect(CX + gap, 0, 6, 466, CAS_RED);
  }
  if (!gap) {                               // shut: the R across the seam
    gfx->fillCircle(CX, CY, 92, 0x0841);
    gfx->drawCircle(CX, CY, 92, CAS_RED);
    gfx->drawCircle(CX, CY, 91, CAS_RED);
    casMotif(CM_ROCKET, CX, CY, 120, CAS_RED, CAS_RED);
    casText(CX, CY + 104, T(S_CAS_ROCKET), CAS_RED, 2);
  }
}

void renderCasino() {
  casSlotsAuto();
  casRouletteAuto();
  switch (casView) {
    case CV_ROULETTE: renderCasRoulette(); break;
    case CV_SLOTS: renderCasSlots(); break;
    case CV_HL: renderCasHl(); break;
    case CV_BJ: renderCasBj(); break;
    case CV_COINS: renderCasCoins(); break;
    case CV_SHOP: renderCasShop(); break;
    default: renderCasLobby(); break;
  }
  if (casinoDoorsShowing()) casDrawDoors();
  uiChrome();
  gfx->flush();
}

void casinoTap(int16_t x, int16_t y) {
  if (casinoDoorsShowing()) return;          // still coming through the door
  switch (casView) {
    case CV_ROULETTE: casRouletteTap(x, y); return;
    case CV_SLOTS: casSlotsTap(x, y); return;
    case CV_HL: casHlTap(x, y); return;
    case CV_BJ: casBjTap(x, y); return;
    case CV_COINS: casCoinsTap(x, y); return;
    case CV_SHOP: casShopTap(x, y); return;
    default: break;
  }
  for (uint8_t i = 0; i < CAS_LOBBY_BTNS; i++) {
    int bx, by, bw, bh; casLobbyBtn(i, &bx, &by, &bw, &bh);
    if (!inBox(x, y, bx, by, bw, bh)) continue;
    static const uint8_t V[CAS_LOBBY_BTNS] = { CV_SLOTS, CV_HL, CV_BJ, CV_ROULETTE, CV_COINS, CV_SHOP };
    casView = V[i];
    casArmed = -1;
    casLastWin = 0;
    sfxPlay(SFX_TAP);
    return;
  }
}

// Down = back, one level: a game to the lobby (a hand in play is settled as
// casinoClose() does), the lobby out to the training menu.
void casinoSwipeDown() {
  if (casView == CV_LOBBY) { casinoClose(); trainOpen = true; return; }
  if (casView == CV_HL && casHl.state == HL_GUESS) casPay(casHl.cashOut());
  if (casView == CV_SLOTS && slShown < 3) { slShown = 3; casSlotsSettle(); }
  if (casView == CV_BJ && casBj.state == BJ_PLAYER) { casBj.stand(); casBjFinish(); }
  if (casView == CV_ROULETTE) casRouletteSettleNow();
  casView = CV_LOBBY;
  casArmed = -1;
}
uint8_t casinoView() { return casView; }
// For casino_ui_test: what the screens hold, read-only.
const uint8_t *casSlotStops() { return slStop; }
const Blackjack &casBlackjack() { return casBj; }
const HighLow &casHighLow() { return casHl; }
uint8_t casRouletteResult() { return rlResult; }
bool casRouletteSpinning() { return rlSpinning; }
uint8_t casRouletteBetKind() { return rlBet; }
uint8_t casRouletteBetArg() { return rlArg; }
// Tests that do not run real time step past the door animation with this.
void casinoDbgSkipDoor() { casOpenedAt = millis() - CAS_DOOR_MS - 1; }
