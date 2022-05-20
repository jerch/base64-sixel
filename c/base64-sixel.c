#include <stdint.h>
#include "base64-sixel.h"

static const char STAND2SIXEL[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,125,  0,  0,  0,126,
  115,116,117,118,119,120,121,122,123,124,  0,  0,  0,  0,  0,  0,
    0, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
   78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,  0,  0,  0,  0,  0,
    0, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,
  104,105,106,107,108,109,110,111,112,113,114,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

// FIXME: pre-initialize and make const
static uint32_t ENC_MASK1[256] = {0};
static uint32_t ENC_MASK2[256] = {0};
static uint32_t ENC_MASK3[256] = {0};

// FIXME: to be removed
static inline void init_masks() {
  uint8_t temp[4];
  uint32_t accu_int = 0;
  uint8_t *accu = (uint8_t *) &accu_int;
  for (int i = 0; i < 256; ++i) {
    accu[2] = i;
    temp[3] = accu_int;
    temp[2] = accu_int >> 6;
    temp[1] = accu_int >> 12;
    temp[0] = accu_int >> 18;
    ENC_MASK1[i] = (*(uint32_t *) temp) & 0x3F3F3F3F;
  }
  accu_int = 0;
  for (int i = 0; i < 256; ++i) {
    accu[1] = i;
    temp[3] = accu_int;
    temp[2] = accu_int >> 6;
    temp[1] = accu_int >> 12;
    temp[0] = accu_int >> 18;
    ENC_MASK2[i] = (*(uint32_t *) temp) & 0x3F3F3F3F;
  }
  accu_int = 0;
  for (int i = 0; i < 256; ++i) {
    accu[0] = i;
    temp[3] = accu_int;
    temp[2] = accu_int >> 6;
    temp[1] = accu_int >> 12;
    temp[0] = accu_int >> 18;
    ENC_MASK3[i] = (*(uint32_t *) temp) & 0x3F3F3F3F;
  }
}

#ifdef USE_SIMD
ssize_t base64sixel_encode(const uint8_t *src, size_t srclen, const uint8_t *dst) {
  __m128i *dst_v = (__m128i*) dst;
  unsigned char *inp = src;
  unsigned char *inp_end = src + srclen;
  while (inp + 12 <= inp_end) {
    __m128i v1 = _mm_loadu_si128((__m128i *) inp);
    __m128i v2 = _mm_shuffle_epi8(v1, _mm_set_epi8(
      10, 11,  9, 10,
       7,  8,  6,  7,
       4,  5,  3,  4,
       1,  2,  0,  1
    ));

    #ifdef __EMSCRIPTEN__
    // faster on wasm:
    __m128i index_a = _mm_and_si128(_mm_srli_epi32(v2, 10), _mm_set1_epi32(0x0000003f));
    __m128i index_b = _mm_and_si128(_mm_slli_epi32(v2, 4),  _mm_set1_epi32(0x00003f00));
    __m128i index_c = _mm_and_si128(_mm_srli_epi32(v2, 6),  _mm_set1_epi32(0x003f0000));
    __m128i index_d = _mm_and_si128(_mm_slli_epi32(v2, 8),  _mm_set1_epi32(0x3f000000));
    __m128i a_b = _mm_or_si128(index_a, index_b);
    __m128i c_d = _mm_or_si128(index_c, index_d);
    __m128i indices = _mm_or_si128(a_b, c_d);
    #else
    // slightly faster on native (but worse than scalar on wasm due to costly mul emulation):
    __m128i t0 = _mm_and_si128(v2, _mm_set1_epi32(0x0fc0fc00));
    __m128i t1 = _mm_mulhi_epu16(t0, _mm_set1_epi32(0x04000040));
    __m128i t2 = _mm_and_si128(v2, _mm_set1_epi32(0x003f03f0));
    __m128i t3 = _mm_mullo_epi16(t2, _mm_set1_epi32(0x01000010));
    __m128i indices = _mm_or_si128(t1, t3);
    #endif

    __m128i result = _mm_add_epi8(indices, _mm_set1_epi8(63));
    _mm_storeu_si128(dst_v++, result);
    inp += 12;
  }
  // TODO: cleanup mess below
  unsigned char *c = inp;
  unsigned char *c_end = src + srclen;
  unsigned int *dst_int = (unsigned int *) dst_v;
  if (c < c_end) {
    if (!ENC_MASK1[1]) init_masks();
    for (; c + 3 <= c_end; c += 3) {
      *dst_int++ = (ENC_MASK1[*(c+0)] | ENC_MASK2[*(c+1)]  | ENC_MASK3[*(c+2)] ) + 0x3F3F3F3F;
    }
    if (c < c_end) {
      int p = c_end - c;
      unsigned char *d = (unsigned char *) dst_int;
      unsigned int accu = (ENC_MASK1[*(c+0)] | ENC_MASK2[p == 2 ? *(c+1) : 0]) + 0x3F3F3F3F;
      *d++ = accu & 0xFF;
      *d++ = (accu >> 8) & 0xFF;
      if (p == 2) {
        *d++ = (accu >> 16) & 0xFF;
      }
      dst_int = (unsigned int *) d;
    }
    dst_v = (__m128i*) dst_int;
  }
  return ((unsigned char *) dst_v) - dst;
}
#else
ssize_t base64sixel_encode(const uint8_t *src, size_t srclen, const uint8_t *dst) {
  if (!ENC_MASK1[1]) init_masks();  // to be removed...
  uint32_t *dst4 = (uint32_t *) dst;
  uint8_t *c = (uint8_t *) src;
  uint8_t *c_end = c + srclen;
  for (; c + 12 <= c_end; c += 12) { // check for unroll/autovectorize
    *dst4++ = (ENC_MASK1[*(c+0)] | ENC_MASK2[*(c+1)]  | ENC_MASK3[*(c+2)] ) + 0x3F3F3F3F;
    *dst4++ = (ENC_MASK1[*(c+3)] | ENC_MASK2[*(c+4)]  | ENC_MASK3[*(c+5)] ) + 0x3F3F3F3F;
    *dst4++ = (ENC_MASK1[*(c+6)] | ENC_MASK2[*(c+7)]  | ENC_MASK3[*(c+8)] ) + 0x3F3F3F3F;
    *dst4++ = (ENC_MASK1[*(c+9)] | ENC_MASK2[*(c+10)] | ENC_MASK3[*(c+11)]) + 0x3F3F3F3F;
  }
  if (c < c_end) {
    for (; c + 3 <= c_end; c += 3) {
      *dst4++ = (ENC_MASK1[*(c+0)] | ENC_MASK2[*(c+1)] | ENC_MASK3[*(c+2)]) + 0x3F3F3F3F;
    }
    if (c < c_end) {
      int p = c_end - c;
      uint8_t *d = (uint8_t *) dst4;
      uint32_t accu = (ENC_MASK1[*(c+0)] | ENC_MASK2[p == 2 ? *(c+1) : 0]) + 0x3F3F3F3F;
      *d++ = accu & 0xFF;
      *d++ = (accu >> 8) & 0xFF;
      if (p == 2) {
        *d++ = (accu >> 16) & 0xFF;
      }
      dst4 = (uint32_t *) d;
    }
  }
  return (uint8_t *) dst4 - dst;
}
#endif

#ifdef USE_SIMD
int _decode_tail(unsigned char *cc, unsigned char *dst, int length, uint8_t *src_orig, uint8_t *dst_orig) {
  unsigned int *inp = (unsigned int *) cc;
  unsigned char temp[4];
  unsigned char accu[4];
  int l = length >> 2;
  while (l--) {
    if ((*((int *) temp) = *inp++ - 0x3F3F3F3F) & 0xC0C0C0C0) {
      inp--;
      break;
    } else {
      *((int *) accu) = temp[3] | temp[2]<<6 | temp[1]<<12 | temp[0]<<18;
      *dst++ = accu[2];
      *dst++ = accu[1];
      *dst++ = accu[0];
    }
  }
  // error and tail handling
  if ((unsigned char *) inp < cc + length) {
    *((int *) temp) = 0;
    unsigned char *c = (unsigned char *) inp;
    int end = cc + length - c;
    int p = 0;
    for (;p < 4 && p < end; ++p) {
      if ((temp[p] = c[p] - 63) & 0xC0) {
        // error: non sixel char
        return -(c+p+1 - src_orig);
      }
    }
    // tail
    if (p == 1) {
      // just one tail char is treated as error
      // (check what RFC says about it)
      return ~length;
    }
    *((int *) accu) = temp[2]<<6 | temp[1]<<12 | temp[0]<<18;
    *dst++ = accu[2];
    if (p == 3) {
      *dst++ = accu[1];
    }
  }
  return dst - dst_orig;
}
ssize_t base64sixel_decode(const uint8_t *src, size_t srclen, const uint8_t *dst) {
  uint8_t *d = (uint8_t *) dst;
  __m128i *inp = (__m128i*) src;
  __m128i error = _mm_setzero_si128();
  int l = srclen >> 4;
  while (l--) {
    __m128i v1 = _mm_loadu_si128(inp++);
    // subtract 63
    __m128i v2 = _mm_add_epi8(v1, _mm_set1_epi8(-63));
    // error detection: just aggregate, eval afterwards
    __m128i e1 = _mm_and_si128(v2, _mm_set1_epi8(0xC0));
    error = _mm_or_si128(error, e1);
    // merge 4x6 bits to 3x8
    __m128i v3 = _mm_maddubs_epi16(v2, _mm_set1_epi32(0x01400140));
    __m128i v4 = _mm_madd_epi16(v3, _mm_set1_epi32(0x00011000));
    // merge into 12 bytes LE
    __m128i v5 = _mm_shuffle_epi8(v4, _mm_set_epi8(
      16, 16, 16, 16,
      12, 13, 14,  8,
       9, 10,  4,  5,
       6, 0,   1,  2
    ));
    _mm_storeu_si128((__m128i *) d, v5);
    d += 12;
  }
  // postponed error handling: all lanes in error should be zero
  if (_mm_movemask_epi8(_mm_cmpeq_epi8(error, _mm_setzero_si128())) != 0xFFFF) {
    // there was an error somewhere in the data,
    // thus we redo decoding in scalar to find error position
    // this penalizes bad cases, good data can run at full speed
    return _decode_tail(src, dst, srclen, src, dst);
  }
  if ((uint8_t *) inp < src + srclen) {
    return _decode_tail((uint8_t *) inp, d, srclen - ((srclen >> 4) << 4), src, dst);
  }
  return d - dst;

}
#else
ssize_t base64sixel_decode(const uint8_t *src, size_t srclen, const uint8_t *dst) {
  uint8_t *d = (uint8_t *) dst;
  uint32_t *inp = (uint32_t *) src;
  uint8_t temp[4];
  uint8_t accu[4];
  int l = srclen >> 2;
  while (l--) {
    if ((*((uint32_t *) temp) = *inp++ - 0x3F3F3F3F) & 0xC0C0C0C0) {
      inp--;
      break;
    } else {
      *((uint32_t *) accu) = temp[3] | temp[2]<<6 | temp[1]<<12 | temp[0]<<18;
      *d++ = accu[2];
      *d++ = accu[1];
      *d++ = accu[0];
    }
  }
  // error and tail handling
  if ((uint8_t *) inp < src + srclen) {
    *((int *) temp) = 0;
    uint8_t *c = (uint8_t *) inp;
    int end = src + srclen - c;
    int p = 0;
    for (;p < 4 && p < end; ++p) {
      if ((temp[p] = c[p] - 63) & 0xC0) {
        // error: non sixel char
        return -(c+p+1 - src);
      }
    }
    // tail
    if (p == 1) {
      // just one tail char is treated as error
      // (check what RFC says about it)
      return ~srclen;
    }
    *((uint32_t *) accu) = temp[2]<<6 | temp[1]<<12 | temp[0]<<18;
    *d++ = accu[2];
    if (p == 3) {
      *d++ = accu[1];
    }
  }
  return d - dst;
}
#endif

ssize_t base64sixel_transcode(const uint8_t *src, size_t srclen, const uint8_t *dst) {
  uint8_t *c = (uint8_t *) src;
  uint8_t *c_end8 = c + srclen - 8;
  uint8_t *d = (uint8_t *) dst;
  while (c < c_end8) {
    if (
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++]) ||
      !(*d++ = STAND2SIXEL[*c++])
    ) {
      d--;
      uint8_t v = *(c - 1);
      // exit early on first padding char =
      if (v == 61) return d - dst;
      // skip SP, CR and LF, negative number as error indicator (~position in chunk)
      if (!(v == 32 || v == 13 || v == 10)) return -(c - src);
    }
  }
  // handle tail
  uint8_t *c_end = (uint8_t *) src + srclen;
  while (c < c_end) {
    if (!(*d++ = STAND2SIXEL[*c++])) {
      d--;
      uint8_t v = *(c - 1);
      if (v == 61) return d - dst;
      if (!(v == 32 || v == 13 || v == 10)) return -(c - src);
    }
  }
  return d - dst;
}

// TODO: stream interface?
