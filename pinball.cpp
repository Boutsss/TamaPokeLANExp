// The arcade's pinball table. See pinball.h for the layout and the rules.
#include "pinball.h"
#include <Arduino.h>
#include <math.h>

// A triangle under the top lanes, clear of the flippers' line of fire.
static const float BUMPER_XY[PB_BUMPERS][2] = { { 233, 160 }, { 186, 210 }, { 280, 210 } };
static const float LANE_XY[PB_LANES][2] = { { 196, 70 }, { 233, 70 }, { 270, 70 } };
static const float SAUCER_XY[2] = { 148, 128 };
#define DROP_X 64.0f
static const float DROP_Y[PB_DROPS] = { 178, 204, 230 };   // each 18 tall
#define DROP_H 18.0f

void pbBumper(uint8_t i, float *x, float *y) { *x = BUMPER_XY[i % PB_BUMPERS][0]; *y = BUMPER_XY[i % PB_BUMPERS][1]; }
void pbLane(uint8_t i, float *x, float *y) { *x = LANE_XY[i % PB_LANES][0]; *y = LANE_XY[i % PB_LANES][1]; }
void pbSaucer(float *x, float *y) { *x = SAUCER_XY[0]; *y = SAUCER_XY[1]; }
void pbDrop(uint8_t i, float *x0, float *y0, float *x1, float *y1) {
  *x0 = *x1 = DROP_X; *y0 = DROP_Y[i % PB_DROPS]; *y1 = *y0 + DROP_H;
}

// An inlane guide: from the rim at PB_GUIDE_Y down to its flipper's pivot.
void pbGuide(bool right, float *x0, float *y0, float *x1, float *y1) {
  float dy = PB_GUIDE_Y - PB_CY;
  float dx = sqrtf(PB_WALL_R * PB_WALL_R - dy * dy);
  *x0 = right ? PB_CX + dx : PB_CX - dx;
  *y0 = PB_GUIDE_Y;
  // It ends FLUSH with the flipper's top, not at the pivot's centre: the
  // flipper is a capsule PB_FLIP_W thick and the guide 2, so ending at the
  // centre left the flipper's rounded end standing proud of the guide -- a
  // ledge every slow ball came to rest on, forever (pinball_test found 120).
  *x1 = (right ? PB_PIVOT_RX : PB_PIVOT_LX) + (right ? -2.0f : 2.0f);
  *y1 = PB_PIVOT_Y - (PB_FLIP_W - 2.0f);
}

// The slingshots: A top, B bottom outer, C bottom inner. A-C is the kicking
// face, toward the middle; the gap under B-C and above the guide is the inlane.
void pbSlingshot(bool right, float *ax, float *ay, float *bx, float *by, float *cx, float *cy) {
  float s = right ? -1.0f : 1.0f, o = right ? 466.0f : 0.0f;
  // The face slopes at ~37 degrees, so its kick goes UP and in, toward the
  // bumpers. A steeper face kicked flat across, into the other slingshot, and
  // pinball_test caught a ball ping-ponging between the two for 40 minutes.
  *ax = o + s * 98;  *ay = 290;
  *bx = o + s * 108; *by = 336;
  *cx = o + s * 146; *cy = 326;
}
void pbInlane(bool right, float *x, float *y) { *x = right ? 466 - 120 : 120; *y = 356; }
// The top of the orbit, where a ball that rode it is counted.
void pbOrbitSensor(float *x, float *y) { *x = 325; *y = 62; }

