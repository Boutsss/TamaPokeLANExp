#pragma once
#include <stdint.h>

// The customisable trainer ("YOU"): a 16x16 paper doll composed at draw time
// from these layers. All original pixel art, drawn for this project.
//
// Every grid is 16 rows of 16 chars. '.' leaves what is under it; any other
// char paints that palette SLOT (wardrobe.cpp fills the slots from the chosen
// items):
//   '#' outline   's' skin   'e' eyes   'h' hair
//   'c' top       'x' top accent        'w' white
//   'd' legs      'y' legs accent       'f' shoes
//   'k' hat       'K' hat accent        'g' face item
//   'b' back      'B' back accent
// Layers are painted in this order: back, BODY, legs, top, shoes, face, hair,
// hat -- so a cape shows only at the body's edges and a hat sits over hair.

typedef const char *const WGrid[16];

static WGrid W_BODY = {
  "................",
  "................",
  ".....######.....",
  "....#ssssss#....",
  "...#ssssssss#...",
  "..#ssssssssss#..",
  "..#ssessssess#..",
  "..#ssssssssss#..",
  "...#sss##sss#...",
  "....##ssss##....",
  ".##cccccccccc##.",
  ".#s#cccccccc#s#.",
  ".#s#cccccccc#s#.",
  "..##dddddddd##..",
  "...#ddd##ddd#...",
  "...#fff..fff#...",
};

#define W_ROWS(...) { __VA_ARGS__ }
#define W_BLANK "................"

