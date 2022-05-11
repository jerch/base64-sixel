/**
 * Base64-sixel for WASM.
 * 
 * Copyright (c) 2022 Joerg Breitbart.
 * @license MIT
 */


/**
 * TODO:
 * - encode (scalar + SIMD)
 * - switch to pure C
 * - wasm build:
 *   - separate scalar and SIMD builds
 *   - stream aware decode/encode on static memory (needs ctx object)
 * - native build:
 *   - pointered interface (no static memory)
 *   - makefile + header
 * - small cmdline tool for encode/transcode/decode (see coreutils/base64)
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


/** operate on static memory for wasm */
static unsigned char CHUNK[CHUNK_SIZE+1] __attribute__((aligned(16)));
static unsigned char TARGET[CHUNK_SIZE] __attribute__((aligned(16)));


// exported functions
extern "C" {
  void* get_chunk_address() { return &CHUNK[0]; }
  void* get_target_address() { return &TARGET[0]; }
  int transcode(int length);
  int decode(int length);
}


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
  unsigned char *c_end = c + length;
  *c_end = 0;
  while (c < c_end) {
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
      if (c - 1 >= c_end) {
        return dst - TARGET;
      }
      unsigned char v = *(c - 1);
      // exit early on first padding char =
      if (v == 61) {
        return dst - TARGET;
      }
      // skip SP, CR and LF
      if (!(v == 32 || v == 13 || v == 10)) {
        // negative number as error indicator (~position in chunk)
        return -(c - CHUNK);
      }
    }
  }
  return dst - TARGET;
}

int encode(int length) {
  // TODO: SIMD and scalar version
  return 0;
}


#ifdef USE_SIMD

/**
 * @brief Helper for SIMD path to do tail decoding and error handling.
 *
 * @param c       Pointer to current read position in CHUNK.
 * @param dst     Pointer to current write position in TARGET.
 * @param length  Amount of characters left in CHUNK to be handled.
 * @return int    Byte write length in TARGET, or error code.
 */
int _decode_tail(unsigned char *c, unsigned char *dst, int length) {
  unsigned int *inp = (unsigned int *) c;
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
}

#endif


/**
 * @brief Decode base64-sixel characters loaded in CHUNK.
 *
 * Uses either scalar or SIMD version, depending on compile settings.
 *
 * @param length  Amount of characters loaded in CHUNK to be decoded.
 * @return int    Byte write length in TARGET, or error code.
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
