// ---------- the 18 type sprites (ported from vunguyen2712/TamaPoke) ----------
// The 12x12 type sprites and their five-shade ramps, taken on their own from
// vunguyen2712's ui_movefx.ino (the battle move effects, which this base does
// not carry). The ARCADE's MEMORY cards and ECHO pads wear them; fxTypeIcon()
// and fxTypeCol() are the accessors it uses. Art and palette unchanged.
//
// This tab is concatenated BEFORE ui_train.ino and AFTER ui_arcade.ino (tabs go
// alphabetically after TamaPoke.ino), so the arcade forward-declares the two.

typedef char FxMap[12][13];

static const FxMap FXP_FLAME = {
  "....K.......", "...KLK......", "...KLMK..K..", "..KLMMK.KLK.",
  "..KLMMDKKMK.", ".KLMWMMDKMDK", ".KMWWLMMDMDK", "KMWLLLWMMMDK",
  "KMWLLLLWMMDK", "KMWLLLLWMDK.", ".KMWLLWMMDK.", "..KKKKKKKK..",
};
static const FxMap FXP_DROP = {
  ".....KK.....", ".....KK.....", "....KLMK....", "....KLMK....",
  "...KLMMMK...", "..KLWMMMDK..", "..KWWMMMDK..", ".KLWMMMMMDK.",
  ".KLWMMMMMDK.", ".KLMMMMMDDK.", "..KMMMMDDK..", "...KKKKKK...",
};
static const FxMap FXP_LEAF = {
  "..........KK", "........KKMK", "......KKLMDK", "....KKLLMMDK",
  "...KLLMMMDK.", "..KLMMWMDDK.", ".KLMMWMMDK..", ".KMMWMMDK...",
  "KMMWMDDK....", "KMWDDKK.....", "KWKK........", "K...........",
};
static const FxMap FXP_SHARD = {
  ".....KK.....", "....KWLK....", "....KWLK....", "...KWLLMK...",
  "...KWLMMK...", "..KWLLMMDK..", "..KWLMMMDK..", "..KWLMMDDK..",
  "...KLMMDK...", "...KLMDDK...", "....KMDK....", ".....KK.....",
};
static const FxMap FXP_ROCK = {
  "....KKKK....", "..KKLLMMKK..", ".KLLWLMMMDK.", ".KLWLMMMMDK.",
  "KLLLMMMDMMDK", "KLMMMMDMMMDK", "KMMMMDMMMDDK", "KMMDMMMMDDDK",
  ".KMMMMMDDDK.", ".KDMMDDDDDK.", "..KKDDDDKK..", "....KKKK....",
};
static const FxMap FXP_BOLT = {
  "......KKKK..", ".....KLLWK..", "....KLLWK...", "...KLLWK....",
  "..KLLWKKKK..", ".KLWWWWLLK..", ".KKKKLWLK...", "....KLWK....",
  "...KLWK.....", "..KLWK......", "..KWK.......", "..KK........",
};
static const FxMap FXP_STAR = {
  ".....KK.....", ".....KWK....", "KK..KLWK..KK", "KWK.KLWK.KWK",
  ".KLKKLLKKLK.", "..KLLWWLLK..", "KKKLWWWWLKKK", "KWLLLWWLLLWK",
  ".KKKLLLLKKK.", "..KLKLLKLK..", ".KLK.KK.KLK.", ".KK......KK.",
};
static const FxMap FXP_SPARK = {
  ".....K......", ".....K......", "....KWK.....", "....KWK.....",
  "...KWLWK....", "KKKWLLLWKKK.", "...KWLWK....", "....KWK.....",
  "....KWK.....", ".....K......", ".....K......", "............",
};
static const FxMap FXP_ORB = {
  "....KKKK....", "..KKMMMMKK..", ".KMLLMMMMDK.", ".KLWWLMMMDK.",
  "KMLWWLMMMMDK", "KMLLLMMMMMDK", "KMMMMMMMMDDK", "KMMMMMMMMDDK",
  ".KMMMMMMDDK.", ".KDMMMMDDDK.", "..KKDDDDKK..", "....KKKK....",
};
static const FxMap FXP_FEATHER = {
  "..........KK", ".........KWK", "........KWLK", ".......KWLMK",
  "......KWLMK.", ".....KWLMMK.", "....KWLMMK..", "...KWLMMK...",
  "..KWLMDK....", "..KLMDK.....", ".KDKK.......", "KK..........",
};
static const FxMap FXP_NEEDLE = {
  "..........KK", ".........KWK", "........KWK.", ".......KWK..",
  "......KLK...", ".....KLK....", "....KMK.....", "...KMK......",
  "..KMDK......", ".KMDK.......", "KDDK........", "KKK.........",
};
static const FxMap FXP_HEART = {
  "............", ".KKK...KKK..", "KLWLK.KLMMK.", "KWLMMKMMMDK.",
  "KLMMMMMMMDK.", "KMMMMMMMDDK.", ".KMMMMMMDK..", "..KMMMMDK...",
  "...KMMDK....", "....KDK.....", ".....K......", "............",
};
static const FxMap FXP_ARROW = {
  ".....KK.....", "....KWLK....", "...KWLLMK...", "..KWLLLMMK..",
  ".KWLLLLMMMK.", "KKKKLLMMKKKK", "...KLLMMK...", "...KLLMMK...",
  "...KLLMDK...", "...KLMMDK...", "...KMMDDK...", "...KKKKKK...",
};
static const FxMap FXP_NOTE = {
  "......KKKKK.", "......KWLLK.", "......KLKKK.", "......KLK...",
  "......KLK...", "......KLK...", "......KLK...", "...KKKKLK...",
  "..KWLLMMK...", "..KLMMMDK...", "..KMMMDDK...", "...KKKKK....",
};

