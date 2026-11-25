// ---------- the ARCADE's fifth and sixth games: PINBALL and SURF ----------
// The arcade's session rules (grades, prizes, banking) are ui_arcade.ino's;
// this tab only plays and draws the two games. Both are free, like the
// other four, and pay joy, bond and the daily rank prizes through arcFinish().
//
//   PINBALL  hold the LEFT half of the panel for the left flipper, the RIGHT
//            half for the right. Three balls. The physics are pinball.cpp
//            (headless; pinball_test plays thousands of balls through it).
//            Points are score / PB_PTS_DIV.
//   SURF     the creature YOU are raising rides the wave (its own sprite,
//            trnPet()). Drag up and down to steer: catch stars and shells,
//            dodge the rocks. 45 s, three lives.
//
// The panel is AMOLED: black really is off, so both games use saturated
// colour on deep, dark grounds rather than the cream the menus use.

// =====================================================================
// PINBALL
// =====================================================================
static Pinball arcPb;
#define PIN_TRAIL 6
static float pinTrailX[PIN_TRAIL], pinTrailY[PIN_TRAIL];
static uint8_t pinTrailN = 0;
static uint8_t pinMultShown = 1;
// The three bumpers wear a type each; it picks the icon and the colours.
static const uint8_t PIN_BUMPER_TYPE[PB_BUMPERS] = { T_ELECTRIC, T_FIRE, T_WATER };

void pinStart() {
  arcPb.start();
  arcPb.serveMs = 0;
  pinTrailN = 0;
  pinMultShown = 1;
}
// A ball rolling: swipes do not leave (a flipper press that slides would end
// the game), and taps do nothing (the flippers are HELD, not tapped).
bool pinBallLive() { return arcPb.state == PB_ROLL; }
uint8_t pinState() { return arcPb.state; }
// The flippers, from every touch sample: left half, left flipper.
void pinPeek(bool pressed, int16_t x) {
  arcPb.upL = pressed && x < CX;
  arcPb.upR = pressed && x >= CX;
}
// For tests.
Pinball &arcPinball() { return arcPb; }

// Advance the table by dt and turn what happened into points, sound and
// sparkle. True when the last ball has gone.
bool pinStep(uint32_t dt) {
  arcPb.step(dt);
  uint16_t e = arcPb.events;   // 16 bits: ORBIT and everything after it live above 255
  arcPb.events = 0;
  uint32_t pts = arcPb.score / PB_PTS_DIV;
  arcPts = (uint16_t)(pts > 65000 ? 65000 : pts);
  if (e & PBE_BUMPER) {
    sfxPlay(SFX_HIT);
    for (uint8_t i = 0; i < PB_BUMPERS; i++)
      if (arcPb.flash[i] >= 170) {
        float x, y; pbBumper(i, &x, &y);
        trnBurst((int)x, (int)y, fxTypeCol(PIN_BUMPER_TYPE[i], 3), 8, 4.0f);
        char b[8]; snprintf(b, sizeof(b), "+%u", (unsigned)(100 * arcPb.mult));
        trnPopup((int)x, (int)y - 26, b, UI_WHITE, 2);
      }
  }
  if (e & PBE_LANE) { sfxPlay(SFX_STATUS); trnBurst((int)arcPb.bx, (int)arcPb.by, C565(0xff, 0xd8, 0x30), 10, 3.5f); }
  if (e & PBE_SLING) sfxPlay(SFX_TAP);
  if (e & PBE_INLANE) trnPopup((int)arcPb.bx, (int)arcPb.by - 20, "+100", C565(0xff, 0xe0, 0x40), 1);
  if (e & PBE_DROP) { sfxPlay(SFX_HIT); trnBurst((int)arcPb.bx - 10, (int)arcPb.by, C565(0x30, 0xff, 0x90), 8, 3.0f); }
  if (e & PBE_BANK) { sfxPlay(SFX_SUPER); trnBanner(T(S_PB_BANK), C565(0x30, 0xff, 0x90), 900); }
  if (e & PBE_ORBIT) { sfxPlay(SFX_LEVEL); trnPopup((int)arcPb.bx, (int)arcPb.by + 24, T(S_PB_ORBIT), C565(0x90, 0xf0, 0xff), 2); }
  if (e & PBE_CATCH) { sfxPlay(SFX_MEDAL); trnBanner(T(S_PB_CATCH), C565(0xff, 0x40, 0x40), 900); trnJolt(3, 160); }
  if (e & PBE_EJECT) sfxPlay(SFX_HIT);
  if (e & PBE_BONUS) {
    sfxPlay(SFX_SUPER);
    char b[16]; snprintf(b, sizeof(b), "x%u!", (unsigned)arcPb.mult);
    trnBanner(b, C565(0xff, 0x40, 0xd0), 900);
    trnJolt(4, 220);
  }
  if (e & PBE_DRAIN) { sfxPlay(SFX_FAINT); pinTrailN = 0; }
  if (e & PBE_FLIP) sfxPlay(SFX_TAP);
  if (arcPb.state == PB_ROLL) {
    for (int i = PIN_TRAIL - 1; i > 0; i--) { pinTrailX[i] = pinTrailX[i - 1]; pinTrailY[i] = pinTrailY[i - 1]; }
    pinTrailX[0] = arcPb.bx; pinTrailY[0] = arcPb.by;
    if (pinTrailN < PIN_TRAIL) pinTrailN++;
  }
  return arcPb.state == PB_OVER;
}

