#pragma once
#include <stdint.h>

// The Game Corner's Coin Case ceiling. Its own header because Pet (which holds
// the coins) and casino.cpp (whose games pay up to it) both need the one
// number, and neither should include the other for it.
//
// 10,000,000 since v3.47 (it was 9,999, the real Coin Case's). Pet::coins is a
// uint32_t; the saves keep their old 16-bit field holding a downgrade-safe
// value (min(coins, 65535) -- an older build clamps that to its own 9,999)
// and carry the full count APPENDED, the same rule as the 16-bit move IDs.
#define COIN_CAP 10000000UL
