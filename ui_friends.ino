// FRIENDS: a daily gift swap and a pet visit between two boards, ported from
// vunguyen2712/TamaPoke. Reached from the LAN screen's FRIENDS button; the
// session rides the same Link as a LAN battle, in LINK_MODE_FRIENDS (link.h),
// and all the rules and the two-phase commit live in friends.cpp. This file is
// only the screen: three states (pick HOST/JOIN, paired, choosing a gift) plus
// the visit tableau.
//
// Layout is asked, never restated: frRowCenterY() / frRowAt() are what both
// the renderer and friends_ui_test use.


bool friendsOpen = false;
static bool frPicking = false;   // the gift item chooser is up
static uint8_t frPickIdx = 0;

static uint32_t frDay() { return pet.currentDay(); }

// ---- layout: two big rows, finger-sized, inside the round panel's chord
#define FR_ROW_H 72
int frRowTop(uint8_t r) { return 128 + r * 100; }
int frRowCenterY(uint8_t r) { return frRowTop(r) + FR_ROW_H / 2; }
static int frRowHalf(uint8_t r) {
  int h = uiSafeHalfWidthFor(frRowTop(r), frRowTop(r) + FR_ROW_H) - 24;
  return h > 150 ? 150 : h;
}
int frRowAt(int16_t x, int16_t y) {
  for (uint8_t r = 0; r < 2; r++) {
    int half = frRowHalf(r);
    if (y >= frRowTop(r) && y < frRowTop(r) + FR_ROW_H && x >= CX - half && x <= CX + half) return r;
  }
  return -1;
}
static void frRow(uint8_t r, const char *label, bool dim) {
  int half = frRowHalf(r), y = frRowTop(r);
  gfx->fillRoundRect(CX - half, y, half * 2, FR_ROW_H, 14, dim ? UI_TRACK : UI_BG_DAY);
  gfx->drawRoundRect(CX - half, y, half * 2, FR_ROW_H, 14, UI_INK);
  gfx->setTextColor(dim ? 0x8410 : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(label) * 6, y + FR_ROW_H / 2 - 8);
  gfx->print(label);
}

// Leaving on purpose: goodbye to the peer, the radio off. Only ever touches the
// link if a FRIENDS session owns it -- a LAN battle is none of our business.
void friendsLeave() {
  frPicking = false;
  if (!friends.active) return;
  if (lan.live()) lan.sendBye();
  linkNowEnd();
  lan.state = LINK_OFF;
  friends.sessionEnd();
}
void friendsOpenScreen() { friendsOpen = true; frPicking = false; frPickIdx = 0; }
// Back out to the LAN screen, ending the session.
void friendsClose() { friendsLeave(); friendsOpen = false; lanOpen = true; }

// Once a loop, from loop(): the link itself is pumped just above it.
void friendsLoop(uint32_t now) {
  if (!friends.active) return;
  friends.poll(lan, pet, bag, frDay(), now);
  // A peer that left, went quiet or was refused: the radio costs current, so
  // it goes off now. The session stays "active" so the screen can say why.
  if (!lan.live() && linkNowUp()) linkNowEnd();
}

static void friendsConnect(bool host) {
  lan.begin(host, pet.trainerName);
  friends.sessionStart(lan, pet);     // mode + our one-creature card, before the radio
  if (!linkNowBegin(&lan)) {          // no radio: say so rather than hang
    lan.state = LINK_REFUSED;
    lan.refusedWhy = LINK_WHY_RADIO;
    return;
  }
  lan.start();
}

static bool frSessionOn() {
  return friends.active && lan.mode == LINK_MODE_FRIENDS && lan.state != LINK_OFF;
}

static const char *frItemName(uint8_t it) {
  return friendsGiftable(it) ? ITEM_TBL[it].name : "?";
}

// A creature centred on (cx, cy): its thumbnail when the pack is here,
// otherwise its dex number in a ring -- the graceful no-art path.
static void frDrawMon(int16_t dex, int cx, int cy) {
  if (dex < 1 || dex > DEX_COUNT) return;
  const uint8_t *th = nullptr;
  if (speciesHasArt(dex) && regionAvailable(regionOfDex(dex))) th = thumbs.get(dex);
  if (th) { drawThumb(th, cx - 40, cy - 40, 2, false); return; }
  gfx->drawCircle(cx, cy, 30, UI_INK);
  char b[8];
  snprintf(b, sizeof(b), "%03d", dex);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(cx - 18, cy - 8);
  gfx->print(b);
}

static void frSmallText(int cx, int y, const char *t, uint16_t col) {
  gfx->setTextColor(col);
  gfx->setTextSize(2);
  gfx->setCursor(cx - (int)strlen(t) * 6, y);
  gfx->print(t);
}

