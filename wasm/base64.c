/**
 * Base64-sixel for WASM.
 * 
 * Copyright (c) 2022 Joerg Breitbart.
 * @license MIT
 */


/**
 * TODO:
 * - encode: cleanup SIMD, more tests
 * - wasm build:
 *   - separate scalar and SIMD builds
 * - native build:
 *   - pointered interface (no static memory)
 *   - makefile + header
 */


// cmdline overridable defines
#ifndef CHUNK_SIZE
  #define CHUNK_SIZE 4096
#endif

/** check SIMD support (build with -msse -msse2 -mssse3 -msse4.1) */
#if defined(__SSE__) && defined(__SSE2__) && defined(__SSSE3__) && defined(__SSE4_1__)
#define USE_SIMD 1
#include <immintrin.h>
#endif

//#define USE_SIMD 1
//#include <immintrin.h>


/** operate on static memory for wasm */
static unsigned char CHUNK[CHUNK_SIZE+1] __attribute__((aligned(16)));
static unsigned char TARGET[CHUNK_SIZE] __attribute__((aligned(16)));

const int ENCODE_LIMIT = CHUNK_SIZE / 4 * 3;
const int DECODE_LIMIT = CHUNK_SIZE;
const int TRANSCODE_LIMIT = CHUNK_SIZE;


// exported functions
#ifdef __cplusplus
extern "C" {
#endif
  void* get_chunk_address() { return &CHUNK[0]; }
  void* get_target_address() { return &TARGET[0]; }
  int encode(int length);
  int decode(int length);
  int transcode(int length);
#ifdef __cplusplus
}
#endif


static char STAND2SIXEL[] = {
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

static unsigned int ENC_MASK1[256] = {0};
static unsigned int ENC_MASK2[256] = {0};
static unsigned int ENC_MASK3[256] = {0};

static inline void init_masks() {
  unsigned char temp[4];
  unsigned int accu_int = 0;
  unsigned char *accu = (unsigned char *) &accu_int;
  for (int i = 0; i < 256; ++i) {
    accu[2] = i;
    temp[3] = accu_int;
    temp[2] = accu_int >> 6;
    temp[1] = accu_int >> 12;
    temp[0] = accu_int >> 18;
    ENC_MASK1[i] = (*(int *) temp) & 0x3F3F3F3F;
  }
  accu_int = 0;
  for (int i = 0; i < 256; ++i) {
    accu[1] = i;
    temp[3] = accu_int;
    temp[2] = accu_int >> 6;
    temp[1] = accu_int >> 12;
    temp[0] = accu_int >> 18;
    ENC_MASK2[i] = (*(int *) temp) & 0x3F3F3F3F;
  }
  accu_int = 0;
  for (int i = 0; i < 256; ++i) {
    accu[0] = i;
    temp[3] = accu_int;
    temp[2] = accu_int >> 6;
    temp[1] = accu_int >> 12;
    temp[0] = accu_int >> 18;
    ENC_MASK3[i] = (*(int *) temp) & 0x3F3F3F3F;
  }
}


/**
 * @brief Encode bytes in CHUNK to base64-sixel characters.
 *
 * Loaded bytes may never exceed ENCODE_LIMIT (no bound checks done).
 *
 * @param length  Amount of bytes loaded in CHUNK to be encoded.
 * @return int    Number of characters written to TARGET.
 */
int encode(int length) {

#ifdef USE_SIMD

  __m128i *dst_v = (__m128i*) TARGET;
  unsigned char *inp = CHUNK;
  unsigned char *inp_end = CHUNK + length;
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
    _mm_store_si128(dst_v++, result);
    inp += 12;
  }
  // TODO: cleanup mess below
  unsigned char *c = inp;
  unsigned char *c_end = CHUNK + length;
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
  return ((unsigned char *) dst_v) - TARGET;

#else

  if (!ENC_MASK1[1]) init_masks();
  unsigned int *dst = (unsigned int *) TARGET;
  unsigned char *c = CHUNK;
  unsigned char *c_end = c + length;
  for (; c + 12 <= c_end; c += 12) {
    *dst++ = (ENC_MASK1[*(c+0)] | ENC_MASK2[*(c+1)]  | ENC_MASK3[*(c+2)] ) + 0x3F3F3F3F;
    *dst++ = (ENC_MASK1[*(c+3)] | ENC_MASK2[*(c+4)]  | ENC_MASK3[*(c+5)] ) + 0x3F3F3F3F;
    *dst++ = (ENC_MASK1[*(c+6)] | ENC_MASK2[*(c+7)]  | ENC_MASK3[*(c+8)] ) + 0x3F3F3F3F;
    *dst++ = (ENC_MASK1[*(c+9)] | ENC_MASK2[*(c+10)] | ENC_MASK3[*(c+11)]) + 0x3F3F3F3F;
  }
  if (c < c_end) {
    for (; c + 3 <= c_end; c += 3) {
      *dst++ = (ENC_MASK1[*(c+0)] | ENC_MASK2[*(c+1)]  | ENC_MASK3[*(c+2)] ) + 0x3F3F3F3F;
    }
    if (c < c_end) {
      int p = c_end - c;
      unsigned char *d = (unsigned char *) dst;
      unsigned int accu = (ENC_MASK1[*(c+0)] | ENC_MASK2[p == 2 ? *(c+1) : 0]) + 0x3F3F3F3F;
      *d++ = accu & 0xFF;
      *d++ = (accu >> 8) & 0xFF;
      if (p == 2) {
        *d++ = (accu >> 16) & 0xFF;
      }
      dst = (unsigned int *) d;
    }
  }
  return (unsigned char *) dst - TARGET;

#endif

}


