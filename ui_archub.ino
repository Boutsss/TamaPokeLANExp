// The ARCADE tile (v3.57): its own stop on the swipe axis, between the MART and
// the GYM, instead of the TRAIN panel's fourth row.
//
// One big cabinet button opens the game list (ui_arcade.ino). Everything else
// here is decoration -- a marquee with chasing bulbs, a starry night, a neon
// floor scrolling toward the horizon -- because an arcade should look like one.
//
// The button is also where the old row's secret went: HELD, it turns over
// exactly as the row did (casinoDrawSpinRow(), the same timing, chime and
// card), just at the cabinet's size. The draw and the tap both ask
// arcHubBtnAt()/the AH_ rectangle, so the hold is on exactly what you see.

#define AH_BTN_W 300
#define AH_BTN_H 124
#define AH_BTN_X (CX - AH_BTN_W / 2)
#define AH_BTN_Y 188
#define AH_FLOOR_Y 360         // the neon floor's horizon
static_assert(AH_BTN_Y + AH_BTN_H < AH_FLOOR_Y, "the cabinet stands above the floor");

bool arcHubBtnAt(int16_t x, int16_t y) {
  return x >= AH_BTN_X && x <= AH_BTN_X + AH_BTN_W && y >= AH_BTN_Y && y <= AH_BTN_Y + AH_BTN_H;
}
int arcHubBtnCenterY() { return AH_BTN_Y + AH_BTN_H / 2; }

static const uint16_t AH_BG = C565(0x16, 0x08, 0x2c);      // deep arcade purple
static const uint16_t AH_PINK = C565(0xff, 0x2e, 0x93);
static const uint16_t AH_CYAN = C565(0x2e, 0xf2, 0xff);
static const uint16_t AH_GOLD = C565(0xff, 0xd2, 0x3f);
static const uint16_t AH_GREEN = C565(0x5c, 0xff, 0x6a);

static void ahText(int cx, int y, const char *s, uint16_t col, uint8_t size) {
  gfx->setTextSize(size);
  gfx->setTextColor(col);
  gfx->setCursor(cx - (int)strlen(s) * 3 * size, y);
  gfx->print(s);
}

// A cheap, stable hash: the stars must not jump about from frame to frame.
static uint32_t ahHash(uint32_t a) { a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16; return a; }

// The cabinet's face, in any width -- the spin narrows it to its edge, so
// everything on it is dropped rather than squeezed when there is no room.
void arcHubFace(int x, int y, int w, int h) {
  bool ok = arcadeEnabled();
  uint32_t t = millis();
  uint16_t body = ok ? AH_PINK : C565(0x70, 0x60, 0x80);
  uint16_t glow = ok ? ((t / 300) % 2 ? AH_CYAN : AH_GOLD) : C565(0x90, 0x90, 0xa0);
  gfx->fillRoundRect(x, y, w, h, 20, body);
  if (w > 16) gfx->fillRoundRect(x + 6, y + 6, w - 12, h / 2 - 4, 14, ok ? C565(0xff, 0x6b, 0xb5) : C565(0x88, 0x78, 0x98));   // a shine
  gfx->drawRoundRect(x, y, w, h, 20, glow);
  if (w > 8) gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 18, glow);
  const char *label = arcadeTitle();
  int cx = x + w / 2;
  if (w > (int)strlen(label) * 24 + 24) {
    ahText(cx + 3, y + 22 + 3, label, C565(0x40, 0x00, 0x30), 4);   // drop shadow
    ahText(cx, y + 22, label, UI_WHITE, 4);
  } else if (w > (int)strlen(label) * 12 + 12) {
    ahText(cx, y + 30, label, UI_WHITE, 2);
  }
  if (!ok) {   // say why, rather than sit there grey
    const char *why = T(pet.isEgg() ? S_ARC_EGG : S_ARC_ASLEEP);
    if (w > (int)strlen(why) * 12 + 24) ahText(cx, y + 74, why, UI_WHITE, 2);
    return;
  }
  if (w > (int)strlen(T(S_ARC_PLAY)) * 12 + 24 && (t / 450) % 3) ahText(cx, y + 70, T(S_ARC_PLAY), AH_GOLD, 2);
  // three arcade buttons along the bottom
  if (w > 150) {
    static const uint16_t BTN[3] = { C565(0xff, 0x40, 0x40), AH_GOLD, C565(0x40, 0x90, 0xff) };
    for (int i = 0; i < 3; i++) {
      int bx = cx - 40 + i * 40, by = y + h - 16;
      bool lit = (t / 200) % 3 == (uint32_t)i;
      gfx->fillCircle(bx, by, 7, lit ? UI_WHITE : BTN[i]);
      gfx->drawCircle(bx, by, 7, C565(0x40, 0x00, 0x30));
    }
  }
}

