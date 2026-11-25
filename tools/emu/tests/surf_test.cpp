// The arcade's SURF and PINBALL through the real sketch.
//
// SURF: a star in the surfer's path scores, a rock takes a life and then
// cannot take another for a moment, a missed star breaks the combo, three
// rocks end the session, and -- the point of it -- the surfer is the creature
// YOU are raising: change the live pet and the surfer changes. A bot plays a
// few hundred sessions to measure the grade thresholds instead of guessing.
//
// PINBALL in the arcade: the flippers follow a finger on each half of the
// panel through the touch path itself, lifting it drops the flipper, taps do
// nothing on the table, and no swipe leaves while a ball rolls.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "pinball.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
uint32_t g_seed=97; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipe(int); void onSwipeV(int);
void emuFireInterrupt();
extern Pet pet; extern Arduino_Canvas *gfx;
extern bool arcadeOpen; extern uint8_t arcGame; extern bool arcOver; extern uint16_t arcPts;
void arcadeOpenFromTrain(); void arcStart(uint8_t); void arcSkipIntro(); void arcFinish(); uint8_t arcGradeOf(uint8_t, uint16_t);
extern uint8_t surfK[8]; extern float surfX[8], surfY[8], surfPetY, surfGoal; extern uint8_t surfLives;
extern uint16_t surfStars, surfShells, surfRocks;
uint8_t surfStep(uint32_t); void surfScore(uint8_t); void surfSpawnAt(uint8_t, uint8_t, float); void surfPeek(bool, int16_t);
Pinball &arcPinball(); bool pinBallLive();
extern uint16_t trnCombo;
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static void pump(int n){ for(int i=0;i<n;i++){ std::this_thread::sleep_for(std::chrono::milliseconds(25)); loop(); } }
static uint32_t regionHash() {   // the surfer's patch of the panel
  uint32_t h = 2166136261u;
  for (int y = 200; y < 330; y++) for (int x = 100; x < 200; x++) h = (h ^ gfx->buffer()[y * 466 + x]) * 16777619u;
  return h;
}
enum { STAR = 1, SHELL = 2, ROCK = 3 };

