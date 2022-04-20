// SPDX-License-Identifier: zlib-acknowledgement
#pragma once

typedef r32 lane_r32;
typedef u32 lane_u32;
typedef V3 lane_v3;

INTERNAL void
conditional_assign(lane_u32 *dest, lane_u32 mask, lane_u32 source)
{
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
