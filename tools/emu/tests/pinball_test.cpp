// The Game Corner's pinball, headless: thousands of balls through the REAL
// physics (pinball.cpp), played by two bots.
//
//  - the table is sound: no ball ever leaves the playfield except by draining
//    between the flippers, and none hangs forever;
//  - the flippers matter: a bot that flips when the ball reaches a flipper
//    scores far more than one that never flips, and a held flipper sends a
//    ball back up into the bumpers;
//  - the rules: bumpers 100 x multiplier, the three targets light and pay a
//    bonus and a multiplier, three balls a game;
//  - the arcade GRADES are measured, not guessed: points are score/PB_PTS_DIV,
//    and the thresholds ui_arcade.ino uses (B 25, A 50, S 100) are checked
//    against what a careful player and a careless one actually reach.
#include "Arduino.h"
#include "Preferences.h"
#include "pinball.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
uint32_t g_seed=83; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// The careful bot: flips the side the ball is over once it is low and
// FALLING -- not while it sits still on a raised flipper, or it would cradle
// the ball forever. It is a PERSON, not a machine: it decides once per
// approach whether it reacts in time, and misses MISS_PCT of them.
static int MISS_PCT = 25;
static bool approaching = false, willFlip = false;
static void careful(Pinball &p) {
  bool low = p.by > PB_PIVOT_Y - 48 && p.vy > 40;
  if (low && !approaching) willFlip = (int)random(0, 100) >= MISS_PCT;
  approaching = low;
  p.upL = low && willFlip && p.bx < PB_CX;
  p.upR = low && willFlip && p.bx >= PB_CX;
}

struct Result { uint32_t score; bool escaped, hung; };
static Result playGame(bool flips, uint32_t frameMs) {
  Pinball p; p.start();
  Result r = { 0, false, false };
  uint32_t t = 0, sinceDrain = 0;
  while (p.state != PB_OVER) {
    if (flips) careful(p); else p.upL = p.upR = false;
    uint8_t ballsBefore = p.balls;
    p.step(frameMs);
    t += frameMs; sinceDrain += frameMs;
    if (p.balls != ballsBefore) sinceDrain = 0;
    float dx = p.bx - PB_CX, dy = p.by - PB_CY;
    if (p.state == PB_ROLL && dx * dx + dy * dy > (PB_WALL_R + 2) * (PB_WALL_R + 2) && p.by < PB_GUIDE_Y) r.escaped = true;
    // STUCK, as opposed to a long rally: barely moving for ten seconds with
    // both flippers down (a cradle is the player's choice, and this bot never
    // cradles).
    static uint32_t still = 0;
    if (p.state == PB_ROLL && p.vx * p.vx + p.vy * p.vy < 30 * 30 && !p.upL && !p.upR) still += frameMs; else still = 0;
    if (still > 10000) { r.hung = true; still = 0; printf("      stuck at (%.1f,%.1f) v=(%.1f,%.1f)\n", p.bx, p.by, p.vx, p.vy); break; }
    if (sinceDrain > 30 * 60 * 1000) break;   // half an hour on one ball: a rally, not a fault
  }
  r.score = p.score;
  return r;
}