// Every fixed wall, built once: the guides, the posts between the top lanes,
// the orbit's inner wall (r = 180 round the right of the rim) and both
// slingshots.
static PbWall WALLS[16];
static uint8_t WALL_N = 0;
static void buildWalls() {
  if (WALL_N) return;
  auto add = [](float x0, float y0, float x1, float y1, float r, uint8_t k) { WALLS[WALL_N++] = { x0, y0, x1, y1, r, k }; };
  for (int g = 0; g < 2; g++) { float a, b, c, d; pbGuide(g == 1, &a, &b, &c, &d); add(a, b, c, d, 2, PBW_WALL); }
  for (int i = 0; i < PB_LANES + 1; i++) {   // four posts make three lanes
    float x = LANE_XY[0][0] - 18.5f + i * 37.0f;
    add(x, 54, x, 84, 3, PBW_WALL);
  }
  static const float ORB[4] = { 22, 0, -25, -50 };   // degrees
  for (int i = 0; i < 3; i++) {
    float a0 = ORB[i] * 0.01745329f, a1 = ORB[i + 1] * 0.01745329f;
    add(PB_CX + cosf(a0) * 180, PB_CY + sinf(a0) * 180, PB_CX + cosf(a1) * 180, PB_CY + sinf(a1) * 180, 3, PBW_WALL);
  }
  for (int g = 0; g < 2; g++) {
    float ax, ay, bx, by, cx, cy; pbSlingshot(g == 1, &ax, &ay, &bx, &by, &cx, &cy);
    add(ax, ay, bx, by, 3, PBW_WALL);
    add(bx, by, cx, cy, 3, PBW_WALL);
    add(ax, ay, cx, cy, 3, PBW_SLING);
  }
}
uint8_t pbWalls(const PbWall **out) { buildWalls(); *out = WALLS; return WALL_N; }

void Pinball::flipperEnds(bool right, float *px, float *py, float *tx, float *ty) const {
  float a = right ? aR : aL;
  *px = right ? PB_PIVOT_RX : PB_PIVOT_LX;
  *py = PB_PIVOT_Y;
  // the right flipper is the left one mirrored: it points LEFT from its pivot
  *tx = *px + (right ? -1.0f : 1.0f) * cosf(a) * PB_FLIP_LEN;
  *ty = *py + sinf(a) * PB_FLIP_LEN;
}

void Pinball::start() {
  score = 0;
  balls = PB_BALLS;
  mult = 1;
  for (auto &l : lit) l = false;
  for (auto &d : dropUp) d = true;
  for (auto &f : flash) f = 0;
  slingFlash[0] = slingFlash[1] = 0;
  dropResetMs = saucerMs = saucerCool = 0;
  orbits = catches = banks = 0;
  aL = aR = PB_REST_A;
  state = PB_SERVE;
  serveMs = 0;
  events = 0;
  inSensor = 0;
}

// Up the left lane (between the rim and the left slingshot), peaking over
// the drop targets and into the bumpers. A little spread, so no two serves
// match.
void Pinball::serve() {
  bx = 72; by = 300;
  vx = 120 + (float)random(0, 90);
  vy = -400 - (float)random(0, 110);
  saucerMs = 0;
  state = PB_ROLL;
}

// Closest point on segment ab to p.
static void closest(float px, float py, float ax, float ay, float bx, float by, float *qx, float *qy, float *t) {
  float dx = bx - ax, dy = by - ay;
  float l2 = dx * dx + dy * dy;
  float u = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
  if (u < 0) u = 0;
  if (u > 1) u = 1;
  *qx = ax + u * dx; *qy = ay + u * dy;
  if (t) *t = u;
}

// Bounce the ball off a surface moving at (sx, sy) along normal n. e is the
// restitution. True if it was moving into the surface.
static bool bounce(float &vx, float &vy, float nx, float ny, float sx, float sy, float e) {
  float rx = vx - sx, ry = vy - sy;
  float vn = rx * nx + ry * ny;
  if (vn >= 0) return false;
  vx -= (1 + e) * vn * nx;
  vy -= (1 + e) * vn * ny;
  return true;
}

