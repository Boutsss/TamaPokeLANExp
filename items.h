#pragma once
#include <stdint.h>

// The bag's item catalogue.
//
// APPEND-ONLY BELOW THE MARKER. An ItemKey is stored RAW in NVS by every
// inventory stack, exactly like a move index in Pet::moves[] -- inserting a key
// in the middle silently rewrites what every player is carrying. Same family of
// trap as § "uint8_t with a dex of 386" in CLAUDE.md: never reinterpret bytes
// that already exist.
//
// 0 is reserved so an empty stack and "POKE BALL" cannot be confused.
using ItemKey = uint8_t;
constexpr ItemKey ITEM_NONE = 0;

enum : uint8_t {
  IT_POKEBALL = 1,
  IT_GREATBALL,
  IT_ULTRABALL,
  IT_MASTERBALL,
  IT_POTION,
  IT_SUPERPOTION,
  IT_HYPERPOTION,
  IT_FULLHEAL,
  IT_PROTEIN,
  IT_IRON,
  IT_CARBOS,
  // Held items and key items are appended so every existing saved ItemKey
  // keeps its meaning on devices already in use.
  IT_ORAN_BERRY,
  IT_SITRUS_BERRY,
  IT_LUM_BERRY,
  IT_LEFTOVERS,
  IT_FOCUS_SASH,
  IT_QUICK_CLAW,
  IT_SHINY_CHARM,
  IT_OVAL_CHARM,
  // APPEND-ONLY BELOW HERE
  // v3.56: every battle held item of Gens 1-7 but Z-crystals, mega stones and
  // the form changers. Their order is their saved ItemKey: append only.
  IT_SILK_SCARF,
  IT_BLACK_BELT,
  IT_SHARP_BEAK,
  IT_POISON_BARB,
  IT_SOFT_SAND,
  IT_HARD_STONE,
  IT_SILVERPOWDER,
  IT_SPELL_TAG,
  IT_METAL_COAT,
  IT_CHARCOAL,
  IT_MYSTIC_WATER,
  IT_MIRACLE_SEED,
  IT_MAGNET,
  IT_TWISTEDSPOON,
  IT_NEVERMELTICE,
  IT_DRAGON_FANG,
  IT_BLACKGLASSES,
  IT_SEA_INCENSE,
  IT_WAVE_INCENSE,
  IT_ODD_INCENSE,
  IT_ROCK_INCENSE,
  IT_ROSE_INCENSE,
  IT_FLAME_PLATE,
  IT_SPLASH_PLATE,
  IT_ZAP_PLATE,
  IT_MEADOW_PLATE,
  IT_ICICLE_PLATE,
  IT_FIST_PLATE,
  IT_TOXIC_PLATE,
  IT_EARTH_PLATE,
  IT_SKY_PLATE,
  IT_MIND_PLATE,
  IT_INSECT_PLATE,
  IT_STONE_PLATE,
  IT_SPOOKY_PLATE,
  IT_DRACO_PLATE,
  IT_DREAD_PLATE,
  IT_IRON_PLATE,
  IT_PIXIE_PLATE,
  IT_NORMAL_GEM,
  IT_FIRE_GEM,
  IT_WATER_GEM,
  IT_ELECTRIC_GEM,
  IT_GRASS_GEM,
  IT_ICE_GEM,
  IT_FIGHTING_GEM,
  IT_POISON_GEM,
  IT_GROUND_GEM,
  IT_FLYING_GEM,
  IT_PSYCHIC_GEM,
  IT_BUG_GEM,
  IT_ROCK_GEM,
  IT_GHOST_GEM,
  IT_DRAGON_GEM,
  IT_DARK_GEM,
  IT_STEEL_GEM,
  IT_FAIRY_GEM,
  IT_CHILAN_BERRY,
  IT_OCCA_BERRY,
  IT_PASSHO_BERRY,
  IT_WACAN_BERRY,
  IT_RINDO_BERRY,
  IT_YACHE_BERRY,
  IT_CHOPLE_BERRY,
  IT_KEBIA_BERRY,
  IT_SHUCA_BERRY,
  IT_COBA_BERRY,
  IT_PAYAPA_BERRY,
  IT_TANGA_BERRY,
  IT_CHARTI_BERRY,
  IT_KASIB_BERRY,
  IT_HABAN_BERRY,
  IT_COLBUR_BERRY,
  IT_BABIRI_BERRY,
  IT_ROSELI_BERRY,
  IT_CHERI_BERRY,
  IT_CHESTO_BERRY,
  IT_PECHA_BERRY,
  IT_RAWST_BERRY,
  IT_ASPEAR_BERRY,
  IT_PERSIM_BERRY,
  IT_BERRY_JUICE,
  IT_FIGY_BERRY,
  IT_WIKI_BERRY,
  IT_MAGO_BERRY,
  IT_AGUAV_BERRY,
  IT_IAPAPA_BERRY,
  IT_ENIGMA_BERRY,
  IT_LIECHI_BERRY,
  IT_GANLON_BERRY,
  IT_SALAC_BERRY,
  IT_PETAYA_BERRY,
  IT_APICOT_BERRY,
  IT_LANSAT_BERRY,
  IT_STARF_BERRY,
  IT_MICLE_BERRY,
  IT_CUSTAP_BERRY,
  IT_KEE_BERRY,
  IT_MARANGA_BERRY,
  IT_JABOCA_BERRY,
  IT_ROWAP_BERRY,
  IT_CHOICE_BAND,
  IT_CHOICE_SPECS,
  IT_CHOICE_SCARF,
  IT_LIFE_ORB,
  IT_ASSAULT_VEST,
  IT_ROCKY_HELMET,
  IT_EXPERT_BELT,
  IT_EVIOLITE,
  IT_BLACK_SLUDGE,
  IT_WEAKNESS_POLICY,
  IT_AIR_BALLOON,
  IT_RED_CARD,
  IT_LIGHT_CLAY,
  IT_METRONOME,
  IT_TOXIC_ORB,
  IT_SHED_SHELL,
  IT_MENTAL_HERB,
  IT_DAMP_ROCK,
  IT_ELECTRIC_SEED,
  IT_PSYCHIC_SEED,
  IT_LIGHT_BALL,
  IT_THICK_CLUB,
  IT_SOUL_DEW,
  IT_ADAMANT_ORB,
  IT_LUSTROUS_ORB,
  IT_GRASSY_SEED,
  IT_MISTY_SEED,
  IT_HEAT_ROCK,
  IT_SMOOTH_ROCK,
  IT_ICY_ROCK,
  IT_TERRAIN_EXTENDER,
  IT_EJECT_BUTTON,
  IT_FLAME_ORB,
  IT_WHITE_HERB,
  IT_POWER_HERB,
  IT_MUSCLE_BAND,
  IT_WISE_GLASSES,
  IT_SCOPE_LENS,
  IT_RAZOR_CLAW,
  IT_KINGS_ROCK,
  IT_RAZOR_FANG,
  IT_WIDE_LENS,
  IT_ZOOM_LENS,
  IT_BRIGHTPOWDER,
  IT_FOCUS_BAND,
  IT_SHELL_BELL,
  IT_BIG_ROOT,
  IT_SAFETY_GOGGLES,
  IT_PROTECTIVE_PADS,
  IT_ABSORB_BULB,
  IT_CELL_BATTERY,
  IT_LUMINOUS_MOSS,
  IT_SNOWBALL,
  IT_BINDING_BAND,
  IT_GRIP_CLAW,
  IT_LEEK,
  IT_LUCKY_PUNCH,
  IT_DEEPSEATOOTH,
  IT_DEEPSEASCALE,
  IT_METAL_POWDER,
  IT_QUICK_POWDER,
  IT_LAX_INCENSE,
  IT_IRON_BALL,
  IT_LAGGING_TAIL,
  IT_FULL_INCENSE,
  IT_STICKY_BARB,
  IT_FLOAT_STONE,
  IT_RING_TARGET,
  ITEM_COUNT,
};

