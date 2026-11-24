// TRAINER CARDS: the player card's last page (EXCHANGE, CARD BOOK) and the
// card book itself. The swap rides a FRIENDS session (ui_friends.ino sends our
// card and files theirs); card.cpp owns the format, the checks and the book.
// Geometry is asked through pcBtnTop()/pcBtnCenterY(), never restated.


bool cardBookOpen = false;
uint8_t cardBookPage = 0;
bool friendsGotCard = false;        // this session's friend card has been filed

// ---- the player card's TRAINER CARDS page
#define PC_BTN_H 76
#define PC_BTN_W 280
int pcBtnTop(uint8_t i) { return 150 + i * 100; }
int pcBtnCenterY(uint8_t i) { return pcBtnTop(i) + PC_BTN_H / 2; }
static int pcBtnAt(int16_t x, int16_t y) {
  if (x < CX - PC_BTN_W / 2 || x > CX + PC_BTN_W / 2) return -1;
  for (uint8_t i = 0; i < 2; i++)
    if (y >= pcBtnTop(i) && y < pcBtnTop(i) + PC_BTN_H) return i;
  return -1;
}

void renderPlayerCards() {
  const char *t = T(S_PC_CARDS);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(t) * 9, 44);
  gfx->print(t);
  char b[40];
  snprintf(b, sizeof(b), T(S_PC_COLLECTED_FMT), (unsigned)cardBookCount(), (unsigned)CARD_BOOK_CAP);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(b) * 6, 92);
  gfx->print(b);
  const char *lab[2] = { T(S_PC_EXCHANGE), T(S_PC_BOOK) };
  for (uint8_t i = 0; i < 2; i++) {
    int y = pcBtnTop(i);
    gfx->fillRoundRect(CX - PC_BTN_W / 2, y, PC_BTN_W, PC_BTN_H, 16, i == 0 ? C565(0xff, 0xf1, 0xc4) : UI_WHITE);
    gfx->drawRoundRect(CX - PC_BTN_W / 2, y, PC_BTN_W, PC_BTN_H, 16, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(lab[i]) * 6, y + PC_BTN_H / 2 - 8);
    gfx->print(lab[i]);
  }
  const char *h = T(S_PC_HINT);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(h) * 3, pcBtnTop(0) + PC_BTN_H + 6);
  gfx->print(h);
}

// True if the tap was this page's (the caller backs out on anything else).
bool playerCardsTap(int16_t x, int16_t y) {
  int b = pcBtnAt(x, y);
  if (b < 0) return false;
  sfxPlay(SFX_TAP);
  playerOpen = false;
  if (b == 0) friendsOpenFromPlayer();
  else { cardBookOpen = true; cardBookPage = 0; }
  return true;
}

// ---- the card book: one collected card per page, on the rim
uint8_t cardBookPages() { return cardBookCount() ? cardBookCount() : 1; }

void renderCardBook() {
  gfx->fillScreen(RGB565_BLACK);
  if (!cardBookCount()) {
    gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
    const char *t = T(S_PC_BOOK);
    gfx->setTextColor(UI_INK); gfx->setTextSize(3);
    gfx->setCursor(CX - (int)strlen(t) * 9, 44); gfx->print(t);
    const char *n = T(S_PC_NONE);
    gfx->setTextSize(2); gfx->setTextColor(UI_INK_SOFT);
    gfx->setCursor(CX - (int)strlen(n) * 6, 200); gfx->print(n);
    const char *h = T(S_PC_HINT);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(h) * 3, 232); gfx->print(h);
    uiChrome();
    gfx->flush();
    return;
  }
  if (cardBookPage >= cardBookCount()) cardBookPage = 0;
  const TrainerCard &c = cardBookAt(cardBookPage);
  drawCardBackground(c.bg, millis());          // THEIR background, as they chose it
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(c.name) * 9, 40);
  gfx->print(c.name);
  const char *ti = rankTitle(c.rank);
  gfx->setTextColor(UI_BAR_WARN);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(ti) * 3, 66);
  gfx->print(ti);
  drawAvatar(c.avatar, CX - AVATAR_PX * 2, 76, 4);
  char b[32];
  const int y0 = 158;
  snprintf(b, sizeof(b), T(S_PC_BADGES_FMT), (unsigned)c.badges);
  gfx->setTextColor(UI_INK); gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(b) * 6, y0); gfx->print(b);
  snprintf(b, sizeof(b), T(S_PC_DEX_FMT), (unsigned)c.dex);
  gfx->setCursor(CX - (int)strlen(b) * 6, y0 + 24); gfx->print(b);
  snprintf(b, sizeof(b), T(S_PC_MEDALS_FMT), (unsigned)c.medals);
  gfx->setCursor(CX - (int)strlen(b) * 6, y0 + 48); gfx->print(b);
  snprintf(b, sizeof(b), T(S_PC_STREAK_FMT), (unsigned)c.bestStreak);
  gfx->setCursor(CX - (int)strlen(b) * 6, y0 + 72); gfx->print(b);
  // their creature at the time: its thumbnail when the pack is here, else its number
  if (c.petDex >= 1 && c.petDex <= DEX_COUNT) {
    const uint8_t *th = (speciesHasArt(c.petDex) && regionAvailable(regionOfDex(c.petDex))) ? thumbs.get(c.petDex) : nullptr;
    if (th) drawThumb(th, CX - 40, 266, 2, false);
    else {
      snprintf(b, sizeof(b), "#%03d", (int)c.petDex);
      gfx->setTextColor(UI_INK); gfx->setTextSize(2);
      gfx->setCursor(CX - (int)strlen(b) * 6, 300); gfx->print(b);
    }
    snprintf(b, sizeof(b), "%s%s", DEX_TBL[c.petDex].name, c.petShiny ? " *" : "");
    gfx->setTextColor(UI_INK); gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(b) * 3, 352); gfx->print(b);
  }
  uiChrome();
  gfx->flush();
}

// Down = back: to the player card's TRAINER CARDS page.
void cardBookBack() {
  cardBookOpen = false;
  playerOpen = true;
  playerPage = PLAYER_CARDS_PAGE;
}

// ---- the exchange itself, once a frame during a FRIENDS session
static uint32_t pcSentAt = 0;
void cardsFriendsPoll(uint32_t now) {
  if (!lan.ready() || lan.mode != LINK_MODE_FRIENDS) return;
  // Ours, every 1.5 s for as long as we are paired: unkept on the link, so it
  // can never displace the gift swap's own resent frame.
  if (now - pcSentAt >= 1500) {
    pcSentAt = now;
    uint8_t w[CARD_WIRE_LEN];
    TrainerCard me = cardMine(pet, pet.avatar, casino.bg, rankShown(), friends.fid);
    lan.sendCard(w, cardToWire(me, w));
  }
  // Theirs, filed once per session (the same card keeps arriving).
  if (lan.cxNew) {
    lan.cxNew = false;
    TrainerCard c;
    if (!friendsGotCard && cardFromWire(lan.cx, lan.cxN, c)) {
      if (!c.fid || c.fid != friends.fid) {
        cardBookAdd(c, friends.fid);
        friendsGotCard = true;
      }
    }
  }
}