// The bot plays like a PERSON, not a machine: it looks again only every
// 250 ms, aims within +-18 px, lets one star in five go, and sees a rock
// coming four times in five. A bot that reads exact positions every frame
// scored S nine games in ten -- which measures nothing about a player.
static float aim = 300; static uint32_t lookIn = 0;
static void bot(bool steer) {
  if (!steer) return;
  if (lookIn > 50) { lookIn -= 50; surfPeek(true, (int16_t)aim); return; }
  lookIn = 250;
  float best = 1e9, goal = surfPetY;
  for (int i = 0; i < 8; i++) {
    if (!surfK[i] || surfK[i] == ROCK || surfX[i] < 150) continue;
    if ((uint32_t)(surfX[i] * 7 + i) % 5 == 0) continue;           // that one got away
    if (surfX[i] < best) { best = surfX[i]; goal = surfY[i]; }
  }
  for (int i = 0; i < 8; i++)
    if (surfK[i] == ROCK && surfX[i] > 140 && surfX[i] < 280 && fabsf(surfY[i] - goal) < 34 && random(5))
      goal += (surfY[i] > goal ? -60 : 60);
  aim = goal + (float)random(-18, 19);
  surfPeek(true, (int16_t)aim);
}
static uint16_t session(bool steer) {
  arcStart(5); arcSkipIntro();
  for (int t = 0; t < 45000 / 50 && surfLives; t++) { bot(steer); surfScore(surfStep(50)); }
  uint16_t p = arcPts;
  arcFinish(); arcGame = 0xFF; arcOver = false;
  return p;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.sleeping = false; pet.energy = 100;
  arcadeOpenFromTrain();

  // ---- SURF: the rules
  arcStart(5); arcSkipIntro();
  for (auto &k : surfK) k = 0;
  surfSpawnAt(0, STAR, surfPetY);
  surfX[0] = 160;
  uint8_t ev = surfStep(60); surfScore(ev);
  ck(surfStars == 1 && arcPts >= 1 && !surfK[0], "a star in the surfer's path is caught and scores");
  surfSpawnAt(1, ROCK, surfPetY); surfX[1] = 160;
  surfScore(surfStep(20));
  ck(surfLives == 2 && trnCombo == 0, "a rock takes a life and the combo");
  surfSpawnAt(2, ROCK, surfPetY); surfX[2] = 162;
  surfScore(surfStep(20));
  ck(surfLives == 2, "a second rock straight after does not: a moment's grace");
  for (auto &k : surfK) k = 0;
  surfSpawnAt(3, STAR, 230); surfGoal = surfPetY = 360; surfX[3] = 40;
  uint16_t c0 = trnCombo;
  surfSpawnAt(4, STAR, surfPetY); surfX[4] = 160; surfScore(surfStep(20));
  ck(trnCombo == c0 + 1, "(the combo counts up)");
  for (int i = 0; i < 12; i++) surfScore(surfStep(50));   // the far star sails past
  ck(trnCombo == 0, "a star that gets past breaks the combo");
  // steering follows the finger, within the sea
  surfPeek(true, 100);
  for (int i = 0; i < 20; i++) surfStep(50);
  ck(surfPetY >= 220 && surfPetY < 240, "the finger steers it, and it stays in the water");
  // three rocks end it, through the screen
  surfLives = 1;
  for (auto &k : surfK) k = 0;
  for (int i = 0; i < 60 && !arcOver; i++) { surfSpawnAt(5, ROCK, surfPetY); surfX[5] = 150; pump(1); }
  ck(arcOver, "the last life ends the session");
  arcGame = 0xFF; arcOver = false;

  // ---- the surfer is YOUR creature
  arcStart(5); arcSkipIntro(); for (auto &k : surfK) k = 0;
  surfGoal = surfPetY = 280;
  render(); uint32_t a = regionHash();
  arcFinish(); arcGame = 0xFF; arcOver = false;
  pet.dbgHatchAs(9, false);
  for (int i = 0; i < 5; i++) render();
  arcStart(5); arcSkipIntro(); for (auto &k : surfK) k = 0;
  surfGoal = surfPetY = 280;
  render(); uint32_t b = regionHash();
  ck(a != b, "change the creature you raise and the surfer changes with it");
  arcFinish(); arcGame = 0xFF; arcOver = false;

  // ---- vertical swipes steer, they do not leave
  arcStart(5); arcSkipIntro();
  onSwipeV(1); onSwipeV(-1);
  ck(arcadeOpen && arcGame == 5 && !arcOver, "a vertical swipe while surfing does not leave");
  arcFinish(); arcGame = 0xFF; arcOver = false;

  // ---- the grades, measured
  {
    std::vector<uint16_t> good, idle;
    for (int g = 0; g < 200; g++) { randomSeed(500 + g); pet.energy = 100; good.push_back(session(true)); }
    for (int g = 0; g < 100; g++) { randomSeed(900 + g); pet.energy = 100; idle.push_back(session(false)); }
    std::sort(good.begin(), good.end()); std::sort(idle.begin(), idle.end());
    auto share = [](const std::vector<uint16_t> &v, uint8_t grade) {
      int n = 0; for (auto p : v) if (arcGradeOf(5, p) >= grade) n++; return 100.0 * n / v.size(); };
    printf("      surf, steering bot: median %u, top 10%% %u; B %.0f%% A %.0f%% S %.0f%%\n",
           good[good.size() / 2], good[good.size() * 9 / 10], share(good, 1), share(good, 2), share(good, 3));
    printf("      surf, idle: median %u; B %.0f%%\n", idle[idle.size() / 2], share(idle, 1));
    printf("      deciles: steering"); for (int q = 1; q < 10; q++) printf(" %u", good[good.size() * q / 10]);
    printf("  idle"); for (int q = 1; q < 10; q++) printf(" %u", idle[idle.size() * q / 10]); printf("\n");
    ck(share(idle, 1) < 15, "not steering almost never makes a B");
    ck(share(good, 1) > 60 && share(good, 3) > 3 && share(good, 3) < 40, "a careful surfer usually makes a B, and an S is earned");
  }

  // ---- PINBALL in the arcade: the flippers through the touch path
  arcStart(4); arcSkipIntro();
  pump(40);
  ck(arcPinball().state == PB_ROLL, "pinball serves its first ball");
  g_touchX = 100; g_touchY = 300; g_touchDown = true; emuFireInterrupt(); pump(4);
  ck(arcPinball().upL && !arcPinball().upR, "a finger on the left half holds the left flipper");
  g_touchDown = false; emuFireInterrupt(); pump(3);
  ck(!arcPinball().upL, "lifting it drops the flipper");
  g_touchX = 380; g_touchY = 40; g_touchDown = true; emuFireInterrupt(); pump(4);
  ck(arcPinball().upR, "the right half, even at the very top, holds the right one");
  g_touchDown = false; emuFireInterrupt(); pump(3);
  ck(arcadeOpen && arcGame == 4 && !arcOver, "and a tap at the top does not leave the table (unlike the other games)");
  arcPinball().bx = 233; arcPinball().by = 150; arcPinball().vx = 0; arcPinball().vy = 0;
  onSwipeV(1); onSwipe(1);
  ck(arcadeOpen && arcGame == 4 && pinBallLive(), "no swipe leaves while a ball rolls");
  arcPinball().score = 5400; arcPinball().balls = 1;
  arcPinball().bx = PB_CX; arcPinball().by = PB_DRAIN_Y + 2; arcPinball().vy = 100;
  pump(4);
  ck(arcOver && arcPts == 54, "the last ball ends the game: points are the score / 100");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