// HSV-ish rainbow: h 0..255 round the colour wheel, full saturation.
static uint16_t pinHue(uint8_t h) {
  uint8_t s = h / 43, f = (uint8_t)((h - s * 43) * 6);
  uint8_t q = 255 - f;
  switch (s) {
    case 0: return C565(255, f, 0);
    case 1: return C565(q, 255, 0);
    case 2: return C565(0, 255, f);
    case 3: return C565(0, q, 255);
    case 4: return C565(f, 0, 255);
    default: return C565(255, 0, q);
  }
}
static void pinPokeBall(int cx, int cy, int r, bool lit, uint32_t now) {
  if (lit) {   // a gold halo that pulses
    int g = r + 4 + (int)((now / 90) % 4);
    gfx->fillCircle(cx, cy, g, C565(0x80, 0x60, 0x00));
    gfx->fillCircle(cx, cy, r + 3, C565(0xff, 0xd0, 0x20));
  }
  gfx->fillCircle(cx, cy, r, UI_INK);
  gfx->fillCircle(cx, cy, r - 1, UI_WHITE);
  for (int dy = -r + 1; dy < 0; dy++) {          // the red top half
    int hw = (int)sqrtf((float)((r - 1) * (r - 1) - dy * dy));
    gfx->drawFastHLine(cx - hw, cy + dy, hw * 2 + 1, lit ? C565(0xff, 0x30, 0x30) : C565(0xe0, 0x20, 0x30));
  }
  gfx->drawFastHLine(cx - r + 1, cy, 2 * r - 1, UI_INK);
  gfx->fillCircle(cx, cy, r / 3 + 1, UI_INK);
  gfx->fillCircle(cx, cy, r / 3, UI_WHITE);
}
static void pinFlipper(bool right) {
  float px, py, tx, ty;
  arcPb.flipperEnds(right, &px, &py, &tx, &ty);
  float dx = tx - px, dy = ty - py, l = sqrtf(dx * dx + dy * dy);
  float nx = -dy / l, ny = dx / l;
  const uint16_t edge = UI_WHITE, body = right ? C565(0xff, 0x40, 0xb0) : C565(0xff, 0x8a, 0x20);
  for (int k = 0; k < 2; k++) {   // white outline, then the colour inside it
    float w = k ? PB_FLIP_W - 1.5f : PB_FLIP_W + 1.0f, wt = w * 0.6f;
    uint16_t c = k ? body : edge;
    int ax = (int)(px + nx * w), ay = (int)(py + ny * w), bx = (int)(px - nx * w), by = (int)(py - ny * w);
    int cx2 = (int)(tx + nx * wt), cy2 = (int)(ty + ny * wt), dx2 = (int)(tx - nx * wt), dy2 = (int)(ty - ny * wt);
    gfx->fillTriangle(ax, ay, bx, by, cx2, cy2, c);
    gfx->fillTriangle(bx, by, cx2, cy2, dx2, dy2, c);
    gfx->fillCircle((int)px, (int)py, (int)w, c);
    gfx->fillCircle((int)tx, (int)ty, (int)wt, c);
  }
  gfx->fillCircle((int)px, (int)py, 2, C565(0xff, 0xff, 0x80));
}

