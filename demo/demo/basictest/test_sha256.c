//
// Created by zhangjiantao on 26-5-8.
//

#include <dev/console.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
} SHA256_CTX;

void t_sha256(SHA256_CTX *ctx);
void t_sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void t_sha256_final(SHA256_CTX *ctx, uint8_t hash[]);

#define ROTRIGHT(word, bits) (((word) >> (bits)) | ((word) << (32 - (bits))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))
#define EP1(x) (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))
#define SIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
  uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

  for (i = 0, j = 0; i < 16; ++i, j += 4)
    m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) |
           (data[j + 3]);
  for (; i < 64; ++i)
    m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];

  for (i = 0; i < 64; ++i) {
    t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
    t2 = EP0(a) + MAJ(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

void t_sha256(SHA256_CTX *ctx) {
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
}

void t_sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
  for (size_t i = 0; i < len; ++i) {
    ctx->data[ctx->datalen] = data[i];
    ctx->datalen++;
    if (ctx->datalen == 64) {
      sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

void t_sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
  uint32_t i = ctx->datalen;

  if (ctx->datalen < 56) {
    ctx->data[i++] = 0x80;
    while (i < 56)
      ctx->data[i++] = 0x00;
  } else {
    ctx->data[i++] = 0x80;
    while (i < 64)
      ctx->data[i++] = 0x00;
    sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }

  ctx->bitlen += ctx->datalen * 8;
  ctx->data[63] = ctx->bitlen;
  ctx->data[62] = ctx->bitlen >> 8;
  ctx->data[61] = ctx->bitlen >> 16;
  ctx->data[60] = ctx->bitlen >> 24;
  ctx->data[59] = ctx->bitlen >> 32;
  ctx->data[58] = ctx->bitlen >> 40;
  ctx->data[57] = ctx->bitlen >> 48;
  ctx->data[56] = ctx->bitlen >> 56;
  sha256_transform(ctx, ctx->data);

  for (i = 0; i < 4; ++i) {
    hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
  }
}

typedef struct {
  const char *title;    // 描述
  const char *input;    // 输入字符串
  const char *expected; // 预期的十六进制字符串
} TestCase;

// 将字节数组转换为十六进制字符串
void bytes_to_hex(const uint8_t *hash, char *output) {
  for (int i = 0; i < 32; i++) {
    sprintf(&output[i * 2], "%02x", hash[i]);
  }
  output[64] = '\0';
}

void run_tests() {
  TestCase tests[] = {
      {"Empty String", "",
       "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"Single 'abc'", "abc",
       "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
      {"Quick Brown Fox", "The quick brown fox jumps over the lazy dog",
       "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"},
      {"Single Space", " ",
       "36a9e7f1c95b82ffb99743e0c5c4ce95d83c9a430aac59f84ef3cbfab6145068"},
      {"55 Bytes (Boundary)",
       "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnop",
       "aa353e009edbaebfc6e494c8d847696896cb8b398e0173a4b5c1b636292d87c7"},
      {"64 Bytes (Full Block)",
       "1234567890123456789012345678901234567890123456789012345678901234",
       "676491965ed3ec50cb7a63ee96315480a95c54426b0b72bca8a0d4ad1285ad55"},
      {"64 'a's",
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
       "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"}};

  int num_tests = sizeof(tests) / sizeof(TestCase);
  int passed = 0;

  printf("=== SHA-256 Automation Test Suite ===\n\n");

  for (int i = 0; i < num_tests; i++) {
    SHA256_CTX ctx;
    uint8_t hash[32];
    char hex_res[65];

    t_sha256(&ctx);
    t_sha256_update(&ctx, (uint8_t *)tests[i].input, strlen(tests[i].input));
    t_sha256_final(&ctx, hash);

    bytes_to_hex(hash, hex_res);

    printf("[%d/%d] Test: %-30s", i + 1, num_tests, tests[i].title);
    if (strcmp(hex_res, tests[i].expected) == 0) {
      tcm_ascii_console_set_color(0, 0, 0, 0, 0xff, 0);
      printf("  Result: PASS\n");
      passed++;
    } else {
      tcm_ascii_console_set_color(0, 0, 0, 0xff, 0, 0);
      printf("  Result: FAIL\n");
      printf("  Expected: %s\n", tests[i].expected);
      printf("  Actual:   %s\n", hex_res);
    }
    tcm_ascii_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);
    printf("-------------------------------------------------------------------"
           "-----\n");
  }

  printf("\nFinal Result: %d/%d Passed.\n", passed, num_tests);
}

#ifdef TESTALL
int test_sha256_main(void) {
#else
int main(int argc, char **argv) {
#endif
  run_tests();
  return 0;
}
