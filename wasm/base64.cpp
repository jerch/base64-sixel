/**
 * Base64-sixel for WASM.
 * 
 * Copyright (c) 2022 Joerg Breitbart.
 * @license MIT
 */


/**
 * TODO:
 * - decode: error detection with position
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

static unsigned char chunk[CHUNK_SIZE] __attribute__((aligned(16)));
static unsigned int eod;
static unsigned char target[CHUNK_SIZE] __attribute__((aligned(16)));


// exported functions
extern "C" {
  void* get_chunk_address() { return &chunk[0]; }
  void* get_target_address() { return &target[0]; }
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
 * Returns number of transcoded characters written to target.
 * In case of an invalid input character the 2-complement of its
 * position in input data is returned as error code.
 * 
 * TODO: Get this faster with more clever lookup handling...
 * 
 * @param length  Amount of characters loaded in chunk to be transcoded.
 * @return int    Amount of characters written to target, or error code.
 */
int transcode(int length) {
  unsigned char *dst = target;
  unsigned char *c = chunk;
  unsigned char *c_end = c + length;
  *c_end = 0;
  while (c < c_end) {
    if (
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++]) &&
      (*dst++ = STAND2SIXEL[*c++])
    ) { continue; }
    else {
      dst--;
      if (c - 1 >= c_end) {
        return dst - target;
      }
      unsigned char v = *(c - 1);
      // exit early on first padding char =
      if (v == 61) {
        return dst - target;
      }
      // skip SP, CR and LF
      if (!(v == 32 || v == 13 || v == 10)) {
        // negative number as error indicator (~position in chunk)
        return -(c - chunk);
      }
    }
  }
  return dst - target;
}

int encode(int length) {
  // TODO: SIMD and scalar version
  return 0;
}


/*
__m128i _mm_madd_epi16 (__m128i a, __m128i b)
Multiply packed signed 16-bit integers in a and b, producing intermediate signed 32-bit integers.
Horizontally add adjacent pairs of intermediate 32-bit integers, and pack the results in dst.
Operation
FOR j := 0 to 3
	i := j*32
	dst[i+31:i] := SignExtend32(a[i+31:i+16]*b[i+31:i+16]) + SignExtend32(a[i+15:i]*b[i+15:i])
ENDFOR

__m128i _mm_maddubs_epi16 (__m128i a, __m128i b)
Vertically multiply each unsigned 8-bit integer from a with the corresponding signed 8-bit integer from b,
producing intermediate signed 16-bit integers. Horizontally add adjacent pairs of intermediate signed 16-bit integers,
and pack the saturated results in dst.
Operation
FOR j := 0 to 7
	i := j*16
	dst[i+15:i] := Saturate16( a[i+15:i+8]*b[i+15:i+8] + a[i+7:i]*b[i+7:i] )
ENDFOR
*/


//#define __SSSE3__
#ifdef __SSSE3__

#include <immintrin.h>

static inline int _decode_tail(unsigned char *c, unsigned char *t, int length) {
  unsigned char temp0, temp1, temp2, temp3;
  unsigned char *c_end = c + length;
  *c_end = 0;
  while (c < c_end) {
    if (
      (temp0 = *c++ - 63) < 64 &&
      (temp1 = *c++ - 63) < 64 &&
      (temp2 = *c++ - 63) < 64 &&
      (temp3 = *c++ - 63) < 64
    ) {
      *t++ = temp0 << 2 | temp1 >> 4;
      *t++ = temp1 << 4 | temp2 >> 2;
      *t++ = temp2 << 6 | temp3;
    } else {
      if (c - 1 >= c_end) {
        if (temp0 < 64) {
          // NOTE: only one byte < 64 at the tail is treated as invalid and
          // returns error as negative data length (last char + 1)
          // this edge case can be grasped by caller by doing a length check
          // (not sure yet, whether to ignore or raise on that)
          // FIXME: check what RFC says about it
          if (temp1 < 64) {
            *t++ = temp0 << 2 | temp1 >> 4;
            if (temp2 < 64) {
              *t++ = temp1 << 4 | temp2 >> 2;
            }
            return t - target;
          }
        }
      }
      return -(c - chunk);
    }
  }
  return t - target;
}

int decode(int length) {
  unsigned char *t = target;
  __m128i *inp = (__m128i*) chunk;
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
    __m128i r1 = _mm_madd_epi16(v3, _mm_set1_epi32(0x00011000));
    // merge into 12 bytes LE
    __m128i r2 = _mm_shuffle_epi8(r1, _mm_set_epi8(
      0x80, 0x80, 0x80, 0x80,
      0x0C, 0x0D, 0x0E, 0x08,
      0x09, 0x0A, 0x04, 0x05,
      0x06, 0x00, 0x01, 0x02
    ));
    _mm_storeu_si128((__m128i *) t, r2);
    t += 12;
  }
  // TODO: cleanup lines below
  // postponed error handling: all lanes in error should be zero
  if (_mm_movemask_epi8(_mm_cmpeq_epi8(error, _mm_setzero_si128())) != 0xFFFF) {
    // basically the whole data is broken here,
    // thus we redo decoding in scalar to find error position
    // while this penalizes bad cases, good data can run at full speed
    return _decode_tail(chunk, target, length);
  }
  if (((unsigned char *) inp) < chunk + length) {
    int processed = (length >> 4) << 4;
    return _decode_tail((unsigned char *) inp, t, length - processed);
  }
  return t - target;
}

#else

// TODO: make faster scalar lookup version
int decode(int length) {
  unsigned char *t = target;
  unsigned char temp0, temp1, temp2, temp3;
  unsigned char *c = chunk;
  unsigned char *c_end = c + length;
  *c_end = 0;
  while (c < c_end) {
    if (
      (temp0 = *c++ - 63) < 64 &&
      (temp1 = *c++ - 63) < 64 &&
      (temp2 = *c++ - 63) < 64 &&
      (temp3 = *c++ - 63) < 64
    ) {
      *t++ = temp0 << 2 | temp1 >> 4;
      *t++ = temp1 << 4 | temp2 >> 2;
      *t++ = temp2 << 6 | temp3;
    } else {
      if (c - 1 >= c_end) {
        if (temp0 < 64) {
          // NOTE: only one byte < 64 at the tail is treated as invalid and
          // returns error as negative data length (last char + 1)
          // this edge case can be grasped by caller by doing a length check
          // (not sure yet, whether to ignore or raise on that)
          // FIXME: check what RFC says about it
          if (temp1 < 64) {
            *t++ = temp0 << 2 | temp1 >> 4;
            if (temp2 < 64) {
              *t++ = temp1 << 4 | temp2 >> 2;
            }
            return t - target;
          }
        }
      }
      return -(c - chunk);
    }
  }
  return t - target;
}


#endif