void renderPinballPlay(uint32_t now, bool intro) {
  int jx = trnJx(now), jy = trnJy(now);
  gfx->fillScreen(RGB565_BLACK);
  // the playfield: a deep indigo to magenta glow, row by row inside the rim
  const uint16_t top = C565(0x10, 0x08, 0x50), mid = C565(0x38, 0x06, 0x70), bot = C565(0x70, 0x04, 0x48);
  for (int y = PB_CY - (int)PB_WALL_R; y < PB_CY + (int)PB_WALL_R; y += 3) {
    int dy = y - PB_CY;
    int hw = (int)sqrtf(PB_WALL_R * PB_WALL_R - (float)(dy * dy));
    int t = y - (PB_CY - (int)PB_WALL_R);
    uint16_t c = t < (int)PB_WALL_R ? lerp565(top, mid, t, (int)PB_WALL_R) : lerp565(mid, bot, t - (int)PB_WALL_R, (int)PB_WALL_R);
    gfx->fillRect(PB_CX - hw + jx, y + jy, 2 * hw, 3, c);
  }
  // twinkling stars in the glass
  for (uint8_t k = 0; k < 18; k++) {
    uint32_t h = (uint32_t)k * 2654435761u;
    int sx = 90 + (int)(h % 290), sy = 70 + (int)((h >> 9) % 230);
    if (((now / 180) + k) % 5) gfx->fillRect(sx + jx, sy + jy, 1, 1, C565(0xa0, 0xa0, 0xff));
    else gfx->fillCircle(sx + jx, sy + jy, 1, UI_WHITE);
  }
  // the rim: a rainbow that turns
  uint8_t spin = (uint8_t)(now / 12);
  for (int a = 0; a < 360; a += 4) {
    float r = a * 0.01745329f;
    int x = PB_CX + (int)(cosf(r) * (PB_WALL_R + 4)), y = PB_CY + (int)(sinf(r) * (PB_WALL_R + 4));
    if (y > PB_GUIDE_Y + 8) continue;
    gfx->fillCircle(x + jx, y + jy, 3, pinHue((uint8_t)(a * 255 / 360 + spin)));
  }
  // the inlane guides, in neon green, and the dark below them
  for (int g = 0; g < 2; g++) {
    float x0, y0, x1, y1; pbGuide(g == 1, &x0, &y0, &x1, &y1);
    float ex = g ? 466 : 0;
    gfx->fillTriangle((int)x0 + jx, (int)y0 + jy, (int)x1 + jx, (int)y1 + jy, (int)ex, 466, RGB565_BLACK);
    gfx->fillTriangle((int)x0 + jx, (int)y0 + jy, (int)ex, (int)y0 - 30, (int)ex, 466, RGB565_BLACK);
    for (int k = -1; k <= 1; k++)
      gfx->drawLine((int)x0 + jx, (int)y0 + k + jy, (int)x1 + jx, (int)y1 + k + jy, k ? C565(0x20, 0xa0, 0x40) : C565(0x80, 0xff, 0x90));
  }
  // every fixed wall: lane posts and the orbit in neon cyan, the slingshots
  // as glowing triangles that flash when they fire
  {
    const PbWall *W; uint8_t n = pbWalls(&W);
    for (uint8_t i = 0; i < n; i++) {
      if (W[i].kind == PBW_SLING) continue;
      if (W[i].y0 >= PB_GUIDE_Y - 1 && W[i].r < 3) continue;   // the guides are drawn above
      for (int k = -1; k <= 1; k++)
        gfx->drawLine((int)W[i].x0 + k + jx, (int)W[i].y0 + jy, (int)W[i].x1 + k + jx, (int)W[i].y1 + jy,
                      k ? C565(0x10, 0x90, 0xc0) : C565(0x90, 0xf0, 0xff));
    }
    for (int g = 0; g < 2; g++) {
      float ax, ay, bx, by, cx, cy; pbSlingshot(g == 1, &ax, &ay, &bx, &by, &cx, &cy);
      bool hot = arcPb.slingFlash[g] > 0;
      gfx->fillTriangle((int)ax + jx, (int)ay + jy, (int)bx + jx, (int)by + jy, (int)cx + jx, (int)cy + jy,
                        hot ? UI_WHITE : C565(0x30, 0x10, 0x60));
      uint16_t e = hot ? C565(0xff, 0xff, 0x80) : C565(0xff, 0x50, 0xd0);
      gfx->drawLine((int)ax + jx, (int)ay + jy, (int)cx + jx, (int)cy + jy, e);
      gfx->drawLine((int)ax + jx + (g ? -1 : 1), (int)ay + jy, (int)cx + jx + (g ? -1 : 1), (int)cy + jy, e);
      gfx->drawLine((int)ax + jx, (int)ay + jy, (int)bx + jx, (int)by + jy, C565(0x90, 0xf0, 0xff));
      gfx->drawLine((int)bx + jx, (int)by + jy, (int)cx + jx, (int)cy + jy, C565(0x90, 0xf0, 0xff));
      float ix, iy; pbInlane(g == 1, &ix, &iy);   // the inlane rollover: a little arrow
      gfx->fillTriangle((int)ix - 5 + jx, (int)iy - 4 + jy, (int)ix + 5 + jx, (int)iy - 4 + jy, (int)ix + (g ? -2 : 2) + jx, (int)iy + 5 + jy, C565(0xff, 0xe0, 0x40));
    }
  }
  // the top lanes: a light under each, lit gold
  for (uint8_t i = 0; i < PB_LANES; i++) {
    float x, y; pbLane(i, &x, &y);
    bool on = arcPb.lit[i];
    if (on) gfx->fillCircle((int)x + jx, (int)y + jy, 9, C565(0x80, 0x60, 0x00));
    gfx->fillCircle((int)x + jx, (int)y + jy, 6, on ? C565(0xff, 0xd8, 0x30) : C565(0x40, 0x30, 0x60));
    gfx->drawCircle((int)x + jx, (int)y + jy, 6, on ? UI_WHITE : C565(0x90, 0x80, 0xc0));
  }
  // the orbit's top: an arrow that glows after a ride
  { float ox, oy; pbOrbitSensor(&ox, &oy);
    uint16_t c = arcPb.orbits ? pinHue((uint8_t)(now / 6)) : C565(0x60, 0x50, 0x90);
    gfx->fillTriangle((int)ox - 6 + jx, (int)oy - 6 + jy, (int)ox - 6 + jx, (int)oy + 6 + jy, (int)ox - 14 + jx, (int)oy + jy, c); }
  // the drop targets: bright when standing, a dim slot when down
  for (uint8_t i = 0; i < PB_DROPS; i++) {
    float x0, y0, x1, y1; pbDrop(i, &x0, &y0, &x1, &y1);
    if (arcPb.dropUp[i]) {
      gfx->fillRect((int)x0 - 3 + jx, (int)y0 + jy, 7, (int)(y1 - y0), C565(0x30, 0xff, 0x90));
      gfx->drawRect((int)x0 - 3 + jx, (int)y0 + jy, 7, (int)(y1 - y0), UI_WHITE);
    } else {
      gfx->fillRect((int)x0 - 1 + jx, (int)y0 + jy, 3, (int)(y1 - y0), C565(0x20, 0x40, 0x30));
    }
  }
  // the CATCH saucer: a Poke Ball sunk into the table, shut while it holds one
  { float sx, sy; pbSaucer(&sx, &sy);
    gfx->fillCircle((int)sx + jx, (int)sy + jy, (int)PB_SAUCER_R + 5, C565(0x08, 0x04, 0x20));
    pinPokeBall((int)sx + jx, (int)sy + jy, (int)PB_SAUCER_R, arcPb.saucerMs > 0, now); }
  // the bumpers: three types, glowing in their colours, white-hot when struck
  for (uint8_t i = 0; i < PB_BUMPERS; i++) {
    float x, y; pbBumper(i, &x, &y);
    uint8_t ty = PIN_BUMPER_TYPE[i];
    bool hot = arcPb.flash[i] > 0;
    int bx = (int)x + jx, by = (int)y + jy, r = (int)PB_BUMPER_R;
    gfx->fillCircle(bx, by, r + 7, fxTypeCol(ty, 0));
    gfx->fillCircle(bx, by, r + 4, hot ? UI_WHITE : fxTypeCol(ty, 3));
    gfx->fillCircle(bx, by, r, hot ? fxTypeCol(ty, 3) : fxTypeCol(ty, 1));
    gfx->drawCircle(bx, by, r - 3, UI_WHITE);
    fxTypeIcon(ty, bx, by, 2);
  }
  pinFlipper(false);
  pinFlipper(true);
  // the ball, chrome, with a fading trail
  if (arcPb.state == PB_ROLL) {
    for (int i = pinTrailN - 1; i >= 1; i--) {
      uint16_t c = lerp565(C565(0x60, 0xe0, 0xff), C565(0x18, 0x08, 0x50), i, PIN_TRAIL);
      gfx->fillCircle((int)pinTrailX[i] + jx, (int)pinTrailY[i] + jy, (int)PB_BALL_R - i, c);
    }
    int bx = (int)arcPb.bx + jx, by = (int)arcPb.by + jy;
    gfx->fillCircle(bx, by, (int)PB_BALL_R, C565(0xb0, 0xb8, 0xc8));
    gfx->fillCircle(bx + 1, by + 1, (int)PB_BALL_R - 2, C565(0x70, 0x78, 0x90));
    gfx->fillCircle(bx - 2, by - 2, 2, UI_WHITE);
  }
  // the HUD: score, the balls left, the multiplier in its own colour
  char b[20];
  snprintf(b, sizeof(b), "%lu", (unsigned long)arcPb.score);
  trnTextO(b, CX, 20, 3, C565(0xff, 0xe0, 0x40));
  // the balls left and the multiplier sit under the flippers, clear of the top lanes
  for (uint8_t i = 0; i < PB_BALLS; i++) {
    bool have = i < arcPb.balls;
    gfx->fillCircle(CX - 18 + i * 18, 438, 6, have ? C565(0xd0, 0xd8, 0xe8) : C565(0x30, 0x30, 0x40));
    if (have) gfx->fillCircle(CX - 20 + i * 18, 436, 2, UI_WHITE);
  }
  if (arcPb.mult > 1) {
    snprintf(b, sizeof(b), "x%u", (unsigned)arcPb.mult);
    gfx->fillRoundRect(CX + 34, 429, 34, 18, 9, pinHue((uint8_t)(now / 8)));
    trnText(b, CX + 51, 434, 1, RGB565_BLACK);
  }
  pinMultShown = arcPb.mult;
  // between balls: say so
  if (!intro && arcPb.state == PB_SERVE && arcPb.balls < PB_BALLS) {
    snprintf(b, sizeof(b), T(S_PB_BALL_FMT), (unsigned)(PB_BALLS - arcPb.balls + 1), (unsigned)PB_BALLS);
    trnTextO(b, CX, 300, 2, UI_WHITE);
  }
}

