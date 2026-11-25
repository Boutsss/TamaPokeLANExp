// The arcade's pinball table: the physics and the rules, with no drawing and
// no hardware, so pinball_test can play thousands of balls headless.
// ui_arcade_games.ino draws it and feeds it the flippers.
//
// An original table laid out for the round 466 px panel, built from the parts
// of a traditional machine -- not a copy of any existing one. Coordinates are
// panel pixels, y down. The playfield is the circle itself.
//
//   top lanes     three rollover lanes between posts under the top of the rim.
//                 250 each; light all three for a 1000 x mult bonus and the
//                 next multiplier (to x5). A flipper press shifts the lit
//                 lanes one to the right (LANE CHANGE), so you can aim.
//   pop bumpers   three, 100 x mult, kicking the ball away
//   orbit         a lane round the right of the rim: ride it to the top for
//                 500 x mult
//   drop targets  a bank of three on the left; each drops for 300, all three
//                 down score 2500 and stand back up
//   CATCH saucer  a Poke Ball hole: it holds the ball, scores 2000 and spits
//                 it back out into the bumpers
//   slingshots    the kicking triangles above the flippers: 10 and a kick
//   inlanes       between each slingshot and its guide, feeding the flipper:
//                 a rollover worth 100
//   flippers      held from either half of the panel
//
// 3 balls a game. It is an ARCADE game (ui_arcade.ino): its points are
// score / PB_PTS_DIV, graded like the others.
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
#define PB_BUMPER_R 18.0f
#define PB_LANES 3            // the top rollover lanes
#define PB_DROPS 3            // the drop-target bank
#define PB_SAUCER_R 11.0f
#define PB_BALLS 3
#define PB_MULT_MAX 5
#define PB_PTS_DIV 100        // arcade points = score / this

enum PbState : uint8_t { PB_IDLE = 0, PB_SERVE, PB_ROLL, PB_OVER };
// One-frame events for sound and flashes, OR'd into Pinball::events.
enum : uint16_t {
  PBE_BUMPER = 1, PBE_LANE = 2, PBE_BONUS = 4, PBE_FLIP = 8, PBE_DRAIN = 16, PBE_WALL = 32,
  PBE_SLING = 64, PBE_INLANE = 128, PBE_ORBIT = 256, PBE_DROP = 512, PBE_BANK = 1024,
  PBE_CATCH = 2048, PBE_EJECT = 4096,
};

// A wall of the table: a capsule from (x0,y0) to (x1,y1). A SLING wall kicks.
struct PbWall { float x0, y0, x1, y1, r; uint8_t kind; };
enum : uint8_t { PBW_WALL = 0, PBW_SLING };

struct Pinball {
  float bx = 0, by = 0, vx = 0, vy = 0;
  float aL = PB_REST_A, aR = PB_REST_A;      // flipper angles (0 = horizontal, + = down)
  bool upL = false, upR = false;             // the player is holding that side
  uint32_t score = 0;
  uint8_t balls = 0;                         // balls left, the one in play included
  uint8_t mult = 1;
  bool lit[PB_LANES] = { false, false, false };
  bool dropUp[PB_DROPS] = { true, true, true };
  uint16_t dropResetMs = 0;                  // the bank stands back up when this runs out
  uint16_t saucerMs = 0;                     // > 0: the ball is held in the CATCH saucer
  uint16_t saucerCool = 0;                   // just ejected: it cannot catch again yet
  bool wasUpL = false, wasUpR = false;       // for LANE CHANGE on a fresh press
  uint16_t orbits = 0, catches = 0, banks = 0;
  uint8_t state = PB_IDLE;
  uint16_t events = 0;                       // since the caller last cleared it
  uint16_t flash[PB_BUMPERS] = { 0, 0, 0 };  // ms of glow left per bumper
  uint16_t slingFlash[2] = { 0, 0 };
  uint32_t serveMs = 0;                      // time spent waiting to serve
  uint8_t inSensor = 0;                      // bit per rollover the ball is over (fires once per pass)

  void start();                // a new game of PB_BALLS
  void serve();                // put the next ball in play
  void step(uint32_t ms);      // advance real time (substepped)
  // Flipper geometry for drawing and tests: pivot and tip of each side.
  void flipperEnds(bool right, float *px, float *py, float *tx, float *ty) const;
};

void pbBumper(uint8_t i, float *x, float *y);
void pbLane(uint8_t i, float *x, float *y);       // a top lane's rollover
void pbDrop(uint8_t i, float *x0, float *y0, float *x1, float *y1);
void pbSaucer(float *x, float *y);
void pbGuide(bool right, float *x0, float *y0, float *x1, float *y1);
// Every fixed wall (guides, lane posts, the orbit, the slingshots), for drawing.
uint8_t pbWalls(const PbWall **out);
void pbSlingshot(bool right, float *ax, float *ay, float *bx, float *by, float *cx, float *cy);
void pbInlane(bool right, float *x, float *y);    // the inlane rollover
void pbOrbitSensor(float *x, float *y);
