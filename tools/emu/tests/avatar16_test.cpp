// The avatar index is 16-bit (v3.45). Every place it is stored or sent keeps
// its old byte meaning the same thing and carries the full value APPENDED, the
// same rule as the 16-bit move IDs:
//
//   the player checkpoint   byte 17 = downgrade-safe low byte, v4 tail = u16
//   the legacy keys         "avtr" = that low byte, "avt2" = the u16
//   a trainer card (wire)   byte 12 = low byte, bytes 30-31 = u16 (32 long)
//   the card book           TrainerCard::avatarHi appended after fid
//   casino ownership        a third bit block, 256..1023, after the second
//
// AVATAR_COUNT is 255 today, so a stored value above it is clamped to 0 on
// load and an index >= 256 cannot round-trip through the game yet. The checks
// below therefore pin the BYTES on the way out, and on the way in forge a
// record whose low byte DISAGREES with its wide field, so that only reading
// the wide field gives the right answer.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "card.h"
#include "casino.h"
#include "avatar_names.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=91; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static uint16_t rd16(const uint8_t *b){ return (uint16_t)(b[0] | (b[1] << 8)); }
static uint32_t rd32(const uint8_t *b){ return b[0] | (b[1]<<8) | (b[2]<<16) | ((uint32_t)b[3]<<24); }
static void wr16(uint8_t *b, uint16_t v){ b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); }
// CRC-16/CCITT-FALSE, the checkpoint's published algorithm ("123456789" -> 0x29B1).
static uint16_t crc16(const uint8_t *d, size_t n){
  uint16_t c=0xFFFF;
  for(size_t i=0;i<n;i++){ c^=(uint16_t)d[i]<<8; for(int k=0;k<8;k++) c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1); }
  return c;
}
static void reseal(std::vector<uint8_t> &b){ wr16(&b[6], (uint16_t)b.size()); wr16(&b[b.size()-2], crc16(b.data(), b.size()-2)); }
// The newer of plyA/plyB, by generation.
static const char *newestPly(){
  NvsStore &s = nvs();
  bool a = s.count("plyA"), b = s.count("plyB");
  if (a && b) return (int32_t)(rd32(&s["plyA"][8]) - rd32(&s["plyB"][8])) > 0 ? "plyA" : "plyB";
  return a ? "plyA" : "plyB";
}
// Where the v4 avatar sits: after the variable arrays (sized by the record's
// own header) and the v2/v3 tail (wallet, steps, coins).
static size_t avatarTailAt(const std::vector<uint8_t> &b){
  size_t need = 42 + 2 * rd16(&b[12]) + 4 * b[15] + 2 * b[14] + b[16];
  return need + 10;
}