// =====================================================================
// SURF
// =====================================================================
#define SURF_OBJS 8
#define SURF_HOR 168             // where the sky meets the sea
#define SURF_Y0 226              // the band the board can ride in
#define SURF_Y1 372
#define SURF_PX 150              // the surfer's x: the sea comes to it
#define SURF_LIVES 3
#define SURF_STEER 0.55f         // px per ms toward the finger
#define SURF_HIT_X 28
#define SURF_HIT_Y 22
#define SURF_SHELL_PCT 9
#define SURF_ROCK_PCT 30
#define SURF_ROCK_FROM 4000UL
#define SURF_SAFE_MS 1000UL      // blinking, after a rock
enum : uint8_t { SURF_NONE = 0, SURF_STAR, SURF_SHELL, SURF_ROCK };
enum : uint8_t { SURF_EV_STAR = 1, SURF_EV_SHELL = 2, SURF_EV_ROCK = 4, SURF_EV_MISS = 8 };

uint8_t surfK[SURF_OBJS];
float surfX[SURF_OBJS], surfY[SURF_OBJS];
float surfPetY = (SURF_Y0 + SURF_Y1) / 2, surfGoal = (SURF_Y0 + SURF_Y1) / 2;
uint8_t surfLives = SURF_LIVES;
uint16_t surfStars = 0, surfShells = 0, surfRocks = 0;
static uint32_t surfNext = 0, surfSafeUntil = 0, surfClock = 0;

