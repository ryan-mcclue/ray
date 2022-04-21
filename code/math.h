// SPDX-License-Identifier: zlib-acknowledgement

#pragma once

#include <math.h>
#include <float.h>
#include <x86intrin.h>

#define ARRAY_COUNT(arr) \
  (sizeof(arr) / sizeof((arr)[0]))

#define R32_MAX FLT_MAX
#define R32_MIN -FLT_MAX
#define U32_MAX UINT32_MAX

// TODO(Ryan): Investigate using gcc extensions for safer macros.
// Do they add any overhead?

#define KILOBYTES(x) \
  ((x) * 1024UL)
#define MEGABYTES(x) \
  (KILOBYTES(x) * 1024UL)
#define GIGABYTES(x) \
  (GIGABYTES(x) * 1024UL)
#define TERABYTES(x) \
  (TERABYTES(x) * 1024UL)

#define MAX(x, y) \
  ((x) > (y) ? (x) : (y))
#define MIN(x, y) \
  ((x) < (y) ? (x) : (y))


INTERNAL r32
square_root(r32 val)
{
    r32 result = 0.0f;

    result = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(val)));

    return result;
}

INTERNAL u32
round_r32_to_u32(r32 real32)
{
    u32 result = 0;

    result = (u32)_mm_cvtss_si32(_mm_set_ss(real32));

    return result;
}

//INTERNAL s32
//RoundReal32ToInt32(r32 Real32)
//{
//    s32 Result = _mm_cvtss_si32(_mm_set_ss(Real32));
//    return(Result);
//}
//
//
//inline s32
//FloorReal32ToInt32(r32 Real32)
//{
//    // TODO(casey): Do we want to forgo the use of SSE 4.1?
//    int32 Result = _mm_cvtss_si32(_mm_floor_ss(_mm_setzero_ps(), _mm_set_ss(Real32)));
//    return(Result);
//}
//
//inline r32
//Round(r32 Real32)
//{
//    r32 Result = _mm_cvtss_r32(_mm_round_ss(_mm_setzero_ps(), _mm_set_ss(Real32),
//                               (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC)));
//    return(Result);
//}
//
//inline r32
//Floor(r32 Real32)
//{
//    // TODO(casey): Do we want to forgo the use of SSE 4.1?
//    r32 Result = _mm_cvtss_r32(_mm_floor_ss(_mm_setzero_ps(), _mm_set_ss(Real32)));
//    return(Result);
//}
//
//inline s32
//CeilReal32ToInt32(r32 Real32)
//{
//    // TODO(casey): Do we want to forgo the use of SSE 4.1?
//    int32 Result = _mm_cvtss_si32(_mm_ceil_ss(_mm_setzero_ps(), _mm_set_ss(Real32)));
//    return(Result);
//}
//



INTERNAL r32
square(r32 x)
{
  r32 result = 0.0f;

  result = x * x;

  return result;
}

INTERNAL r32
lerp(r32 start, r32 end, r32 p)
{
  r32 result = 0.0f;

  result = ((end - start) * p) + start;

  return result;
}

union V2
{
  struct
  {
    r32 x, y;
  };
  struct
  {
    r32 w, h;
  };
  r32 e[2];
};

union V2u
{
  struct
  {
    u32 x, y;
  };
  struct
  {
    u32 w, h;
  };
  u32 e[2];
};

union V2s
{
  struct
  {
    s32 x, y;
  };
  struct
  {
    s32 w, h;
  };
  s32 e[2];
};

INTERNAL V2u
v2u(u32 x, u32 y)
{
  V2u result = {};

  result.x = x;
  result.y = y;
  
  return result;
}

INTERNAL V2s
v2s(s32 x, s32 y)
{
  V2s result = {};

  result.x = x;
  result.y = y;
  
  return result;
}

INTERNAL V2
v2(r32 x, r32 y)
{
  V2 result = {};

  result.x = x;
  result.y = y;
  
  return result;
}

INTERNAL V2
operator*(r32 s, V2 vec)
{
  V2 result = {};

  result.x = vec.x * s;
  result.y = vec.y * s;
  
  return result;
}

INTERNAL V2
operator*(V2 vec, r32 s)
{
  V2 result = s * vec;
  
  return result;
}

INTERNAL V2 &
operator*=(V2 &vec, r32 s)
{
  vec = s * vec;

  return vec;
}

INTERNAL V2
operator+(V2 vec1, V2 vec2)
{
  V2 result = {};

  result.x = vec1.x + vec2.x;
  result.y = vec1.y + vec2.y;

  return result;
}

INTERNAL V2 &
operator+=(V2 &vec1, V2 vec2)
{
  vec1 = vec1 + vec2;

  return vec1;
}

INTERNAL V2
operator-(V2 vec1, V2 vec2)
{
  V2 result = {};

  result.x = vec1.x - vec2.x;
  result.y = vec1.y - vec2.y;

  return result;
}

INTERNAL V2 &
operator-=(V2 &vec1, V2 vec2)
{
  vec1 = vec1 - vec2;

  return vec1;
}

INTERNAL V2
operator-(V2 vec)
{
  V2 result = {};

  result.x = -vec.x;
  result.y = -vec.y;

  return result;
}

INTERNAL r32
vec_dot(V2 vec1, V2 vec2)
{
  r32 result = 0.0f;

  result = (vec1.x * vec2.x) + (vec1.y * vec2.y);

  return result;
}

