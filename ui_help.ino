// The how-to-play card: four short pages for somebody who has never held the
// board. Nothing in the game said that the horizontal axis exists, that up
// opens the creature's card, that down is BACK everywhere, or that pages turn on
// the rim -- all of it had to be stumbled on, and the rim in particular is not
// a gesture anyone guesses.
//
// Shown ONCE, right after the first starter is chosen (NVS "help", so a reboot
// does not repeat it and WIPE brings it back), and from the menu at any time.
// Pages on the rim like every paged screen (uiRimTarget) and with a finger-sized
// NEXT button, because the card is what teaches the rim -- it cannot depend on
// the player already knowing it. Down closes it, as everywhere.

bool helpOpen = false;
uint8_t helpPage = 0;

#define HELP_PAGES 4
#define HELP_BTN_W 200
#define HELP_BTN_Y 322              // a UI_TAP_FINGER-tall button, corners on the glass
static_assert(S_HELP_T4 - S_HELP_T1 == HELP_PAGES - 1 && S_HELP_B4 - S_HELP_B1 == HELP_PAGES - 1,
              "one title and one body per page, contiguous in i18n.h");

uint8_t helpPages() { return HELP_PAGES; }

// The NEXT / GOT IT button. The test aims through this, never a number.
void helpBtn(int *x, int *y, int *w, int *h) {
  *w = HELP_BTN_W; *h = UI_TAP_FINGER; *x = CX - HELP_BTN_W / 2; *y = HELP_BTN_Y;
}

static bool helpSeen() {
  Preferences p;
  uint8_t v = 0;
  if (p.begin("tamapoke", true)) { v = p.getUChar("help", 0); p.end(); }
  return v != 0;
}
static void helpMarkSeen() {
  Preferences p;
  if (p.begin("tamapoke", false)) { if (!p.putUChar("help", 1)) logKeyFailure("help", "help"); p.end(); }
}

void helpOpenNow() { helpOpen = true; helpPage = 0; }
// The first-game hook: open once, never again on this save.
void helpOfferFirstTime() { if (!helpSeen()) helpOpenNow(); }
void helpClose() { helpOpen = false; helpPage = 0; helpMarkSeen(); }

void renderHelp() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (helpPage >= HELP_PAGES) helpPage = 0;
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  const char *top = T(S_HELP);
  gfx->setCursor(CX - (int)strlen(top) * 6, 44);
  gfx->print(top);
  const char *ttl = T((StrId)(S_HELP_T1 + helpPage));
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(ttl) * 9, 84);
  gfx->print(ttl);

  // the body, one line per '\n', each centred
  const char *b = T((StrId)(S_HELP_B1 + helpPage));
  gfx->setTextSize(2);
  int y = 140;
  while (*b) {
    const char *e = strchr(b, '\n');
    int len = e ? (int)(e - b) : (int)strlen(b);
    gfx->setCursor(CX - len * 6, y);
    for (int i = 0; i < len; i++) gfx->print(b[i]);
    y += 34;
    if (!e) break;
    b = e + 1;
  }

  int x, by, w, h; helpBtn(&x, &by, &w, &h);
  bool last = helpPage == HELP_PAGES - 1;
  gfx->fillRoundRect(x, by, w, h, 14, UI_BAR_OK);
  gfx->drawRoundRect(x, by, w, h, 14, UI_INK);
  const char *bl = T(last ? S_HELP_DONE : S_HELP_NEXT);
  gfx->setTextColor(UI_BG_DAY);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(bl) * 6, by + h / 2 - 8);
  gfx->print(bl);
  uiChrome();
  gfx->flush();
}

void helpTap(int16_t px, int16_t py) {
  int x, y, w, h; helpBtn(&x, &y, &w, &h);
  if (px < x || px >= x + w || py < y || py >= y + h) return;   // a miss does nothing
  sfxPlay(SFX_TAP);
  if (helpPage + 1 < HELP_PAGES) helpPage++;
  else helpClose();
}
