// SPDX-License-Identifier: zlib-acknowledgement
#pragma once

struct ImageU32
{
  u32 width, height;
  u32 *pixels;
};

struct BitmapHeader
{
  u16 signature;
  u32 file_size;
  u32 reserved;
  u32 data_offset;
  u32 size;
  u32 width;
  u32 height;
  u16 planes;
  u16 bits_per_pixel;
  u32 compression;
  u32 size_of_bitmap;
  u32 horz_resolution;
  u32 vert_resolution;
  u32 colors_used;
  u32 colors_important;

  u32 red_mask;
  u32 green_mask;
  u32 blue_mask;
} __attribute__((packed));

struct Material
{
  r32 scatter; // 0 is diffuse, 1 is specular
  V3 emitted_colour;
  V3 reflected_colour;
};

struct Plane
{
  V3 normal;
  r32 distance; // distance along normal
  u32 material_index;
};

struct Sphere
{
  V3 position;
  r32 radius;
  u32 material_index;
};

struct World
{ 
  u32 material_count;
  Material *materials;

  u32 plane_count;
  Plane *planes;

  u32 sphere_count;
  Sphere *spheres;
};

struct WorkOrder
{
  World *world;
  ImageU32 *image; 
  u32 x_min;
  u32 y_min; 
  u32 one_past_x_max; 
  u32 one_past_y_max;
};

struct WorkQueue
{
  // these don't require volatile as won't ever be written to by separate threads (or read by?)
  u32 work_order_count;
  WorkOrder *work_orders;

  volatile u32 next_work_order_index;
  volatile u64 bounces_computed;
  volatile u32 tiles_retired_count;
};