static float surfClampY(float y) { return y < SURF_Y0 ? SURF_Y0 : y > SURF_Y1 ? SURF_Y1 : y; }
void surfStart() {
  surfPetY = surfGoal = (SURF_Y0 + SURF_Y1) / 2;
  surfLives = SURF_LIVES;
  surfStars = surfShells = surfRocks = 0;
  for (auto &k : surfK) k = SURF_NONE;
  surfClock = 0;
  surfNext = 300;
  surfSafeUntil = 0;
}
void surfPeek(bool pressed, int16_t y) { if (pressed && y >= ARC_HEADER_Y) surfGoal = surfClampY(y); }
void surfTap(int16_t x, int16_t y) { (void)x; if (y >= ARC_HEADER_Y) surfGoal = surfClampY(y); }
uint8_t surfLivesLeft() { return surfLives; }
// The sea's speed: it quickens through the session.
static float surfSpeed() { return 0.20f + 0.20f * (float)(surfClock > SURF_MS ? SURF_MS : surfClock) / SURF_MS; }   // px per ms
void surfSpawnAt(uint8_t i, uint8_t kind, float y) { surfK[i] = kind; surfX[i] = 500; surfY[i] = surfClampY(y); }

// Advance the sea by dt ms (the clock is the session's own, so a test can
// drive it without real time). Returns SURF_EV_* bits.
uint8_t surfStep(uint32_t dt) {
  if (dt > 200) dt = 200;
  surfClock += dt;
  uint8_t ev = 0;
  float d = surfGoal - surfPetY, mx = SURF_STEER * dt;
  surfPetY += d > mx ? mx : (d < -mx ? -mx : d);
  float v = surfSpeed();
  for (uint8_t i = 0; i < SURF_OBJS; i++) {
    if (!surfK[i]) continue;
    surfX[i] -= v * dt;
    bool near = fabsf(surfX[i] - SURF_PX) < SURF_HIT_X && fabsf(surfY[i] - surfPetY) < SURF_HIT_Y;
    if (near) {
      if (surfK[i] == SURF_ROCK) {
        if (surfClock >= surfSafeUntil) {
          if (surfLives) surfLives--;
          surfRocks++;
          surfSafeUntil = surfClock + SURF_SAFE_MS;
          ev |= SURF_EV_ROCK;
        }
      } else if (surfK[i] == SURF_SHELL) { surfShells++; ev |= SURF_EV_SHELL; surfK[i] = SURF_NONE; continue; }
      else { surfStars++; ev |= SURF_EV_STAR; surfK[i] = SURF_NONE; continue; }
    }
    if (surfX[i] < -30) {
      if (surfK[i] == SURF_STAR) ev |= SURF_EV_MISS;
      surfK[i] = SURF_NONE;
    }
  }
  if (surfClock >= surfNext) {
    for (uint8_t i = 0; i < SURF_OBJS; i++) {
      if (surfK[i]) continue;
      int r = (int)random(100);
      uint8_t k = (surfClock >= SURF_ROCK_FROM && r < SURF_ROCK_PCT) ? SURF_ROCK
                : ((int)random(100) < SURF_SHELL_PCT ? SURF_SHELL : SURF_STAR);
      surfSpawnAt(i, k, SURF_Y0 + (float)random(SURF_Y1 - SURF_Y0 + 1));
      break;
    }
    int gap = 760 - (int)(surfClock * 9 / 1000);   // 760 ms between, down to 360
    surfNext = surfClock + (gap < 360 ? 360 : gap);
  }
  return ev;
}
bool surfOver() { return surfLives == 0; }

