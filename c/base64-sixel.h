#ifndef BASE64SIXEL_H
#define BASE64SIXEL_H

#include <stddef.h>
#include <sys/types.h>

#if defined(__SSE__) && defined(__SSE2__) && defined(__SSSE3__) && defined(__SSE4_1__)
#define USE_SIMD 1
#include <immintrin.h>
#endif
//#define USE_SIMD 1
//#include <immintrin.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 
 * 
 * @param src 
 * @param srclen 
 * @param dst 
 * @return ssize_t 
 */
ssize_t base64sixel_encode(const uint8_t *src, size_t srclen, const uint8_t *dst);

/**
 * @brief 
 * 
 * @param src 
 * @param srclen 
 * @param dst 
 * @return ssize_t 
 */
ssize_t base64sixel_decode(const uint8_t *src, size_t srclen, const uint8_t *dst);

/**
 * @brief 
 * 
 * @param src 
 * @param srclen 
 * @param dst 
 * @return ssize_t 
 */
ssize_t base64sixel_transcode(const uint8_t *src, size_t srclen, const uint8_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* BASE64SIXEL_H */
