// The Game Corner's pinball table. See pinball.h for the layout and rules.
#include "pinball.h"
#include <Arduino.h>
#include <math.h>

// A triangle under the targets, clear of the flippers' line of fire.
static const float BUMPER_XY[PB_BUMPERS][2] = { { 233, 158 }, { 178, 214 }, { 288, 214 } };
static const float TARGET_XY[PB_TARGETS][2] = { { 191, 104 }, { 233, 94 }, { 275, 104 } };

void pbBumper(uint8_t i, float *x, float *y) { *x = BUMPER_XY[i % PB_BUMPERS][0]; *y = BUMPER_XY[i % PB_BUMPERS][1]; }
void pbTarget(uint8_t i, float *x, float *y) { *x = TARGET_XY[i % PB_TARGETS][0]; *y = TARGET_XY[i % PB_TARGETS][1]; }

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
  for (auto &f : flash) f = 0;
  aL = aR = PB_REST_A;
  state = PB_SERVE;
  serveMs = 0;
  events = 0;
}

// Up and in from the left lane, peaking over the bumpers so it drops into
// them. Hard enough to orbit the rim instead and it would skim past everything.
// A little spread, so no two serves match.
void Pinball::serve() {
  bx = 72; by = 300;
  vx = 120 + (float)random(0, 90);
  vy = -400 - (float)random(0, 110);
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
// at the contact (a moving flipper); 0 for walls.
static bool hitCapsule(float &bx, float &by, float &vx, float &vy, float ax, float ay, float cx, float cy,
                       float w, float e, float sxA, float syA, float sxB, float syB) {
  float qx, qy, t;
  closest(bx, by, ax, ay, cx, cy, &qx, &qy, &t);
  float dx = bx - qx, dy = by - qy;
  float d2 = dx * dx + dy * dy, rr = PB_BALL_R + w;
  if (d2 >= rr * rr || d2 < 1e-6f) return false;
  float d = sqrtf(d2), nx = dx / d, ny = dy / d;
  bx = qx + nx * rr; by = qy + ny * rr;
  float sx = sxA + (sxB - sxA) * t, sy = syA + (syB - syA) * t;
  bounce(vx, vy, nx, ny, sx, sy, e);
  return true;
}

#define PB_GRAVITY 560.0f
#define PB_MAX_V 1150.0f
#define PB_FLIP_UP_W 19.0f     // rad/s while held
#define PB_FLIP_DOWN_W 12.0f   // rad/s falling back
#define PB_KICK 430.0f         // a pop bumper's push, px/s
#define PB_SUB_MS 2.5f         // physics substep: the fastest flipper tip moves ~3 px a step

void Pinball::step(uint32_t ms) {
  for (auto &f : flash) f = f > ms ? (uint16_t)(f - ms) : 0;
  if (state == PB_SERVE) {
    serveMs += ms;
    if (serveMs >= 700) serve();
    return;
  }
  if (state != PB_ROLL) return;
  if (ms > 100) ms = 100;                    // a long frame is not a teleport
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
    // the inlane guides
    for (int g = 0; g < 2; g++) {
      float x0, y0, x1, y1;
      pbGuide(g == 1, &x0, &y0, &x1, &y1);
      hitCapsule(bx, by, vx, vy, x0, y0, x1, y1, 2.0f, 0.4f, 0, 0, 0, 0);
    }
    // pop bumpers: they push, whatever came in
    for (uint8_t i = 0; i < PB_BUMPERS; i++) {
      float dx = bx - BUMPER_XY[i][0], dy = by - BUMPER_XY[i][1];
      float rr = PB_BUMPER_R + PB_BALL_R, d2 = dx * dx + dy * dy;
      if (d2 >= rr * rr || d2 < 1e-6f) continue;
      float d = sqrtf(d2), nx = dx / d, ny = dy / d;
      bx = BUMPER_XY[i][0] + nx * rr; by = BUMPER_XY[i][1] + ny * rr;
      // It ADDS energy: a fast ball leaves faster, a slow one at least PB_KICK.
      float vn = vx * nx + vy * ny;
      float out = -vn * 0.9f;
      if (out < PB_KICK) out = PB_KICK;
      vx += (out - vn) * nx;
      vy += (out - vn) * ny;
      score += 100u * mult;
      flash[i] = 180;
      events |= PBE_BUMPER;
    }
    // the ROCKET targets: light them all for the bonus and a bigger multiplier
    for (uint8_t i = 0; i < PB_TARGETS; i++) {
      float dx = bx - TARGET_XY[i][0], dy = by - TARGET_XY[i][1];
      float rr = PB_TARGET_R + PB_BALL_R, d2 = dx * dx + dy * dy;
      if (d2 >= rr * rr || d2 < 1e-6f) continue;
      float d = sqrtf(d2), nx = dx / d, ny = dy / d;
      bx = TARGET_XY[i][0] + nx * rr; by = TARGET_XY[i][1] + ny * rr;
      if (!bounce(vx, vy, nx, ny, 0, 0, 0.7f)) continue;
      if (!lit[i]) { lit[i] = true; score += 250; events |= PBE_TARGET; }
      bool all = true;
      for (bool l : lit) all = all && l;
      if (all) {
        score += 1000u * mult;
        if (mult < PB_MULT_MAX) mult++;
        for (auto &l : lit) l = false;
        events |= PBE_BONUS;
      }
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
