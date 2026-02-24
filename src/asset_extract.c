#include "asset_extract.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================================================================
//  SHA-1 (for ROM identification)
// ================================================================

static void sha1(const uint8 *data, size_t len, uint8 hash[20]) {
  uint32 h0 = 0x67452301, h1 = 0xEFCDAB89;
  uint32 h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

  size_t padded_len = ((len + 8) / 64 + 1) * 64;
  uint8 *padded = (uint8 *)calloc(1, padded_len);
  memcpy(padded, data, len);
  padded[len] = 0x80;
  uint64 bits = (uint64)len * 8;
  for (int i = 0; i < 8; i++)
    padded[padded_len - 1 - i] = (uint8)((bits >> (i * 8)) & 0xFF);

  for (size_t off = 0; off < padded_len; off += 64) {
    uint32 w[80];
    for (int i = 0; i < 16; i++)
      w[i] = ((uint32)padded[off+i*4]<<24) | ((uint32)padded[off+i*4+1]<<16) |
             ((uint32)padded[off+i*4+2]<<8) | padded[off+i*4+3];
    for (int i = 16; i < 80; i++) {
      uint32 t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
      w[i] = (t << 1) | (t >> 31);
    }
    uint32 a=h0, b=h1, c=h2, d=h3, e=h4;
    for (int i = 0; i < 80; i++) {
      uint32 f, k;
      if      (i < 20) { f = (b&c)|((~b)&d);       k = 0x5A827999; }
      else if (i < 40) { f = b^c^d;                 k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b&c)|(b&d)|(c&d);     k = 0x8F1BBCDC; }
      else              { f = b^c^d;                 k = 0xCA62C1D6; }
      uint32 tmp = ((a<<5)|(a>>27)) + f + e + k + w[i];
      e=d; d=c; c=(b<<30)|(b>>2); b=a; a=tmp;
    }
    h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
  }
  free(padded);

  uint32 hh[5] = {h0, h1, h2, h3, h4};
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 4; j++)
      hash[i*4+j] = (uint8)((hh[i] >> (24-j*8)) & 0xFF);
}

// ================================================================
//  SHA-256 (for asset file signature)
// ================================================================

static const uint32 kSha256K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

