// SPDX-License-Identifier: zlib-acknowledgement

#pragma once

#include <math.h>

INTERNAL r32
square(r32 x)
{
  r32 result = 0.0f;

  result = x * x;

  return result;
}

union V2
{
  struct
  {
    r32 x, y;
  };
  r32 e[2];
};

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

  result = sqrt(vec_length_sq(vec));

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
