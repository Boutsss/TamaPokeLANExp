// The Game Corner's pinball table: the physics and the rules, with no drawing
// and no hardware, so pinball_test can play thousands of balls headless.
// ui_casino.ino draws it and feeds it the flippers.
//
// An original table laid out for the round 466 px panel -- not a copy of any
// existing machine. Coordinates are panel pixels, y down. The playfield is
// the circle itself; two inlane guides run from the wall down to the flipper
// pivots, and a ball that falls between the flippers is lost.
//
// Rules: 3 balls a game. Pop bumpers score 100, the three Poke Ball targets
// 250 each; lighting all three scores a 1000 bonus times the multiplier and
// raises the multiplier (to x5), which also multiplies the bumpers. It is an
// ARCADE game (ui_arcade.ino): its points are score / PB_PTS_DIV, graded like
// the others.
#pragma once
#include <stdint.h>

#define PB_CX 233
#define PB_CY 233
#define PB_WALL_R 214.0f      // the playfield's rim (the panel is 233)
#define PB_BALL_R 7.0f
#define PB_GUIDE_Y 330.0f     // where the inlane guides leave the rim
#define PB_PIVOT_Y 392.0f
#define PB_PIVOT_LX 166.0f
#define PB_PIVOT_RX 300.0f
#define PB_FLIP_LEN 58.0f
#define PB_FLIP_W 6.0f        // the flipper's half-thickness (a capsule)
#define PB_REST_A 0.49f       // ~28 degrees below horizontal
#define PB_UP_A (-0.38f)      // ~22 degrees above
#define PB_DRAIN_Y 452.0f
#define PB_BUMPERS 3
#define PB_BUMPER_R 20.0f
#define PB_TARGETS 3
#define PB_TARGET_R 9.0f
#define PB_BALLS 3
#define PB_MULT_MAX 5
#define PB_PTS_DIV 100        // arcade points = score / this

enum PbState : uint8_t { PB_IDLE = 0, PB_SERVE, PB_ROLL, PB_OVER };
// One-frame events for sound and flashes, OR'd into Pinball::events.
enum : uint8_t { PBE_BUMPER = 1, PBE_TARGET = 2, PBE_BONUS = 4, PBE_FLIP = 8, PBE_DRAIN = 16, PBE_WALL = 32 };

struct Pinball {
  float bx = 0, by = 0, vx = 0, vy = 0;
  float aL = PB_REST_A, aR = PB_REST_A;      // flipper angles (0 = horizontal, + = down)
  bool upL = false, upR = false;             // the player is holding that side
  uint32_t score = 0;
  uint8_t balls = 0;                         // balls left, the one in play included
  uint8_t mult = 1;
  bool lit[PB_TARGETS] = { false, false, false };
  uint8_t state = PB_IDLE;
  uint8_t events = 0;                        // since the caller last cleared it
  uint16_t flash[PB_BUMPERS] = { 0, 0, 0 };  // ms of glow left per bumper
  uint32_t serveMs = 0;                      // time spent waiting to serve

  void start();                // a new game of PB_BALLS (the coins are the caller's)
  void serve();                // put the next ball in play
  void step(uint32_t ms);      // advance real time (substepped)
  // Flipper geometry for drawing and tests: pivot and tip of each side.
  void flipperEnds(bool right, float *px, float *py, float *tx, float *ty) const;
};

void pbBumper(uint8_t i, float *x, float *y);
void pbTarget(uint8_t i, float *x, float *y);
void pbGuide(bool right, float *x0, float *y0, float *x1, float *y1);
