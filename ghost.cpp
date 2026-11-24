#include "ghost.h"
#include <Preferences.h>
#include <string.h>

static Ghost g[GHOST_CAP];
static uint8_t gN = 0;
static Ghost gNone;

#define GHOST_VER 1
#define GHOST_KEY "ghosts"
#define GHOST_HDR 4

uint8_t ghostCount() { return gN; }
const Ghost &ghostAt(uint8_t i) { return i < gN ? g[i] : gNone; }

static bool save() {
  uint8_t buf[GHOST_HDR + GHOST_CAP * sizeof(Ghost)];
  buf[0] = GHOST_VER;
  buf[1] = gN;
  uint16_t stride = sizeof(Ghost);
  memcpy(buf + 2, &stride, 2);
  for (uint8_t i = 0; i < gN; i++) memcpy(buf + GHOST_HDR + i * sizeof(Ghost), &g[i], sizeof(Ghost));
  size_t n = GHOST_HDR + gN * sizeof(Ghost);
  Preferences p;
  if (!p.begin("tamapoke", false)) return false;
  bool ok = p.putBytes(GHOST_KEY, buf, n) == n;
  p.end();
  return ok;
}

void ghostsBegin() {
  gN = 0;
  for (auto &x : g) x = Ghost();
  Preferences p;
  if (!p.begin("tamapoke", true)) return;
  size_t n = p.getBytesLength(GHOST_KEY);
  uint8_t buf[GHOST_HDR + GHOST_CAP * 2 * sizeof(Ghost)];
  if (n >= GHOST_HDR && n <= sizeof(buf) && p.getBytes(GHOST_KEY, buf, n) == n) {
    uint16_t stride;
    memcpy(&stride, buf + 2, 2);
    uint8_t cnt = buf[1];
    if (stride && cnt <= GHOST_CAP && GHOST_HDR + (size_t)cnt * stride <= n) {
      for (uint8_t i = 0; i < cnt; i++) {
        // prefix rule: an older, shorter record leaves the tail at its defaults
        memcpy(&g[i], buf + GHOST_HDR + i * stride, stride < sizeof(Ghost) ? stride : sizeof(Ghost));
        g[i].name[LINK_NAME_LEN - 1] = 0;
        if (g[i].n > TRAINER_TEAM_MAX) g[i].n = TRAINER_TEAM_MAX;
      }
      gN = cnt;
    }
  }
  p.end();
  // drop empty records rather than offer a fight against nobody
  uint8_t k = 0;
  for (uint8_t i = 0; i < gN; i++) if (g[i].n) g[k++] = g[i];
  gN = k;
}

static bool sameRival(const Ghost &x, const uint8_t *mac, const char *name) {
  static const uint8_t zero[6] = { 0 };
  bool hasMac = mac && memcmp(mac, zero, 6) != 0;
  if (hasMac) return memcmp(x.mac, mac, 6) == 0;
  return memcmp(x.mac, zero, 6) == 0 && strncmp(x.name, name ? name : "", LINK_NAME_LEN) == 0;
}

bool ghostRemember(const uint8_t *mac, const char *name, const LinkMon *mons, uint8_t n) {
  if (!mons || !n) return false;
  if (n > TRAINER_TEAM_MAX) n = TRAINER_TEAM_MAX;
  Ghost r;
  int at = -1;
  for (uint8_t i = 0; i < gN; i++) if (sameRival(g[i], mac, name)) { at = i; break; }
  if (at >= 0) { r.wins = g[at].wins; r.losses = g[at].losses; }   // the record carries over
  if (mac) memcpy(r.mac, mac, 6);
  snprintf(r.name, sizeof(r.name), "%s", name && name[0] ? name : "RIVAL");
  r.n = n;
  for (uint8_t i = 0; i < n; i++) r.mon[i] = mons[i];
  // most recent first: shift down over the old slot (or off the end)
  int from = at >= 0 ? at : (gN < GHOST_CAP ? gN : GHOST_CAP - 1);
  for (int i = from; i > 0; i--) g[i] = g[i - 1];
  g[0] = r;
  if (at < 0 && gN < GHOST_CAP) gN++;
  return save();
}

void ghostResult(uint8_t i, bool won) {
  if (i >= gN) return;
  uint8_t &c = won ? g[i].wins : g[i].losses;
  if (c < 255) c++;
  save();
}

bool ghostForget(uint8_t i) {
  if (i >= gN) return false;
  for (uint8_t k = i; k + 1 < gN; k++) g[k] = g[k + 1];
  gN--;
  g[gN] = Ghost();
  return save();
}
