// The held BACK button, on every screen below the pet.
//
// Players reported getting stuck in screens with no visible way out (the back
// gesture, swipe down, is invisible), and leaving minigames by accident. So
// there is one button, in one place, on every sub-screen -- and it only works
// when HELD for BACK_HOLD_MS. A tap on it does nothing but say "hold to go
// back"; it never falls through to the screen underneath, which matters most
// in a minigame where the player is tapping fast.
//
// It is drawn from the canvas flush (uiBeforeFlush, called by TouchCanvas on
// the board and by the emulator's canvas), NOT by each renderer: there are
// ~60 flush sites, and a button added screen by screen is case 1 of CLAUDE.md's
// traps waiting to happen -- the screen nobody remembered would be the one a
// player gets stuck in. What it DOES is uiBack(), the same function swipe-down
// uses, so the button cannot take you anywhere the gesture would not.

#define BACK_HOLD_MS 650
// The bottom of the panel, just above the tile dots (TILE_DOT_Y 440). Screens
// keep real buttons down to y ~398 (EXPLORE's GO, the bag's TAKE), so the hit
// area never reaches ABOVE the pill -- a tap on their lower edge must stay
// theirs -- and grows downwards and sideways instead, where nothing is a target.
#define BACK_W 132
#define BACK_H 36
#define BACK_Y 400
#define BACK_PAD_X 12
#define BACK_PAD_DOWN 18    // to y 454: still on the glass at the centre

static bool backOwned = false;      // this gesture started on the button
static bool backCancelled = false;  // ... and the finger slid off it
static bool backFired = false;
static uint32_t backStart = 0;
static uint32_t backHintUntil = 0;

// Which screens carry it. A switch with no default, so a new SCR_ value is a
// compiler warning here rather than a screen with no way out.
bool uiBackShownOn(uint8_t scr) {
  switch ((ScreenId)scr) {
    // home, and first boot: there is nowhere to go back to
    case SCR_MAIN: case SCR_STARTER: case SCR_REGION:
    // a fight is never walked out of; its own BACK lives in its move menu
    case SCR_BATTLE: case SCR_WIN: case SCR_LEARN:
    // their own keys/rows reach the bottom of the panel, and they close on
    // any tap outside or have their own OK/CANCEL
    case SCR_KEYBOARD: case SCR_TASKS: case SCR_CLOCK:
    // the how-to-play card's finger-sized NEXT / GOT IT reaches y ~420: the pill
    // would sit on it, and the card already has a way out on every page
    case SCR_HELP:
    case SCR_COUNT:
      return false;
    // a creature has ended and waits for a slot: backing out would LET IT GO,
    // which is final -- that keeps its own explicit red button instead
    case SCR_PARTY:
      return !partyPick;
    // the menu panel ends at y ~395, so the button sits clear below it: closing
    // it on a tap outside was the only way out, and nothing said so
    case SCR_MENU:
    case SCR_GALLERY: case SCR_DEXPICK: case SCR_MOVEPICK: case SCR_BOX:
    case SCR_CARD: case SCR_PLAYER: case SCR_GYM: case SCR_GYMPICK:
    case SCR_EXPLORE: case SCR_MART: case SCR_LAN: case SCR_PICK: case SCR_TRAIN:
    case SCR_BAGSCR: case SCR_GAME: case SCR_DAYCARE: case SCR_DCPICK:
    case SCR_TOWER: case SCR_CHAL: case SCR_ARCADE: case SCR_TRADE: case SCR_TRADEPICK:
    case SCR_FRIENDS: case SCR_CASINO: case SCR_CASSHOP: case SCR_CARDBOOK:
    case SCR_BOXFILTER: case SCR_MONSTATS: case SCR_BOXLIST: case SCR_AVPICK:
    case SCR_BADGES: case SCR_WARDROBE:
      return true;
  }
  return false;
}
// Never on the tile axis itself (PLAYER, DAYCARE, PARTY, EXPLORE, MART, GYM,
// DEX -- each tile's chooser included): those are one swipe from the pet and are
// navigated sideways, and on EXPLORE the pill covered the shiny-chain message.
// The button is for getting back OUT of what those screens open. Asked of
// uiTileIndex(), the axis's own table, so a new tile cannot get it by mistake.
bool uiBackShown() {
  if (uiTileIndex() >= 0) return false;
  return uiBackShownOn(uiCurrentScreen());
}

// Geometry, asked of the firmware rather than restated (the tests aim here).
void uiBackRect(int *x, int *y, int *w, int *h) {
  *x = CX - BACK_W / 2; *y = BACK_Y; *w = BACK_W; *h = BACK_H;
}
bool uiBackHit(int16_t px, int16_t py) {
  int x, y, w, h; uiBackRect(&x, &y, &w, &h);
  return px >= x - BACK_PAD_X && px <= x + w + BACK_PAD_X &&
         py >= y && py <= y + h + BACK_PAD_DOWN;
}

// The touch path's first question. True = this sample belongs to the button,
// and nothing else may see it. `down` is the first sample of a gesture.
bool uiBackTouch(bool pressed, bool down, int16_t px, int16_t py) {
  if (down) {
    backOwned = false;
    if ((dimStage > 0) || screenOff) return false;   // that touch only wakes the panel
    if (!uiBackShown() || !uiBackHit(px, py)) return false;
    backOwned = true; backCancelled = false; backFired = false;
    backStart = millis();
    lastInteract = millis();
    return true;
  }
  if (!backOwned) return false;
  if (pressed) {
    lastInteract = millis();
    if (!uiBackHit(px, py)) backCancelled = true;   // slid off: let go to try again
    if (!backFired && !backCancelled && millis() - backStart >= BACK_HOLD_MS) {
      backFired = true;
      sfxPlay(SFX_TAP);
      uiBack();
    }
    return true;
  }
  // lifted
  if (!backFired) backHintUntil = millis() + 1500;   // a tap: say how it works
  backOwned = false;
  return true;
}

// 0..255: how far the current hold has got, for the fill.
static uint8_t backProgress() {
  if (!backOwned || backCancelled || backFired) return 0;
  uint32_t t = millis() - backStart;
  return t >= BACK_HOLD_MS ? 255 : (uint8_t)(t * 255 / BACK_HOLD_MS);
}

void uiBeforeFlush() {
  if (!uiBackShown()) return;
  int x, y, w, h; uiBackRect(&x, &y, &w, &h);
  uint8_t p = backProgress();
  gfx->fillRoundRect(x, y, w, h, h / 2, UI_WHITE);
  if (p) {
    int fw = (int)w * p / 255;
    if (fw < h) fw = h;   // a round end from the first frame
    gfx->fillRoundRect(x, y, fw, h, h / 2, UI_BAR_OK);
  }
  gfx->drawRoundRect(x, y, w, h, h / 2, UI_INK);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, h / 2 - 1, UI_INK);
  // a left-pointing chevron, then the label
  const char *lbl = T(S_BACK_BTN);
  int tw = (int)strlen(lbl) * 12;
  int gx = CX - (tw + 16) / 2;
  int cy = y + h / 2;
  gfx->fillTriangle(gx, cy, gx + 9, cy - 8, gx + 9, cy + 8, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(gx + 16, cy - 7);
  gfx->print(lbl);
  if (millis() < backHintUntil) {
    const char *hint = T(S_BACK_HOLD);
    int hw = (int)strlen(hint) * 6 + 16;
    gfx->fillRoundRect(CX - hw / 2, y - 22, hw, 18, 6, UI_INK);
    gfx->setTextColor(UI_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(hint) * 3, y - 17);
    gfx->print(hint);
  }
}
