// SPDX-License-Identifier: zlib-acknowledgement
#pragma once

/*
 *   struct r32_lane
 *   {
 *     real_lane value;
 *   }
 *   struct u32_lane
 *   {
 *     integer_lane value;
 *   }
 * }
 *
 * struct lane_v3
 * {
 *   r32_lane x;
 *   r32_lane y;
 *   r32_lane z;
 * }
 * 
 */

#define LANE_WIDTH 1

#if (LANE_WIDTH == 8)

#elif (LANE_WIDTH == 4)

typedef real_lane __m128;
typedef integer_lane __m128i;

#define LANE_R32_ADD(a, b) _mm_add_ps(a.value, b.value)
#define LANE_R32_MIN(a, b) _mm_min_ps(a.value, b.value)
#define LANE_R32_MAX(a, b) _mm_max_ps(a.value, b.value)

INTERNAL lane_u32
and_not(lane_u32 a, lane_u32 b)
{
  lane_u32 result = {};

  result = _mm_andnot_si128(a.value, b.value);

  return result;
}

INTERNAL lane_r32
operator-(lane_r32 a)
{
  lane_r32 result = {};

  result = lane_r32_from_r32(0) - a;

  return result;
}

INTERNAL lane_u32
operator<(lane_r32 a, lane_r32 b)
{
  lane_u32 result = {};

  result = _mm_castps_si128(_mm_cmplt_ps(a.value, b.value));

  return result;
}

INTERNAL lane_u32
operator<<(lane_u32 a, u32 shift)
{
  lane_u32 result;

  result.value = _mm_slli_epi32(a.value, shift);

  return result;
}

INTERNAL lane_u32
operator>>(lane_u32 a, u32 shift)
{
  lane_u32 result;

  result.value = _mm_srli_epi32(a.value, shift);

  return result;
}

INTERNAL lane_u32
operator^(lane_u32 a, lane_u32 b)
{
  lane_u32 result;

  result = _mm_xor_si128(a.value, b.value);

  return result;
}

INTERNAL lane_r32
operator+(lane_r32 a, lane_r32 b)
{
  lane_r32 result;

  result = _mm_add_ps(a.value, b.value);

  return result;
}


INTERNAL lane_r32
operator/(lane_r32 a, r32 div)
{
  lane_r32 result;

  result.v = _mm_div_ps(a.result, _mm_set1_ps(div));

  return result;
}

INTERNAL lane_r32
lane_r32_from_lane_u32(lane_u32 a)
{
  lane_r32 result;

  result.value = _mm_cvtepi32_ps(a.value);

  return result;
}

INTERNAL lane_r32
lane_r32_from_u32(u32 replicate)
{
  lane_r32 result;

  result.value = _mm_set1_ps((r32)replicate);

  return result;
}

INTERNAL lane_r32
lane_r32_from_r32(r32 replicate)
{
  lane_r32 result;

  result.value = _mm_set1_ps(replicate);

  return result;
}

// state is seed
INTERNAL lane_u32
xor_shift_u32(u32 *random_series)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	lane_u32 x = *random_series;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
  *random_series = x;

	return x;
}

INTERNAL lane_r32
lane_r32_from_u32(lane_u32 a)
{
  lane_r32 result = (lane_r32)a;

  return result;
}

INTERNAL lane_r32
random_unilateral(u32 *random_series)
{
  lane_r32 result = 0.0f;

  // rand() implements mutexes so each thread has to run serially, use rand_r()
  
  result = lane_r32_from_u32(xor_shift_u32(random_series)) / lane_r32_from_u32(U32_MAX);

  return result;
}

INTERNAL lane_r32
random_bilateral(u32 *random_series)
{
  lane_r32 result = 0.0f;

  result = -1.0f + 2.0f * random_unilateral(random_series);

  return result;
}

INTERNAL lane_r32
random_bilateral_lane(u32 *random_series)
{
  lane_r32 result = _mm_set_epi32(random_bilateral(random_series),
                                  random_bilateral(random_series),
                                  random_bilateral(random_series),
                                  random_bilateral(random_series));

  return result;
}

#elif (LANE_WIDTH == 1)

typedef r32 lane_r32;
typedef u32 lane_u32;
typedef V3 lane_v3;

INTERNAL void
conditional_assign(lane_u32 *dest, lane_u32 mask, lane_u32 source)
{
  mask = (mask ? 0xffffffff : 0);

  *dest = (~mask & *dest) | (mask & source);
}

INTERNAL void
conditional_assign(lane_r32 *dest, lane_u32 mask, lane_r32 source)
{
  // TODO(Ryan): How does this reinterpretation not break things?
  u32 result = (~mask & *(u32 *)dest) | (mask & *(u32 *)&source);
  *dest = *(r32 *)&result;
}

INTERNAL void
conditional_assign(lane_v3 *dest, lane_u32 mask, lane_v3 source)
{
  conditional_assign(&dest->x, mask, source.x);
  conditional_assign(&dest->y, mask, source.y);
  conditional_assign(&dest->z, mask, source.z);
}

INTERNAL lane_r32
max(lane_r32 a, lane_r32 b)
{
  return (a > b ? a : b);
}

INTERNAL b32
mask_is_zeroed(lane_u32 lane_mask)
{
  return (lane_mask == 0);
}

INTERNAL r32
horizontal_add(lane_r32 a)
{
  // IMPORTANT(Ryan): We are only doing this in one dimension for now
  return a;
}

INTERNAL V3
horizontal_add(lane_v3 a)
{
  V3 result = {};

  result.x = horizontal_add(a.x);
  result.y = horizontal_add(a.y);
  result.z = horizontal_add(a.z);

  return result;
}

INTERNAL lane_r32
random_bilateral_lane(u32 *random_series)
{
  lane_r32 result = random_bilateral(random_series);

  return result;
}

#else
#error Lane width must bet set to 1!
#endif

struct lane_v3
{
  lane_r32 x;
  lane_r32 y;
  lane_r32 z;
};

#if (LANE_WIDTH != 1)

INTERNAL lane_v3
lane_v3_from_v3(V3 replicate)
{
  lane_v3 result = {};

  result.x = lane_r32_from_r32(replicate.x);
  result.y = lane_r32_from_r32(replicate.y);
  result.z = lane_r32_from_r32(replicate.z);

  return result;
}

INTERNAL lane_u32 &
operator-=(lane_u32 &a, lane_u32 b)
{
  a = a - b;

  return a;
}

INTERNAL lane_u32 &
operator^=(lane_u32 &a, lane_u32 b)
{
  a.value = a ^ b;

  return a;
}

INTERNAL void
conditional_assign(lane_u32 *dest, lane_u32 mask, lane_u32 source)
{
  // IMPORTANT(Ryan): This works as masks obtained from simd comparison will be all 1s or all 0s
  *dest = and_not(mask, *dest) | (mask & source);
}

INTERNAL lane_r32
clamp01(lane_r32 a)
{
  lane_r32 result = {};

  result.value = LANE_R32_MIN(LANE_R32_MAX(a.value, lane_r32_from_r32(0.0f)), 1.0f);

  return result;
}

#endif