static void renderFriendsVisit() {
  featBegin(T(S_FR_VISIT));
  int16_t me = pet.isEgg() ? 0 : pet.speciesId;
  frDrawMon(me, CX - 80, 170);
  frDrawMon(friends.peerMon.dex, CX + 80, 170);
  char b[32];
  snprintf(b, sizeof(b), "%s%s", pet.nick[0] ? pet.nick : DEX_TBL[me].name, pet.shiny ? "*" : "");
  frSmallText(CX - 80, 230, b, UI_INK);
  snprintf(b, sizeof(b), "LV %u", pet.level());
  frSmallText(CX - 80, 252, b, UI_TRACK);
  snprintf(b, sizeof(b), "%s%s", friends.peerMon.name[0] ? friends.peerMon.name
                                                         : DEX_TBL[friends.peerMon.dex].name,
           friends.peerMon.shiny ? "*" : "");
  frSmallText(CX + 80, 230, b, UI_INK);
  snprintf(b, sizeof(b), "LV %u", friends.peerMon.level);
  frSmallText(CX + 80, 252, b, UI_TRACK);
  if (friends.grantedVnum == friends.vnum) {
    if (friends.visitBonus) snprintf(b, sizeof(b), T(S_FR_BONUS), FRIEND_VISIT_JOY, friends.visitBond);
    else snprintf(b, sizeof(b), "%s", T(S_FR_NOBONUS));
    featLine(300, b, UI_BAR_OK);
  } else {
    featLine(300, T(S_FR_WAITING), UI_TRACK);
  }
  featBackPill();
  gfx->flush();
}

// The line under the GIFT row: where the swap stands, in one phrase.
static void frGiftStatus(char *b, size_t n, uint16_t &col) {
  col = UI_TRACK;
  if (friends.settled) {
    snprintf(b, n, "%s", friends.settledGot ? T(S_FR_ARRIVED) : T(S_FR_RETURNED));
    col = UI_BAR_OK;
    if (friends.gph == FG_IDLE) return;
  }
  switch (friends.gph) {
    case FG_OFFER: snprintf(b, n, "%s", T(S_FR_WAITING)); return;
    case FG_PREPARED: case FG_COMMITTED: snprintf(b, n, "%s", T(S_FR_SWAPPING)); return;
    case FG_DONE:
      snprintf(b, n, T(S_FR_GOT), frItemName(friends.gotItem));
      col = UI_BAR_OK;
      return;
    case FG_NO:
      col = UI_BAR_BAD;
      snprintf(b, n, "%s", friends.gNo == FN_BAD_ITEM ? T(S_FR_BAD)
                         : friends.gNo == FN_FULL ? T(S_FR_FULL) : T(S_FR_CALLED));
      return;
    default: break;
  }
  if (friends.settled) return;
  switch (friends.giftBlock(frDay())) {
    case FN_DAY_USED: snprintf(b, n, "%s", T(S_FR_USED)); return;
    case FN_PENDING: snprintf(b, n, "%s", T(S_FR_TRANSIT)); col = UI_BAR_BAD; return;
    case FN_SAME_ID: snprintf(b, n, "%s", T(S_FR_SAMEID)); col = UI_BAR_BAD; return;
    default: break;
  }
  uint8_t po = friends.peerOffer();
  if (po) snprintf(b, n, T(S_FR_OFFERS), frItemName(po));
  else if (friends.havePeer && !friends.peerCanGift) snprintf(b, n, "%s", T(S_FR_PEER_USED));
  else b[0] = 0;
}

static void renderFriendsPick() {
  featBegin(T(S_FR_GIFT));
  uint8_t list[ITEM_COUNT];
  uint8_t n = friendsGiftList(bag, list, ITEM_COUNT);
  char b[32];
  if (n) {
    if (frPickIdx >= n) frPickIdx = 0;
    snprintf(b, sizeof(b), "< %s x%u >", ITEM_TBL[list[frPickIdx]].name,
             (unsigned)bag.count(list[frPickIdx]));
  } else {
    snprintf(b, sizeof(b), "%s", T(S_FR_SMALL));   // an empty bag gives a small gift
  }
  frRow(0, b, false);
  frRow(1, T(S_FR_OFFER), false);
  featBackPill();
  uiChrome();
  gfx->flush();
}

