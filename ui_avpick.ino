// The avatar picker: tap the avatar on the player card and choose from a grid
// of every one you have unlocked, instead of cycling through them a tap at a
// time (which stopped being reasonable once there could be 255 of them).
//
// A level deeper than the player card (SCR_AVPICK): pages on the rim like every
// paged screen (uiRimTarget), down walks back to the card, a tap wears the one
// you touched. Cells are UI_TAP_FINGER, the size new work is held to.
//
// "Unlocked" is rankAvatarUnlocked(), the one rule the card already used, so
// the picker can never offer an avatar the card would refuse.

bool avPickOpen = false;     // only ever meaningful while playerOpen
uint8_t avPickPage = 0;

#define AVP_COLS 3
#define AVP_ROWS 3
#define AVP_PER (AVP_COLS * AVP_ROWS)
#define AVP_CELL UI_TAP_FINGER
#define AVP_GAP 4
#define AVP_TOP 82

// The unlocked avatars in index order; returns how many. `out` may be null.
// The order is the free set (RED, BLUE, GIRL ...), then the extra starters,
// then everything else -- so a new player's ten choices sit on the first pages
// instead of after 247 shop entries they do not own.
uint16_t avPickList(uint16_t *out, uint16_t cap) {
  uint16_t n = 0;
  auto take = [&](uint16_t a) {
    if (rankAvatarUnlocked(a)) { if (out && n < cap) out[n] = a; n++; }
  };
  take(AVATAR_YOU);   // the customisable trainer first: it is the one you dress
  for (uint16_t a = 0; a < AVATAR_BASE_COUNT; a++) take(a);
  for (uint16_t k = 0; k < STARTER_AVATAR_COUNT; k++) take(STARTER_AVATAR_FIRST + k);
  for (uint16_t a = AVATAR_BASE_COUNT; a < AVATAR_COUNT; a++)
    if ((a < STARTER_AVATAR_FIRST || a >= STARTER_AVATAR_FIRST + STARTER_AVATAR_COUNT) && a != AVATAR_YOU) take(a);
  return n;
}
uint8_t avPickPages() {
  uint16_t n = avPickList(nullptr, 0);
  return n ? (uint8_t)((n + AVP_PER - 1) / AVP_PER) : 1;
}
static_assert((AVATAR_COUNT + AVP_PER - 1) / AVP_PER <= 255, "avPickPage is a uint8_t");

// Cell i of the page: its top-left. The tests aim through this, never a number.
void avPickCell(uint8_t i, int *x, int *y) {
  const int w = AVP_COLS * AVP_CELL + (AVP_COLS - 1) * AVP_GAP;
  *x = CX - w / 2 + (i % AVP_COLS) * (AVP_CELL + AVP_GAP);
  *y = AVP_TOP + (i / AVP_COLS) * (AVP_CELL + AVP_GAP);
}

// How many of the Battle Tower's legendary costumes are owned -- shown only
// once there is at least one, so the prize stays a discovery.
static uint16_t avPickLegends() {
  uint16_t n = 0;
  for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) n += casinoOwns(PK_AVATAR, TOWER_AVATARS[k]);
  return n;
}

void avPickOpenNow() {
  avPickOpen = true;
  // open on the page that holds what you are wearing
  static uint16_t list[AVATAR_COUNT];
  uint16_t n = avPickList(list, AVATAR_COUNT);
  avPickPage = 0;
  for (uint16_t k = 0; k < n; k++) if (list[k] == pet.avatar) { avPickPage = (uint8_t)(k / AVP_PER); break; }
}

void renderAvPick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  static uint16_t list[AVATAR_COUNT];
  uint16_t n = avPickList(list, AVATAR_COUNT);
  if (avPickPage >= avPickPages()) avPickPage = 0;
  char t[32];
  snprintf(t, sizeof(t), T(S_AV_PICK_FMT), (unsigned)n);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(t) * 6, 38);
  gfx->print(t);
  uint16_t legends = avPickLegends();
  if (legends) {
    snprintf(t, sizeof(t), T(S_AV_LEGENDS_FMT), (unsigned)legends, (unsigned)TOWER_AVATAR_COUNT);
    gfx->setTextColor(0x895F);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(t) * 3, 62);
    gfx->print(t);
  }
  for (uint8_t i = 0; i < AVP_PER; i++) {
    uint16_t k = (uint16_t)avPickPage * AVP_PER + i;
    if (k >= n) break;
    uint16_t a = list[k];
    int x, y; avPickCell(i, &x, &y);
    bool on = a == pet.avatar;
    gfx->fillRoundRect(x, y, AVP_CELL, AVP_CELL, 12, on ? UI_BAR_OK : UI_WHITE);
    gfx->drawRoundRect(x, y, AVP_CELL, AVP_CELL, 12, UI_INK);
    // 3x is 48 px; the legendary costumes' aura needs 2 cells (6 px) beside it
    drawAvatar(a, x + (AVP_CELL - AVATAR_PX * 3) / 2, y + 10, 3);
    const char *nm = AVATARS[a].name;
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(x + (AVP_CELL - (int)strlen(nm) * 6) / 2, y + AVP_CELL - 18);
    gfx->print(nm);
  }
  uiChrome();
  gfx->flush();
}

// True if the tap was the picker's (it always is while the picker is up).
bool avPickTap(int16_t px, int16_t py) {
  static uint16_t list[AVATAR_COUNT];
  uint16_t n = avPickList(list, AVATAR_COUNT);
  for (uint8_t i = 0; i < AVP_PER; i++) {
    uint16_t k = (uint16_t)avPickPage * AVP_PER + i;
    if (k >= n) break;
    int x, y; avPickCell(i, &x, &y);
    if (px >= x && px < x + AVP_CELL && py >= y && py < y + AVP_CELL) {
      pet.avatar = list[k];
      pet.saveNow();
      avPickOpen = false;          // worn: back to the card, which shows it
      sfxPlay(SFX_TAP);
      return true;
    }
  }
  return true;   // a miss inside the picker does nothing -- it never closes it
}
