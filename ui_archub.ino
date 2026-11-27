// The ARCADE tile (v3.57): its own stop on the swipe axis, between the MART and
// the GYM, instead of the TRAIN panel's fourth row.
//
// One big button -- the arcade's lit marquee sign, chasing bulbs and neon
// letters -- opens the game list (ui_arcade.ino). Round it, a starry night
// and a neon floor scrolling toward the horizon, because an arcade should look
// like one.
//
// The button is also where the old row's secret went: HELD, it turns over
// exactly as the row did (casinoDrawSpinRow(), the same timing, chime and
// card), just at the cabinet's size. The draw and the tap both ask
// arcHubBtnAt()/the AH_ rectangle, so the hold is on exactly what you see.

// One button, the whole page's centrepiece: the lit marquee sign itself.
#define AH_BTN_W 340
#define AH_BTN_H 176
#define AH_BTN_X (CX - AH_BTN_W / 2)
#define AH_BTN_Y (CY - AH_BTN_H / 2 - 14)
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

// The button's face: the arcade's lit marquee -- bulbs chasing round its
// frame, ARCADE in walking neon colours, TAP TO PLAY blinking under it. Drawn
// at any width, because the hold narrows it to its edge as it turns over: what
// has no room is dropped rather than squeezed.
void arcHubFace(int x, int y, int w, int h) {
  static const uint16_t NEON[5] = { AH_PINK, AH_CYAN, AH_GOLD, AH_GREEN, C565(0xb0, 0x6b, 0xff) };
  bool ok = arcadeEnabled();
  uint32_t t = millis();
  uint16_t lit = ok ? AH_GOLD : C565(0xa0, 0x98, 0x80), dim = C565(0x60, 0x40, 0x10);
  gfx->fillRoundRect(x, y, w, h, 20, C565(0x2a, 0x0e, 0x46));
  gfx->drawRoundRect(x, y, w, h, 20, lit);
  if (w > 8) gfx->drawRoundRect(x + 3, y + 3, w - 6, h - 6, 17, ok ? AH_PINK : C565(0x70, 0x60, 0x80));
  // bulbs round the frame, chasing (still and dim while the arcade is shut)
  uint32_t phase = ok ? t / 110 : 0;
  int n = 0;
  for (int bx = x + 18; bx <= x + w - 18; bx += 22, n++) {
    gfx->fillCircle(bx, y + 11, 4, ok && (phase + n) % 3 == 0 ? lit : dim);
    gfx->fillCircle(bx, y + h - 11, 4, ok && (phase + n + 1) % 3 == 0 ? lit : dim);
  }
  if (w > 40)
    for (int by = y + 33; by <= y + h - 33; by += 22, n++) {
      gfx->fillCircle(x + 11, by, 4, ok && (phase + n) % 3 == 0 ? lit : dim);
      gfx->fillCircle(x + w - 11, by, 4, ok && (phase + n + 2) % 3 == 0 ? lit : dim);
    }
  // ARCADE, every letter its own neon colour, the colours walking along
  const char *title = arcadeTitle();
  int len = (int)strlen(title);
  uint8_t size = 6;
  while (size > 2 && len * 6 * size > w - 50) size--;
  if (len * 6 * size <= w - 40) {
    int cw = 6 * size, x0 = x + w / 2 - len * cw / 2, ty = y + h / 2 - 4 * size - (ok ? 16 : 18);
    int bob = ok && (t / 250) % 2 ? 2 : 0;
    gfx->setTextSize(size);
    for (int i = 0; i < len; i++) {
      char ch[2] = { title[i], 0 };
      gfx->setTextColor(C565(0x10, 0x00, 0x20));
      gfx->setCursor(x0 + i * cw + 4, ty + 4);
      gfx->print(ch);
      gfx->setTextColor(ok ? NEON[(i + t / 300) % 5] : C565(0x90, 0x88, 0xa0));
      gfx->setCursor(x0 + i * cw, ty + ((i % 2) ? bob : -bob));
      gfx->print(ch);
    }
  }
  int cx = x + w / 2, sub = y + h / 2 + 30;
  if (!ok) {   // say why, rather than sit there dark
    const char *why = T(S_ARC_EGG);   // (asleep no longer shuts it; S_ARC_ASLEEP is unused)
    if (w > (int)strlen(why) * 12 + 30) ahText(cx, sub, why, UI_WHITE, 2);
    return;
  }
  if (w > (int)strlen(T(S_ARC_PLAY)) * 12 + 30 && (t / 450) % 3) ahText(cx, sub, T(S_ARC_PLAY), AH_GOLD, 2);
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

  // what is inside, every game named (two lines if one will not fit)
  {
    char l1[64] = "", l2[64] = "";
    for (uint8_t g = 0; g < ARC_GAMES; g++) {
      char *l = (strlen(l1) + strlen(T(arcName(g))) + 2) * 6 <= 300 && !l2[0] ? l1 : l2;
      size_t n = strlen(l);
      snprintf(l + n, 64 - n, n ? "  %s" : "%s", T(arcName(g)));
    }
    int gy = AH_BTN_Y + AH_BTN_H + 14;   // under the sign
    ahText(CX, gy, l1, C565(0xc8, 0xb0, 0xff), 1);
    if (l2[0]) ahText(CX, gy + 11, l2, C565(0xc8, 0xb0, 0xff), 1);
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