int main(){
  static_assert(sizeof(Pet::avatar) == 2, "the live avatar index is 16-bit");
  static_assert(AVATAR_COUNT > 200, "the forged values below must be real avatars");
  const uint16_t A = 200, LOW = 5;   // a shop avatar, and a decoy low byte

  Pet pet; pet.begin(); party.begin();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // ---- writing
  pet.avatar = A; ck(pet.saveNow(), "a save with avatar 200 lands");
  {
    auto &b = nvs()[newestPly()];
    ck(rd16(&b[4]) >= 4 && b[17] == A && b.size() >= avatarTailAt(b) + 2 + 2 && rd16(&b[avatarTailAt(b)]) == A,
       "the checkpoint carries 200 in byte 17 AND in its v4 tail");
    ck(!nvs().count("avtr") && !nvs().count("avt2"), "and no legacy key is written any more (v3.50)");
  }
  pet.avatar = 300; pet.saveNow();   // past 255: not a real avatar yet, so only the bytes are checked
  {
    auto &b = nvs()[newestPly()];
    ck(b[17] == 0 && rd16(&b[avatarTailAt(b)]) == 300,
       "an index past 255 leaves 0 in byte 17 (what a downgrade sees) and the whole value in the tail");
    ck(!nvs().count("avtr") && !nvs().count("avt2"), "still no legacy key");
  }
  pet.avatar = A; pet.saveNow();

  // ---- reading the checkpoint: forge byte 17 to disagree with the tail
  {
    std::vector<uint8_t> b = nvs()[newestPly()];
    b[17] = LOW; reseal(b);
    nvs()["plyA"] = b; nvs()["plyB"] = b;
    nvs()["avt2"] = { 9, 0 }; nvs()["avtr"] = { 9 };   // stale legacy keys saying something else again
    Pet p; p.begin();
    ck(p.avatar == A, "the checkpoint's v4 tail is what loads, not byte 17 or the legacy keys");

    std::vector<uint8_t> v3(b.begin(), b.begin() + avatarTailAt(b));   // a v3 record ends before it
    v3.push_back(0); v3.push_back(0); reseal(v3);
    nvs()["plyA"] = v3; nvs()["plyB"] = v3;
    Pet q; q.begin();
    ck(q.avatar == LOW, "a v3 record (no tail) still loads its byte 17");
  }

  // ---- reading the legacy keys, with no checkpoint to believe
  {
    nvs().erase("plyA"); nvs().erase("plyB");
    nvs()["avt2"] = { (uint8_t)A, (uint8_t)(A >> 8) }; nvs()["avtr"] = { LOW };   // a pre-checkpoint save
    Pet p; p.begin();
    ck(p.avatar == A, "with no checkpoint, avt2 wins over avtr");
    nvs().erase("plyA"); nvs().erase("plyB"); nvs().erase("avt2");
    Pet q; q.begin();
    ck(q.avatar == LOW, "and a save from before avt2 existed reads avtr");
    nvs().erase("plyA"); nvs().erase("plyB");
    nvs()["avt2"] = { (uint8_t)AVATAR_COUNT, (uint8_t)(AVATAR_COUNT >> 8) };
    Pet r; r.begin();
    ck(r.avatar == 0, "an index past the art is clamped to 0, never drawn");
  }

  // ---- trainer cards on the wire
  {
    TrainerCard c; c.setAvatar(300); snprintf(c.name, sizeof(c.name), "ALI");
    uint8_t w[CARD_WIRE_LEN];
    ck(cardToWire(c, w) == 32 && w[12] == 0 && rd16(w + 30) == 300,
       "a card sends 32 bytes: 0 at byte 12 for an old reader, the full index at 30");
    c.setAvatar(A); cardToWire(c, w); w[12] = LOW;
    TrainerCard got;
    ck(cardFromWire(w, 32, got) && got.avatar() == A, "a new card's avatar comes from bytes 30-31");
    ck(cardFromWire(w, 30, got) && got.avatar() == LOW, "a 30-byte card from an older board still reads, from byte 12");
    ck(!cardFromWire(w, 29, got), "and anything shorter is not a card");
    wr16(w + 30, AVATAR_COUNT);
    ck(cardFromWire(w, 32, got) && got.avatar() == 0, "an avatar past the art off the wire is clamped");
  }

  // ---- the card book: a record written before avatarHi existed
  {
    nvs().erase("cards");
    TrainerCard old; snprintf(old.name, sizeof(old.name), "OLD"); old.setAvatar(LOW); old.fid = 77;
    const uint16_t oldStride = offsetof(TrainerCard, avatarHi);   // what v3.44 stored
    std::vector<uint8_t> blob = { 1, 1, (uint8_t)oldStride, (uint8_t)(oldStride >> 8) };
    blob.insert(blob.end(), (const uint8_t *)&old, (const uint8_t *)&old + oldStride);
    nvs()["cards"] = blob;
    cardBookBegin();
    ck(cardBookCount() == 1 && cardBookAt(0).avatar() == LOW && cardBookAt(0).fid == 77,
       "an old card book loads with its avatar intact");
    TrainerCard nu; snprintf(nu.name, sizeof(nu.name), "NEW"); nu.setAvatar(A); nu.fid = 78;
    cardBookAdd(nu, 1);
    cardBookBegin();
    ck(cardBookCount() == 2 && cardBookAt(0).avatar() == A && cardBookAt(1).avatar() == LOW,
       "a 16-bit avatar survives the book's save and reload, beside the old one");
  }

  // ---- casino ownership: a v3.44 record (56 bytes, two avatar blocks)
  {
    nvs().erase("casino");
    casinoBegin();
    casino.avatarsHi[(A - 128) / 8] |= (uint8_t)(1u << (A % 8));
    casino.avatarsX[3] = 0x5A;    // nothing sells this high yet: the bytes must still persist
    ck(casinoSave(), "the casino record saves");
    ck(nvs()["casino"].size() == 157 && nvs()["casino"][1] == 157, "at 157 bytes (v3.55 added the welcome flag and the hourly gift), all three blocks");
    casinoBegin();
    ck(casinoOwns(PK_AVATAR, A) && casino.avatarsX[3] == 0x5A, "and every block comes back");
    auto &v = nvs()["casino"]; v.resize(56); v[1] = 56;
    casinoBegin();
    bool xEmpty = true; for (auto b : casino.avatarsX) if (b) xEmpty = false;
    ck(casinoOwns(PK_AVATAR, A) && xEmpty, "a v3.44 record keeps its 128-255 block and owns nothing past it");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
