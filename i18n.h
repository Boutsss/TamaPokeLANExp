#pragma once
#include <Arduino.h>

// Idiomas soportados. La fuente del firmware no tiene acentos: ambos textos van
// sin tildes ni enes (igual que ya iba el espanol).
enum Lang : uint8_t { LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT, LANG_COUNT };
#define LANG_DEFAULT LANG_EN  // idioma por defecto: ingles

extern Lang gLang;  // idioma activo (definido en i18n.cpp)

// IDs de cadena. El orden debe coincidir con la tabla STRINGS de i18n.cpp.
// uint16_t, NOT uint8_t: the table passed 255 entries when the feature ports
// landed (v3.29). Same trap as CLAUDE.md "uint8_t with a dex of 386" -- a byte
// wide enum stops compiling, which is the lucky version of it; never store a
// StrId in a uint8_t, and loop over STR_COUNT with an int.
enum StrId : uint16_t {
  // estado del bicho (statusMsg)
  S_EVOLVING, S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD, S_CHUBBY, S_IS_SHINY, S_HAPPY,
  // ceremonias de despedida
  S_FAREWELL, S_RUNAWAY, S_GOODBYE,
  // huevo
  S_EGG_HDR, S_EGG_LEGEND, S_EGG_RARE, S_EGG_TOUCH, S_EGG_MOVES, S_EGG_ALMOST,
  // formatos compartidos
  S_POKEDEX_FMT,   // "POKEDEX %u/151"
  S_NAME_FMT,      // "%s%s Nv.%u"
  // dialogo soltar
  S_RELEASE_FMT, S_YES, S_NO,
  // minijuego y saco
  S_HITS_FMT, S_STR_GAIN_FMT, S_NEW_RECORD, S_RECORD_FMT, S_HIT_FAST,
  S_SCORE_FMT, S_GREAT_JOY, S_PLUS_JOY,
  // reloj / ajustes
  S_SET_TIME, S_HOUR, S_MIN, S_CLOCK_CANCEL, S_LANG_LABEL,
  // celebracion
  S_MEDAL_BANNER, S_GREAT, S_STREAK_DAYS_FMT,
  // ficha: perfil
  S_STREAK_FMT, S_VIN, S_BERRY_UNK, S_BERRY_RED, S_BERRY_BLUE, S_BERRY_GREEN,
  S_INFO_FMT, S_RENAME_HINT,
  // ficha: combate
  S_BATTLE, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_TRAIN_STR,
  // ficha: medallas
  S_MEDALS_FMT, S_BACK,
  // teclado y galeria
  S_NAME, S_DETAIL_BACK,
  // barras
  S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG,
  // marcador en vivo del minijuego
  S_REC_FMT,
  // ficha: pagina de progreso
  S_PROGRESS, S_LVL_FMT, S_NEXT_LVL_FMT, S_EVO_LABEL, S_FINAL_FORM,
  S_EVO_READY, S_EVO_BLOCKED, S_EVO_IN_FMT, S_MISTAKES_FMT,
  // interruptor de sonido (ajustes)
  S_SND_ON, S_SND_OFF,
  S_EVO_TAP,        // texto del boton de evolucion
  S_FAREWELL_BTN,   // texto del boton de despedida (lleva el nombre: "%s ...")
  S_RUNAWAY_BTN,    // texto del boton de escapada por abandono (final triste)
  // dialogos de decision (evolucionar/mantener, despedirse/quedaros)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // titulo de la eleccion del inicial (primera vez)
  S_NO_SPRITES, S_LOAD_SPRITES,  // aviso cuando falta el sprite en la SD
  S_STAT_VIT,   // vitalidad (abreviatura de PS/HP en cada idioma)
  S_IV_FMT,     // valor individual junto a cada stat ("IV %u")
  // menu overlay (tap the name on the main screen) and the party
  S_MENU_TITLE, S_SETTINGS, S_CLOSE,
  S_PARTY_FMT,      // "PARTY %u/6"
  S_PARTY,          // short navigation label
  S_PARTY_EMPTY,    // empty slot marker
  S_PARTY_JOINED,   // "%s joined the party!"
  S_PARTY_FULL,     // prompt: pick someone to replace
  S_PARTY_LETGO,    // decline the swap
  // stats shortcut in the menu, and the training submenu behind the 5th icon
  S_STATS,          // menu row: jump straight to the stats card page
  S_TRAIN,          // training submenu title
  S_TR_ATK,         // row: train strength (the punching bag)
  S_TR_SPE,         // row: train speed (the ball game)
  S_TR_DEF,         // row: defence -- passive, shown for information only
  S_TR_DEF_HINT,    // why the defence row does nothing when tapped
  // moves card page + the move picker behind it
  S_MOVES,          // card page title
  S_MOVE_EMPTY,     // an unused move slot
  S_MOVE_PICK,      // picker title: choose what goes in this slot
  S_MOVE_TAP,       // hint on the moves page: tap a slot to change it
  S_MOVE_PWR,       // "PWR %u" next to an attacking move
  S_MOVE_STATUS,    // shown instead of power for a status move
  // level-up learn prompt
  S_LEARN_Q,        // "%s wants to learn" (name of the creature)
  S_LEARN_SKIP,     // decline the new move
  // battle
  S_BTL_USED,       // "%s used %s"
  S_BTL_SUPER, S_BTL_WEAK, S_BTL_IMMUNE,
  S_BTL_MISS, S_BTL_CRIT, S_BTL_FAINT,
  S_BTL_HURTSELF,   // confusion
  S_BTL_STATUS,     // "%s was %s" -- ailment name comes from S_AIL_*
  S_AIL_PARA, S_AIL_BURN, S_AIL_POISON, S_AIL_SLEEP, S_AIL_FREEZE, S_AIL_CONFUSE,
  S_BTL_WIN, S_BTL_LOSE,
  S_BTL_SENDS,      // "%s sends out %s"
  S_BTL_GO,         // "Go, %s!"
  S_GYMS,           // gym list title
  S_BADGES_FMT,     // "BADGES %u/8"
  S_TRAINER,        // player card title
  S_SPD_GAIN_FMT,   // "SPEED +%u" after the reaction test
  S_AVATAR_HINT,    // "tap: change avatar"
  S_MEDALS_TOTAL_FMT,  // "%u earned in all"
  S_EASY, S_HARD,   // gym ladder difficulty toggle
  S_PICK_FMT,       // "CHOSEN %u/%u"
  S_FIGHT,          // start the battle
  S_LOCKED,         // a gym not yet unlocked
  S_BTL_SWITCH,     // battle menu: bring on another creature
  S_BTL_BEAT,       // "%s defeated!" on the win screen
  S_BTL_NEWBADGE,   // a badge earned for the first time
  S_VOL_FMT,        // volume level, e.g. "VOL 7"
  S_BOX_FMT,        // "BOX %u/%u"
  S_BOX_SWAP,       // "swap with %s: pick a slot"
  S_BOX_BTN,        // the button on the party screen
  S_REVIVE,         // bring a banked creature back as a companion
  S_REVIVE_EGG,     // why it is unavailable right now
  // LAN battle
  S_LAN, S_LAN_HOST, S_LAN_JOIN, S_LAN_WAIT, S_LAN_READY, S_LAN_REFUSED,
  S_LAN_PICK, S_LAN_VS,
  S_LAN_GONE, S_LAN_WAITFOE, S_LAN_REMATCH,
  S_BTL_RUN,        // the third option in a fight: leave it
  S_EGG_REGION,     // hint under the egg's region pill
  S_WIN_TRAIN_FMT,  // "%s +%u" -- the training a gym win is worth
  S_WIN_MAXED,      // ... or that there is nowhere left to train
  S_CHOOSE_REGION,  // first boot: pick the region your starter comes from
  S_RETIRE,         // menu row: retire the creature on demand
  S_RETIRE_Q,       // ...and the confirmation
  S_RETIRE_COST,    // what retiring early costs the next creature
  S_EVO_SLOW,       // shown on the card while a creature carries that debt
  S_NEED_PACK,      // region chooser: this region's sprite pack is not on the SD
  // Letting a BANKED creature go for good, from the party or box detail sheet.
  // The confirmation itself reuses S_RELEASE_FMT / S_YES / S_NO, which the live
  // pet's own release dialog already owns -- the question is the same question.
  S_RELEASE_BTN,    // the button on the detail sheet
  S_RELEASE_GONE,   // ...and the warning under it: this one does not come back
  S_BOX_TAKE,       // box detail sheet: move this creature into the party
  S_RETIRE_GONE,    // retire confirm: an early retire is not banked either
  // Wild encounters, the bag, and choosing which creature you raise.
  S_BAG,            // menu row, and the battle menu's fourth option
  S_BAG_EMPTY,      // ...with nothing in it
  S_EXPLORE,        // gym screen: go looking for a wild creature
  S_WILD_MET,       // "A wild %s appeared!"
  S_WILD_FLED,      // "%s fled!"
  S_GOT_AWAY,       // running from a wild fight succeeded
  S_NO_ESCAPE,      // ...and failed, which costs the turn
  S_THREW,          // "You threw a %s"
  S_CAUGHT,         // "%s was caught!"
  S_BROKE_FREE,     // the ball failed
  S_ITEM_USED,      // "Used %s"
  S_ITEM_NOUSE,     // the item was valid but had nothing to do
  S_FOUND_ITEM,     // a wild win's drop
  S_FOCUS,          // party/box sheet: raise this one instead of the live pet
  S_FOCUS_NOW,      // ...and the label once it already is the one being raised
  // A way out of "waiting for the rival" that does not require the rival's
  // radio to ever answer. S_YES/S_NO on the confirm are reused, same as the
  // release dialog does.
  S_LAN_QUIT,       // the button, once the wait has run long enough to show it
  S_LAN_QUIT_Q,     // ...and the confirmation
  // The rival record on the LAN pairing screen. Two variants each because the
  // table has no pluralization rule of its own -- some languages need a
  // different word at exactly 1 (English "time"/"times"), some do not
  // (French "fois", German "Mal" do not change), so each language just
  // writes both and the render code picks by count == 1.
  S_LAN_DEFEATED_1, S_LAN_DEFEATED_N,
  S_LAN_LOST_1, S_LAN_LOST_N,
  // the Poke Mart
  S_MART,           // screen title
  S_MART_RATE,      // the exchange rate, e.g. "1 pedometer step = $1"
  S_MART_STEPS_FMT, // "your lifetime step count: %lu"
  S_WALLET_FMT,     // "Wallet: $%lu"
  S_MART_BUY_Q,     // buy confirm: "Buy %s?"
  S_MART_PRICE_FMT, // buy confirm sub-line: "COST: $%lu"
  S_DC_TITLE,       // daycare: screen title
  S_DC_PICK,        // daycare: an empty parent slot: tap to choose
  S_DC_BREED,       // daycare: the breed button
  S_DC_SWAP,        // daycare: make the uncracked egg the bred one
  S_DC_SURE,        // daycare: second tap confirms the swap
  S_DC_V_EMPTY,     // daycare: verdict: nothing picked yet
  S_DC_V_SAME,      // daycare: verdict
  S_DC_V_DITTO,     // daycare: verdict
  S_DC_V_LEGEND,    // daycare: verdict
  S_DC_V_TYPE,      // daycare: verdict
  S_DC_V_NOEGG,     // daycare: verdict: the egg could not hatch here
  S_DC_V_OK,        // daycare: verdict
  S_DC_FULL,        // daycare: an egg is already queued
  S_DC_WAIT,        // daycare: bred today already
  S_DC_TODAY,       // daycare: cooldown is clear
  S_DC_MADE,        // daycare: an egg was just bred
  S_DC_SWAPPED,     // daycare: the live egg was swapped
  S_DC_NEXT_FMT,    // daycare: the queued egg: "NEXT EGG: %s"
  S_DC_NONE,        // daycare: the picker has nobody to show
  S_DC_SAVEFAIL,    // daycare: the breed did not save
  S_EVO_BOND_FMT,  // card: a friendship evolution waiting on the bond, "BOND 32/50"
  S_NAT_HARDY,     // nature (keep the 25 in NAT_ order)
  S_NAT_LONELY,    // nature (keep the 25 in NAT_ order)
  S_NAT_BRAVE,     // nature (keep the 25 in NAT_ order)
  S_NAT_ADAMANT,   // nature (keep the 25 in NAT_ order)
  S_NAT_NAUGHTY,   // nature (keep the 25 in NAT_ order)
  S_NAT_BOLD,      // nature (keep the 25 in NAT_ order)
  S_NAT_DOCILE,    // nature (keep the 25 in NAT_ order)
  S_NAT_RELAXED,   // nature (keep the 25 in NAT_ order)
  S_NAT_IMPISH,    // nature (keep the 25 in NAT_ order)
  S_NAT_LAX,       // nature (keep the 25 in NAT_ order)
  S_NAT_TIMID,     // nature (keep the 25 in NAT_ order)
  S_NAT_HASTY,     // nature (keep the 25 in NAT_ order)
  S_NAT_SERIOUS,   // nature (keep the 25 in NAT_ order)
  S_NAT_JOLLY,     // nature (keep the 25 in NAT_ order)
  S_NAT_NAIVE,     // nature (keep the 25 in NAT_ order)
  S_NAT_MODEST,    // nature (keep the 25 in NAT_ order)
  S_NAT_MILD,      // nature (keep the 25 in NAT_ order)
  S_NAT_QUIET,     // nature (keep the 25 in NAT_ order)
  S_NAT_BASHFUL,   // nature (keep the 25 in NAT_ order)
  S_NAT_RASH,      // nature (keep the 25 in NAT_ order)
  S_NAT_CALM,      // nature (keep the 25 in NAT_ order)
  S_NAT_GENTLE,    // nature (keep the 25 in NAT_ order)
  S_NAT_SASSY,     // nature (keep the 25 in NAT_ order)
  S_NAT_CAREFUL,   // nature (keep the 25 in NAT_ order)
  S_NAT_QUIRKY,    // nature (keep the 25 in NAT_ order)
  S_PTL_GLUTTON,   // what the trait loves
  S_PTL_SLEEPY,    // loves
  S_PTL_PLAYFUL,   // loves
  S_PTL_TIDY,      // loves
  S_PTL_CUDDLY,    // loves
  S_PTL_ENERGETIC, // loves
  S_PTL_PICKY,     // loves (keep the 7 in PL_ order)
  S_PT_LOVES_FMT,  // card: "<trait> - loves <thing>"
  S_CHAIN_FMT,     // explore: a live shiny chain, "CHAIN PIDGEY x5"
  S_TW_TITLE,      // tower: screen title
  S_TW_WHY,        // tower: why it is locked
  S_TW_FLOOR_FMT,  // tower: "FLOOR 3"
  S_TW_STREAK_FMT, // tower: "STREAK 2  BEST 9"
  S_TW_NEXTRW_FMT, // tower: next reward floor
  S_TW_RULES,      // tower: the rules
  S_TW_SMART,      // tower: expert AI from floor 7
  S_TW_START,      // tower: start a run
  S_TW_CONT,       // tower: continue a run
  S_TW_BTN,        // gym chooser: the tower button
  S_RM_PILL,       // ladder pill: rematch mode
  S_CH_TITLE,      // weekly challenge
  S_CH_WHY,        // weekly challenge
  S_CH_DONE,       // weekly challenge
  S_CH_R_TYPE,     // weekly challenge
  S_CH_R_LEVEL,    // weekly challenge
  S_CH_R_SOLO,     // weekly challenge
  S_CH_R_NOIT,     // weekly challenge
  S_CH_R_TRIO,     // weekly challenge
  S_CH_D_TYPE,     // weekly challenge
  S_CH_D_LEVEL,    // weekly challenge
  S_CH_D_SOLO,     // weekly challenge
  S_CH_D_NOIT,     // weekly challenge
  S_CH_D_TRIO,     // weekly challenge
  S_CH_REWARD,     // weekly challenge
  S_CH_RESET,      // weekly challenge
  S_CH_PAID,       // weekly challenge
  S_CH_NOFIT,      // weekly challenge
  S_CH_REFUSED,    // weekly challenge
  S_CH_NONE,       // weekly challenge
  S_CH_BTN,        // gym chooser: the weekly challenge button
  S_LAN_SHORT,     // gym chooser: the LAN button, short
  S_CH_START,      // weekly challenge: start
  S_RK_TITLE1,     // rank 1 title (keep the 10 in order)
  S_RK_TITLE2,     // rank 2 title (keep the 10 in order)
  S_RK_TITLE3,     // rank 3 title (keep the 10 in order)
  S_RK_TITLE4,     // rank 4 title (keep the 10 in order)
  S_RK_TITLE5,     // rank 5 title (keep the 10 in order)
  S_RK_TITLE6,     // rank 6 title (keep the 10 in order)
  S_RK_TITLE7,     // rank 7 title (keep the 10 in order)
  S_RK_TITLE8,     // rank 8 title (keep the 10 in order)
  S_RK_TITLE9,     // rank 9 title (keep the 10 in order)
  S_RK_TITLE10,    // rank 10 title (keep the 10 in order)
  S_RK_RANK_FMT,   // player card: "RANK 3 / 10"
  S_RK_PTS_FMT,    // player card: points and the next rank
  S_RK_UP,         // main screen: rank-up banner
  S_TRN_MISS,      // ported (vunguyen2712)
  S_TRN_COMBO,     // ported (vunguyen2712)
  S_TRN_GO,        // ported (vunguyen2712)
  S_TRN_PTS,       // ported (vunguyen2712)
  S_TRN_CRIT,      // ported (vunguyen2712)
  S_TRN_WEAK,      // ported (vunguyen2712)
  S_TRN_MAXED,     // ported (vunguyen2712)
  S_TRN_FEVER,     // ported (vunguyen2712)
  S_TRN_HBAG,      // ported (vunguyen2712)
  S_TRN_HSPD,      // ported (vunguyen2712)
  S_TRN_2BALL,     // ported (vunguyen2712)
  S_TRN_DEF,       // ported (vunguyen2712)
  S_TRN_WIND,      // ported (vunguyen2712)
  S_TRN_HBALL,     // ported (vunguyen2712)
  S_ARC_OUCH,      // ported (vunguyen2712)
  S_ARC_CLEAR,     // ported (vunguyen2712)
  S_ARC_ROUND,     // ported (vunguyen2712)
  S_ARC_TURN,      // ported (vunguyen2712)
  S_ARC_WATCH,     // ported (vunguyen2712)
  S_ARC_TITLE,     // ported (vunguyen2712)
  S_ARC_BEST,      // ported (vunguyen2712)
  S_ARC_NEW,       // ported (vunguyen2712)
  S_ARC_FIRST,     // ported (vunguyen2712)
  S_ARC_ALLS,      // ported (vunguyen2712)
  S_ARC_REWARD,    // ported (vunguyen2712)
  S_ARC_PRIZE,     // ported (vunguyen2712)
  S_ARC_TAPON,     // ported (vunguyen2712)
  S_ARC_TODAY_S,   // ported (vunguyen2712)
  S_ARC_TODAY_A,   // ported (vunguyen2712)
  S_ARC_PRIZE_DONE, // ported (vunguyen2712)
  S_ARC_PRIZE_READY, // ported (vunguyen2712)
  S_ARC_CATCH,     // ported (vunguyen2712)
  S_ARC_MEMORY,    // ported (vunguyen2712)
  S_ARC_ECHO,      // ported (vunguyen2712)
  S_ARC_POP,       // ported (vunguyen2712)
  S_ARC_HCATCH,    // ported (vunguyen2712)
  S_ARC_HMEMORY,   // ported (vunguyen2712)
  S_ARC_HECHO,     // ported (vunguyen2712)
  S_ARC_HPOP,      // ported (vunguyen2712)
  S_TK_FEED,       // task
  S_TK_FAV,        // task
  S_TK_CLEAN,      // task
  S_TK_PLAY,       // task
  S_TK_TRAIN,      // task
  S_TK_CAREDAY,    // task
  S_TK_HATCH,      // task
  S_TK_EVOLVE,     // task
  S_TK_GYM,        // task
  S_TK_REMATCH,    // task
  S_TK_SEEN,       // task
  S_TK_WILDWIN,    // task
  S_TK_CATCH,      // task (keep the 13 in TK_ order)
  S_TK_DAILY_T,    // tasks page title
  S_TK_WEEKLY_T,   // tasks page title
  S_TK_BONUS,      // tasks: daily bonus row
  S_TK_CLAIM,      // tasks: claimable
  S_TK_CLAIMED,    // tasks: paid
  S_TK_MENU,       // menu row
  S_TRD_BTN,       // LAN: trade
  S_TRD_YOU,       // trade: my side
  S_TRD_THEY,      // trade: their side
  S_TRD_PICK,      // trade: empty card
  S_TRD_NONE,      // trade: nothing offered
  S_TRD_ACCEPT,    // trade: accept
  S_TRD_WAIT,      // trade: waiting
  S_TRD_DONE,      // trade: finished
  S_TRD_NOROOM,    // trade: party and box full
  S_TRD_FAIL,      // trade: our store failed
  S_TRD_EVOLVED,   // trade: evolved on arrival
  S_FR_VISIT,      // ported (vunguyen2712)
  S_FR_BONUS,      // ported (vunguyen2712)
  S_FR_NOBONUS,    // ported (vunguyen2712)
  S_FR_WAITING,    // ported (vunguyen2712)
  S_FR_ARRIVED,    // ported (vunguyen2712)
  S_FR_RETURNED,   // ported (vunguyen2712)
  S_FR_SWAPPING,   // ported (vunguyen2712)
  S_FR_GOT,        // ported (vunguyen2712)
  S_FR_BAD,        // ported (vunguyen2712)
  S_FR_FULL,       // ported (vunguyen2712)
  S_FR_CALLED,     // ported (vunguyen2712)
  S_FR_USED,       // ported (vunguyen2712)
  S_FR_TRANSIT,    // ported (vunguyen2712)
  S_FR_SAMEID,     // ported (vunguyen2712)
  S_FR_OFFERS,     // ported (vunguyen2712)
  S_FR_PEER_USED,  // ported (vunguyen2712)
  S_FR_GIFT,       // ported (vunguyen2712)
  S_FR_SMALL,      // ported (vunguyen2712)
  S_FR_OFFER,      // ported (vunguyen2712)
  S_FR_TITLE,      // ported (vunguyen2712)
  S_FR_PICK,       // ported (vunguyen2712)
  S_FR_NORADIO,    // ported (vunguyen2712)
  S_FR_MODE,       // ported (vunguyen2712)
  S_FR_VERSION,    // ported (vunguyen2712)
  S_FR_GONE,       // ported (vunguyen2712)
  S_FR_LATER,      // ported (vunguyen2712)
  S_FR_WAIT,       // ported (vunguyen2712)
  S_FR_EGG,        // ported (vunguyen2712)
  S_CAS_HELLO,     // the Game Corner / LAN practice
  S_CAS_HEH,       // the Game Corner / LAN practice
  S_CAS_LUCKY,     // the Game Corner / LAN practice
  S_CAS_SECRET,    // the Game Corner / LAN practice
  S_CAS_COINS_FMT, // the Game Corner / LAN practice
  S_CAS_BET_FMT,   // the Game Corner / LAN practice
  S_CAS_TITLE,     // the Game Corner / LAN practice
  S_CAS_SLOTS,     // the Game Corner / LAN practice
  S_CAS_HL,        // the Game Corner / LAN practice
  S_CAS_BJ,        // the Game Corner / LAN practice
  S_CAS_BUY,       // the Game Corner / LAN practice
  S_CAS_PRIZES,    // the Game Corner / LAN practice
  S_CAS_WIN_FMT,   // the Game Corner / LAN practice
  S_CAS_STOP,      // the Game Corner / LAN practice
  S_CAS_SPIN,      // the Game Corner / LAN practice
  S_CAS_DEAL,      // the Game Corner / LAN practice
  S_CAS_POT_FMT,   // the Game Corner / LAN practice
  S_CAS_LOSE,      // the Game Corner / LAN practice
  S_CAS_HIGHER,    // the Game Corner / LAN practice
  S_CAS_LOWER,     // the Game Corner / LAN practice
  S_CAS_CASHOUT,   // the Game Corner / LAN practice
  S_CAS_HIT,       // the Game Corner / LAN practice
  S_CAS_STAND,     // the Game Corner / LAN practice
  S_CAS_DOUBLE,    // the Game Corner / LAN practice
  S_CAS_BJWIN,     // the Game Corner / LAN practice
  S_CAS_PUSH,      // the Game Corner / LAN practice
  S_CAS_BUST,      // the Game Corner / LAN practice
  S_CAS_WALLET_FMT, // the Game Corner / LAN practice
  S_CAS_PACK_FMT,  // the Game Corner / LAN practice
  S_CAS_BACKS,     // the Game Corner / LAN practice
  S_CAS_AVATARS,   // the Game Corner / LAN practice
  S_CAS_CARDS,     // the Game Corner / LAN practice
  S_CAS_EQUIPPED,  // the Game Corner / LAN practice
  S_CAS_OWNED,     // the Game Corner / LAN practice
  S_CAS_AGAIN,     // the Game Corner / LAN practice
  S_LAN_PRACTICE,  // the Game Corner / LAN practice
  S_LAN_PRACTICE_HINT, // the Game Corner / LAN practice
  S_LAN_NO_RIVALS, // the Game Corner / LAN practice
  S_PC_CARDS,      // trainer cards
  S_PC_COLLECTED_FMT, // trainer cards
  S_PC_EXCHANGE,   // trainer cards
  S_PC_BOOK,       // trainer cards
  S_PC_HINT,       // trainer cards
  S_PC_NONE,       // trainer cards
  S_PC_BADGES_FMT, // trainer cards
  S_PC_DEX_FMT,    // trainer cards
  S_PC_MEDALS_FMT, // trainer cards
  S_PC_STREAK_FMT, // trainer cards
  S_PC_SWAPPED,    // trainer cards
  S_CAS_ROULETTE,  // the Game Corner
  S_CAS_RED,       // the Game Corner
  S_CAS_BLACK,     // the Game Corner
  S_CAS_ODD,       // the Game Corner
  S_CAS_EVEN,      // the Game Corner
  S_CAS_ROCKET,    // the Game Corner
  S_CAS_GREEN,     // the Game Corner
  S_BF_ALL,        // box filters and favourites
  S_BF_FAV,        // box filters and favourites
  S_BF_SHINY,      // box filters and favourites
  S_BF_HELD,       // box filters and favourites
  S_BF_S_LEVEL,    // box filters and favourites
  S_BF_S_IV,       // box filters and favourites
  S_BF_S_ATK,      // box filters and favourites
  S_BF_S_DEF,      // box filters and favourites
  S_BF_S_SPE,      // box filters and favourites
  S_BF_S_HP,       // box filters and favourites
  S_BF_S_DEX,      // box filters and favourites
  S_BF_S_NAME,     // box filters and favourites
  S_BF_S_SLOT,     // box filters and favourites
  S_BF_FILTER,     // box filters and favourites
  S_BF_FAVLOCK,    // box filters and favourites
  S_BF_FOUND_FMT,  // box filters and favourites
  S_BF_SORT_FMT,   // box filters and favourites
  S_BRI_FMT,       // max brightness level, e.g. "LIGHT 7"
  S_MS_BTN,        // the STATS page (ui_monstats.ino)
  S_MS_SPA,        // the STATS page (ui_monstats.ino)
  S_MS_SPD,        // the STATS page (ui_monstats.ino)
  S_MS_IVTOT,        // the STATS page (ui_monstats.ino)
  S_MS_HELD,        // the STATS page (ui_monstats.ino)
  S_MS_NATURE,        // the STATS page (ui_monstats.ino)
  S_MS_BOND,        // the STATS page (ui_monstats.ino)
  S_MS_TR,        // the STATS page (ui_monstats.ino)
  S_ARC_PINBALL,   // the arcade: PINBALL
  S_PB_HOW,        // the arcade: PINBALL
  S_PB_SCORE,      // the arcade: PINBALL
  S_ARC_SURF,      // the arcade: SURF / PINBALL
  S_ARC_HSURF,     // the arcade: SURF / PINBALL
  S_PB_BALL_FMT,   // the arcade: SURF / PINBALL
  S_PB_BANK,       // the arcade: PINBALL
  S_PB_ORBIT,      // the arcade: PINBALL
  S_PB_CATCH,      // the arcade: PINBALL
  S_BOX_MOVE,      // the PC box: move, overview, search
  S_BOX_MOVE_FMT,  // the PC box: move, overview, search
  S_BOX_RENAME_FMT, // the PC box: move, overview, search
  S_BF_SEARCH,     // the PC box: move, overview, search
  S_MS_SPNOTE,     // the STATS page: special stats share training
  S_BN_PROTECTED,
  S_BN_BRACED,
  S_BN_ENDURED,
  S_BN_FLINCHED,
  S_BN_FAILED,
  S_BN_RAIN,
  S_BN_SUN,
  S_BN_SAND,
  S_BN_HAIL,
  S_BN_WEATHER_END,
  S_BN_SAND_HURT,
  S_BN_HAIL_HURT,
  S_BN_ROCKS,
  S_BN_SPIKES,
  S_BN_TSPIKES,
  S_BN_WEB,
  S_BN_ROCKS_HURT,
  S_BN_SPIKES_HURT,
  S_BN_WEB_SLOW,
  S_BN_HAZARDS_GONE,
  S_BN_REFLECT,
  S_BN_LSCREEN,
  S_BN_VEIL,
  S_BN_SCREEN_BROKE,
  S_BN_SCREEN_END,
  S_BN_SUB_UP,
  S_BN_SUB_HIT,
  S_BN_SUB_GONE,
  S_BN_REST,
  S_BN_SEEDED,
  S_BN_SEED_DRAIN,
  S_BN_BOUND,
  S_BN_BIND_HURT,
  S_BN_TRAPPED,
  S_BN_DROWSY,
  S_BN_TAUNTED,
  S_BN_TAUNT_BLOCK,
  S_BN_ENCORED,
  S_BN_CURSED,
  S_BN_CURSE_HURT,
  S_BN_DRUM,
  S_BN_STAGES_RESET,
  S_BN_PSYCHUP,
  S_BN_PAINSPLIT,
  S_BN_TAILWIND,
  S_BN_TRICKROOM,
  S_BN_DESTINY,
  S_BN_DESTINY_TOOK,
  S_BN_PERISH,
  S_BN_AQUARING,
  S_BN_RING_HEAL,
  S_BN_WISH,
  S_BN_WISH_HEAL,
  S_BN_SAFEGUARD,
  S_BN_MIST,
  S_BN_PUMPED,
  S_BN_NOTHING,
  S_BN_CHARGED,
  S_BN_TERRAIN,
  S_BN_KNOCKED,
  S_BN_OHKO,
  S_BN_FUTURE,
  S_BN_FUTURE_HIT,
  S_BN_TIRED,
  S_BN_TOXIC,
  S_BN_FOCUS_LOST,
  S_BN_SELF_FAINT,
  S_BN_INVERTED,
  S_BN_ASLEEP,
  S_BN_WOKE,
  S_BN_CURED,
  S_BN_STAGE_UP,
  S_BN_TRAPPED_NOSWITCH,
  S_BN_BLOWN_AWAY,
  S_BN_DRAGGED,
  S_BN_WENT_BACK,
  S_DEX_SWIPE,
  S_DEX_NOENTRY,
  S_TW_COSTUME,     // tower floor 100: "NEW COSTUME!"
  S_CAS_MONS,       // the prize counter's creature tab
  S_CAS_TO_BOX,     // a prize creature delivered to the PC box
  S_CAS_BOX_FULL,   // ... refused: no room in the box
  S_CAS_ARMORED,    // the armored skin's tag on its prize tile
  S_AV_PICK_FMT,    // the avatar picker's title: "AVATARS %u" unlocked
  S_AV_LEGENDS_FMT, // ... and the tower's legendary costumes owned, once any are
  S_HELP,           // the menu row and the card's title: "HOW TO PLAY"
  S_HELP_NEXT,      // the how-to-play card's button, before the last page
  S_HELP_DONE,      // ... and on the last page: "GOT IT"
  S_HELP_T1, S_HELP_T2, S_HELP_T3, S_HELP_T4,   // page titles, in order
  S_HELP_B1, S_HELP_B2, S_HELP_B3, S_HELP_B4,   // page bodies: lines split on '\n'
  S_BACK_BTN,      // the held back button's label
  S_BACK_HOLD,     // shown after a short tap on it
  S_CAS_WELCOME_FMT, // the dealer's one-time gift: lines on \n, %u coins
  S_CAS_WELCOME_TAP, // under it
  S_STAR_HINT,     // trainer card: under the stars, until one is tapped
  S_STAR_D0,       // red star (stars.h order)
  S_STAR_D1,       // blue star
  S_STAR_D2,       // silver star
  S_STAR_D3,       // gold star
  S_STAR_D4,       // purple star
  S_STAR_D5,       // black star
  S_WARDROBE,      // the trainer card's button and the screen's title
  S_WD_SKIN,       // wardrobe row
  S_WD_EYES,       // wardrobe row
  S_WD_HAIR,       // wardrobe row
  S_WD_HAIRCOL,    // wardrobe row
  S_WD_HAT,        // wardrobe row
  S_WD_TOP,        // wardrobe row
  S_WD_LEGS,       // wardrobe row
  S_WD_SHOES,      // wardrobe row
  S_WD_FACE,       // wardrobe row
  S_WD_BACK,       // wardrobe row
  S_WD_OWNED_FMT,  // unlocked cosmetics / all
  S_ARC_COS_FMT,   // arcade results: a cosmetic won for an S
  S_ARC_COS_WAIT_FMT, // arcade results: S, but the cosmetic is on cooldown
  S_EGG_WALK_FMT,  // the egg's line while it hatches by walking: steps, of
  S_CAS_GIFT_FMT,  // the dealer's hourly gift, in his bubble
  S_CAS_GIFT_WAIT_FMT, // ... and when he will give again
  S_WD_GLOVES,     // wardrobe row
  STR_COUNT
};

const char *T(StrId id);       // texto en el idioma activo
const char *moveName(uint16_t move);  // localized display name; move ID stays stable
const char *medalName(int i);  // banner de medalla (MED_COUNT)
const char *medalLabel(int i); // etiqueta corta de medalla
const char *medalDesc(int i);  // descripcion larga de medalla

void loadLang();             // lee el idioma de NVS (llamar en setup)
void setLang(Lang l);        // cambia y persiste el idioma