enum ItemCategory : uint8_t {
  IC_BALL = 0,   // only usable on a WILD foe
  IC_HEAL,       // restores HP, in battle or out
  IC_CURE,       // clears an ailment
  IC_TRAIN,      // permanently raises one training stat, out of battle
  IC_HELD,       // equipped by an individual creature; not used from the bag
  IC_KEY,        // permanent key item; not held or consumed
};

// A ball whose param is this catches without rolling. It is a sentinel rather
// than a huge percentage so wildCaptureChance() cannot round it back below 100.
constexpr int16_t ITEM_CATCH_GUARANTEED = -1;

// Which training stat an IC_TRAIN item feeds. Matches Pet::rewardTraining()'s
// `which` so the two never disagree about what 0/1/2 mean.
enum : uint8_t { TRAIN_ATK = 0, TRAIN_DEF, TRAIN_SPE };

// What a held item does in battle (battle.cpp is the only reader). `fa`/`fb`
// are its arguments, and their meaning depends on the effect: a type
// (T_* in dex.h), a stat (SI_* in battle.h), an ailment (AIL_*), a move
// category (MC_*), a weather (W_*) or a terrain (TR_*). They are written as
// plain numbers so this header does not pull those tables into every file;
// battle.cpp static_asserts the values it relies on.
enum HeldFx : uint8_t {
  HX_NONE = 0,
  HX_HEAL_FLAT, HX_HEAL_PCT, HX_LUM, HX_LEFTOVERS, HX_SASH, HX_QUICK_CLAW,
  HX_TYPE_BOOST, HX_PLATE, HX_GEM, HX_RESIST, HX_CURE, HX_PINCH_HEAL, HX_ENIGMA,
  HX_PINCH_STAT, HX_LANSAT, HX_STARF, HX_MICLE, HX_CUSTAP, HX_HIT_STAT, HX_JABOCA,
  HX_CHOICE, HX_LIFE_ORB, HX_ASSAULT_VEST, HX_ROCKY_HELMET, HX_EXPERT_BELT, HX_EVIOLITE,
  HX_BLACK_SLUDGE, HX_WEAKNESS_POLICY, HX_AIR_BALLOON, HX_RED_CARD, HX_LIGHT_CLAY,
  HX_METRONOME, HX_TOXIC_ORB, HX_SHED_SHELL, HX_MENTAL_HERB, HX_WEATHER_ROCK, HX_SEED,
  HX_SPECIES, HX_SPECIES_BOOST, HX_TERRAIN_EXT, HX_EJECT_BUTTON, HX_FLAME_ORB,
  HX_WHITE_HERB, HX_POWER_HERB, HX_MUSCLE_BAND, HX_WISE_GLASSES, HX_CRIT, HX_FLINCH,
  HX_WIDE_LENS, HX_ZOOM_LENS, HX_BRIGHT_POWDER, HX_FOCUS_BAND, HX_SHELL_BELL, HX_BIG_ROOT,
  HX_SAFETY_GOGGLES, HX_PROTECTIVE_PADS, HX_ABSORB, HX_BINDING_BAND, HX_GRIP_CLAW,
  HX_SPECIES_CRIT, HX_IRON_BALL, HX_LAGGING, HX_STICKY_BARB, HX_FLOAT_STONE, HX_RING_TARGET,
};