// The session's points: a star is 1 + the combo's multiplier - 1, a shell 5.
void surfScore(uint8_t ev) {
  if (ev & SURF_EV_STAR) {
    trnComboUp();
    arcPts += trnMult(trnCombo);
    trnPopup(SURF_PX + 20, (int)surfPetY - 60, "+1", C565(0xff, 0xe0, 0x40), 2);
    trnBurst(SURF_PX + 10, (int)surfPetY - 20, C565(0xff, 0xe8, 0x60), 8, 3.0f);
    sfxPlay(SFX_PLAY);
  }
  if (ev & SURF_EV_SHELL) {
    trnComboUp();
    arcPts += 5 + trnMult(trnCombo) - 1;
    trnPopup(SURF_PX + 20, (int)surfPetY - 60, "+5", C565(0xff, 0x70, 0xc0), 3);
    trnBurst(SURF_PX + 10, (int)surfPetY - 20, C565(0xff, 0x90, 0xd0), 14, 4.5f);
    sfxPlay(SFX_MEDAL);
  }
  if (ev & SURF_EV_ROCK) {
    trnComboBreak(SURF_PX, (int)surfPetY - 30);
    trnJolt(6, 300);
    sfxPlay(SFX_HIT);
  }
  if (ev & SURF_EV_MISS) trnComboBreak(40, (int)surfPetY - 30);
}