#ifdef USE_SIMD

/**
 * @brief Decode helper for SIMD path to do tail decoding and error handling.
 *
 * @param c       Pointer to current read position in CHUNK.
 * @param dst     Pointer to current write position in TARGET.
 * @param length  Number of characters left in CHUNK to be handled.
 * @return int    Number of bytes written to TARGET, or error code.
 */
int _decode_tail(unsigned char *cc, unsigned char *dst, int length) {
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
        return -(c+p+1 - CHUNK);
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
  return dst - TARGET;
}

#endif


/**
 * @brief Decode base64-sixel characters loaded in CHUNK.
 *
 * Uses either scalar or SIMD version, depending on compile settings.
 *
 * @param length  Number of characters loaded in CHUNK to be decoded.
 * @return int    Number of bytes written to TARGET, or error code.
 */
int decode(int length) {

#ifdef USE_SIMD

  unsigned char *dst = TARGET;
  __m128i *inp = (__m128i*) CHUNK;
  __m128i error = _mm_setzero_si128();
  int l = length >> 4;
  while (l--) {
    __m128i v1 = _mm_load_si128(inp++);
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
    _mm_storeu_si128((__m128i *) dst, v5);
    dst += 12;
  }
  // postponed error handling: all lanes in error should be zero
  if (_mm_movemask_epi8(_mm_cmpeq_epi8(error, _mm_setzero_si128())) != 0xFFFF) {
    // there was an error somewhere in the data,
    // thus we redo decoding in scalar to find error position
    // this penalizes bad cases, good data can run at full speed
    return _decode_tail(CHUNK, TARGET, length);
  }
  if ((unsigned char *) inp < CHUNK + length) {
    return _decode_tail((unsigned char *) inp, dst, length - ((length >> 4) << 4));
  }
  return dst - TARGET;

#else

  unsigned char *dst = TARGET;
  unsigned int *inp = (unsigned int *) CHUNK;
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
  if ((unsigned char *) inp < CHUNK + length) {
    *((int *) temp) = 0;
    unsigned char *c = (unsigned char *) inp;
    int end = CHUNK + length - c;
    int p = 0;
    for (;p < 4 && p < end; ++p) {
      if ((temp[p] = c[p] - 63) & 0xC0) {
        // error: non sixel char
        return -(c+p+1 - CHUNK);
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
  return dst - TARGET;

#endif

}


/**
 * @brief Transcode base64-standard to base64-sixel (scalar).
 *
 * Allows SP, CR, LF and '=' as padding character in input data.
 * SP, CR and LF are skipped, '=' returns early.
 *
 * Returns number of transcoded characters written to TARGET.
 * In case of an invalid input character the 2-complement of its
 * position in input data is returned as error code.
 *
 * @param length  Amount of characters loaded in CHUNK to be transcoded.
 * @return int    Amount of characters written to TARGET, or error code.
 */
int transcode(int length) {
  unsigned char *dst = TARGET;
  unsigned char *c = CHUNK;
  unsigned char *c_end8 = CHUNK + length - 8;
  while (c < c_end8) {
    if (
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++]) ||
      !(*dst++ = STAND2SIXEL[*c++])
    ) {
      dst--;
      unsigned char v = *(c - 1);
      // exit early on first padding char =
      if (v == 61) return dst - TARGET;
      // skip SP, CR and LF, negative number as error indicator (~position in chunk)
      if (!(v == 32 || v == 13 || v == 10)) return -(c - CHUNK);
    }
  }
  // handle tail
  unsigned char *c_end = CHUNK + length;
  while (c < c_end) {
    if (!(*dst++ = STAND2SIXEL[*c++])) {
      dst--;
      unsigned char v = *(c - 1);
      if (v == 61) return dst - TARGET;
      if (!(v == 32 || v == 13 || v == 10)) return -(c - CHUNK);
    }
  }
  return dst - TARGET;
}