INTERNAL r32
vec_length_sq(V2 vec)
{
  r32 result = 0.0f;

  result = vec_dot(vec, vec);

  return result;
}

INTERNAL r32
vec_length(V2 vec)
{
  r32 result = 0.0f;

  result = square_root(vec_length_sq(vec));

  return result;
}

INTERNAL V2
vec_noz(V2 vec)
{
  V2 result = {};

  r32 length_sq = vec_length_sq(vec);
  if (length_sq > square(0.0001f))
  {
    result = vec * (1.0f / vec_length(vec));
  }

  return result;
}

union V3
{
  struct 
  {
    r32 x, y, z;
  };
  struct 
  {
    r32 r, g, b;
  };
  r32 e[3];
};

INTERNAL V3
v3(r32 x, r32 y, r32 z)
{
  V3 result = {};

  result.x = x;
  result.y = y;
  result.z = z;
  
  return result;
}

INTERNAL V3
vec_hadamard(V3 vec1, V3 vec2)
{
  V3 result = {};

  result.x = vec1.x * vec2.x;
  result.y = vec1.y * vec2.y;
  result.z = vec1.z * vec2.z;

  return result;
}

INTERNAL V3
vec_cross(V3 vec1, V3 vec2)
{
  V3 result = {};

  result.x = (vec1.y * vec2.z) - (vec1.z * vec2.y);
  result.y = (vec1.z * vec2.x) - (vec1.x * vec2.z);
  result.z = (vec1.x * vec2.y) - (vec1.y * vec2.x);

  return result;
}

INTERNAL V3
operator*(r32 s, V3 vec)
{
  V3 result = {};

  result.x = vec.x * s;
  result.y = vec.y * s;
  result.z = vec.z * s;
  
  return result;
}

INTERNAL V3
operator*(V3 vec, r32 s)
{
  V3 result = s * vec;
  
  return result;
}

INTERNAL V3 &
operator*=(V3 &vec, r32 s)
{
  vec = s * vec;

  return vec;
}

INTERNAL V3
operator+(V3 vec1, V3 vec2)
{
  V3 result = {};

  result.x = vec1.x + vec2.x;
  result.y = vec1.y + vec2.y;
  result.z = vec1.z + vec2.z;

  return result;
}

INTERNAL V3 &
operator+=(V3 &vec1, V3 vec2)
{
  vec1 = vec1 + vec2;

  return vec1;
}

INTERNAL V3
operator-(V3 vec1, V3 vec2)
{
  V3 result = {};

  result.x = vec1.x - vec2.x;
  result.y = vec1.y - vec2.y;
  result.z = vec1.z - vec2.z;

  return result;
}

INTERNAL V3
operator-(V3 vec)
{
  V3 result = {};
  
  result.x = -vec.x;
  result.y = -vec.y;
  result.z = -vec.z;

  return result;
}

INTERNAL r32
vec_dot(V3 vec1, V3 vec2)
{
  r32 result = 0.0f;

  result = (vec1.x * vec2.x) + (vec1.y * vec2.y) + (vec1.z * vec2.z);

  return result;
}

INTERNAL r32
vec_length_sq(V3 vec)
{
  r32 result = 0.0f;

  result = vec_dot(vec, vec);

  return result;
}

INTERNAL r32
vec_length(V3 vec)
{
  r32 result = 0.0f;

  result = sqrt(vec_length_sq(vec));

  return result;
}

INTERNAL V3
vec_noz(V3 vec)
{
  V3 result = {};

  r32 length_sq = vec_length_sq(vec);
  if (length_sq > square(0.0001f))
  {
    result = vec * (1.0f / vec_length(vec));
  }

  return result;
}

INTERNAL V3
lerp(V3 start, V3 end, r32 p)
{
  V3 result = {};

  result = ((end - start) * p) + start;

  return result;
}

union V4
{
  struct
  {
    union
    {
      V3 xyz;
      struct
      {
        r32 x, y, z;
      };
    };
    r32 w;
  };

  struct
  {
    union
    {
      V3 rgb;
      struct
      {
        r32 r, g, b;
      };
    };
    r32 a;
  };

  r32 e[4];
};

INTERNAL V4
v4(r32 x, r32 y, r32 z, r32 w)
{
  V4 result = {};

  result.x = x;
  result.y = y;
  result.z = z;
  result.w = w;
  
  return result;
}

INTERNAL u32
pack_4x8(V4 val)
{
  u32 result = 0;

  result = (round_r32_to_u32(val.x) << 24 |
            round_r32_to_u32(val.y) << 16 & 0xFF0000 |
            round_r32_to_u32(val.z) << 8 & 0xFF00 |
            round_r32_to_u32(val.w) & 0xFF);

  return result;
}

INTERNAL V4
linear1_to_srgb255(V4 colour)
{
  V4 result = {};

  result.r = 255.0f * square_root(colour.r);
  result.g = 255.0f * square_root(colour.g);
  result.b = 255.0f * square_root(colour.b);
  result.a = 255.0f * square_root(colour.a);

  return result;
}

INTERNAL r32
exact_linear1_to_srgb1(r32 linear1)
{
  if (linear1 < 0.0f)
  {
    linear1 = 0.0f;
  }
  if (linear1 > 1.0f)
  {
    linear1 = 1.0f;
  }

  r32 result = 0.0f;

  result = linear1 * 12.92f;
  if (linear1 > 0.0031308f)
  {
    result = 1.055f * pow(linear1, 1.0f/2.4f) - 0.055f;
  }

  return result;
}