// Colour ramps per type, K D M L W, indexed by PkType.
static const uint16_t FX_PAL[18][5] = {
  { 0x2104, 0x8410, 0xC618, 0xEF7D, 0xFFFF },  // NORMAL
  { 0x5800, 0xD800, 0xFC00, 0xFFE0, 0xFFFF },  // FIRE
  { 0x0010, 0x021F, 0x2C7F, 0x969F, 0xFFFF },  // WATER
  { 0x4200, 0xE5A0, 0xFFE0, 0xFFF4, 0xFFFF },  // ELECTRIC
  { 0x0200, 0x1C03, 0x5647, 0xAF8F, 0xE7F9 },  // GRASS
  { 0x018C, 0x3CBB, 0x869E, 0xCF9F, 0xFFFF },  // ICE
  { 0x3000, 0xA142, 0xDAC6, 0xFDAF, 0xFFFF },  // FIGHTING
  { 0x2004, 0x68F1, 0xAA39, 0xDCBE, 0xFFFF },  // POISON
  { 0x3901, 0x82A5, 0xBC68, 0xE650, 0xFF9A },  // GROUND
  { 0x194A, 0x6C19, 0xADFE, 0xDF3F, 0xFFFF },  // FLYING
  { 0x4004, 0xC94D, 0xFAD4, 0xFD5A, 0xFFFF },  // PSYCHIC
  { 0x2100, 0x6C61, 0xAE45, 0xDF8F, 0xFFFF },  // BUG
  { 0x2902, 0x7B27, 0xB50C, 0xDE72, 0xF79A },  // ROCK
  { 0x1004, 0x414F, 0x7A97, 0xBCBE, 0xF71F },  // GHOST
  { 0x0806, 0x40F7, 0x6ADF, 0xB51F, 0xFFFF },  // DRAGON
  { 0x0000, 0x3103, 0x6248, 0xA40E, 0xE656 },  // DARK
  { 0x2124, 0x6B90, 0xAD77, 0xD6FC, 0xFFFF },  // STEEL
  { 0x4006, 0xDAD4, 0xFCB9, 0xFE7C, 0xFFFF },  // FAIRY
};
// The particle each type throws around.
static const FxMap *const FX_PART[18] = {
  &FXP_STAR, &FXP_FLAME, &FXP_DROP, &FXP_BOLT, &FXP_LEAF, &FXP_SHARD,
  &FXP_STAR, &FXP_ORB, &FXP_ROCK, &FXP_FEATHER, &FXP_SPARK, &FXP_NEEDLE,
  &FXP_ROCK, &FXP_ORB, &FXP_FLAME, &FXP_ORB, &FXP_SPARK, &FXP_HEART,
};

// The arcade's MEMORY cards and ECHO pads wear the same type sprites and
// ramps (ui_arcade.ino) -- an accessor rather than a second copy of the art.
void fxTypeIcon(uint8_t type, int cx, int cy, int s) {
  if (type >= 18) type = 0;
  if (s < 1) s = 1;
  // not fxSpr(): that one clips to the battle scene above FX_CLIP_Y
  const FxMap &m = *FX_PART[type];
  const uint16_t *p = FX_PAL[type];
  for (int r = 0; r < 12; r++)
    for (int c = 0; c < 12; c++) {
      const char *k = strchr("KDMLW", m[r][c]);
      if (!k || !m[r][c]) continue;
      gfx->fillRect(cx - 6 * s + c * s, cy - 6 * s + r * s, s, s, p[k - "KDMLW"]);
    }
}
uint16_t fxTypeCol(uint8_t type, uint8_t k) { return FX_PAL[type < 18 ? type : 0][k < 5 ? k : 4]; }