// A capsule: segment ab of half-thickness w. (sx, sy) is the surface velocity
// at the contact (a moving flipper); 0 for walls. Returns 0 for no contact,
// else 1, or 2 when the ball was moving into it. *onx/*ony get the normal.
static int hitCapsule(float &bx, float &by, float &vx, float &vy, float ax, float ay, float cx, float cy,
                      float w, float e, float sxA, float syA, float sxB, float syB, float *onx = nullptr, float *ony = nullptr) {
  float qx, qy, t;
  closest(bx, by, ax, ay, cx, cy, &qx, &qy, &t);
  float dx = bx - qx, dy = by - qy;
  float d2 = dx * dx + dy * dy, rr = PB_BALL_R + w;
  if (d2 >= rr * rr || d2 < 1e-6f) return 0;
  float d = sqrtf(d2), nx = dx / d, ny = dy / d;
  bx = qx + nx * rr; by = qy + ny * rr;
  float sx = sxA + (sxB - sxA) * t, sy = syA + (syB - syA) * t;
  if (onx) { *onx = nx; *ony = ny; }
  return bounce(vx, vy, nx, ny, sx, sy, e) ? 2 : 1;
}

#define PB_GRAVITY 560.0f
#define PB_MAX_V 1150.0f
#define PB_FLIP_UP_W 19.0f     // rad/s while held
#define PB_FLIP_DOWN_W 12.0f   // rad/s falling back
#define PB_KICK 430.0f         // a pop bumper's push, px/s
#define PB_SLING_KICK 360.0f   // a slingshot's
#define PB_SUB_MS 2.5f         // physics substep: the fastest flipper tip moves ~3 px a step
#define PB_SAUCER_HOLD 900     // ms the CATCH saucer holds the ball
#define PB_SENSOR_R 11.0f

// A rollover: fires once as the ball passes over it (bit b of inSensor).
static bool rollover(uint8_t &inS, uint8_t b, float bx, float by, float sx, float sy) {
  float dx = bx - sx, dy = by - sy;
  bool over = dx * dx + dy * dy < PB_SENSOR_R * PB_SENSOR_R;
  bool fire = over && !(inS & (1 << b));
  if (over) inS |= (uint8_t)(1 << b); else inS &= (uint8_t)~(1 << b);
  return fire;
}