// The Battle Tower's reward tiers. Measured, not guessed: S and A follow how
// often each item is actually run in Smogon's Gen 7 OU usage statistics
// (smogon.com/stats, 2018-02, 1695+ ladder): S is everything above ~3% of
// held items (Leftovers 19%, Choice Scarf 11%, Choice Specs 6%, Assault Vest
// 6%, Life Orb, Rocky Helmet, Focus Sash, Choice Band ~3-4%), A the dependable
// second rank (Black Sludge, Eviolite, the terrain seeds, the pinch-heal
// berries, Light Clay, Weakness Policy...). B is the situational majority --
// type boosters, plates, gems, resist berries, lenses -- and C what is rarely
// worth a slot (Oran, single-status cures, Lagging Tail, Ring Target).
enum : uint8_t { HT_NONE = 0, HT_C, HT_B, HT_A, HT_S };

struct ItemEntry {
  const char *name;
  uint8_t category;
  int16_t param;       // IC_BALL: catch multiplier % · IC_HEAL: HP · IC_TRAIN: stat id
  uint8_t amount;      // IC_TRAIN: how many points
  uint8_t dropWeight;  // 0 = never drops from a wild win
  uint16_t price;      // Poke Mart cost, in Pokedollars; 0 = not sold there
  uint8_t fx = HX_NONE;   // IC_HELD: what it does in battle (HeldFx)
  uint8_t fa = 0, fb = 0; // its arguments, see HeldFx
  uint8_t tier = HT_NONE; // the tower's reward tier
};