static void surfRock(int x, int y) {
  gfx->fillCircle(x, y + 2, 20, C565(0x18, 0x20, 0x30));
  gfx->fillCircle(x - 4, y - 2, 17, C565(0x6a, 0x6e, 0x80));
  gfx->fillCircle(x + 9, y + 4, 12, C565(0x52, 0x56, 0x68));
  gfx->fillCircle(x - 9, y - 9, 5, C565(0xa8, 0xac, 0xbc));
  for (int k = -22; k <= 22; k += 6) gfx->fillCircle(x + k, y + 16 + (k & 4 ? 1 : 0), 4, UI_WHITE);   // foam
}
static void surfShell(int x, int y, uint32_t now) {
  int b = (int)((now / 120) % 3);
  gfx->fillCircle(x, y, 17 + b, C565(0x80, 0x10, 0x60));
  for (int k = -2; k <= 2; k++) {
    int ex = x + k * 6, ey = y - 14 + (k < 0 ? -k : k) * 2;
    gfx->fillTriangle(x, y + 10, ex - 4, ey, ex + 4, ey, k & 1 ? C565(0xff, 0x80, 0xc0) : C565(0xff, 0xb0, 0xdc));
  }
  gfx->fillCircle(x, y + 9, 4, C565(0xff, 0xd0, 0xe8));
}