void Pinball::step(uint32_t ms) {
  for (auto &f : flash) f = f > ms ? (uint16_t)(f - ms) : 0;
  for (auto &f : slingFlash) f = f > ms ? (uint16_t)(f - ms) : 0;
  if (dropResetMs) {
    dropResetMs = dropResetMs > ms ? (uint16_t)(dropResetMs - ms) : 0;
    if (!dropResetMs) for (auto &d : dropUp) d = true;
  }
  if (state == PB_SERVE) {
    serveMs += ms;
    if (serveMs >= 700) serve();
    return;
  }
  if (state != PB_ROLL) return;
  buildWalls();
  if (ms > 100) ms = 100;                    // a long frame is not a teleport
  // LANE CHANGE: a fresh press on either flipper shifts the lit lanes right
  if ((upL && !wasUpL) || (upR && !wasUpR)) {
    bool last = lit[PB_LANES - 1];
    for (int i = PB_LANES - 1; i > 0; i--) lit[i] = lit[i - 1];
    lit[0] = last;
  }
  wasUpL = upL; wasUpR = upR;
  int n = (int)((float)ms / PB_SUB_MS + 0.999f);
  float dt = (float)ms / 1000.0f / (float)n;
  for (int s = 0; s < n; s++) {
    // flippers: where they were, then where they are
    float pLx, pLy, tL0x, tL0y, pRx, pRy, tR0x, tR0y;
    flipperEnds(false, &pLx, &pLy, &tL0x, &tL0y);
    flipperEnds(true, &pRx, &pRy, &tR0x, &tR0y);
    float wasL = aL, wasR = aR;
    if (upL) { aL -= PB_FLIP_UP_W * dt; if (aL < PB_UP_A) aL = PB_UP_A; }
    else     { aL += PB_FLIP_DOWN_W * dt; if (aL > PB_REST_A) aL = PB_REST_A; }
    if (upR) { aR -= PB_FLIP_UP_W * dt; if (aR < PB_UP_A) aR = PB_UP_A; }
    else     { aR += PB_FLIP_DOWN_W * dt; if (aR > PB_REST_A) aR = PB_REST_A; }
    if ((wasL > PB_UP_A && aL == PB_UP_A) || (wasR > PB_UP_A && aR == PB_UP_A)) events |= PBE_FLIP;
    float tL1x, tL1y, tR1x, tR1y;
    flipperEnds(false, &pLx, &pLy, &tL1x, &tL1y);
    flipperEnds(true, &pRx, &pRy, &tR1x, &tR1y);

    // the CATCH saucer: while it holds the ball nothing else moves it
    uint32_t sub = (uint32_t)(dt * 1000.0f + 0.5f);
    if (saucerCool) saucerCool = saucerCool > sub ? (uint16_t)(saucerCool - sub) : 0;
    if (saucerMs) {
      saucerMs = saucerMs > sub ? (uint16_t)(saucerMs - sub) : 0;
      bx = SAUCER_XY[0]; by = SAUCER_XY[1]; vx = vy = 0;
      if (!saucerMs) {   // spit it out, down and right into the bumpers
        vx = 230 + (float)random(0, 80);
        vy = 200 + (float)random(0, 80);
        saucerCool = 500;
        events |= PBE_EJECT;
      }
      continue;
    }

    vy += PB_GRAVITY * dt;
    float sp = sqrtf(vx * vx + vy * vy);
    if (sp > PB_MAX_V) { vx *= PB_MAX_V / sp; vy *= PB_MAX_V / sp; }
    bx += vx * dt;
    by += vy * dt;

    // the rim, above the inlane guides
    if (by < PB_GUIDE_Y) {
      float dx = bx - PB_CX, dy = by - PB_CY, d = sqrtf(dx * dx + dy * dy);
      if (d > PB_WALL_R - PB_BALL_R && d > 0) {
        float nx = -dx / d, ny = -dy / d;     // pointing inwards
        bx = PB_CX - nx * (PB_WALL_R - PB_BALL_R);
        by = PB_CY - ny * (PB_WALL_R - PB_BALL_R);
        if (bounce(vx, vy, nx, ny, 0, 0, 0.5f) && sp > 250) events |= PBE_WALL;
      }
    }
    // the fixed walls; a slingshot's face kicks
    for (uint8_t w = 0; w < WALL_N; w++) {
      const PbWall &W = WALLS[w];
      float nx = 0, ny = 0;
      int h = hitCapsule(bx, by, vx, vy, W.x0, W.y0, W.x1, W.y1, W.r, 0.4f, 0, 0, 0, 0, &nx, &ny);
      // a slingshot fires on a real hit (its switch needs a little force), and
      // only along its face's normal -- up and in
      if (h == 2 && W.kind == PBW_SLING && vx * nx + vy * ny > 40) {
        float vn = vx * nx + vy * ny;
        if (vn < PB_SLING_KICK) { vx += (PB_SLING_KICK - vn) * nx; vy += (PB_SLING_KICK - vn) * ny; }
        score += 10;
        slingFlash[W.x0 > PB_CX ? 1 : 0] = 160;
        events |= PBE_SLING;
      }
    }
    // the drop targets: down for 300 when struck from the playfield side
    for (uint8_t i = 0; i < PB_DROPS; i++) {
      if (!dropUp[i]) continue;
      float x0, y0, x1, y1; pbDrop(i, &x0, &y0, &x1, &y1);
      float nx = 0, ny = 0;
      int h = hitCapsule(bx, by, vx, vy, x0, y0, x1, y1, 3, 0.5f, 0, 0, 0, 0, &nx, &ny);
      if (h == 2 && nx > 0.3f) {
        dropUp[i] = false;
        score += 300;
        events |= PBE_DROP;
        bool all = true;
        for (bool d : dropUp) all = all && !d;
        if (all) { score += 2500; banks++; dropResetMs = 1200; events |= PBE_BANK; }
      }
    }
    // pop bumpers: they push, whatever came in, and add energy
    for (uint8_t i = 0; i < PB_BUMPERS; i++) {
      float dx = bx - BUMPER_XY[i][0], dy = by - BUMPER_XY[i][1];
      float rr = PB_BUMPER_R + PB_BALL_R, d2 = dx * dx + dy * dy;
      if (d2 >= rr * rr || d2 < 1e-6f) continue;
      float d = sqrtf(d2), nx = dx / d, ny = dy / d;
      bx = BUMPER_XY[i][0] + nx * rr; by = BUMPER_XY[i][1] + ny * rr;
      float vn = vx * nx + vy * ny;
      float out = -vn * 0.9f;
      if (out < PB_KICK) out = PB_KICK;
      vx += (out - vn) * nx;
      vy += (out - vn) * ny;
      score += 100u * mult;
      flash[i] = 180;
      events |= PBE_BUMPER;
    }
    // the CATCH saucer: a slow enough ball drops in
    if (!saucerCool) {
      float dx = bx - SAUCER_XY[0], dy = by - SAUCER_XY[1];
      if (dx * dx + dy * dy < 9 * 9 && vx * vx + vy * vy < 700 * 700) {
        saucerMs = PB_SAUCER_HOLD;
        score += 2000;
        catches++;
        events |= PBE_CATCH;
        bx = SAUCER_XY[0]; by = SAUCER_XY[1]; vx = vy = 0;
        continue;
      }
    }
    // rollovers: the top lanes, the orbit, the inlanes
    for (uint8_t i = 0; i < PB_LANES; i++) {
      if (!rollover(inSensor, i, bx, by, LANE_XY[i][0], LANE_XY[i][1])) continue;
      if (!lit[i]) { lit[i] = true; score += 250; events |= PBE_LANE; }
      bool all = true;
      for (bool l : lit) all = all && l;
      if (all) {
        score += 1000u * mult;
        if (mult < PB_MULT_MAX) mult++;
        for (auto &l : lit) l = false;
        events |= PBE_BONUS;
      }
    }
    { float ox, oy; pbOrbitSensor(&ox, &oy);
      if (rollover(inSensor, 3, bx, by, ox, oy)) { score += 500u * mult; orbits++; events |= PBE_ORBIT; } }
    for (int g = 0; g < 2; g++) {
      float ix, iy; pbInlane(g == 1, &ix, &iy);
      if (rollover(inSensor, (uint8_t)(4 + g), bx, by, ix, iy)) { score += 100; events |= PBE_INLANE; }
    }
    // the flippers: a moving surface, so a ball struck on the way up leaves
    // with the flipper's speed at that point along it
    float invDt = 1.0f / dt;
    hitCapsule(bx, by, vx, vy, pLx, pLy, tL1x, tL1y, PB_FLIP_W, 0.25f,
               0, 0, (tL1x - tL0x) * invDt, (tL1y - tL0y) * invDt);
    hitCapsule(bx, by, vx, vy, pRx, pRy, tR1x, tR1y, PB_FLIP_W, 0.25f,
               0, 0, (tR1x - tR0x) * invDt, (tR1y - tR0y) * invDt);

    // lost: between the flippers, or (never, but never trust it) off the table
    float ox = bx - PB_CX, oy = by - PB_CY;
    if (by > PB_DRAIN_Y || ox * ox + oy * oy > (PB_WALL_R + 30) * (PB_WALL_R + 30)) {
      events |= PBE_DRAIN;
      if (balls) balls--;
      upL = upR = false;
      if (balls) { state = PB_SERVE; serveMs = 0; }
      else state = PB_OVER;
      return;
    }
  }
}