void renderArcadeHub() {
  uint32_t t = millis();
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, AH_BG);

  // stars, twinkling
  for (uint8_t i = 0; i < 40; i++) {
    uint32_t hsh = ahHash(i * 2654435761U + 17);
    int sx = 40 + (int)(hsh % 386), sy = 30 + (int)((hsh >> 9) % 320);
    int dx = sx - CX, dy = sy - CY;
    if (dx * dx + dy * dy > 220 * 220) continue;
    bool on = ((t / 160) + (hsh >> 20)) % 7 != 0;
    if (on) gfx->drawPixel(sx, sy, (hsh >> 4) & 1 ? UI_WHITE : AH_CYAN);
  }

  // the marquee: a lit sign with bulbs chasing round its frame
  const int mx = CX - 150, my = 52, mw = 300, mh = 104;
  gfx->fillRoundRect(mx, my, mw, mh, 16, C565(0x2a, 0x0e, 0x46));
  gfx->drawRoundRect(mx, my, mw, mh, 16, AH_GOLD);
  {
    const int step = 20;
    int n = 0;
    uint32_t phase = t / 120;
    for (int bx = mx + 14; bx <= mx + mw - 14; bx += step, n++) {
      gfx->fillCircle(bx, my + 8, 3, (phase + n) % 3 == 0 ? AH_GOLD : C565(0x60, 0x40, 0x10));
      gfx->fillCircle(bx, my + mh - 8, 3, (phase + n + 1) % 3 == 0 ? AH_GOLD : C565(0x60, 0x40, 0x10));
    }
  }
  // ARCADE, every letter its own neon colour, the colours walking along
  {
    static const uint16_t NEON[5] = { AH_PINK, AH_CYAN, AH_GOLD, AH_GREEN, C565(0xb0, 0x6b, 0xff) };
    const char *title = arcadeTitle();
    int len = (int)strlen(title);
    uint8_t size = len <= 8 ? 5 : 4;
    int cw = 6 * size, x0 = CX - len * cw / 2, ty = my + mh / 2 - 4 * size;
    int bob = ((t / 250) % 2) ? 1 : 0;
    gfx->setTextSize(size);
    for (int i = 0; i < len; i++) {
      char ch[2] = { title[i], 0 };
      gfx->setTextColor(C565(0x10, 0x00, 0x20));
      gfx->setCursor(x0 + i * cw + 3, ty + 3);
      gfx->print(ch);
      gfx->setTextColor(NEON[(i + t / 300) % 5]);
      gfx->setCursor(x0 + i * cw, ty + ((i % 2) ? bob : -bob));
      gfx->print(ch);
    }
  }
  // what is inside, every game named (two lines if one will not fit)
  {
    char l1[64] = "", l2[64] = "";
    for (uint8_t g = 0; g < ARC_GAMES; g++) {
      char *l = (strlen(l1) + strlen(T(arcName(g))) + 2) * 6 <= 300 && !l2[0] ? l1 : l2;
      size_t n = strlen(l);
      snprintf(l + n, 64 - n, n ? "  %s" : "%s", T(arcName(g)));
    }
    ahText(CX, l2[0] ? 162 : 166, l1, C565(0xc8, 0xb0, 0xff), 1);
    if (l2[0]) ahText(CX, 173, l2, C565(0xc8, 0xb0, 0xff), 1);
  }

  // the cabinet button -- or, while held, the card turning over
  if (casinoSpinning()) casinoDrawSpinRow(AH_BTN_X, AH_BTN_Y, AH_BTN_W, AH_BTN_H, arcadeTitle(), arcHubFace);
  else arcHubFace(AH_BTN_X, AH_BTN_Y, AH_BTN_W, AH_BTN_H);

  // the neon floor: lines to a vanishing point, rungs scrolling toward you
  for (int k = -6; k <= 6; k++) {
    int x1 = CX + k * 64;
    gfx->drawLine(CX + k * 10, AH_FLOOR_Y, x1, 466, C565(0x9a, 0x1c, 0xc8));
  }
  gfx->drawFastHLine(0, AH_FLOOR_Y, 466, AH_PINK);
  float scroll = (float)(t % 900) / 900.0f;
  for (int r = 0; r < 6; r++) {
    float f = ((float)r + scroll) / 6.0f;                 // 0 at the horizon, 1 at the bottom
    int ry = AH_FLOOR_Y + (int)((466 - AH_FLOOR_Y) * f * f);
    gfx->drawFastHLine(0, ry, 466, C565(0x9a, 0x1c, 0xc8));
  }
  // the bezel, so the floor does not spill past the glass
  for (int rr = 231; rr < 240; rr++) gfx->drawCircle(CX, CY, rr, RGB565_BLACK);

  uiChrome();
  gfx->flush();
}

void arcHubTap(int16_t x, int16_t y) {
  if (!arcHubBtnAt(x, y)) return;
  if (!arcadeEnabled()) { sfxPlay(SFX_DENY); return; }
  sfxPlay(SFX_TAP);
  arcadeOpen = true;     // the game list, on top of this page; backing out lands here
  arcadeOpened();
}