int main(){
  // ---- rules, by hand
  {
    Pinball p; p.start();
    ck(p.balls == PB_BALLS && p.state == PB_SERVE && p.mult == 1, "a game starts with three balls, waiting to serve");
    p.step(800);
    ck(p.state == PB_ROLL, "the ball serves itself");
    float x, y; pbBumper(0, &x, &y);
    p.bx = x; p.by = y - PB_BUMPER_R - PB_BALL_R + 2; p.vx = 0; p.vy = 300;
    p.events = 0; p.step(5);
    ck(p.score == 100 && (p.events & PBE_BUMPER) && p.vy < 0, "a bumper scores 100 and kicks the ball away");
    for (uint8_t i = 0; i < PB_TARGETS; i++) {
      pbTarget(i, &x, &y);
      p.bx = x; p.by = y + PB_TARGET_R + PB_BALL_R - 2; p.vx = 0; p.vy = -300;
      p.step(5);
    }
    ck(p.mult == 2 && !p.lit[0] && p.score >= 100 + 3 * 250 + 1000, "all three targets: the bonus, x2, and they reset");
    pbBumper(1, &x, &y);
    uint32_t before = p.score;
    p.bx = x; p.by = y - PB_BUMPER_R - PB_BALL_R + 2; p.vx = 0; p.vy = 300;
    p.step(5);
    ck(p.score - before == 200, "the multiplier doubles a bumper");
    p.bx = PB_CX; p.by = PB_DRAIN_Y + 1; p.vy = 100; p.vx = 0;
    p.step(5);
    ck(p.balls == 2 && p.state == PB_SERVE, "a drained ball takes one away and serves the next");
  }
  // ---- a held flipper sends the ball back up
  {
    Pinball p; p.start(); p.step(800);
    float px, py, tx, ty; p.flipperEnds(false, &px, &py, &tx, &ty);
    p.bx = px + (tx - px) * 0.6f; p.by = py + (ty - py) * 0.6f - PB_FLIP_W - PB_BALL_R - 1;
    p.vx = 0; p.vy = 50;
    p.upL = true;
    float minY = 1000;
    for (int i = 0; i < 60; i++) { p.step(10); if (p.by < minY) minY = p.by; if (p.state != PB_ROLL) break; }
    ck(minY < 200, "a flip sends the ball back up the table");
    printf("      a flip from 60%% along the flipper reached y=%.0f\n", minY);
  }
  // ---- a ball resting on a raised flipper stays there (a cradle), and the
  //      gap between the flippers at rest is wide enough to drain
  {
    Pinball p; p.start(); p.step(800);
    float lpx, lpy, ltx, lty, rpx, rpy, rtx, rty;
    p.flipperEnds(false, &lpx, &lpy, &ltx, &lty); p.flipperEnds(true, &rpx, &rpy, &rtx, &rty);
    float gap = (rtx - ltx) - 2 * PB_FLIP_W;
    ck(gap > 2 * PB_BALL_R, "at rest the flippers leave a drain wide enough for the ball");
    printf("      drain gap %.1f px for a %.0f px ball\n", gap, 2 * PB_BALL_R);
  }
  // ---- thousands of games, at the panel's real frame times
  for (uint32_t frame : { 85u, 100u }) {
    std::vector<uint32_t> good, idle;
    bool escaped = false, hung = false;
    for (int g = 0; g < 400; g++) {
      randomSeed(1000 + g);
      Result a = playGame(true, frame);  good.push_back(a.score); escaped |= a.escaped; hung |= a.hung;
      Result b = playGame(false, frame); idle.push_back(b.score); escaped |= b.escaped; hung |= b.hung;
    }
    std::sort(good.begin(), good.end()); std::sort(idle.begin(), idle.end());
    auto share = [](const std::vector<uint32_t> &v, uint32_t pts) {
      int n = 0; for (auto sc : v) if (sc / PB_PTS_DIV >= pts) n++; return 100.0 * n / v.size(); };
    printf("      %u ms frames -- careful: median %u, top 10%% %u; B %.0f%%  A %.0f%%  S %.0f%%\n",
           frame, good[good.size() / 2], good[good.size() * 9 / 10], share(good, 25), share(good, 50), share(good, 100));
    printf("      %u ms frames -- never flips: median %u; B %.0f%%\n", frame, idle[idle.size() / 2], share(idle, 25));
    printf("      careful deciles:");
    for (int q = 1; q < 10; q++) printf(" %u", good[good.size() * q / 10]);
    printf("\n");
    ck(!escaped, "no ball ever leaves the table except down the drain");
    ck(!hung, "no ball hangs forever");
    ck(good[good.size() / 2] > 2 * idle[idle.size() / 2], "flipping matters: a careful player scores well over twice a careless one");
    ck(share(idle, 25) < 10, "a player who never flips almost never makes even a B");
    ck(share(good, 25) > 55 && share(good, 100) > 3 && share(good, 100) < 30,
       "a careful player usually makes a B, and an S is a real but rare achievement");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
