#include "dexinfo.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool dexInfoRead(DexReadFn rd, void *ctx, int16_t dex, uint8_t lang, DexEntryText &out) {
  out = DexEntryText();
  if (!rd) return false;
  uint8_t h[8];
  if (rd(ctx, 0, h, 8) != 8 || memcmp(h, "TDX1", 4)) return false;
  uint16_t count = rd16(h + 4);
  uint8_t langs = h[6];
  if (dex < 1 || dex > count || !langs || lang >= langs) return false;
  uint8_t b[4];
  if (rd(ctx, 8 + 2u * (dex - 1), b, 2) != 2) return false;
  out.heightDm = rd16(b);
  uint32_t at = 8 + 2u * count + 4u * ((uint32_t)(dex - 1) * langs + lang);
  if (rd(ctx, at, b, 4) != 4) return false;
  uint32_t off = rd32(b);
  // genus NUL text NUL, both bounded: whatever is on the card, never run past a buffer
  uint8_t s[DEXINFO_GENUS + DEXINFO_TEXT];
  uint32_t n = rd(ctx, off, s, sizeof(s));
  if (!n) return false;
  uint32_t i = 0, g = 0;
  while (i < n && s[i] && g < DEXINFO_GENUS - 1) out.genus[g++] = (char)s[i++];
  out.genus[g] = 0;
  while (i < n && s[i]) i++;          // a genus longer than the buffer is cut, not merged into the text
  i++;
  uint32_t t = 0;
  while (i < n && s[i] && t < DEXINFO_TEXT - 1) {
    char c = (char)s[i++];
    out.text[t++] = (c >= 32 && c < 127) ? c : ' ';   // the panel font is ASCII only
  }
  out.text[t] = 0;
  return out.genus[0] || out.text[0];
}