// Indexed by ItemKey, so slot 0 is the empty filler and itemEntry() can return a
// reference for any key without a bounds decision at every call site.
//
// Ball prices are this game's own (v3.56), set well below the real games'
// $200/$600/$1200: one real step is $1, so those cost a long walk per ball.
// Training items keep the real games' price. Healing items (potions, FULL
// HEAL) are NOT sold any more -- they come from wild drops only -- and MASTER
// BALL never was, in any game. Berries are wild drops too.
static const ItemEntry ITEM_TBL[ITEM_COUNT] = {
  { "-",            IC_BALL,  0,                      0, 0,  0 },
  { "POKE BALL",    IC_BALL,  100,                    0, 30, 50 },
  { "GREAT BALL",   IC_BALL,  150,                    0, 15, 100 },
  { "ULTRA BALL",   IC_BALL,  200,                    0, 6,  200 },
  { "MASTER BALL",  IC_BALL,  ITEM_CATCH_GUARANTEED,  0, 1,  0 },
  { "POTION",       IC_HEAL,  20,                     0, 20, 0 },
  { "SUPER POTION", IC_HEAL,  50,                     0, 10, 0 },
  { "HYPER POTION", IC_HEAL,  120,                    0, 4,  0 },
  { "FULL HEAL",    IC_CURE,  0,                      0, 10, 0 },
  { "PROTEIN",      IC_TRAIN, TRAIN_ATK,              5, 8,  9800 },
  { "IRON",         IC_TRAIN, TRAIN_DEF,              5, 8,  9800 },
  { "CARBOS",       IC_TRAIN, TRAIN_SPE,              5, 8,  9800 },
  { "ORAN BERRY",   IC_HELD,  10,                     0, 12, 0, HX_HEAL_FLAT, 0, 0, HT_C },
  { "SITRUS BERRY", IC_HELD,  25,                     0, 5,  0, HX_HEAL_PCT,  0, 0, HT_A },
  { "LUM BERRY",    IC_HELD,  0,                      0, 4,  0, HX_LUM,       0, 0, HT_A },
  { "LEFTOVERS",    IC_HELD,  0,                      0, 0,  0, HX_LEFTOVERS, 0, 0, HT_S },
  { "FOCUS SASH",   IC_HELD,  0,                      0, 0,  0, HX_SASH,      0, 0, HT_S },
  { "QUICK CLAW",   IC_HELD,  20,                     0, 0,  0, HX_QUICK_CLAW, 0, 0, HT_B },
  { "SHINY CHARM",  IC_KEY,   0,                      0, 0,  0 },
  { "OVAL CHARM",   IC_KEY,   0,                      0, 0,  0 },
  { "SILK SCARF",       IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 0, 0, HT_B },   // T_NORMAL
  { "BLACK BELT",       IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 6, 0, HT_B },   // T_FIGHTING
  { "SHARP BEAK",       IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 9, 0, HT_B },   // T_FLYING
  { "POISON BARB",      IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 7, 0, HT_B },   // T_POISON
  { "SOFT SAND",        IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 8, 0, HT_B },   // T_GROUND
  { "HARD STONE",       IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 12, 0, HT_B },   // T_ROCK
  { "SILVERPOWDER",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 11, 0, HT_B },   // T_BUG
  { "SPELL TAG",        IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 13, 0, HT_B },   // T_GHOST
  { "METAL COAT",       IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 16, 0, HT_B },   // T_STEEL
  { "CHARCOAL",         IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 1, 0, HT_B },   // T_FIRE
  { "MYSTIC WATER",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 2, 0, HT_B },   // T_WATER
  { "MIRACLE SEED",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 4, 0, HT_B },   // T_GRASS
  { "MAGNET",           IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 3, 0, HT_B },   // T_ELECTRIC
  { "TWISTEDSPOON",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 10, 0, HT_B },   // T_PSYCHIC
  { "NEVERMELTICE",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 5, 0, HT_B },   // T_ICE
  { "DRAGON FANG",      IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 14, 0, HT_B },   // T_DRAGON
  { "BLACKGLASSES",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 15, 0, HT_B },   // T_DARK
  { "SEA INCENSE",      IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 2, 0, HT_C },   // T_WATER
  { "WAVE INCENSE",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 2, 0, HT_C },   // T_WATER
  { "ODD INCENSE",      IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 10, 0, HT_C },   // T_PSYCHIC
  { "ROCK INCENSE",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 12, 0, HT_C },   // T_ROCK
  { "ROSE INCENSE",     IC_HELD, 0, 0, 0, 0, HX_TYPE_BOOST, 4, 0, HT_C },   // T_GRASS
  { "FLAME PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 1, 0, HT_B },   // T_FIRE
  { "SPLASH PLATE",     IC_HELD, 0, 0, 0, 0, HX_PLATE, 2, 0, HT_B },   // T_WATER
  { "ZAP PLATE",        IC_HELD, 0, 0, 0, 0, HX_PLATE, 3, 0, HT_B },   // T_ELECTRIC
  { "MEADOW PLATE",     IC_HELD, 0, 0, 0, 0, HX_PLATE, 4, 0, HT_B },   // T_GRASS
  { "ICICLE PLATE",     IC_HELD, 0, 0, 0, 0, HX_PLATE, 5, 0, HT_B },   // T_ICE
  { "FIST PLATE",       IC_HELD, 0, 0, 0, 0, HX_PLATE, 6, 0, HT_B },   // T_FIGHTING
  { "TOXIC PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 7, 0, HT_B },   // T_POISON
  { "EARTH PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 8, 0, HT_B },   // T_GROUND
  { "SKY PLATE",        IC_HELD, 0, 0, 0, 0, HX_PLATE, 9, 0, HT_B },   // T_FLYING
  { "MIND PLATE",       IC_HELD, 0, 0, 0, 0, HX_PLATE, 10, 0, HT_B },   // T_PSYCHIC
  { "INSECT PLATE",     IC_HELD, 0, 0, 0, 0, HX_PLATE, 11, 0, HT_B },   // T_BUG
  { "STONE PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 12, 0, HT_B },   // T_ROCK
  { "SPOOKY PLATE",     IC_HELD, 0, 0, 0, 0, HX_PLATE, 13, 0, HT_B },   // T_GHOST
  { "DRACO PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 14, 0, HT_B },   // T_DRAGON
  { "DREAD PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 15, 0, HT_B },   // T_DARK
  { "IRON PLATE",       IC_HELD, 0, 0, 0, 0, HX_PLATE, 16, 0, HT_B },   // T_STEEL
  { "PIXIE PLATE",      IC_HELD, 0, 0, 0, 0, HX_PLATE, 17, 0, HT_B },   // T_FAIRY
  { "NORMAL GEM",       IC_HELD, 0, 0, 0, 0, HX_GEM, 0, 0, HT_B },   // T_NORMAL
  { "FIRE GEM",         IC_HELD, 0, 0, 0, 0, HX_GEM, 1, 0, HT_B },   // T_FIRE
  { "WATER GEM",        IC_HELD, 0, 0, 0, 0, HX_GEM, 2, 0, HT_B },   // T_WATER
  { "ELECTRIC GEM",     IC_HELD, 0, 0, 0, 0, HX_GEM, 3, 0, HT_B },   // T_ELECTRIC
  { "GRASS GEM",        IC_HELD, 0, 0, 0, 0, HX_GEM, 4, 0, HT_B },   // T_GRASS
  { "ICE GEM",          IC_HELD, 0, 0, 0, 0, HX_GEM, 5, 0, HT_B },   // T_ICE
  { "FIGHTING GEM",     IC_HELD, 0, 0, 0, 0, HX_GEM, 6, 0, HT_B },   // T_FIGHTING
  { "POISON GEM",       IC_HELD, 0, 0, 0, 0, HX_GEM, 7, 0, HT_B },   // T_POISON
  { "GROUND GEM",       IC_HELD, 0, 0, 0, 0, HX_GEM, 8, 0, HT_B },   // T_GROUND
  { "FLYING GEM",       IC_HELD, 0, 0, 0, 0, HX_GEM, 9, 0, HT_B },   // T_FLYING
  { "PSYCHIC GEM",      IC_HELD, 0, 0, 0, 0, HX_GEM, 10, 0, HT_B },   // T_PSYCHIC
  { "BUG GEM",          IC_HELD, 0, 0, 0, 0, HX_GEM, 11, 0, HT_B },   // T_BUG
  { "ROCK GEM",         IC_HELD, 0, 0, 0, 0, HX_GEM, 12, 0, HT_B },   // T_ROCK
  { "GHOST GEM",        IC_HELD, 0, 0, 0, 0, HX_GEM, 13, 0, HT_B },   // T_GHOST
  { "DRAGON GEM",       IC_HELD, 0, 0, 0, 0, HX_GEM, 14, 0, HT_B },   // T_DRAGON
  { "DARK GEM",         IC_HELD, 0, 0, 0, 0, HX_GEM, 15, 0, HT_B },   // T_DARK
  { "STEEL GEM",        IC_HELD, 0, 0, 0, 0, HX_GEM, 16, 0, HT_B },   // T_STEEL
  { "FAIRY GEM",        IC_HELD, 0, 0, 0, 0, HX_GEM, 17, 0, HT_B },   // T_FAIRY
  { "CHILAN BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 0, 0, HT_B },   // T_NORMAL
  { "OCCA BERRY",       IC_HELD, 0, 0, 0, 0, HX_RESIST, 1, 0, HT_B },   // T_FIRE
  { "PASSHO BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 2, 0, HT_B },   // T_WATER
  { "WACAN BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 3, 0, HT_B },   // T_ELECTRIC
  { "RINDO BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 4, 0, HT_B },   // T_GRASS
  { "YACHE BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 5, 0, HT_B },   // T_ICE
  { "CHOPLE BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 6, 0, HT_B },   // T_FIGHTING
  { "KEBIA BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 7, 0, HT_B },   // T_POISON
  { "SHUCA BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 8, 0, HT_B },   // T_GROUND
  { "COBA BERRY",       IC_HELD, 0, 0, 0, 0, HX_RESIST, 9, 0, HT_B },   // T_FLYING
  { "PAYAPA BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 10, 0, HT_B },   // T_PSYCHIC
  { "TANGA BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 11, 0, HT_B },   // T_BUG
  { "CHARTI BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 12, 0, HT_B },   // T_ROCK
  { "KASIB BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 13, 0, HT_B },   // T_GHOST
  { "HABAN BERRY",      IC_HELD, 0, 0, 0, 0, HX_RESIST, 14, 0, HT_B },   // T_DRAGON
  { "COLBUR BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 15, 0, HT_B },   // T_DARK
  { "BABIRI BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 16, 0, HT_B },   // T_STEEL
  { "ROSELI BERRY",     IC_HELD, 0, 0, 0, 0, HX_RESIST, 17, 0, HT_B },   // T_FAIRY
  { "CHERI BERRY",      IC_HELD, 0, 0, 0, 0, HX_CURE, 1, 0, HT_C },   // AIL_PARA
  { "CHESTO BERRY",     IC_HELD, 0, 0, 0, 0, HX_CURE, 4, 0, HT_C },   // AIL_SLEEP
  { "PECHA BERRY",      IC_HELD, 0, 0, 0, 0, HX_CURE, 3, 0, HT_C },   // AIL_POISON
  { "RAWST BERRY",      IC_HELD, 0, 0, 0, 0, HX_CURE, 2, 0, HT_C },   // AIL_BURN
  { "ASPEAR BERRY",     IC_HELD, 0, 0, 0, 0, HX_CURE, 5, 0, HT_C },   // AIL_FREEZE
  { "PERSIM BERRY",     IC_HELD, 0, 0, 0, 0, HX_CURE, 6, 0, HT_C },   // AIL_CONFUSE
  { "BERRY JUICE",      IC_HELD, 20, 0, 0, 0, HX_HEAL_FLAT, 0, 0, HT_C },
  { "FIGY BERRY",       IC_HELD, 50, 0, 0, 0, HX_PINCH_HEAL, 0, 0, HT_A },
  { "WIKI BERRY",       IC_HELD, 50, 0, 0, 0, HX_PINCH_HEAL, 0, 0, HT_A },
  { "MAGO BERRY",       IC_HELD, 50, 0, 0, 0, HX_PINCH_HEAL, 0, 0, HT_A },
  { "AGUAV BERRY",      IC_HELD, 50, 0, 0, 0, HX_PINCH_HEAL, 0, 0, HT_A },
  { "IAPAPA BERRY",     IC_HELD, 50, 0, 0, 0, HX_PINCH_HEAL, 0, 0, HT_A },
  { "ENIGMA BERRY",     IC_HELD, 25, 0, 0, 0, HX_ENIGMA, 0, 0, HT_B },
  { "LIECHI BERRY",     IC_HELD, 0, 0, 0, 0, HX_PINCH_STAT, 0, 0, HT_B },   // SI_ATK
  { "GANLON BERRY",     IC_HELD, 0, 0, 0, 0, HX_PINCH_STAT, 1, 0, HT_B },   // SI_DEF
  { "SALAC BERRY",      IC_HELD, 0, 0, 0, 0, HX_PINCH_STAT, 4, 0, HT_B },   // SI_SPE
  { "PETAYA BERRY",     IC_HELD, 0, 0, 0, 0, HX_PINCH_STAT, 2, 0, HT_B },   // SI_SPA
  { "APICOT BERRY",     IC_HELD, 0, 0, 0, 0, HX_PINCH_STAT, 3, 0, HT_B },   // SI_SPD
  { "LANSAT BERRY",     IC_HELD, 0, 0, 0, 0, HX_LANSAT, 0, 0, HT_B },
  { "STARF BERRY",      IC_HELD, 0, 0, 0, 0, HX_STARF, 0, 0, HT_B },
  { "MICLE BERRY",      IC_HELD, 0, 0, 0, 0, HX_MICLE, 0, 0, HT_B },
  { "CUSTAP BERRY",     IC_HELD, 0, 0, 0, 0, HX_CUSTAP, 0, 0, HT_B },
  { "KEE BERRY",        IC_HELD, 0, 0, 0, 0, HX_HIT_STAT, 0, 1, HT_B },   // MC_PHYS SI_DEF
  { "MARANGA BERRY",    IC_HELD, 0, 0, 0, 0, HX_HIT_STAT, 1, 3, HT_B },   // MC_SPEC SI_SPD
  { "JABOCA BERRY",     IC_HELD, 0, 0, 0, 0, HX_JABOCA, 0, 0, HT_B },   // MC_PHYS
  { "ROWAP BERRY",      IC_HELD, 0, 0, 0, 0, HX_JABOCA, 1, 0, HT_B },   // MC_SPEC
  { "CHOICE BAND",      IC_HELD, 0, 0, 0, 0, HX_CHOICE, 0, 0, HT_S },   // SI_ATK
  { "CHOICE SPECS",     IC_HELD, 0, 0, 0, 0, HX_CHOICE, 2, 0, HT_S },   // SI_SPA
  { "CHOICE SCARF",     IC_HELD, 0, 0, 0, 0, HX_CHOICE, 4, 0, HT_S },   // SI_SPE
  { "LIFE ORB",         IC_HELD, 0, 0, 0, 0, HX_LIFE_ORB, 0, 0, HT_S },
  { "ASSAULT VEST",     IC_HELD, 0, 0, 0, 0, HX_ASSAULT_VEST, 0, 0, HT_S },
  { "ROCKY HELMET",     IC_HELD, 0, 0, 0, 0, HX_ROCKY_HELMET, 0, 0, HT_S },
  { "EXPERT BELT",      IC_HELD, 0, 0, 0, 0, HX_EXPERT_BELT, 0, 0, HT_A },
  { "EVIOLITE",         IC_HELD, 0, 0, 0, 0, HX_EVIOLITE, 0, 0, HT_A },
  { "BLACK SLUDGE",     IC_HELD, 0, 0, 0, 0, HX_BLACK_SLUDGE, 0, 0, HT_A },
  { "WEAKNESS POLICY",  IC_HELD, 0, 0, 0, 0, HX_WEAKNESS_POLICY, 0, 0, HT_A },
  { "AIR BALLOON",      IC_HELD, 0, 0, 0, 0, HX_AIR_BALLOON, 0, 0, HT_A },
  { "RED CARD",         IC_HELD, 0, 0, 0, 0, HX_RED_CARD, 0, 0, HT_A },
  { "LIGHT CLAY",       IC_HELD, 0, 0, 0, 0, HX_LIGHT_CLAY, 0, 0, HT_A },
  { "METRONOME",        IC_HELD, 0, 0, 0, 0, HX_METRONOME, 0, 0, HT_A },
  { "TOXIC ORB",        IC_HELD, 0, 0, 0, 0, HX_TOXIC_ORB, 0, 0, HT_A },
  { "SHED SHELL",       IC_HELD, 0, 0, 0, 0, HX_SHED_SHELL, 0, 0, HT_A },
  { "MENTAL HERB",      IC_HELD, 0, 0, 0, 0, HX_MENTAL_HERB, 0, 0, HT_A },
  { "DAMP ROCK",        IC_HELD, 0, 0, 0, 0, HX_WEATHER_ROCK, 1, 0, HT_A },   // W_RAIN
  { "ELECTRIC SEED",    IC_HELD, 0, 0, 0, 0, HX_SEED, 1, 1, HT_A },   // TR_ELECTRIC SI_DEF
  { "PSYCHIC SEED",     IC_HELD, 0, 0, 0, 0, HX_SEED, 4, 3, HT_A },   // TR_PSYCHIC SI_SPD
  { "LIGHT BALL",       IC_HELD, 0, 0, 0, 0, HX_SPECIES, 0, 0, HT_A },
  { "THICK CLUB",       IC_HELD, 0, 0, 0, 0, HX_SPECIES, 1, 0, HT_A },
  { "SOUL DEW",         IC_HELD, 0, 0, 0, 0, HX_SPECIES_BOOST, 0, 0, HT_A },
  { "ADAMANT ORB",      IC_HELD, 0, 0, 0, 0, HX_SPECIES_BOOST, 1, 0, HT_A },
  { "LUSTROUS ORB",     IC_HELD, 0, 0, 0, 0, HX_SPECIES_BOOST, 2, 0, HT_A },
  { "GRASSY SEED",      IC_HELD, 0, 0, 0, 0, HX_SEED, 2, 1, HT_B },   // TR_GRASSY SI_DEF
  { "MISTY SEED",       IC_HELD, 0, 0, 0, 0, HX_SEED, 3, 3, HT_B },   // TR_MISTY SI_SPD
  { "HEAT ROCK",        IC_HELD, 0, 0, 0, 0, HX_WEATHER_ROCK, 2, 0, HT_B },   // W_SUN
  { "SMOOTH ROCK",      IC_HELD, 0, 0, 0, 0, HX_WEATHER_ROCK, 3, 0, HT_B },   // W_SAND
  { "ICY ROCK",         IC_HELD, 0, 0, 0, 0, HX_WEATHER_ROCK, 4, 0, HT_B },   // W_HAIL
  { "TERRAIN EXTENDER", IC_HELD, 0, 0, 0, 0, HX_TERRAIN_EXT, 0, 0, HT_B },
  { "EJECT BUTTON",     IC_HELD, 0, 0, 0, 0, HX_EJECT_BUTTON, 0, 0, HT_B },
  { "FLAME ORB",        IC_HELD, 0, 0, 0, 0, HX_FLAME_ORB, 0, 0, HT_B },
  { "WHITE HERB",       IC_HELD, 0, 0, 0, 0, HX_WHITE_HERB, 0, 0, HT_B },
  { "POWER HERB",       IC_HELD, 0, 0, 0, 0, HX_POWER_HERB, 0, 0, HT_B },
  { "MUSCLE BAND",      IC_HELD, 0, 0, 0, 0, HX_MUSCLE_BAND, 0, 0, HT_B },
  { "WISE GLASSES",     IC_HELD, 0, 0, 0, 0, HX_WISE_GLASSES, 0, 0, HT_B },
  { "SCOPE LENS",       IC_HELD, 0, 0, 0, 0, HX_CRIT, 0, 0, HT_B },
  { "RAZOR CLAW",       IC_HELD, 0, 0, 0, 0, HX_CRIT, 0, 0, HT_B },
  { "KINGS ROCK",       IC_HELD, 0, 0, 0, 0, HX_FLINCH, 0, 0, HT_B },
  { "RAZOR FANG",       IC_HELD, 0, 0, 0, 0, HX_FLINCH, 0, 0, HT_B },
  { "WIDE LENS",        IC_HELD, 0, 0, 0, 0, HX_WIDE_LENS, 0, 0, HT_B },
  { "ZOOM LENS",        IC_HELD, 0, 0, 0, 0, HX_ZOOM_LENS, 0, 0, HT_B },
  { "BRIGHTPOWDER",     IC_HELD, 0, 0, 0, 0, HX_BRIGHT_POWDER, 0, 0, HT_B },
  { "FOCUS BAND",       IC_HELD, 0, 0, 0, 0, HX_FOCUS_BAND, 0, 0, HT_B },
  { "SHELL BELL",       IC_HELD, 0, 0, 0, 0, HX_SHELL_BELL, 0, 0, HT_B },
  { "BIG ROOT",         IC_HELD, 0, 0, 0, 0, HX_BIG_ROOT, 0, 0, HT_B },
  { "SAFETY GOGGLES",   IC_HELD, 0, 0, 0, 0, HX_SAFETY_GOGGLES, 0, 0, HT_B },
  { "PROTECTIVE PADS",  IC_HELD, 0, 0, 0, 0, HX_PROTECTIVE_PADS, 0, 0, HT_B },
  { "ABSORB BULB",      IC_HELD, 0, 0, 0, 0, HX_ABSORB, 2, 2, HT_B },   // T_WATER SI_SPA
  { "CELL BATTERY",     IC_HELD, 0, 0, 0, 0, HX_ABSORB, 3, 0, HT_B },   // T_ELECTRIC SI_ATK
  { "LUMINOUS MOSS",    IC_HELD, 0, 0, 0, 0, HX_ABSORB, 2, 3, HT_B },   // T_WATER SI_SPD
  { "SNOWBALL",         IC_HELD, 0, 0, 0, 0, HX_ABSORB, 5, 0, HT_B },   // T_ICE SI_ATK
  { "BINDING BAND",     IC_HELD, 0, 0, 0, 0, HX_BINDING_BAND, 0, 0, HT_B },
  { "GRIP CLAW",        IC_HELD, 0, 0, 0, 0, HX_GRIP_CLAW, 0, 0, HT_B },
  { "LEEK",             IC_HELD, 0, 0, 0, 0, HX_SPECIES_CRIT, 0, 0, HT_B },
  { "LUCKY PUNCH",      IC_HELD, 0, 0, 0, 0, HX_SPECIES_CRIT, 1, 0, HT_B },
  { "DEEPSEATOOTH",     IC_HELD, 0, 0, 0, 0, HX_SPECIES, 2, 0, HT_B },
  { "DEEPSEASCALE",     IC_HELD, 0, 0, 0, 0, HX_SPECIES, 3, 0, HT_B },
  { "METAL POWDER",     IC_HELD, 0, 0, 0, 0, HX_SPECIES, 4, 0, HT_B },
  { "QUICK POWDER",     IC_HELD, 0, 0, 0, 0, HX_SPECIES, 5, 0, HT_B },
  { "LAX INCENSE",      IC_HELD, 0, 0, 0, 0, HX_BRIGHT_POWDER, 0, 0, HT_C },
  { "IRON BALL",        IC_HELD, 0, 0, 0, 0, HX_IRON_BALL, 0, 0, HT_C },
  { "LAGGING TAIL",     IC_HELD, 0, 0, 0, 0, HX_LAGGING, 0, 0, HT_C },
  { "FULL INCENSE",     IC_HELD, 0, 0, 0, 0, HX_LAGGING, 0, 0, HT_C },
  { "STICKY BARB",      IC_HELD, 0, 0, 0, 0, HX_STICKY_BARB, 0, 0, HT_C },
  { "FLOAT STONE",      IC_HELD, 0, 0, 0, 0, HX_FLOAT_STONE, 0, 0, HT_C },
  { "RING TARGET",      IC_HELD, 0, 0, 0, 0, HX_RING_TARGET, 0, 0, HT_C },
};

// ItemKey is a uint8_t and friends.cpp uses 0xEE as a known-invalid key.
static_assert(ITEM_COUNT < 0xEE, "ItemKey is 8 bits; split the catalogue before it reaches 0xEE");

inline bool itemValid(ItemKey k) { return k > ITEM_NONE && k < ITEM_COUNT; }
inline const ItemEntry &itemEntry(ItemKey k) {
  return ITEM_TBL[itemValid(k) ? k : ITEM_NONE];
}
inline bool itemIsBall(ItemKey k) {
  return itemValid(k) && itemEntry(k).category == IC_BALL;
}
inline bool itemIsHeld(ItemKey k) {
  return itemValid(k) && itemEntry(k).category == IC_HELD;
}
inline uint8_t heldFx(ItemKey k) { return itemIsHeld(k) ? itemEntry(k).fx : HX_NONE; }
inline uint8_t itemTier(ItemKey k) { return itemIsHeld(k) ? itemEntry(k).tier : HT_NONE; }
inline bool itemIsKey(ItemKey k) {
  return itemValid(k) && itemEntry(k).category == IC_KEY;
}
// Usable from the bag on the main screen. Only training items: the pet has no
// persistent HP or ailment outside a fight -- those live on the Combatant and
// are discarded when it ends -- so a potion in the field would have nothing to
// act on and would silently vanish.
inline bool itemUsableInField(ItemKey k) {
  return itemValid(k) && itemEntry(k).category == IC_TRAIN;
}
// Usable from inside a fight. Training is permanent and belongs to the care
// sim, so it stays out of the battle bag.
inline bool itemUsableInBattle(ItemKey k) {
  return itemValid(k) && (itemEntry(k).category == IC_BALL ||
                          itemEntry(k).category == IC_HEAL ||
                          itemEntry(k).category == IC_CURE);
}

// The Poke Mart's catalogue, derived from `price` rather than restated as its
// own list -- an item becomes sellable the moment it is given a price, the same
// way an item becomes a possible wild drop the moment it is given a dropWeight.
inline bool itemForSale(ItemKey k) { return itemValid(k) && itemEntry(k).price > 0; }
inline uint8_t martSellableCount() {
  uint8_t n = 0;
  for (uint8_t k = 1; k < ITEM_COUNT; k++) if (itemForSale(k)) n++;
  return n;
}
// The i-th sellable key, for the Mart's paged list -- same shape as
// Inventory::keyAt() for the bag.
inline ItemKey martKeyAt(uint8_t i) {
  for (uint8_t k = 1; k < ITEM_COUNT; k++) {
    if (!itemForSale(k)) continue;
    if (!i) return k;
    i--;
  }
  return ITEM_NONE;
}