void renderSurfPlay(uint32_t now, bool intro, uint32_t dt) {
  if (!intro && !arcOver) {
    uint8_t ev = surfStep(dt);
    surfScore(ev);
    if (surfOver()) arcFinish();
  }
  int jx = trnJx(now), jy = trnJy(now);
  // the sky: a tropical gradient, a sun that glows
  for (int y = 0; y < SURF_HOR; y += 4)
    gfx->fillRect(0, y, 466, 4, lerp565(C565(0x08, 0x40, 0xd8), C565(0x70, 0xd8, 0xff), y, SURF_HOR));
  gfx->fillCircle(350 + jx, 124 + jy, 34, C565(0xff, 0xc8, 0x40));
  gfx->fillCircle(350 + jx, 124 + jy, 26, C565(0xff, 0xf0, 0x80));
  for (int k = 0; k < 3; k++) {   // drifting clouds
    int cx = (int)((k * 170 + 466 - (now / (40 + k * 15)) % 600)) - 60, cy = 70 + k * 26;
    gfx->fillCircle(cx, cy, 14, UI_WHITE);
    gfx->fillCircle(cx + 16, cy - 6, 18, UI_WHITE);
    gfx->fillCircle(cx + 34, cy, 13, UI_WHITE);
  }
  // the sea: deep at the bottom, turquoise at the horizon, rolling crests
  for (int y = SURF_HOR; y < 466; y += 4)
    gfx->fillRect(0, y, 466, 4, lerp565(C565(0x10, 0xc8, 0xf0), C565(0x02, 0x24, 0x80), y - SURF_HOR, 466 - SURF_HOR));
  float t = (float)now / 1000.0f;
  for (int w = 0; w < 6; w++) {
    int base = SURF_HOR + 14 + w * 46;
    float sp = 40.0f + w * 25.0f, amp = 3.0f + w * 1.5f;
    uint16_t c = w & 1 ? C565(0x9a, 0xf0, 0xff) : C565(0xe8, 0xfc, 0xff);
    for (int x = 0; x < 466; x += 6) {
      int y = base + (int)(amp * sinf((x + t * sp) / (36.0f + w * 6)));
      gfx->fillRect(x + jx, y + jy, 6, w < 2 ? 2 : 3, c);
    }
  }
  // what the sea brings
  for (uint8_t i = 0; i < SURF_OBJS; i++) {
    if (!surfK[i]) continue;
    int x = (int)surfX[i] + jx, y = (int)surfY[i] + jy;
    if (surfK[i] == SURF_ROCK) surfRock(x, y);
    else if (surfK[i] == SURF_SHELL) surfShell(x, y, now);
    else trnStar(x, y + (int)(3 * sinf(t * 6 + i)), 13, C565(0xff, 0xe0, 0x30));
  }
  // the surfer: YOUR creature, on a bright board, throwing spray behind it
  bool blink = surfClock < surfSafeUntil && ((now / 90) & 1);
  int py = (int)surfPetY + jy;
  for (int k = 1; k <= 6; k++) {
    int sx = SURF_PX - 30 - k * 9 + jx, sy = py + 4 - (int)((k * 7 + now / 40) % 9);
    gfx->fillCircle(sx, sy, 5 - k / 2, UI_WHITE);
  }
  if (!blink) {
    arcEllipse(SURF_PX + jx, py + 4, 38, 8, C565(0x20, 0x20, 0x30));
    arcEllipse(SURF_PX + jx, py + 2, 36, 7, C565(0xff, 0x3c, 0x8c));
    gfx->fillRect(SURF_PX - 30 + jx, py + 1, 60, 2, UI_WHITE);
    trnPet(PMD_WALKR, SURF_PX + jx, py - 2, now, true, 2);
  }
  trnHud(arcPts, now, arcUntil, SURF_MS, UI_WHITE);
  for (uint8_t i = 0; i < SURF_LIVES; i++) {   // lives as hearts, under the clock
    int hx = CX - 22 + i * 22, hy = 84;
    uint16_t c = i < surfLives ? C565(0xff, 0x30, 0x60) : C565(0x30, 0x30, 0x40);
    gfx->fillCircle(hx - 4, hy, 5, c); gfx->fillCircle(hx + 4, hy, 5, c);
    gfx->fillTriangle(hx - 9, hy + 1, hx + 9, hy + 1, hx, hy + 11, c);
  }
}