void renderFriends() {
  uint32_t now = millis();
  bool on = frSessionOn();
  if (on && lan.state == LINK_READY && friends.visiting(now)) { renderFriendsVisit(); return; }
  if (on && lan.state == LINK_READY && frPicking) { renderFriendsPick(); return; }
  featBegin(T(S_FR_TITLE));

  if (!on || lan.state == LINK_REFUSED || lan.state == LINK_LOST) {
    const char *msg = T(S_FR_PICK);
    uint16_t col = UI_TRACK;
    if (on && lan.state == LINK_REFUSED) {
      col = UI_BAR_BAD;
      switch (lan.refusedWhy) {
        case LINK_WHY_RADIO: msg = T(S_FR_NORADIO); break;
        case LINK_WHY_MODE: msg = T(S_FR_MODE); break;
        case LINK_WHY_SAMEID: msg = T(S_FR_SAMEID); break;
        default: msg = T(S_FR_VERSION); break;    // proto or build: update both
      }
    } else if (on && lan.state == LINK_LOST) {
      col = UI_BAR_BAD;
      msg = T(S_FR_GONE);
    }
    featLine(76, msg, col);
    // A swap cut off halfway says what happens next rather than nothing.
    if (on && friends.gph == FG_PREPARED) featLine(100, T(S_FR_LATER), UI_TRACK);
    frRow(0, T(S_LAN_HOST), false);
    frRow(1, T(S_LAN_JOIN), false);
    if (friends.rec.state != FR_NONE) featLine(frRowTop(1) + FR_ROW_H + 14, T(S_FR_TRANSIT), UI_TRACK);
    featBackPill();
    uiChrome();
    gfx->flush();
    return;
  }

  if (lan.state != LINK_READY) {          // pairing
    featLine(76, T(S_FR_WAIT), UI_TRACK);
    featBackPill();
    uiChrome();
    gfx->flush();
    return;
  }

  // Paired: who, their creature, and the two things to do together.
  char b[40];
  featLine(72, lan.peerName[0] ? lan.peerName : "?", UI_INK);
  if (friends.peerMon.valid) {
    snprintf(b, sizeof(b), "%s LV %u%s", DEX_TBL[friends.peerMon.dex].name,
             friends.peerMon.level, friends.peerMon.shiny ? " *" : "");
    featLine(96, b, UI_TRACK);
  }
  bool giftDim = !(friends.gph == FG_IDLE && friends.giftBlock(frDay()) == FN_NONE &&
                   friends.havePeer);
  frRow(0, T(S_FR_GIFT), giftDim);
  uint16_t col;
  frGiftStatus(b, sizeof(b), col);
  if (b[0]) featLine(frRowTop(0) + FR_ROW_H + 8, b, col);
  frRow(1, T(S_FR_VISIT), !friends.canMeet());
  int noteY = frRowTop(1) + FR_ROW_H + 8;
  if (!friends.canMeet() && friends.havePeer) featLine(noteY, T(S_FR_EGG), UI_TRACK);
  else if (friends.havePeer && !friends.visitBonusDue(frDay())) featLine(noteY, T(S_FR_NOBONUS), UI_TRACK);
  featBackPill();
  uiChrome();
  gfx->flush();
}

void friendsTap(int16_t x, int16_t y) {
  bool on = frSessionOn();
  if (on && lan.state == LINK_READY && friends.visiting(millis())) {
    if (featBackHit(x, y)) { sfxPlay(SFX_TAP); friends.visitAt = 0; }   // done looking
    return;
  }
  if (on && lan.state == LINK_READY && frPicking) {
    if (featBackHit(x, y)) { sfxPlay(SFX_TAP); frPicking = false; return; }
    uint8_t list[ITEM_COUNT];
    uint8_t n = friendsGiftList(bag, list, ITEM_COUNT);
    int r = frRowAt(x, y);
    if (r == 0 && n) { sfxPlay(SFX_TAP); frPickIdx = (uint8_t)((frPickIdx + 1) % n); return; }
    if (r == 1) {
      uint8_t it = n ? list[frPickIdx < n ? frPickIdx : 0] : 0;
      bool ok = friends.offerGift(lan, bag, it, frDay());
      sfxPlay(ok ? SFX_TAP : SFX_DENY);
      frPicking = false;
    }
    return;
  }
  if (featBackHit(x, y)) { sfxPlay(SFX_TAP); friendsClose(); return; }
  int r = frRowAt(x, y);
  if (!on || lan.state == LINK_REFUSED || lan.state == LINK_LOST) {
    if (r == 0 || r == 1) {
      sfxPlay(SFX_TAP);
      friendsLeave();               // a refused / lost session is closed first
      friendsConnect(r == 0);
    }
    return;
  }
  if (lan.state != LINK_READY) return;
  if (r == 0) {
    if (friends.gph == FG_IDLE && friends.havePeer &&
        friends.giftBlock(frDay()) == FN_NONE) {
      sfxPlay(SFX_TAP);
      frPicking = true;
      frPickIdx = 0;
    } else {
      sfxPlay(SFX_DENY);
    }
  } else if (r == 1) {
    sfxPlay(friends.startVisit(lan, pet, millis()) ? SFX_TAP : SFX_DENY);
  }
}

// Down is BACK, one level: the chooser or the visit first, then the screen.
void friendsSwipeDown() {
  if (frPicking) { frPicking = false; return; }
  if (friends.visiting(millis())) { friends.visitAt = 0; return; }
  friendsClose();
}