// ---------------------------------------------------------------- hair styles
static WGrid W_HAIR_SHORT = { W_BLANK, W_BLANK, ".....######.....", "....#hhhhhh#....", "...#hhhhhhhh#...",
  "..#hhhhhhhhhh#..", "...h........h...", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_SPIKY = { "....#.#..#.#....", "....#h#hh#h#....", "...#hhhhhhhh#...", "...#hhhhhhhh#...",
  "..#hhhhhhhhhh#..", "..#hhhhhhhhhh#..", "...h........h...", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_SIDEPART = { W_BLANK, W_BLANK, ".....######.....", "....#hhhhhh#....", "...#hhhhhhhh#...",
  "..#hhhhhhhsss#..", "...h............", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_CURLY = { W_BLANK, "....##.##.##....", "...#hh#hh#hh#...", "..#hhhhhhhhhh#..",
  ".#hhhhhhhhhhhh#.", ".#hhhhhhhhhhhh#.", ".#h..........h#.", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_BUZZ = { W_BLANK, W_BLANK, ".....######.....", "....#hhhhhh#....", "...#hhhhhhhh#...",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_LONG = { W_BLANK, W_BLANK, ".....######.....", "....#hhhhhh#....", "...#hhhhhhhh#...",
  "..#hhhhhhhhhh#..", "..#h........h#..", "..#h........h#..", "..#h........h#..", "..#hh......hh#..",
  "..#h........h#..", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_PONYTAIL = { W_BLANK, W_BLANK, ".....######.....", "....#hhhhhh#..##", "...#hhhhhhhh##h#",
  "..#hhhhhhhhhh#h#", "...h........h##.", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_BOB = { W_BLANK, W_BLANK, "....########....", "...#hhhhhhhh#...", "..#hhhhhhhhhh#..",
  ".#hhhhhhhhhhhh#.", ".#hh........hh#.", ".#hh........hh#.", ".#hh........hh#.", "..##........##..",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_BRAIDS = { W_BLANK, W_BLANK, ".....######.....", "....#hhhhhh#....", "...#hhhhhhhh#...",
  "..#hhhhhhhhhh#..", ".#h..........h#.", ".#h..........h#.", ".#h..........h#.", ".#h..........h#.",
  ".#w..........w#.", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_BUNS = { "..###......###..", "..#hh#....#hh#..", "..###hhhhhh###..", "....#hhhhhh#....",
  "...#hhhhhhhh#...", "..#hhhhhhhhhh#..", "...h........h...", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAIR_MOHAWK = { "......####......", "......#hh#......", ".....#hhhh#.....", "....#shhhhs#....",
  "...#sshhhhss#...", "..#ssshhhhsss#..", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };

// ---------------------------------------------------------------- hats
static WGrid W_HAT_CAP = { W_BLANK, "....########....", "...#kkkkkkkk#...", "...#kkkKKkkk#...",
  "..############..", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_BEANIE = { "......#KK#......", ".....#kkkk#.....", "....#kkkkkk#....", "...#kkkkkkkk#...",
  "...#KKKKKKKK#...", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_SUNHAT = { W_BLANK, ".....######.....", "....#kkkkkk#....", "....#KKKKKK#....",
  "#kkkkkkkkkkkkkk#", ".##############.", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_BERET = { W_BLANK, "......####......", "...##kkkkkk#....", "..#kkkkkkkkkk#..",
  "...##########...", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_BANDANA = { W_BLANK, W_BLANK, W_BLANK, "...#kkkkkkkk#...", "..#kkKkkkkKkk#..",
  "..#kkkkkkkkkk#kk", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_WITCH = { ".......##.......", "......#kk#......", ".....#kkkk#.....", "....#KKKKKK#....",
  ".#kkkkkkkkkkkk#.", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_CROWN = { W_BLANK, "....K.K..K.K....", "....KKKKKKKK....", "....KkKKKKkK....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_TOPHAT = { "....########....", "....#kkkkkk#....", "....#kkkkkk#....", "....#KKKKKK#....",
  "..############..", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_FLOWER = { W_BLANK, W_BLANK, "..........KK....", ".........KkkK...", "..........KK....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_PHONES = { W_BLANK, W_BLANK, ".....######.....", "....#kkkkkk#....", "...k........k...",
  "..kk........kk..", ".#KK........KK#.", ".#KK........KK#.", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_HAT_BOW = { W_BLANK, "....##....##....", "...#kk#..#kk#...", "...#kkk##kkk#...",
  "....##kKKk##....", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };

// ---------------------------------------------------------------- tops (shape; colour is the item's)
static WGrid W_TOP_HOODIE = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  "...#cssssssc#...", W_BLANK, W_BLANK, ".#s#ccxxxxcc#s#.", W_BLANK, W_BLANK, W_BLANK };
static WGrid W_TOP_JACKET = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  ".##ccccwwcccc##.", ".#s#cccwwccc#s#.", ".#s#cccwwccc#s#.", W_BLANK, W_BLANK, W_BLANK };
static WGrid W_TOP_STRIPED = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, ".#s#xxxxxxxx#s#.", W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_TOP_TANK = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  "..#sccccccccs#..", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_TOP_DRESS = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, ".#s#xxxxxxxx#s#.", "..#cccccccccc#..", "...#cccccccc#...", W_BLANK };
static WGrid W_TOP_COAT = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, ".#s#cccxcccc#s#.", ".#s#cccxcccc#s#.", "..##cccccccc##..", W_BLANK, W_BLANK };
static WGrid W_TOP_KIMONO = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  ".##ccwcccccc##..", W_BLANK, ".#s#xxxxxxxx#s#.", W_BLANK, W_BLANK, W_BLANK };
static WGrid W_TOP_SUIT = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  ".##ccccwwcccc##.", ".#s#cccxxccc#s#.", W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_TOP_BALL = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, ".#s####ww####s#.", ".#s#wwwwwwww#s#.", W_BLANK, W_BLANK, W_BLANK };

// ---------------------------------------------------------------- legs
static WGrid W_LEGS_SHORTS = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, "...#sss##sss#...", W_BLANK };
static WGrid W_LEGS_SKIRT = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, "..#dddddddddd#..", "....#s#..#s#....", W_BLANK };
static WGrid W_LEGS_LONGSKIRT = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, "..#dddddddddd#..", "..#dddddddddd#..", W_BLANK };
static WGrid W_LEGS_PLAID = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, "..#dydydydydy#..", "....#s#..#s#....", W_BLANK };
static WGrid W_LEGS_CARGO = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, "...#dyd##dyd#...", W_BLANK };
static WGrid W_LEGS_TRACK = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, "..##yddddddy##..", "...#ydd##ddy#...", W_BLANK };

// ---------------------------------------------------------------- shoes
static WGrid W_SHOES_BOOTS = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, "...#fff##fff#...", W_BLANK };
static WGrid W_SHOES_SANDALS = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, "...#sfs..sfs#..." };

// ---------------------------------------------------------------- face
static WGrid W_FACE_GLASSES = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, "....geggggeg....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_FACE_SHADES = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, "....gggggggg....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_FACE_MASK = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  "..#gggggggggg#..", "...#gggggggg#...", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_FACE_EYEPATCH = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, "...g......g.....", "..........gg....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_FACE_BLUSH = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, "....g......g....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_FACE_BANDAGE = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, ".........ggg....",
  W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };

// ---------------------------------------------------------------- back (behind the body)
static WGrid W_BACK_PACK = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  "bb..............", "bB..............", "bb..............", W_BLANK, W_BLANK, W_BLANK };
static WGrid W_BACK_CAPE = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  "b..............b", "bb............bb", "bb............bb", "bbb..........bbb", "bbb..........bbb", W_BLANK };
static WGrid W_BACK_WINGS = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK,
  "bb............bb", "bBb..........bBb", "bBb..........bBb", ".b............b.", W_BLANK, W_BLANK, W_BLANK, W_BLANK };
static WGrid W_BACK_SWORD = { W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, "..............B.", ".............Bb.",
  "............bb..", W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK, W_BLANK };
