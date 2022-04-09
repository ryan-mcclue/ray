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

  u64 bounces_computed;
};