static uint32 sha256_rotr(uint32 x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256(const uint8 *data, size_t len, uint8 hash[32]) {
  uint32 h[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  };

  size_t padded_len = ((len + 8) / 64 + 1) * 64;
  uint8 *padded = (uint8 *)calloc(1, padded_len);
  memcpy(padded, data, len);
  padded[len] = 0x80;
  uint64 bits = (uint64)len * 8;
  for (int i = 0; i < 8; i++)
    padded[padded_len - 1 - i] = (uint8)((bits >> (i * 8)) & 0xFF);

  for (size_t off = 0; off < padded_len; off += 64) {
    uint32 w[64];
    for (int i = 0; i < 16; i++)
      w[i] = ((uint32)padded[off+i*4]<<24) | ((uint32)padded[off+i*4+1]<<16) |
             ((uint32)padded[off+i*4+2]<<8) | padded[off+i*4+3];
    for (int i = 16; i < 64; i++) {
      uint32 s0 = sha256_rotr(w[i-15],7) ^ sha256_rotr(w[i-15],18) ^ (w[i-15]>>3);
      uint32 s1 = sha256_rotr(w[i-2],17) ^ sha256_rotr(w[i-2],19) ^ (w[i-2]>>10);
      w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32 a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh2=h[7];
    for (int i = 0; i < 64; i++) {
      uint32 S1 = sha256_rotr(e,6) ^ sha256_rotr(e,11) ^ sha256_rotr(e,25);
      uint32 ch = (e&f)^((~e)&g);
      uint32 tmp1 = hh2 + S1 + ch + kSha256K[i] + w[i];
      uint32 S0 = sha256_rotr(a,2) ^ sha256_rotr(a,13) ^ sha256_rotr(a,22);
      uint32 maj = (a&b)^(a&c)^(b&c);
      uint32 tmp2 = S0 + maj;
      hh2=g; g=f; f=e; e=d+tmp1; d=c; c=b; b=a; a=tmp1+tmp2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh2;
  }
  free(padded);
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 4; j++)
      hash[i*4+j] = (uint8)((h[i] >> (24-j*8)) & 0xFF);
}

// ================================================================
//  ROM identification
// ================================================================

typedef struct {
  const char *hash_hex;
  const char *code;
  const char *name;
} RomEntry;

static const RomEntry kKnownRoms[] = {
  {"6D4F10A8B10E10DBE624CB23CF03B88BB8252973", "us",
   "Legend of Zelda, The - A Link to the Past (USA)"},
  {"2E62494967FB0AFDF5DA1635607F9641DF7C6559", "de",
   "Legend of Zelda, The - A Link to the Past (Germany)"},
  {"229364A1B92A05167CD38609B1AA98F7041987CC", "fr",
   "Legend of Zelda, The - A Link to the Past (France)"},
  {"C1C6C7F76FFF936C534FF11F87A54162FC0AA100", "fr-c",
   "Legend of Zelda, The - A Link to the Past (Canada)"},
  {"7C073A222569B9B8E8CA5FCB5DFEC3B5E31DA895", "en",
   "Legend of Zelda, The - A Link to the Past (Europe)"},
  {"461FCBD700D1332009C0E85A7A136E2A8E4B111E", "es",
   "Spanish"},
  {"3C4D605EEFDA1D76F101965138F238476655B11D", "pl",
   "Polish"},
  {"D0D09ED41F9C373FE6AFDCCAFBF0DA8C88D3D90D", "pt",
   "Portuguese"},
  {"B2A07A59E64C498BC1B2F28728F9BF4014C8D582", "redux",
   "English Redux"},
  {"9325C22EB0A2A1F0017157C8B620BC3A605CEDE1", "redux",
   "English Redux (alt)"},
  {"FA8ADFDBA2697C9A54D583A1284A22AC764C7637", "nl",
   "Dutch"},
  {"43CD3438469B2C3FE879EA2F410B3EF3CB3F1CA4", "sv",
   "Swedish"},
  {NULL, NULL, NULL}
};

static char g_error[256];

static int hex_to_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Strip SMC header if present, return pointer into rom (not a new alloc)
static const uint8 *StripSMCHeader(const uint8 *rom, size_t *size) {
  if ((*size & 0xFFFFF) == 0x200) {
    rom += 0x200;
    *size -= 0x200;
  }
  return rom;
}

static const RomEntry *FindRomByHash(const uint8 *rom, size_t rom_size) {
  size_t sz = rom_size;
  const uint8 *data = StripSMCHeader(rom, &sz);

  uint8 hash[20];
  sha1(data, sz, hash);

  for (int r = 0; kKnownRoms[r].hash_hex; r++) {
    const char *hex = kKnownRoms[r].hash_hex;
    bool match = true;
    for (int i = 0; i < 20 && match; i++) {
      int hi = hex_to_nibble(hex[i*2]);
      int lo = hex_to_nibble(hex[i*2+1]);
      if (hi < 0 || lo < 0 || hash[i] != (uint8)((hi << 4) | lo))
        match = false;
    }
    if (match) return &kKnownRoms[r];
  }
  return NULL;
}

const char *AssetExtract_IdentifyROM(const uint8 *rom, size_t rom_size) {
  const RomEntry *e = FindRomByHash(rom, rom_size);
  return e ? e->code : NULL;
}

const char *AssetExtract_GetROMName(const uint8 *rom, size_t rom_size) {
  const RomEntry *e = FindRomByHash(rom, rom_size);
  return e ? e->name : NULL;
}

const char *AssetExtract_GetError(void) {
  return g_error[0] ? g_error : NULL;
}

// ================================================================
//  ROM access helpers (LoROM addressing with bank crossing)
// ================================================================

static const uint8 *g_rom;
static size_t g_rom_size;

static uint8 rom_get_byte(uint32 addr) {
  uint32 file_off = ((addr >> 16) & 0x7F) * 0x8000 + (addr & 0x7FFF);
  if (file_off >= g_rom_size) return 0;
  return g_rom[file_off];
}

static uint16 rom_get_word(uint32 addr) {
  return rom_get_byte(addr) | ((uint16)rom_get_byte(addr + 1) << 8);
}

static uint32 rom_get_24(uint32 addr) {
  return rom_get_byte(addr) | ((uint32)rom_get_byte(addr + 1) << 8) |
         ((uint32)rom_get_byte(addr + 2) << 16);
}

// Advance addr by 1 with bank crossing
static uint32 rom_advance(uint32 addr) {
  addr++;
  if ((addr & 0x8000) == 0)
    addr += 0x8000;
  return addr;
}

// Read n bytes from ROM with bank crossing
static void rom_get_bytes(uint32 addr, uint8 *dst, size_t n) {
  for (size_t i = 0; i < n; i++) {
    dst[i] = rom_get_byte(addr);
    addr = rom_advance(addr);
  }
}

// Read n words (little-endian) from ROM with bank crossing
static void rom_get_words(uint32 addr, uint16 *dst, size_t n) {
  for (size_t i = 0; i < n; i++) {
    dst[i] = rom_get_byte(addr) | ((uint16)rom_get_byte(addr + 1) << 8);
    addr += 2;
    if ((addr & 0x8000) == 0)
      addr += 0x8000;
  }
}

static int8 rom_get_int8(uint32 addr) {
  uint8 b = rom_get_byte(addr);
  return (int8)b;
}

// ================================================================
//  LZ decompression (for determining compressed data length)
// ================================================================

// Decompresses and returns both the decompressed data and the compressed length.
// The caller must free() the returned pointer.
static uint8 *lz_decomp(uint32 ea, bool offset_is_be, size_t *decomp_len, size_t *comp_len) {
  size_t result_cap = 0x10000;
  size_t result_len = 0;
  uint8 *result = (uint8 *)malloc(result_cap);
  uint32 start_ea = ea;

  for (;;) {
    uint8 b = rom_get_byte(ea);
    ea = rom_advance(ea);
    if (b == 0xFF) break;

    int cmd, lx;
    if ((b & 0xE0) != 0xE0) {
      cmd = b & 0xE0;
      lx = (b & 0x1F) + 1;
    } else {
      cmd = (b << 3) & 0xE0;
      uint8 b2 = rom_get_byte(ea);
      ea = rom_advance(ea);
      lx = ((b & 3) << 8 | b2) + 1;
    }

    // Ensure capacity
    if (result_len + (size_t)lx > result_cap) {
      while (result_len + (size_t)lx > result_cap)
        result_cap *= 2;
      result = (uint8 *)realloc(result, result_cap);
    }

    if (cmd == 0x00) { // literal
      for (int i = 0; i < lx; i++) {
        result[result_len++] = rom_get_byte(ea);
        ea = rom_advance(ea);
      }
    } else if (cmd & 0x80) { // copy (0x80, 0xA0, 0xC0)
      uint16 offs = (uint16)rom_get_byte(ea) << 8;
      ea = rom_advance(ea);
      offs |= rom_get_byte(ea);
      ea = rom_advance(ea);
      if (!offset_is_be)
        offs = (uint16)((offs >> 8) | (offs << 8));
      for (int i = 0; i < lx; i++) {
        result[result_len] = result[offs];
        result_len++;
        offs++;
      }
    } else if ((cmd & 0x40) == 0) { // memset (0x20)
      uint8 val = rom_get_byte(ea);
      ea = rom_advance(ea);
      for (int i = 0; i < lx; i++)
        result[result_len++] = val;
    } else if ((cmd & 0x20) == 0) { // memset16 (0x40)
      uint8 b1 = rom_get_byte(ea);
      ea = rom_advance(ea);
      uint8 b2 = rom_get_byte(ea);
      ea = rom_advance(ea);
      uint8 args[2] = {b1, b2};
      for (int i = 0; i < lx; i++) {
        result[result_len++] = args[i & 1];
      }
    } else { // increment (0x60)
      uint8 val = rom_get_byte(ea);
      ea = rom_advance(ea);
      for (int i = 0; i < lx; i++) {
        result[result_len++] = val;
        val = (val + 1) & 0xFF;
      }
    }
  }

  if (decomp_len) *decomp_len = result_len;
  if (comp_len) *comp_len = (ea - start_ea) & 0x7FFF;
  return result;
}

// ================================================================
//  Dynamic buffer and asset builder
// ================================================================

typedef struct {
  uint8 *data;
  size_t size;
  size_t cap;
} DynBuf;

static void db_init(DynBuf *b) {
  b->data = NULL;
  b->size = 0;
  b->cap = 0;
}

static void db_ensure(DynBuf *b, size_t extra) {
  if (b->size + extra > b->cap) {
    size_t new_cap = b->cap ? b->cap * 2 : 4096;
    while (new_cap < b->size + extra)
      new_cap *= 2;
    b->data = (uint8 *)realloc(b->data, new_cap);
    b->cap = new_cap;
  }
}

static void db_append(DynBuf *b, const void *data, size_t len) {
  db_ensure(b, len);
  memcpy(b->data + b->size, data, len);
  b->size += len;
}

static void db_append_byte(DynBuf *b, uint8 v) {
  db_ensure(b, 1);
  b->data[b->size++] = v;
}

static void db_append_u16le(DynBuf *b, uint16 v) {
  uint8 buf[2] = { (uint8)(v & 0xFF), (uint8)(v >> 8) };
  db_append(b, buf, 2);
}

static void db_append_u32le(DynBuf *b, uint32 v) {
  uint8 buf[4] = { (uint8)(v & 0xFF), (uint8)((v >> 8) & 0xFF),
                    (uint8)((v >> 16) & 0xFF), (uint8)((v >> 24) & 0xFF) };
  db_append(b, buf, 4);
}

static void db_free(DynBuf *b) {
  free(b->data);
  b->data = NULL;
  b->size = b->cap = 0;
}

// ================================================================
//  pack_arrays implementation
// ================================================================

// Pack an array of (data, size) pairs into the format used by FindIndexInMemblk
static void pack_arrays(DynBuf *out, const uint8 **items, const size_t *item_sizes, size_t count) {
  if (count == 0) return;

  // Compute cumulative offsets
  size_t total_data = 0;
  for (size_t i = 0; i < count; i++)
    total_data += item_sizes[i];

  bool use_16bit = (total_data < 65536 && count <= 8192);

  // Write offset table (count-1 entries)
  size_t offs = 0;
  for (size_t i = 0; i < count - 1; i++) {
    offs += item_sizes[i];
    if (use_16bit)
      db_append_u16le(out, (uint16)offs);
    else
      db_append_u32le(out, (uint32)offs);
  }

  // Write concatenated data
  for (size_t i = 0; i < count; i++)
    if (item_sizes[i] > 0)
      db_append(out, items[i], item_sizes[i]);

  // Write trailer count
  if (use_16bit)
    db_append_u16le(out, (uint16)(count - 1));
  else
    db_append_u16le(out, (uint16)(8192 + count - 1));
}

// ================================================================
//  Asset accumulator
// ================================================================

#define MAX_ASSETS 200

typedef struct {
  char *names[MAX_ASSETS];
  uint8 *data[MAX_ASSETS];
  size_t sizes[MAX_ASSETS];
  int count;
} AssetList;

static void assets_init(AssetList *a) {
  memset(a, 0, sizeof(*a));
}

static void assets_add(AssetList *a, const char *name, const uint8 *data, size_t size) {
  int i = a->count++;
  a->names[i] = strdup(name);
  a->data[i] = (uint8 *)malloc(size);
  memcpy(a->data[i], data, size);
  a->sizes[i] = size;
}

static void assets_add_move(AssetList *a, const char *name, uint8 *data, size_t size) {
  int i = a->count++;
  a->names[i] = strdup(name);
  a->data[i] = data;
  a->sizes[i] = size;
}

static void assets_free(AssetList *a) {
  for (int i = 0; i < a->count; i++) {
    free(a->names[i]);
    free(a->data[i]);
  }
  a->count = 0;
}

// Convenience: add uint16 array (writes as little-endian bytes)
static void assets_add_words(AssetList *a, const char *name, const uint16 *words, size_t count) {
  size_t sz = count * 2;
  uint8 *buf = (uint8 *)malloc(sz);
  for (size_t i = 0; i < count; i++) {
    buf[i*2] = (uint8)(words[i] & 0xFF);
    buf[i*2+1] = (uint8)(words[i] >> 8);
  }
  assets_add_move(a, name, buf, sz);
}

// Convenience: add int8 array
static void assets_add_int8s(AssetList *a, const char *name, const int8 *vals, size_t count) {
  assets_add(a, name, (const uint8 *)vals, count);
}

// Convenience: add int16 array (writes as little-endian bytes)
static void assets_add_int16s(AssetList *a, const char *name, const int16 *vals, size_t count) {
  size_t sz = count * 2;
  uint8 *buf = (uint8 *)malloc(sz);
  for (size_t i = 0; i < count; i++) {
    uint16 v = (uint16)vals[i];
    buf[i*2] = (uint8)(v & 0xFF);
    buf[i*2+1] = (uint8)(v >> 8);
  }
  assets_add_move(a, name, buf, sz);
}

// Convenience: add packed array (from DynBuf)
static void assets_add_packed(AssetList *a, const char *name, DynBuf *buf) {
  int i = a->count++;
  a->names[i] = strdup(name);
  a->data[i] = buf->data;
  a->sizes[i] = buf->size;
  // Transfer ownership: zero out buf so caller doesn't free it
  buf->data = NULL;
  buf->size = buf->cap = 0;
}

// ================================================================
//  Sound bank extraction (assets 0-2)
// ================================================================

static void load_sound_bank_from_rom(uint32 ea, uint8 *memory) {
  memset(memory, 0, 65536);  // Will use 0 for "not loaded"
  // We use a separate marker array to track which bytes were loaded
  // Actually: the Python code uses None vs a value. We'll track with a separate array.
  for (;;) {
    uint16 numbytes = rom_get_word(ea);
    uint16 target = rom_get_word(ea + 2);
    if (numbytes == 0) break;
    ea += 4;
    for (int i = 0; i < numbytes; i++) {
      memory[target + i] = rom_get_byte(ea);
      ea = rom_advance(ea);
    }
  }
}

static void produce_loadable_seq(const uint8 *memory, const uint8 *loaded, DynBuf *out) {
  int start = 0, i = 0;
  while (start < 0x10000) {
    // Find end of loaded region
    while (i < 0x10000 && loaded[i]) i++;
    int j = i;
    // Skip unloaded region
    while (i < 0x10000 && !loaded[i]) i++;

    if (j == start) {
      start = i;
      continue;
    }
    // Output: [len_lo, len_hi, addr_lo, addr_hi, data...]
    uint16 len = (uint16)(j - start);
    db_append_byte(out, (uint8)(len & 0xFF));
    db_append_byte(out, (uint8)(len >> 8));
    db_append_byte(out, (uint8)(start & 0xFF));
    db_append_byte(out, (uint8)(start >> 8));
    db_append(out, memory + start, len);
    start = i;
  }
  // Terminator
  db_append_byte(out, 0);
  db_append_byte(out, 0);
}

static void extract_sound_bank(AssetList *a, const char *name, uint32 rom_addr) {
  uint8 *memory = (uint8 *)calloc(65536, 1);
  uint8 *loaded = (uint8 *)calloc(65536, 1);

  // Load bank into memory image, tracking which bytes were loaded
  uint32 ea = rom_addr;
  for (;;) {
    uint16 numbytes = rom_get_word(ea);
    uint16 target = rom_get_word(ea + 2);
    if (numbytes == 0) break;
    ea += 4;
    for (int i = 0; i < numbytes; i++) {
      memory[target + i] = rom_get_byte(ea);
      loaded[target + i] = 1;
      ea = rom_advance(ea);
    }
  }

  DynBuf out;
  db_init(&out);
  produce_loadable_seq(memory, loaded, &out);
  assets_add_packed(a, name, &out);

  free(memory);
  free(loaded);
}

static void extract_sound_banks(AssetList *a) {
  extract_sound_bank(a, "kSoundBank_intro", 0x998000);
  extract_sound_bank(a, "kSoundBank_indoor", 0x9B8000);
  extract_sound_bank(a, "kSoundBank_ending", 0x9AD380);
}

// ================================================================
//  Dungeon room data extraction (assets 3-55)
// ================================================================

// Read room objects raw from ROM (they're stored in the same format as the asset)
static void read_room_layer(uint32 *addr, DynBuf *data) {
  uint32 p = *addr;
  // Read 3-byte entries until 0xFFFF or 0xF0FF
  for (;;) {
    uint8 p0 = rom_get_byte(p);
    uint8 p1 = rom_get_byte(p + 1);
    uint16 A = p0 | ((uint16)p1 << 8);
    db_append_byte(data, p0);
    db_append_byte(data, p1);
    if (A == 0xFFFF) { p += 2; break; }
    if (A == 0xFFF0) {
      p += 2;
      // Read doors
      for (;;) {
        uint8 d0 = rom_get_byte(p);
        uint8 d1 = rom_get_byte(p + 1);
        db_append_byte(data, d0);
        db_append_byte(data, d1);
        p += 2;
        if ((d0 | ((uint16)d1 << 8)) == 0xFFFF) break;
      }
      break;
    }
    db_append_byte(data, rom_get_byte(p + 2));
    p += 3;
  }
  *addr = p;
}

// append_scan_bytes: find longest overlap between end of big and start of little
static size_t append_scan_bytes(DynBuf *big, const uint8 *little, size_t little_len) {
  for (size_t n = little_len; ; n--) {
    if (n == 0) {
      size_t offset = big->size;
      db_append(big, little, little_len);
      return offset;
    }
    if (big->size >= n && memcmp(big->data + big->size - n, little, n) == 0) {
      size_t offset = big->size - n;
      db_append(big, little + n, little_len - n);
      return offset;
    }
  }
}

static void extract_entrance_data(AssetList *a, const char *prefix, int set, int count) {
  // Base addresses for set 0 (entrances, 133) and set 1 (starting points, 7)
  const uint32 rooms_base   = set ? 0x82DB6E : 0x82C813;
  const uint32 rc_base      = set ? 0x82DB7C : 0x82C91D;
  const uint32 scrollx_base = set ? 0x82DBB4 : 0x82CD45;
  const uint32 scrolly_base = set ? 0x82DBC2 : 0x82CE4F;
  const uint32 playerx_base = set ? 0x82DBDE : 0x82D063;
  const uint32 playery_base = set ? 0x82DBD0 : 0x82CF59;
  const uint32 camerax_base = set ? 0x82DBFA : 0x82D277;
  const uint32 cameray_base = set ? 0x82DBEC : 0x82D16D;
  const uint32 blockset_base = set ? 0x82DC08 : 0x82D381;
  const uint32 floor_base   = set ? 0x82DC0F : 0x82D406;
  const uint32 palace_base  = set ? 0x82DC16 : 0x82D48B;
  const uint32 startbg_base = set ? 0x82DC1D : 0x82D595;
  const uint32 quad1_base   = set ? 0x82DC24 : 0x82D61A;
  const uint32 quad2_base   = set ? 0x82DC2B : 0x82D69F;
  const uint32 door_base    = set ? 0x82DC32 : 0x82D724;
  const uint32 music_base   = set ? 0x82DC4E : 0x82D82E;

  uint16 *rooms = (uint16 *)malloc(count * sizeof(uint16));
  uint8 *relcoords = (uint8 *)malloc(count * 8);
  uint16 *scrollx = (uint16 *)malloc(count * sizeof(uint16));
  uint16 *scrolly = (uint16 *)malloc(count * sizeof(uint16));
  uint16 *playerx = (uint16 *)malloc(count * sizeof(uint16));
  uint16 *playery = (uint16 *)malloc(count * sizeof(uint16));
  uint16 *camerax = (uint16 *)malloc(count * sizeof(uint16));
  uint16 *cameray = (uint16 *)malloc(count * sizeof(uint16));
  uint8 *blockset = (uint8 *)malloc(count);
  int8 *floor_arr = (int8 *)malloc(count);
  int8 *palace = (int8 *)malloc(count);
  uint8 *doorway = (uint8 *)malloc(count);
  uint8 *startbg = (uint8 *)malloc(count);
  uint8 *quad1 = (uint8 *)malloc(count);
  uint8 *quad2 = (uint8 *)malloc(count);
  uint16 *doorsettings = (uint16 *)malloc(count * sizeof(uint16));
  uint8 *musictrack = (uint8 *)malloc(count);

  for (int i = 0; i < count; i++) {
    rooms[i] = rom_get_word(rooms_base + i * 2);
    rom_get_bytes(rc_base + i * 8, relcoords + i * 8, 8);
    scrollx[i] = rom_get_word(scrollx_base + i * 2);
    scrolly[i] = rom_get_word(scrolly_base + i * 2);
    playerx[i] = rom_get_word(playerx_base + i * 2);
    playery[i] = rom_get_word(playery_base + i * 2);
    camerax[i] = rom_get_word(camerax_base + i * 2);
    cameray[i] = rom_get_word(cameray_base + i * 2);
    blockset[i] = rom_get_byte(blockset_base + i);
    floor_arr[i] = rom_get_int8(floor_base + i);
    int8 pval = rom_get_int8(palace_base + i);
    palace[i] = (int8)(pval < 0 ? -1 : (pval & ~1));
    doorway[i] = set ? 0 : (uint8)rom_get_int8(0x82D510 + i);
    startbg[i] = rom_get_byte(startbg_base + i);
    quad1[i] = rom_get_byte(quad1_base + i) & 0x22;
    quad2[i] = rom_get_byte(quad2_base + i);
    doorsettings[i] = rom_get_word(door_base + i * 2);
    musictrack[i] = rom_get_byte(music_base + i);
  }

  char name[64];
  snprintf(name, sizeof(name), "%srooms", prefix);
  assets_add_words(a, name, rooms, count);
  snprintf(name, sizeof(name), "%srelativeCoords", prefix);
  assets_add(a, name, relcoords, count * 8);
  snprintf(name, sizeof(name), "%sscrollX", prefix);
  assets_add_words(a, name, scrollx, count);
  snprintf(name, sizeof(name), "%sscrollY", prefix);
  assets_add_words(a, name, scrolly, count);
  snprintf(name, sizeof(name), "%splayerX", prefix);
  assets_add_words(a, name, playerx, count);
  snprintf(name, sizeof(name), "%splayerY", prefix);
  assets_add_words(a, name, playery, count);
  snprintf(name, sizeof(name), "%scameraX", prefix);
  assets_add_words(a, name, camerax, count);
  snprintf(name, sizeof(name), "%scameraY", prefix);
  assets_add_words(a, name, cameray, count);
  snprintf(name, sizeof(name), "%sblockset", prefix);
  assets_add(a, name, blockset, count);
  snprintf(name, sizeof(name), "%sfloor", prefix);
  assets_add_int8s(a, name, floor_arr, count);
  snprintf(name, sizeof(name), "%spalace", prefix);
  assets_add_int8s(a, name, palace, count);
  snprintf(name, sizeof(name), "%sdoorwayOrientation", prefix);
  assets_add(a, name, doorway, count);
  snprintf(name, sizeof(name), "%sstartingBg", prefix);
  assets_add(a, name, startbg, count);
  snprintf(name, sizeof(name), "%squadrant1", prefix);
  assets_add(a, name, quad1, count);
  snprintf(name, sizeof(name), "%squadrant2", prefix);
  assets_add(a, name, quad2, count);
  snprintf(name, sizeof(name), "%sdoorSettings", prefix);
  assets_add_words(a, name, doorsettings, count);

  if (set == 1) {
    // Starting points have entrance field
    uint16 *entrance = (uint16 *)malloc(count * sizeof(uint16));
    for (int i = 0; i < count; i++)
      entrance[i] = rom_get_word(0x82DC40 + i * 2);
    snprintf(name, sizeof(name), "%sentrance", prefix);
    // Note: entrance is uint8 in assets.h but uint16 in extract; the Python writes uint8
    // Actually looking at the Python: add_asset_uint8(prefix+'entrance', [...])
    // So it's uint8 values. The rom_get_word gets a uint16 but the values fit in uint8.
    uint8 *entrance_u8 = (uint8 *)malloc(count);
    for (int i = 0; i < count; i++)
      entrance_u8[i] = (uint8)entrance[i];
    assets_add(a, name, entrance_u8, count);
    free(entrance);
    free(entrance_u8);
  }

  snprintf(name, sizeof(name), "%smusicTrack", prefix);
  assets_add(a, name, musictrack, count);

  free(rooms); free(relcoords); free(scrollx); free(scrolly);
  free(playerx); free(playery); free(camerax); free(cameray);
  free(blockset); free(floor_arr); free(palace); free(doorway);
  free(startbg); free(quad1); free(quad2); free(doorsettings);
  free(musictrack);
}

static void read_room_objects_raw(uint32 addr, DynBuf *data) {
  // Read 3-byte entries until 0xFFFF. No door support for default/overlay rooms.
  for (;;) {
    uint8 p0 = rom_get_byte(addr);
    uint8 p1 = rom_get_byte(addr + 1);
    uint16 A = p0 | ((uint16)p1 << 8);
    db_append_byte(data, p0);
    db_append_byte(data, p1);
    addr += 2;
    if (A == 0xFFFF) break;
    db_append_byte(data, rom_get_byte(addr));
    addr++;
  }
}

static void extract_dungeon_rooms(AssetList *a) {
  DynBuf data, room_headers;
  db_init(&data);
  db_init(&room_headers);
  uint16 offsets[320];
  uint16 door_offsets[320];
  uint16 header_offsets[320];
  uint16 sign_texts[320];

  // Chests
  uint8 chests_raw[504];
  rom_get_bytes(0x81E96E, chests_raw, 504);

  // Pits hurt player
  uint16 pits_raw[57];
  for (int i = 0; i < 57; i++)
    pits_raw[i] = rom_get_word(0x80990C + i * 2);
  // Sort and deduplicate
  // Simple insertion sort
  for (int i = 1; i < 57; i++) {
    uint16 key = pits_raw[i];
    int j = i - 1;
    while (j >= 0 && pits_raw[j] > key) {
      pits_raw[j + 1] = pits_raw[j];
      j--;
    }
    pits_raw[j + 1] = key;
  }
  int pits_count = 0;
  uint16 pits_dedup[57];
  for (int i = 0; i < 57; i++) {
    if (pits_count == 0 || pits_raw[i] != pits_dedup[pits_count - 1])
      pits_dedup[pits_count++] = pits_raw[i];
  }

  for (int room = 0; room < 320; room++) {
    // Room data
    uint32 room_addr = rom_get_24(0x1F8000 + room * 3);
    offsets[room] = (uint16)data.size;

    uint8 floor_byte = rom_get_byte(room_addr);
    uint8 layout_byte = rom_get_byte(room_addr + 1);
    db_append_byte(&data, floor_byte);
    db_append_byte(&data, layout_byte);

    uint32 p = room_addr + 2;
    // Layer 1
    read_room_layer(&p, &data);
    // Layer 2
    read_room_layer(&p, &data);
    // Layer 3 - its door offset is what we record
    // For layer 3, we need to find the door offset
    size_t pre_layer3 = data.size;
    read_room_layer(&p, &data);
    // Find the door offset in layer 3: scan for 0xF0FF marker
    door_offsets[room] = 0;
    {
      size_t pos = pre_layer3;
      while (pos + 1 < data.size) {
        uint16 val = data.data[pos] | ((uint16)data.data[pos + 1] << 8);
        if (val == 0xFFF0) {
          door_offsets[room] = (uint16)(pos + 2);
          break;
        }
        if (val == 0xFFFF) break;
        pos += 3;
      }
    }

    // Room header
    uint32 hp = 0x4F502 + room * 2;
    uint16 hword = rom_get_word(hp);
    uint32 haddr = 0x40000 | hword;
    if (haddr == 0x4FFEF)
      haddr = 0x82EDC5; // just some place with zeros

    uint8 hdr[14];
    rom_get_bytes(haddr, hdr, 14);
    header_offsets[room] = (uint16)append_scan_bytes(&room_headers, hdr, 14);

    // Tele msg
    sign_texts[room] = rom_get_word(0x87F61D + room * 2);
  }

  assets_add_move(a, "kDungeonRoom", data.data, data.size);
  data.data = NULL; data.size = data.cap = 0;
  assets_add_words(a, "kDungeonRoomOffs", offsets, 320);
  assets_add_words(a, "kDungeonRoomDoorOffs", door_offsets, 320);
  assets_add_move(a, "kDungeonRoomHeaders", room_headers.data, room_headers.size);
  room_headers.data = NULL; room_headers.size = room_headers.cap = 0;
  assets_add_words(a, "kDungeonRoomHeadersOffs", header_offsets, 320);
  assets_add(a, "kDungeonRoomChests", chests_raw, 504);
  assets_add_words(a, "kDungeonRoomTeleMsg", sign_texts, 320);
  assets_add_words(a, "kDungeonPitsHurtPlayer", pits_dedup, pits_count);

  // Entrance data (set=0, 133 entries)
  extract_entrance_data(a, "kEntranceData_", 0, 133);
  // Starting point data (set=1, 7 entries)
  extract_entrance_data(a, "kStartingPoint_", 1, 7);

  // Default rooms (8 entries, pointer table at 0x84EF2F)
  {
    DynBuf ddata;
    db_init(&ddata);
    uint16 doffsets[8];
    for (int i = 0; i < 8; i++) {
      uint32 room_addr = rom_get_24(0x84EF2F + i * 3);
      doffsets[i] = (uint16)ddata.size;
      read_room_objects_raw(room_addr, &ddata);
    }
    assets_add_move(a, "kDungeonRoomDefault", ddata.data, ddata.size);
    ddata.data = NULL;
    assets_add_words(a, "kDungeonRoomDefaultOffs", doffsets, 8);
  }

  // Overlay rooms (19 entries, pointer table at 0x84ECC0)
  {
    DynBuf odata;
    db_init(&odata);
    uint16 ooffsets[19];
    for (int i = 0; i < 19; i++) {
      uint32 room_addr = rom_get_24(0x84ECC0 + i * 3);
      ooffsets[i] = (uint16)odata.size;
      read_room_objects_raw(room_addr, &odata);
    }
    assets_add_move(a, "kDungeonRoomOverlay", odata.data, odata.size);
    odata.data = NULL;
    assets_add_words(a, "kDungeonRoomOverlayOffs", ooffsets, 19);
  }

  // Dungeon secrets
  {
    DynBuf sdata;
    db_init(&sdata);
    // First 640 bytes are the offset table (320 rooms * 2 bytes)
    db_ensure(&sdata, 640);
    memset(sdata.data, 0, 640);
    sdata.size = 640;
    // Mark all as "not yet set"
    uint8 *offsets_set = (uint8 *)calloc(320, 1);

    for (int room = 0; room < 320; room++) {
      uint32 ea = 0x810000 | rom_get_word(0x81DB69 + room * 2);
      if (rom_get_word(ea) == 0xFFFF) continue;

      offsets_set[room] = 1;
      uint16 off = (uint16)sdata.size;
      sdata.data[room * 2] = (uint8)(off & 0xFF);
      sdata.data[room * 2 + 1] = (uint8)(off >> 8);

      while (rom_get_word(ea) != 0xFFFF) {
        db_append_byte(&sdata, rom_get_byte(ea));
        db_append_byte(&sdata, rom_get_byte(ea + 1));
        db_append_byte(&sdata, rom_get_byte(ea + 2));
        ea += 3;
      }
      db_append_byte(&sdata, 0xFF);
      db_append_byte(&sdata, 0xFF);
    }

    // Fill unset rooms with pointer to last 0xFF 0xFF
    uint16 empty_off = (uint16)(sdata.size - 2);
    for (int room = 0; room < 320; room++) {
      if (!offsets_set[room]) {
        sdata.data[room * 2] = (uint8)(empty_off & 0xFF);
        sdata.data[room * 2 + 1] = (uint8)(empty_off >> 8);
      }
    }
    free(offsets_set);
    assets_add_move(a, "kDungeonSecrets", sdata.data, sdata.size);
    sdata.data = NULL;
  }

  // Tile attrs
  {
    uint16 toffs[21];
    rom_get_words(0x8E9000, toffs, 21);
    assets_add_words(a, "kDungAttrsForTile_Offs", toffs, 21);

    uint8 *tdata = (uint8 *)malloc(1024);
    rom_get_bytes(0x8E902A, tdata, 1024);
    assets_add_move(a, "kDungAttrsForTile", tdata, 1024);
  }

  // Movable blocks
  {
    uint16 mblocks[198];
    rom_get_words(0x84F1DE, mblocks, 198);
    assets_add_words(a, "kMovableBlockDataInit", mblocks, 198);
  }

  // Torches
  {
    uint16 torches[144];
    rom_get_words(0x84F36A, torches, 144);
    assets_add_words(a, "kTorchDataInit", torches, 144);

    uint16 torches_junk[48];
    rom_get_words(0x84F48A, torches_junk, 48);
    assets_add_words(a, "kTorchDataJunk", torches_junk, 48);
  }
}

// ================================================================
//  Enemy damage data (asset 56)
// ================================================================

static void extract_enemy_damage(AssetList *a) {
  size_t decomp_len = 0;
  uint8 *decomp = lz_decomp(0x83E800, true, &decomp_len, NULL);
  assets_add_move(a, "kEnemyDamageData", decomp, decomp_len);
}

// ================================================================
//  Link graphics (asset 57)
// ================================================================

static void extract_link_graphics(AssetList *a) {
  uint8 *data = (uint8 *)malloc(28672);
  rom_get_bytes(0x108000, data, 28672);
  assets_add_move(a, "kLinkGraphics", data, 28672);
}

// ================================================================
//  Dungeon sprites (assets 58-59)
// ================================================================

static void extract_dungeon_sprites(AssetList *a) {
  DynBuf data;
  db_init(&data);
  uint16 offsets[320];
  memset(offsets, 0, sizeof(offsets));

  // Start with sentinel [0x00, 0xFF]
  db_append_byte(&data, 0x00);
  db_append_byte(&data, 0xFF);

  for (int room = 0; room < 320; room++) {
    uint32 ea = 0x890000 | rom_get_word(0x89D62E + room * 2);
    uint8 sort_mode = rom_get_byte(ea);
    ea++;

    // Check if room has sprites
    if (rom_get_byte(ea) == 0xFF && sort_mode == 0)
      continue;

    offsets[room] = (uint16)data.size;
    db_append_byte(&data, sort_mode);

    while (rom_get_byte(ea) != 0xFF) {
      db_append_byte(&data, rom_get_byte(ea));
      db_append_byte(&data, rom_get_byte(ea + 1));
      db_append_byte(&data, rom_get_byte(ea + 2));
      ea += 3;
    }
    db_append_byte(&data, 0xFF);
  }

  assets_add_move(a, "kDungeonSprites", data.data, data.size);
  data.data = NULL;
  assets_add_words(a, "kDungeonSpriteOffs", offsets, 320);
}

// ================================================================
//  Map32 to Map16 (assets 60-63)
// ================================================================

static void extract_map32_to_map16(AssetList *a) {
  // 4 result arrays, each for one quadrant
  DynBuf res[4];
  for (int j = 0; j < 4; j++) db_init(&res[j]);

  static const uint32 bases[4] = {0x838000, 0x83B400, 0x848000, 0x84B400};

  for (int i = 0; i < 2218; i++) {
    uint16 vals[4][4]; // vals[quadrant][entry_within_group]

    for (int q = 0; q < 4; q++) {
      uint32 ea = bases[q] + i * 6;
      uint8 ov[6];
      rom_get_bytes(ea, ov, 6);
      vals[q][0] = ov[0] | (uint16)((ov[4] >> 4) << 8);
      vals[q][1] = ov[1] | (uint16)((ov[4] & 0xF) << 8);
      vals[q][2] = ov[2] | (uint16)((ov[5] >> 4) << 8);
      vals[q][3] = ov[3] | (uint16)((ov[5] & 0xF) << 8);
    }

    // Now group into packs of 4 entries and re-pack
    // The Python iterates i from 0..2217, then groups them in blocks of 4 (i, i+1, i+2, i+3).
    // Actually no - the Python does: for i in range(0, len(tab), 4): for j in range(4): res[j].extend(pack(tab[i][j],tab[i+1][j],tab[i+2][j],tab[i+3][j]))
    // tab has 2218*4 = 8872 entries indexed by i*4+j. This means the outer i steps by 4 over 8872 entries.
    // Wait - let me re-read. The extract writes tab[i*4+j] = (t0[j], t1[j], t2[j], t3[j]) for each source entry i.
    // So tab has 2218*4 entries. Then compile iterates 0..8871 step 4.
    // We need to match exactly. Let me restructure.
    (void)vals; // will use below
  }

  // Re-do this properly. First build the full table.
  // tab[k] = [val0, val1, val2, val3] for k=0..8871
  size_t tab_count = 2218 * 4;
  uint16 (*tab)[4] = (uint16 (*)[4])malloc(tab_count * 4 * sizeof(uint16));

  for (int i = 0; i < 2218; i++) {
    uint16 t[4][4];
    for (int q = 0; q < 4; q++) {
      uint32 ea = bases[q] + i * 6;
      uint8 ov[6];
      rom_get_bytes(ea, ov, 6);
      t[q][0] = ov[0] | (uint16)((ov[4] >> 4) << 8);
      t[q][1] = ov[1] | (uint16)((ov[4] & 0xF) << 8);
      t[q][2] = ov[2] | (uint16)((ov[5] >> 4) << 8);
      t[q][3] = ov[3] | (uint16)((ov[5] & 0xF) << 8);
    }
    for (int j = 0; j < 4; j++) {
      tab[i * 4 + j][0] = t[0][j];
      tab[i * 4 + j][1] = t[1][j];
      tab[i * 4 + j][2] = t[2][j];
      tab[i * 4 + j][3] = t[3][j];
    }
  }

  // Now pack: for i in range(0, tab_count, 4): for j in range(4): res[j] += pack(tab[i][j], tab[i+1][j], tab[i+2][j], tab[i+3][j])
  for (size_t i = 0; i < tab_count; i += 4) {
    for (int j = 0; j < 4; j++) {
      uint16 a0 = tab[i][j], a1 = tab[i+1][j], a2 = tab[i+2][j], a3 = tab[i+3][j];
      uint8 packed[6];
      packed[0] = (uint8)(a0 & 0xFF);
      packed[1] = (uint8)(a1 & 0xFF);
      packed[2] = (uint8)(a2 & 0xFF);
      packed[3] = (uint8)(a3 & 0xFF);
      packed[4] = (uint8)((a0 >> 8) << 4 | (a1 >> 8));
      packed[5] = (uint8)((a2 >> 8) << 4 | (a3 >> 8));
      db_append(&res[j], packed, 6);
    }
  }

  free(tab);

  static const char *names[4] = {"kMap32ToMap16_0","kMap32ToMap16_1","kMap32ToMap16_2","kMap32ToMap16_3"};
  for (int j = 0; j < 4; j++) {
    assets_add_move(a, names[j], res[j].data, res[j].size);
    res[j].data = NULL;
  }
}

// ================================================================
//  Sprite/BG graphics (assets 64-65)
// ================================================================

static const uint32 kCompSpritePtrs[108] = {
  0x10f000,0x10f600,0x10fc00,0x118200,0x118800,0x118e00,0x119400,0x119a00,
  0x11a000,0x11a600,0x11ac00,0x11b200,0x14fffc,0x1585d4,0x158ab6,0x158fbe,
  0x1593f8,0x1599a6,0x159f32,0x15a3d7,0x15a8f1,0x15aec6,0x15b418,0x15b947,
  0x15bed0,0x15c449,0x15c975,0x15ce7c,0x15d394,0x15d8ac,0x15ddc0,0x15e34c,
  0x15e8e8,0x15ee31,0x15f3a6,0x15f92d,0x15feba,0x1682ff,0x1688e0,0x168e41,
  0x1692df,0x169883,0x169cd0,0x16a26e,0x16a275,0x16a787,0x16aa06,0x16ae9d,
  0x16b3ff,0x16b87e,0x16be6b,0x16c13d,0x16c619,0x16cbbb,0x16d0f1,0x16d641,
  0x16d95a,0x16dd99,0x16e278,0x16e760,0x16ed25,0x16f20f,0x16f6b7,0x16fa5f,
  0x16fd29,0x1781cd,0x17868d,0x178b62,0x178fd5,0x179527,0x17994b,0x179ea7,
  0x17a30e,0x17a805,0x17acf8,0x17b2a2,0x17b7f9,0x17bc93,0x17c237,0x17c78e,
  0x17cd55,0x17d2bc,0x17d82f,0x17dcec,0x17e1cc,0x17e36b,0x17e842,0x17eb38,
  0x17ed58,0x17f06c,0x17f4fd,0x17fa39,0x17ff86,0x18845c,0x1889a1,0x188d64,
  0x18919d,0x189610,0x189857,0x189b24,0x189dd2,0x18a03f,0x18a4ed,0x18a7ba,
  0x18aedf,0x18af0d,0x18b520,0x18b953,
};

static const uint32 kCompBgPtrs[115] = {
  0x11b800,0x11bce2,0x11c15f,0x11c675,0x11cb84,0x11cf4c,0x11d2ce,0x11d726,
  0x11d9cf,0x11dec4,0x11e393,0x11e893,0x11ed7d,0x11f283,0x11f746,0x11fc21,
  0x11fff2,0x128498,0x128a0e,0x128f30,0x129326,0x129804,0x129d5b,0x12a272,
  0x12a6fe,0x12aa77,0x12ad83,0x12b167,0x12b51d,0x12b840,0x12bd54,0x12c1c9,
  0x12c73d,0x12cc86,0x12d198,0x12d6b1,0x12db6a,0x12e0ea,0x12e6bd,0x12eb51,
  0x12f135,0x12f6c5,0x12fc71,0x138129,0x138693,0x138bad,0x139117,0x139609,
  0x139b21,0x13a074,0x13a619,0x13ab2b,0x13b00c,0x13b4f5,0x13b9eb,0x13bebf,
  0x13c3ce,0x13c817,0x13cb68,0x13cfb5,0x13d460,0x13d8c2,0x13dd7a,0x13e266,
  0x13e7af,0x13ece5,0x13f245,0x13f6f0,0x13fc30,0x1480e9,0x14863b,0x148a7c,
  0x148f2a,0x149346,0x1497ed,0x149cc2,0x14a173,0x14a61d,0x14ab5d,0x14b083,
  0x14b4bd,0x14b94e,0x14be0e,0x14c291,0x14c7ba,0x14cce4,0x14d1db,0x14d6bd,
  0x14db77,0x14ded1,0x14e2ac,0x14e754,0x14ebae,0x14ef4e,0x14f309,0x14f6f4,
  0x14fa55,0x14ff8c,0x14ff93,0x14ff9a,0x14ffa1,0x14ffa8,0x14ffaf,0x14ffb6,
  0x14ffbd,0x14ffc4,0x14ffcb,0x14ffd2,0x14ffd9,0x14ffe0,0x14ffe7,0x14ffee,
  0x14fff5,0x18b520,0x18b953,
};

static void extract_sprite_gfx(AssetList *a) {
  const uint8 *items[108];
  size_t item_sizes[108];
  uint8 *allocs[108];

  for (int i = 0; i < 108; i++) {
    if (i < 12) {
      // Uncompressed: 0x600 bytes
      allocs[i] = (uint8 *)malloc(0x600);
      rom_get_bytes(kCompSpritePtrs[i], allocs[i], 0x600);
      items[i] = allocs[i];
      item_sizes[i] = 0x600;
    } else {
      // Compressed: decompress to find length, copy raw compressed bytes
      size_t comp_len = 0;
      uint8 *decomp = lz_decomp(kCompSpritePtrs[i], false, NULL, &comp_len);
      free(decomp);
      allocs[i] = (uint8 *)malloc(comp_len);
      rom_get_bytes(kCompSpritePtrs[i], allocs[i], comp_len);
      items[i] = allocs[i];
      item_sizes[i] = comp_len;
    }
  }

  DynBuf packed;
  db_init(&packed);
  pack_arrays(&packed, items, item_sizes, 108);
  assets_add_packed(a, "kSprGfx", &packed);

  for (int i = 0; i < 108; i++) free(allocs[i]);
}

static void extract_bg_gfx(AssetList *a) {
  const uint8 *items[115];
  size_t item_sizes[115];
  uint8 *allocs[115];

  for (int i = 0; i < 115; i++) {
    size_t comp_len = 0;
    uint8 *decomp = lz_decomp(kCompBgPtrs[i], false, NULL, &comp_len);
    free(decomp);
    allocs[i] = (uint8 *)malloc(comp_len);
    rom_get_bytes(kCompBgPtrs[i], allocs[i], comp_len);
    items[i] = allocs[i];
    item_sizes[i] = comp_len;
  }

  DynBuf packed;
  db_init(&packed);
  pack_arrays(&packed, items, item_sizes, 115);
  assets_add_packed(a, "kBgGfx", &packed);

  for (int i = 0; i < 115; i++) free(allocs[i]);
}

// ================================================================
//  Misc data (assets 66-93)
// ================================================================

static void extract_misc(AssetList *a) {
  uint8 *buf;

  // 66: kOverworldMapGfx
  buf = (uint8 *)malloc(0x4000);
  rom_get_bytes(0x18C000, buf, 0x4000);
  assets_add_move(a, "kOverworldMapGfx", buf, 0x4000);

  // 67: kLightOverworldTilemap
  buf = (uint8 *)malloc(4096);
  rom_get_bytes(0xAC727, buf, 4096);
  assets_add_move(a, "kLightOverworldTilemap", buf, 4096);

  // 68: kDarkOverworldTilemap
  buf = (uint8 *)malloc(1024);
  rom_get_bytes(0xAD727, buf, 1024);
  assets_add_move(a, "kDarkOverworldTilemap", buf, 1024);

  // 69: kPredefinedTileData - 6438 words
  {
    uint16 *w = (uint16 *)malloc(6438 * 2);
    rom_get_words(0x9B52, w, 6438);
    assets_add_words(a, "kPredefinedTileData", w, 6438);
    free(w);
  }

  // 70: kMap16ToMap8 - 3752*4 words
  {
    uint16 *w = (uint16 *)malloc(3752 * 4 * 2);
    rom_get_words(0x8F8000, w, 3752 * 4);
    assets_add_words(a, "kMap16ToMap8", w, 3752 * 4);
    free(w);
  }

  // 71-73: uint8 arrays
  buf = (uint8 *)malloc(256); rom_get_bytes(0x888450, buf, 256);
  assets_add_move(a, "kGeneratedWishPondItem", buf, 256);

  buf = (uint8 *)malloc(256); rom_get_bytes(0x8890FC, buf, 256);
  assets_add_move(a, "kGeneratedBombosArr", buf, 256);

  buf = (uint8 *)malloc(256); rom_get_bytes(0x8EAD25, buf, 256);
  assets_add_move(a, "kGeneratedEndSequence15", buf, 256);

  // 74: kEnding_Credits_Text
  buf = (uint8 *)malloc(1989); rom_get_bytes(0x8EB178, buf, 1989);
  assets_add_move(a, "kEnding_Credits_Text", buf, 1989);

  // 75: kEnding_Credits_Offs - 394 words
  {
    uint16 *w = (uint16 *)malloc(394 * 2);
    rom_get_words(0x8EB93D, w, 394);
    assets_add_words(a, "kEnding_Credits_Offs", w, 394);
    free(w);
  }

  // 76-78
  {
    uint16 *w = (uint16 *)malloc(160 * 2);
    rom_get_words(0x8EB038, w, 160);
    assets_add_words(a, "kEnding_MapData", w, 160);
    free(w);
  }
  {
    uint16 *w = (uint16 *)malloc(17 * 2);
    rom_get_words(0x8EC2E1, w, 17);
    assets_add_words(a, "kEnding0_Offs", w, 17);
    free(w);
  }
  buf = (uint8 *)malloc(917); rom_get_bytes(0x8EBF4C, buf, 917);
  assets_add_move(a, "kEnding0_Data", buf, 917);

  // 79-93: Palettes
  #define ADD_PAL(name, addr, count) do { \
    uint16 *_w = (uint16 *)malloc((count) * 2); \
    rom_get_words(addr, _w, count); \
    assets_add_words(a, name, _w, count); \
    free(_w); \
  } while(0)

  ADD_PAL("kPalette_DungBgMain", 0x9BD734, 1800);
  ADD_PAL("kPalette_MainSpr", 0x9BD218, 120);
  ADD_PAL("kPalette_ArmorAndGloves", 0x9BD308, 75);
  ADD_PAL("kPalette_Sword", 0x9BD630, 12);
  ADD_PAL("kPalette_Shield", 0x9BD648, 12);
  ADD_PAL("kPalette_SpriteAux3", 0x9BD39E, 84);
  ADD_PAL("kPalette_MiscSprite_Indoors", 0x9BD446, 77);
  ADD_PAL("kPalette_SpriteAux1", 0x9BD4E0, 168);
  ADD_PAL("kPalette_OverworldBgMain", 0x9BE6C8, 210);
  ADD_PAL("kPalette_OverworldBgAux12", 0x9BE86C, 420);
  ADD_PAL("kPalette_OverworldBgAux3", 0x9BE604, 98);
  ADD_PAL("kPalette_PalaceMapBg", 0x9BE544, 96);
  ADD_PAL("kPalette_PalaceMapSpr", 0x9BD70A, 21);
  ADD_PAL("kHudPalData", 0x9BD660, 64);
  ADD_PAL("kOverworldMapPaletteData", 0x8ADB27, 256);

  #undef ADD_PAL
}

// ================================================================
//  Dialogue (assets 94-96)
// ================================================================

// US alphabet for encoding dictionary entries
static const char *kTextAlphabet_US[] = {
  "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P",
  "Q","R","S","T","U","V","W","X","Y","Z","a","b","c","d","e","f",
  "g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v",
  "w","x","y","z","0","1","2","3","4","5","6","7","8","9","!","?",
  "-",".",",",
  "[...]",">","(",")",
  "[Ankh]","[Waves]","[Snake]","[LinkL]","[LinkR]",
  "\"","[Up]","[Down]","[Left]",
  "[Right]","'","[1HeartL]","[1HeartR]","[2HeartL]","[3HeartL]","[3HeartR]",
  "[4HeartL]","[4HeartR]"," ","<","[A]","[B]","[X]","[Y]",
};

static const char *kTextDictionary_US[] = {
  "    ","   ","  ","'s ","and ",
  "are ","all ","ain","and","at ",
  "ast","an","at","ble","ba",
  "be","bo","can ","che","com",
  "ck","des","di","do","en ",
  "er ","ear","ent","ed ","en",
  "er","ev","for","fro","give ",
  "get","go","have","has","her",
  "hi","ha","ight ","ing ","in",
  "is","it","just","know","ly ",
  "la","lo","man","ma","me",
  "mu","n't ","non","not","open",
  "ound","out ","of","on","or",
  "per","ple","pow","pro","re ",
  "re","some","se","sh","so",
  "st","ter ","thin","ter","tha",
  "the","thi","to","tr","up",
  "ver","with","wa","we","wh",
  "wi","you","Her","Tha","The",
  "Thi","You",
};
#define NUM_DICT_ENTRIES 97

static const uint8 kText_CommandLengths_US[25] = {
  1,1,1,1,2,2,2,2,1,1,1,1,1,1,1,1,2,2,2,2,1,1,1,1,1
};

// Extra dialogue bytes inserted at index 4 if only 396 messages found
static const uint8 kExtraDialogue[] = {
  0x7A,0x00,0x34,0x40,0x59,0x6C,0x00,0x41,0x59,0x35,0x40,0x59,0x6C,0x01,
  0x75,0x36,0x40,0x59,0x6C,0x02,0x41,0x59,0x37,0x40,0x59,0x6C,0x03
};

static void extract_dialogue(AssetList *a) {
  // Encode dictionary
  // Each dictionary entry is encoded as character indices
  DynBuf dict_entries[NUM_DICT_ENTRIES];
  const uint8 *dict_ptrs[NUM_DICT_ENTRIES];
  size_t dict_sizes[NUM_DICT_ENTRIES];

  // Build char->index map for alphabet
  // The alphabet has 95 entries (indices 0-94)
  // We need to match single characters to their index
  for (int d = 0; d < NUM_DICT_ENTRIES; d++) {
    db_init(&dict_entries[d]);
    const char *word = kTextDictionary_US[d];
    size_t wlen = strlen(word);
    for (size_t ci = 0; ci < wlen; ci++) {
      // Find this character in the alphabet
      char ch = word[ci];
      int found = -1;
      for (int ai = 0; ai < 95; ai++) {
        if (strlen(kTextAlphabet_US[ai]) == 1 && kTextAlphabet_US[ai][0] == ch) {
          found = ai;
          break;
        }
      }
      if (found >= 0)
        db_append_byte(&dict_entries[d], (uint8)found);
    }
    dict_ptrs[d] = dict_entries[d].data;
    dict_sizes[d] = dict_entries[d].size;
  }

  DynBuf dict_packed;
  db_init(&dict_packed);
  pack_arrays(&dict_packed, dict_ptrs, dict_sizes, NUM_DICT_ENTRIES);

  // Scan dialogue from ROM
  // US format: start at 0x9C8000, COMMAND_START=0x67, SWITCH_BANK=0x80, FINISH=0xFF
  uint32 p = 0x9C8000;
  int msg_count = 0;
  // Collect messages: each message is bytes until 0x7F (EndMessage), excluding the 0x7F
  size_t msgs_cap = 512;
  uint8 **msgs = (uint8 **)malloc(msgs_cap * sizeof(uint8 *));
  size_t *msg_sizes = (size_t *)malloc(msgs_cap * sizeof(size_t));

  DynBuf cur_msg;
  db_init(&cur_msg);

  for (;;) {
    uint8 c = rom_get_byte(p);
    if (c == 0x7F) { // EndMessage
      // Save current message (excluding 0x7F)
      if (msg_count >= (int)msgs_cap) {
        msgs_cap *= 2;
        msgs = (uint8 **)realloc(msgs, msgs_cap * sizeof(uint8 *));
        msg_sizes = (size_t *)realloc(msg_sizes, msgs_cap * sizeof(size_t));
      }
      msgs[msg_count] = cur_msg.data;
      msg_sizes[msg_count] = cur_msg.size;
      msg_count++;
      cur_msg.data = NULL; cur_msg.size = cur_msg.cap = 0;
      db_init(&cur_msg);
      p++;
      continue;
    }
    if (c == 0xFF) { // FINISH
      break;
    }
    if (c == 0x80) { // SWITCH_BANK - jump to 0x8EDF40
      p = 0x8EDF40;
      // Reset current message
      if (cur_msg.data) { free(cur_msg.data); }
      db_init(&cur_msg);
      continue;
    }
    if (c >= 0x67 && c < 0x80) {
      // Command
      int cmd_idx = c - 0x67;
      int cmd_len = (cmd_idx < 25) ? kText_CommandLengths_US[cmd_idx] : 1;
      db_append_byte(&cur_msg, c);
      for (int j = 1; j < cmd_len; j++) {
        p++;
        db_append_byte(&cur_msg, rom_get_byte(p));
      }
    } else {
      // Character byte
      db_append_byte(&cur_msg, c);
    }
    p++;
  }
  db_free(&cur_msg);

  // If only 396 messages, insert extra at index 4
  if (msg_count == 396) {
    // Make room
    if (msg_count + 1 >= (int)msgs_cap) {
      msgs_cap *= 2;
      msgs = (uint8 **)realloc(msgs, msgs_cap * sizeof(uint8 *));
      msg_sizes = (size_t *)realloc(msg_sizes, msgs_cap * sizeof(size_t));
    }
    // Shift messages 4..end right by 1
    memmove(msgs + 5, msgs + 4, (msg_count - 4) * sizeof(uint8 *));
    memmove(msg_sizes + 5, msg_sizes + 4, (msg_count - 4) * sizeof(size_t));
    // Insert at index 4
    msgs[4] = (uint8 *)malloc(sizeof(kExtraDialogue));
    memcpy(msgs[4], kExtraDialogue, sizeof(kExtraDialogue));
    msg_sizes[4] = sizeof(kExtraDialogue);
    msg_count++;
  }

  // Pack dialogue messages
  DynBuf dialogue_packed;
  db_init(&dialogue_packed);
  pack_arrays(&dialogue_packed, (const uint8 **)msgs, msg_sizes, msg_count);

  // Pack [dict_packed, dialogue_packed] into one language entry
  const uint8 *lang_items[2] = { dict_packed.data, dialogue_packed.data };
  size_t lang_sizes[2] = { dict_packed.size, dialogue_packed.size };
  DynBuf lang_entry;
  db_init(&lang_entry);
  pack_arrays(&lang_entry, lang_items, lang_sizes, 2);

  // kDialogue is packed[1 entry] = the language entry
  const uint8 *dial_items[1] = { lang_entry.data };
  size_t dial_sizes[1] = { lang_entry.size };
  DynBuf dial_packed;
  db_init(&dial_packed);
  pack_arrays(&dial_packed, dial_items, dial_sizes, 1);
  assets_add_packed(a, "kDialogue", &dial_packed);

  // kDialogueFont: packed[1 entry] = pack_arrays([font_data, font_width])
  {
    uint8 font_data[4096];
    rom_get_bytes(0x8E8000, font_data, 4096);
    uint8 font_width[99];
    rom_get_bytes(0x8ECADF, font_width, 99);

    const uint8 *font_items[2] = { font_data, font_width };
    size_t font_sizes[2] = { 4096, 99 };
    DynBuf font_packed;
    db_init(&font_packed);
    pack_arrays(&font_packed, font_items, font_sizes, 2);

    const uint8 *fp_items[1] = { font_packed.data };
    size_t fp_sizes[1] = { font_packed.size };
    DynBuf fp_outer;
    db_init(&fp_outer);
    pack_arrays(&fp_outer, fp_items, fp_sizes, 1);
    assets_add_packed(a, "kDialogueFont", &fp_outer);
    db_free(&font_packed);
  }

  // kDialogueMap: packed[1 entry] = pack_arrays([b"us", bytes([0, 0, 0])])
  {
    const uint8 us_str[] = {'u', 's'};
    const uint8 flags[] = {0, 0, 0};
    const uint8 *map_items[2] = { us_str, flags };
    size_t map_sizes[2] = { 2, 3 };
    DynBuf map_packed;
    db_init(&map_packed);
    pack_arrays(&map_packed, map_items, map_sizes, 2);

    const uint8 *mp_items[1] = { map_packed.data };
    size_t mp_sizes[1] = { map_packed.size };
    DynBuf mp_outer;
    db_init(&mp_outer);
    pack_arrays(&mp_outer, mp_items, mp_sizes, 1);
    assets_add_packed(a, "kDialogueMap", &mp_outer);
    db_free(&map_packed);
  }

  // Cleanup
  db_free(&dict_packed);
  db_free(&dialogue_packed);
  db_free(&lang_entry);
  for (int i = 0; i < NUM_DICT_ENTRIES; i++)
    db_free(&dict_entries[i]);
  for (int i = 0; i < msg_count; i++)
    free(msgs[i]);
  free(msgs);
  free(msg_sizes);
}

// ================================================================
//  Dungeon map (assets 97-98)
// ================================================================

static void extract_dungeon_map(AssetList *a) {
  static const int kSizes[14] = {75,125,50,75,175,75,50,75,50,200,150,75,100,200};

  const uint8 *floor_items[14];
  size_t floor_sizes[14];
  const uint8 *tile_items[14];
  size_t tile_sizes[14];
  uint8 *floor_allocs[14];
  uint8 *tile_allocs[14];

  for (int i = 0; i < 14; i++) {
    uint32 addr = 0xA0000 + rom_get_word(0x8AF605 + i * 2);
    floor_allocs[i] = (uint8 *)malloc(kSizes[i]);
    rom_get_bytes(addr, floor_allocs[i], kSizes[i]);
    floor_items[i] = floor_allocs[i];
    floor_sizes[i] = kSizes[i];

    // Count non-0xF bytes
    int nonzero = 0;
    for (int j = 0; j < kSizes[i]; j++)
      if (floor_allocs[i][j] != 0x0F) nonzero++;

    uint32 tile_addr = 0xA0000 + rom_get_word(0x8AFBE4 + i * 2);
    tile_allocs[i] = (uint8 *)malloc(nonzero);
    rom_get_bytes(tile_addr, tile_allocs[i], nonzero);
    tile_items[i] = tile_allocs[i];
    tile_sizes[i] = nonzero;
  }

  DynBuf packed_floor, packed_tiles;
  db_init(&packed_floor);
  db_init(&packed_tiles);
  pack_arrays(&packed_floor, floor_items, floor_sizes, 14);
  pack_arrays(&packed_tiles, tile_items, tile_sizes, 14);
  assets_add_packed(a, "kDungMap_FloorLayout", &packed_floor);
  assets_add_packed(a, "kDungMap_Tiles", &packed_tiles);

  for (int i = 0; i < 14; i++) {
    free(floor_allocs[i]);
    free(tile_allocs[i]);
  }
}

// ================================================================
//  BG tilemaps (assets 99-104)
// ================================================================

static void extract_tilemaps(AssetList *a) {
  static const uint32 kSrcs[6] = {0xCDD6D, 0xCE7BF, 0xCE2A8, 0xCE63C, 0xCE456, 0xEDA9C};

  for (int idx = 0; idx < 6; idx++) {
    uint32 p = kSrcs[idx];
    uint32 p_org = p;

    while (!(rom_get_byte(p) & 0x80)) {
      int is_memset = rom_get_byte(p + 2) & 0x40;
      int len = ((rom_get_byte(p + 2) * 256 + rom_get_byte(p + 3)) & 0x3FFF) + 1;
      p += 4;
      p += is_memset ? 2 : (uint32)len;
    }
    size_t total = p - p_org + 1;

    uint8 *buf = (uint8 *)malloc(total);
    rom_get_bytes(p_org, buf, total);

    char name[32];
    snprintf(name, sizeof(name), "kBgTilemap_%d", idx);
    assets_add_move(a, name, buf, total);
  }
}

// ================================================================
//  Overworld compressed (assets 105-106)
// ================================================================

static void extract_overworld_compressed(AssetList *a) {
  // Hibytes: 160 entries
  {
    const uint8 *items[160];
    size_t item_sizes[160];
    uint8 *allocs[160];

    for (int i = 0; i < 160; i++) {
      uint32 addr = rom_get_24(0x82F94D + i * 3);
      size_t comp_len = 0;
      uint8 *decomp = lz_decomp(addr, true, NULL, &comp_len);
      free(decomp);
      allocs[i] = (uint8 *)malloc(comp_len);
      rom_get_bytes(addr, allocs[i], comp_len);
      items[i] = allocs[i];
      item_sizes[i] = comp_len;
    }

    DynBuf packed;
    db_init(&packed);
    pack_arrays(&packed, items, item_sizes, 160);
    assets_add_packed(a, "kOverworld_Hibytes_Comp", &packed);
    for (int i = 0; i < 160; i++) free(allocs[i]);
  }

  // Lobytes: 160 entries
  {
    const uint8 *items[160];
    size_t item_sizes[160];
    uint8 *allocs[160];

    for (int i = 0; i < 160; i++) {
      uint32 addr = rom_get_24(0x82FB2D + i * 3);
      size_t comp_len = 0;
      uint8 *decomp = lz_decomp(addr, true, NULL, &comp_len);
      free(decomp);
      allocs[i] = (uint8 *)malloc(comp_len);
      rom_get_bytes(addr, allocs[i], comp_len);
      items[i] = allocs[i];
      item_sizes[i] = comp_len;
    }

    DynBuf packed;
    db_init(&packed);
    pack_arrays(&packed, items, item_sizes, 160);
    assets_add_packed(a, "kOverworld_Lobytes_Comp", &packed);
    for (int i = 0; i < 160; i++) free(allocs[i]);
  }
}

// ================================================================
//  Overworld tables (assets 107-164)
// ================================================================

static bool is_area_head(int i) {
  return i >= 128 || rom_get_byte(0x82A5EC + (i & 63)) == (i & 63);
}

static void awrite_u8(uint8 *arr, int area, int key, uint8 value, const uint8 *is_small) {
  arr[key] = value;
  if (area < 128 && !is_small[area]) {
    arr[key + 1] = value;
    arr[key + 8] = value;
    arr[key + 9] = value;
  }
}

static void awrite_u16(uint16 *arr, int area, int key, uint16 value, const uint8 *is_small) {
  arr[key] = value;
  if (area < 128 && !is_small[area]) {
    arr[key + 1] = value;
    arr[key + 8] = value;
    arr[key + 9] = value;
  }
}

static void extract_overworld_tables(AssetList *a) {
  // Read is_small array from ROM
  uint8 is_small[192];
  rom_get_bytes(0x82F88D, is_small, 192);
  assets_add(a, "kOverworldMapIsSmall", is_small, 192);

  // Aux tile theme indexes (128 bytes)
  uint8 aux_tile[128];
  rom_get_bytes(0x80FC9C, aux_tile, 128);
  assets_add(a, "kOverworldAuxTileThemeIndexes", aux_tile, 128);

  // BG palettes (136 bytes)
  uint8 bg_pal[136];
  rom_get_bytes(0x80FD1C, bg_pal, 136);
  assets_add(a, "kOverworldBgPalettes", bg_pal, 136);

  // Sign text (128 words)
  uint16 sign_text[128];
  rom_get_words(0x87F51D, sign_text, 128);
  assets_add_words(a, "kOverworld_SignText", sign_text, 128);

  // Music sets (256 + 96 bytes)
  uint8 music_sets[256];
  rom_get_bytes(0x82C303, music_sets, 256);
  assets_add(a, "kOwMusicSets", music_sets, 256);

  uint8 music_sets2[96];
  rom_get_bytes(0x82C403, music_sets2, 96);
  assets_add(a, "kOwMusicSets2", music_sets2, 96);

  // Bird travel data (17 entries)
  {
    uint16 screen[17], loadoffs[17], scrollx[17], scrolly[17];
    uint16 posx[17], posy[17], camerax[17], cameray[17];
    int8 unk1[17], unk3[17];
    for (int i = 0; i < 17; i++) {
      screen[i] = rom_get_word(0x82EAE5 + i * 2);
      loadoffs[i] = rom_get_word(0x82EB07 + i * 2);
      scrolly[i] = rom_get_word(0x82EB29 + i * 2);
      scrollx[i] = rom_get_word(0x82EB4B + i * 2);
      posy[i] = rom_get_word(0x82EB6D + i * 2);
      posx[i] = rom_get_word(0x82EB8F + i * 2);
      cameray[i] = rom_get_word(0x82EBB1 + i * 2);
      camerax[i] = rom_get_word(0x82EBD3 + i * 2);
      unk1[i] = rom_get_int8(0x82EBF5 + i * 2);
      unk3[i] = rom_get_int8(0x82EC17 + i * 2);
    }
    assets_add_words(a, "kBirdTravel_ScreenIndex", screen, 17);
    assets_add_words(a, "kBirdTravel_Map16LoadSrcOff", loadoffs, 17);
    assets_add_words(a, "kBirdTravel_ScrollX", scrollx, 17);
    assets_add_words(a, "kBirdTravel_ScrollY", scrolly, 17);
    assets_add_words(a, "kBirdTravel_LinkXCoord", posx, 17);
    assets_add_words(a, "kBirdTravel_LinkYCoord", posy, 17);
    assets_add_words(a, "kBirdTravel_CameraXScroll", camerax, 17);
    assets_add_words(a, "kBirdTravel_CameraYScroll", cameray, 17);
    assets_add_int8s(a, "kBirdTravel_Unk1", unk1, 17);
    assets_add_int8s(a, "kBirdTravel_Unk3", unk3, 17);
  }

  // Whirlpool areas (8 entries)
  {
    uint16 whirlpool[8];
    for (int i = 0; i < 8; i++)
      whirlpool[i] = rom_get_word(0x82ECF8 + i * 2);
    assets_add_words(a, "kWhirlpoolAreas", whirlpool, 8);
  }

  // Overworld entrances (129 entries)
  {
    uint16 ow_area[129], ow_pos[129];
    uint8 ow_id[129];
    for (int i = 0; i < 129; i++) {
      ow_area[i] = rom_get_word(0x9BB96F + i * 2);
      ow_pos[i] = rom_get_word(0x9BBA71 + i * 2);
      ow_id[i] = rom_get_byte(0x9BBB73 + i);
    }
    assets_add_words(a, "kOverworld_Entrance_Area", ow_area, 129);
    assets_add_words(a, "kOverworld_Entrance_Pos", ow_pos, 129);
    assets_add(a, "kOverworld_Entrance_Id", ow_id, 129);
  }

  // Fall holes (19 entries)
  {
    uint16 fh_area[19], fh_pos[19];
    uint8 fh_entrance[19];
    for (int i = 0; i < 19; i++) {
      fh_pos[i] = rom_get_word(0x9BB800 + i * 2) + 0x400;
      fh_area[i] = rom_get_word(0x9BB826 + i * 2);
      fh_entrance[i] = rom_get_byte(0x9BB84C + i);
    }
    assets_add_words(a, "kFallHole_Area", fh_area, 19);
    assets_add_words(a, "kFallHole_Pos", fh_pos, 19);
    assets_add(a, "kFallHole_Entrances", fh_entrance, 19);
  }

  // Exit data (79 entries)
  {
    uint8 exit_screen[79];
    uint16 exit_rooms[79], exit_loadoffs[79], exit_scrollx[79], exit_scrolly[79];
    uint16 exit_posx[79], exit_posy[79], exit_camerax[79], exit_cameray[79];
    uint16 exit_ndoor[79], exit_fdoor[79];
    int8 exit_unk1[79], exit_unk3[79];

    for (int i = 0; i < 79; i++) {
      exit_rooms[i] = rom_get_word(0x82DD8A + i * 2);
      exit_screen[i] = rom_get_byte(0x82DE28 + i);
      exit_loadoffs[i] = rom_get_word(0x82DE77 + i * 2);
      exit_scrolly[i] = rom_get_word(0x82DF15 + i * 2);
      exit_scrollx[i] = rom_get_word(0x82DFB3 + i * 2);
      exit_posy[i] = rom_get_word(0x82E051 + i * 2);
      exit_posx[i] = rom_get_word(0x82E0EF + i * 2);
      exit_cameray[i] = rom_get_word(0x82E18D + i * 2);
      exit_camerax[i] = rom_get_word(0x82E22B + i * 2);
      exit_unk1[i] = rom_get_int8(0x82E2C9 + i);
      exit_unk3[i] = rom_get_int8(0x82E318 + i);
      exit_ndoor[i] = rom_get_word(0x82E367 + i * 2);
      exit_fdoor[i] = rom_get_word(0x82E405 + i * 2);
    }

    assets_add(a, "kExitData_ScreenIndex", exit_screen, 79);
    assets_add_words(a, "kExitDataRooms", exit_rooms, 79);
    assets_add_words(a, "kExitData_Map16LoadSrcOff", exit_loadoffs, 79);
    assets_add_words(a, "kExitData_ScrollX", exit_scrollx, 79);
    assets_add_words(a, "kExitData_ScrollY", exit_scrolly, 79);
    assets_add_words(a, "kExitData_XCoord", exit_posx, 79);
    assets_add_words(a, "kExitData_YCoord", exit_posy, 79);
    assets_add_words(a, "kExitData_CameraXScroll", exit_camerax, 79);
    assets_add_words(a, "kExitData_CameraYScroll", exit_cameray, 79);
    assets_add_words(a, "kExitData_NormalDoor", exit_ndoor, 79);
    assets_add_words(a, "kExitData_FancyDoor", exit_fdoor, 79);
    assets_add_int8s(a, "kExitData_Unk1", exit_unk1, 79);
    assets_add_int8s(a, "kExitData_Unk3", exit_unk3, 79);
  }

  // Special exits (16 entries for rooms 0x180-0x18F)
  {
    uint16 sp_top[16], sp_bottom[16], sp_left[16], sp_right[16];
    int16 sp_tab4[16], sp_tab5[16], sp_tab6[16], sp_tab7[16];
    uint16 sp_leftedge[16];
    uint8 sp_dir[16], sp_sprgfx[16], sp_auxgfx[16], sp_palbg[16], sp_palspr[16];

    for (int i = 0; i < 16; i++) {
      sp_top[i] = rom_get_word(0x82E6E1 + i * 2);
      sp_bottom[i] = rom_get_word(0x82E701 + i * 2);
      sp_left[i] = rom_get_word(0x82E721 + i * 2);
      sp_right[i] = rom_get_word(0x82E741 + i * 2);
      sp_tab4[i] = (int16)rom_get_word(0x82E761 + i * 2);
      sp_tab5[i] = (int16)rom_get_word(0x82E7A1 + i * 2);
      sp_tab6[i] = (int16)rom_get_word(0x82E781 + i * 2);
      sp_tab7[i] = (int16)rom_get_word(0x82E7C1 + i * 2);
      sp_leftedge[i] = rom_get_word(0x82E7E1 + i * 2);
      sp_dir[i] = rom_get_byte(0x82E801 + i);
      sp_sprgfx[i] = rom_get_byte(0x82E811 + i);
      sp_auxgfx[i] = rom_get_byte(0x82E821 + i);
      sp_palbg[i] = rom_get_byte(0x82E831 + i);
      sp_palspr[i] = rom_get_byte(0x82E841 + i);
    }

    assets_add_words(a, "kSpExit_Top", sp_top, 16);
    assets_add_words(a, "kSpExit_Bottom", sp_bottom, 16);
    assets_add_words(a, "kSpExit_Left", sp_left, 16);
    assets_add_words(a, "kSpExit_Right", sp_right, 16);
    assets_add_int16s(a, "kSpExit_Tab4", sp_tab4, 16);
    assets_add_int16s(a, "kSpExit_Tab5", sp_tab5, 16);
    assets_add_int16s(a, "kSpExit_Tab6", sp_tab6, 16);
    assets_add_int16s(a, "kSpExit_Tab7", sp_tab7, 16);
    assets_add_words(a, "kSpExit_LeftEdgeOfMap", sp_leftedge, 16);
    assets_add(a, "kSpExit_Dir", sp_dir, 16);
    assets_add(a, "kSpExit_SprGfx", sp_sprgfx, 16);
    assets_add(a, "kSpExit_AuxGfx", sp_auxgfx, 16);
    assets_add(a, "kSpExit_PalBg", sp_palbg, 16);
    assets_add(a, "kSpExit_PalSpr", sp_palspr, 16);
  }

  // Overworld secrets
  {
    DynBuf sdata;
    db_init(&sdata);
    uint16 secret_offs[128];
    memset(secret_offs, 0xFF, sizeof(secret_offs)); // sentinel

    for (int area = 0; area < 128; area++) {
      if (!is_area_head(area)) continue;
      uint32 ea = 0x9B0000 | rom_get_word(0x9BC2F9 + area * 2);
      if (rom_get_word(ea) == 0xFFFF) continue;

      uint16 off = (uint16)sdata.size;
      awrite_u16(secret_offs, area, area, off, is_small);

      while (rom_get_word(ea) != 0xFFFF) {
        db_append_byte(&sdata, rom_get_byte(ea));
        db_append_byte(&sdata, rom_get_byte(ea + 1));
        db_append_byte(&sdata, rom_get_byte(ea + 2));
        ea += 3;
      }
      db_append_byte(&sdata, 0xFF);
      db_append_byte(&sdata, 0xFF);
    }

    // Fill unset offsets
    uint16 empty_off = sdata.size >= 2 ? (uint16)(sdata.size - 2) : 0;
    for (int i = 0; i < 128; i++) {
      if (secret_offs[i] == 0xFFFF)
        secret_offs[i] = empty_off;
    }

    assets_add_words(a, "kOverworldSecrets_Offs", secret_offs, 128);
    assets_add_move(a, "kOverworldSecrets", sdata.data, sdata.size);
    sdata.data = NULL;
  }

  // Overworld sprites
  {
    uint16 sprite_offs[144 * 3];
    memset(sprite_offs, 0, sizeof(sprite_offs));

    DynBuf sprite_data;
    db_init(&sprite_data);
    db_append_byte(&sprite_data, 0xFF); // sentinel

    uint8 sprite_gfx[256];
    uint8 sprite_pal[256];
    rom_get_bytes(0x80FA41, sprite_gfx, 256);
    rom_get_bytes(0x80FB41, sprite_pal, 256);

    // Helper to extract one sprite set
    // base_addr is the pointer table base for this sprite set
    // areas is the range to process
    // stage_idxs is which stage slots to update in sprite_offs
    // info_stage is the gfx/pal index stage (0-3)
    struct { uint32 base; int start; int end; int stages[3]; int num_stages; int info_stage; } ranges[] = {
      {0x89C881, 0, 64, {0, -1, -1}, 1, 0}, // Beginning
      {0x89C901, 0, 64, {1, -1, -1}, 1, 1}, // FirstPart
      {0x89CA21, 0, 64, {2, -1, -1}, 1, 2}, // SecondPart (areas 0-63)
      {0x89CA21, 64, 144, {1, 2, -1}, 2, 3}, // Sprites (areas 64-143)
    };

    for (int r = 0; r < 4; r++) {
      for (int area = ranges[r].start; area < ranges[r].end; area++) {
        if (!is_area_head(area)) continue;
        uint32 ea = 0x890000 | rom_get_word(ranges[r].base + area * 2);
        if (rom_get_byte(ea) == 0xFF) continue;

        uint16 off = (uint16)sprite_data.size;
        for (int s = 0; s < ranges[r].num_stages; s++) {
          int stage = ranges[r].stages[s];
          sprite_offs[stage * 144 + area] = off;
          if (area < 128 && !is_small[area]) {
            sprite_offs[stage * 144 + area + 1] = off;
            sprite_offs[stage * 144 + area + 8] = off;
            sprite_offs[stage * 144 + area + 9] = off;
          }
        }

        while (rom_get_byte(ea) != 0xFF) {
          db_append_byte(&sprite_data, rom_get_byte(ea));
          db_append_byte(&sprite_data, rom_get_byte(ea + 1));
          db_append_byte(&sprite_data, rom_get_byte(ea + 2));
          ea += 3;
        }
        db_append_byte(&sprite_data, 0xFF);
      }
    }

    assets_add_words(a, "kOverworldSpriteOffs", sprite_offs, 144 * 3);
    assets_add_move(a, "kOverworldSprites", sprite_data.data, sprite_data.size);
    sprite_data.data = NULL;
    assets_add(a, "kOverworldSpriteGfx", sprite_gfx, 256);
    assets_add(a, "kOverworldSpritePalettes", sprite_pal, 256);
  }

  // Final assets
  {
    uint8 *buf = (uint8 *)malloc(512);
    rom_get_bytes(0x8E9459, buf, 512);
    assets_add_move(a, "kMap8DataToTileAttr", buf, 512);
  }
  {
    uint8 *buf = (uint8 *)malloc(3824);
    rom_get_bytes(0x9BF110, buf, 3824);
    assets_add_move(a, "kSomeTileAttr", buf, 3824);
  }
}

// ================================================================
//  Asset file writer
// ================================================================

static bool write_asset_file(AssetList *a, const char *output_path) {
  // Build key_sig: concatenation of all asset names with null terminators
  DynBuf key_sig;
  db_init(&key_sig);
  for (int i = 0; i < a->count; i++) {
    size_t nlen = strlen(a->names[i]);
    db_append(&key_sig, (const uint8 *)a->names[i], nlen + 1);
  }

  // Build header
  // 16 bytes: "Zelda3_v0     \n\0"
  // 32 bytes: SHA-256 of key_sig
  // 32 bytes: zeros
  // 4 bytes: uint32 asset_count
  // 4 bytes: uint32 key_sig_length
  // uint32[count]: sizes
  // key_sig bytes
  // For each asset: 4-byte aligned data

  DynBuf file;
  db_init(&file);

  // Header string (16 bytes)
  const uint8 hdr_str[16] = {'Z','e','l','d','a','3','_','v','0',' ',' ',' ',' ',' ','\n','\0'};
  db_append(&file, hdr_str, 16);

  // SHA-256 of key_sig
  uint8 sig_hash[32];
  sha256(key_sig.data, key_sig.size, sig_hash);
  db_append(&file, sig_hash, 32);

  // 32 bytes zeros
  uint8 zeros[32];
  memset(zeros, 0, 32);
  db_append(&file, zeros, 32);

  // asset count and key_sig length (little-endian uint32)
  db_append_u32le(&file, (uint32)a->count);
  db_append_u32le(&file, (uint32)key_sig.size);

  // Sizes array
  for (int i = 0; i < a->count; i++)
    db_append_u32le(&file, (uint32)a->sizes[i]);

  // Key sig
  db_append(&file, key_sig.data, key_sig.size);

  // Asset data (4-byte aligned)
  for (int i = 0; i < a->count; i++) {
    // Align to 4 bytes
    while (file.size & 3)
      db_append_byte(&file, 0);
    if (a->sizes[i] > 0)
      db_append(&file, a->data[i], a->sizes[i]);
  }

  // Write to file
  FILE *f = fopen(output_path, "wb");
  if (!f) {
    snprintf(g_error, sizeof(g_error), "Cannot write to %s", output_path);
    db_free(&file);
    db_free(&key_sig);
    return false;
  }
  fwrite(file.data, 1, file.size, f);
  fclose(f);

  db_free(&file);
  db_free(&key_sig);
  return true;
}

// ================================================================
//  Public API
// ================================================================

bool AssetExtract_BuildFromROM(const uint8 *rom, size_t rom_size,
                               const char *output_path) {
  g_error[0] = 0;

  // Identify ROM
  const char *lang = AssetExtract_IdentifyROM(rom, rom_size);
  if (!lang) {
    snprintf(g_error, sizeof(g_error),
             "Unrecognized ROM. Please provide a valid "
             "Zelda 3: A Link to the Past SNES ROM.");
    return false;
  }
  if (strcmp(lang, "us") != 0) {
    snprintf(g_error, sizeof(g_error),
             "Expected US ROM but got '%s'. "
             "Please provide the US version first.", lang);
    return false;
  }

  // Strip SMC header and set up ROM access
  size_t stripped_size = rom_size;
  g_rom = StripSMCHeader(rom, &stripped_size);
  g_rom_size = stripped_size;

  printf("Extracting assets from ROM (%zu bytes)...\n", g_rom_size);

  AssetList assets;
  assets_init(&assets);

  // Extract all 165 assets in the correct order
  extract_sound_banks(&assets);        // 0-2
  extract_dungeon_rooms(&assets);      // 3-55
  extract_enemy_damage(&assets);       // 56
  extract_link_graphics(&assets);      // 57
  extract_dungeon_sprites(&assets);    // 58-59
  extract_map32_to_map16(&assets);     // 60-63
  extract_sprite_gfx(&assets);        // 64
  extract_bg_gfx(&assets);            // 65
  extract_misc(&assets);               // 66-93
  extract_dialogue(&assets);           // 94-96
  extract_dungeon_map(&assets);        // 97-98
  extract_tilemaps(&assets);           // 99-104
  extract_overworld_compressed(&assets); // 105-106
  extract_overworld_tables(&assets);   // 107-164

  if (assets.count != 165) {
    snprintf(g_error, sizeof(g_error),
             "Internal error: expected 165 assets but built %d", assets.count);
    assets_free(&assets);
    return false;
  }

  printf("Writing %d assets to %s...\n", assets.count, output_path);
  bool ok = write_asset_file(&assets, output_path);
  assets_free(&assets);

  if (ok)
    printf("Asset extraction complete.\n");

  return ok;
}

bool AssetExtract_AddLanguage(const uint8 *us_rom, size_t us_rom_size,
                              const uint8 *lang_rom, size_t lang_rom_size,
                              const char *output_path) {
  g_error[0] = 0;
  snprintf(g_error, sizeof(g_error),
           "Language ROM support is not yet implemented in native extraction. "
           "Only US ROM extraction is available.");
  return false;
}
