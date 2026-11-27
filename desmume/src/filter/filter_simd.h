#ifndef DESMUME_FILTER_SIMD_H
#define DESMUME_FILTER_SIMD_H

#include "../types.h"

#if !defined(DESMUME_FILTER_FORCE_SCALAR) && defined(ENABLE_SSE2)
#define DESMUME_FILTER_SIMD_ENABLED
namespace filter_simd
{
using Vec4u32 = __m128i;

inline Vec4u32 load(const u32 *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
inline void store(u32 *p, Vec4u32 v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
inline Vec4u32 set1(u32 v) { return _mm_set1_epi32(static_cast<int>(v)); }
inline Vec4u32 bitAnd(Vec4u32 a, Vec4u32 b) { return _mm_and_si128(a, b); }
inline Vec4u32 bitOr(Vec4u32 a, Vec4u32 b) { return _mm_or_si128(a, b); }
inline Vec4u32 bitXor(Vec4u32 a, Vec4u32 b) { return _mm_xor_si128(a, b); }
inline Vec4u32 andNot(Vec4u32 a, Vec4u32 b) { return _mm_andnot_si128(a, b); }
inline Vec4u32 select(Vec4u32 mask, Vec4u32 yes, Vec4u32 no)
{
	return bitOr(bitAnd(mask, yes), andNot(mask, no));
}
inline Vec4u32 equal(Vec4u32 a, Vec4u32 b) { return _mm_cmpeq_epi32(a, b); }
inline Vec4u32 add(Vec4u32 a, Vec4u32 b) { return _mm_add_epi32(a, b); }
inline Vec4u32 shiftRight1(Vec4u32 a) { return _mm_srli_epi32(a, 1); }
inline Vec4u32 shiftRight2(Vec4u32 a) { return _mm_srli_epi32(a, 2); }
inline Vec4u32 interleaveLow(Vec4u32 a, Vec4u32 b) { return _mm_unpacklo_epi32(a, b); }
inline Vec4u32 interleaveHigh(Vec4u32 a, Vec4u32 b) { return _mm_unpackhi_epi32(a, b); }

inline Vec4u32 average2RgbFloor(Vec4u32 a, Vec4u32 b)
{
	const Vec4u32 mask = set1(0x00FEFEFE);
	return add(bitAnd(a, b), shiftRight1(bitAnd(bitXor(a, b), mask)));
}

inline Vec4u32 average4RgbFloor(Vec4u32 a, Vec4u32 b, Vec4u32 c, Vec4u32 d)
{
	const Vec4u32 highMask = set1(0x3F3F3F3F);
	const Vec4u32 lowMask = set1(0x03030303);
	Vec4u32 high = add(
		add(bitAnd(shiftRight2(a), highMask), bitAnd(shiftRight2(b), highMask)),
		add(bitAnd(shiftRight2(c), highMask), bitAnd(shiftRight2(d), highMask)));
	Vec4u32 low = add(
		add(bitAnd(a, lowMask), bitAnd(b, lowMask)),
		add(bitAnd(c, lowMask), bitAnd(d, lowMask)));
	return add(high, bitAnd(shiftRight2(low), lowMask));
}
}

#elif !defined(DESMUME_FILTER_FORCE_SCALAR) && defined(ENABLE_NEON_A64)
#define DESMUME_FILTER_SIMD_ENABLED
namespace filter_simd
{
using Vec4u32 = uint32x4_t;

inline Vec4u32 load(const u32 *p) { return vld1q_u32(p); }
inline void store(u32 *p, Vec4u32 v) { vst1q_u32(p, v); }
inline Vec4u32 set1(u32 v) { return vdupq_n_u32(v); }
inline Vec4u32 bitAnd(Vec4u32 a, Vec4u32 b) { return vandq_u32(a, b); }
inline Vec4u32 bitOr(Vec4u32 a, Vec4u32 b) { return vorrq_u32(a, b); }
inline Vec4u32 bitXor(Vec4u32 a, Vec4u32 b) { return veorq_u32(a, b); }
inline Vec4u32 andNot(Vec4u32 a, Vec4u32 b) { return vbicq_u32(b, a); }
inline Vec4u32 select(Vec4u32 mask, Vec4u32 yes, Vec4u32 no)
{
	return vbslq_u32(mask, yes, no);
}
inline Vec4u32 equal(Vec4u32 a, Vec4u32 b) { return vceqq_u32(a, b); }
inline Vec4u32 add(Vec4u32 a, Vec4u32 b) { return vaddq_u32(a, b); }
inline Vec4u32 shiftRight1(Vec4u32 a) { return vshrq_n_u32(a, 1); }
inline Vec4u32 shiftRight2(Vec4u32 a) { return vshrq_n_u32(a, 2); }
inline Vec4u32 interleaveLow(Vec4u32 a, Vec4u32 b) { return vzip1q_u32(a, b); }
inline Vec4u32 interleaveHigh(Vec4u32 a, Vec4u32 b) { return vzip2q_u32(a, b); }

inline Vec4u32 average2RgbFloor(Vec4u32 a, Vec4u32 b)
{
	const Vec4u32 mask = set1(0x00FEFEFE);
	return add(bitAnd(a, b), shiftRight1(bitAnd(bitXor(a, b), mask)));
}

inline Vec4u32 average4RgbFloor(Vec4u32 a, Vec4u32 b, Vec4u32 c, Vec4u32 d)
{
	const Vec4u32 highMask = set1(0x3F3F3F3F);
	const Vec4u32 lowMask = set1(0x03030303);
	Vec4u32 high = add(
		add(bitAnd(shiftRight2(a), highMask), bitAnd(shiftRight2(b), highMask)),
		add(bitAnd(shiftRight2(c), highMask), bitAnd(shiftRight2(d), highMask)));
	Vec4u32 low = add(
		add(bitAnd(a, lowMask), bitAnd(b, lowMask)),
		add(bitAnd(c, lowMask), bitAnd(d, lowMask)));
	return add(high, bitAnd(shiftRight2(low), lowMask));
}
}
#endif

#endif
