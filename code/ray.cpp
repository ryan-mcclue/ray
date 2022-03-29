// SPDX-License-Identifier: zlib-acknowledgement

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define INTERNAL      static
#define GLOBAL        static
#define LOCAL_PERSIST static

typedef unsigned int uint;
typedef uint8_t      u8;
typedef uint16_t     u16;
typedef uint32_t     u32;
typedef uint64_t     u64;
typedef int8_t       s8;
typedef int16_t      s16;
typedef int32_t      s32;
typedef int64_t      s64;
typedef float        r32;
typedef double       r64;

#if defined(RAY_INTERNAL)
  INTERNAL void __bp_non_debugger(char const *file_name, char const *func_name, int line_num)
  { 
    fprintf(stderr, "BREAKPOINT TRIGGERED! (%s:%s:%d)\n", file_name, func_name, line_num);
    exit(1);
  }
  INTERNAL void __bp_debugger(char const *file_name, char const *func_name, int line_num) { return; }
  GLOBAL void (*__bp)(char const *, char const *, int) = __bp_non_debugger;

  INTERNAL void __ebp_non_debugger(char const *file_name, char const *func_name, int line_num)
  { 
    fprintf(stderr, "ERRNO BREAKPOINT TRIGGERED! (%s:%s:%d)\n\"%s\"\n", file_name, func_name, line_num, strerror(errno));
    exit(1);
  }
  INTERNAL void __ebp_debugger(char const *file_name, char const *func_name, int line_num)
  { 
    char *errno_str = strerror(errno);
    return;
  }
  GLOBAL void (*__ebp)(char const *, char const *, int) = __ebp_non_debugger;

  #define BP() __bp(__FILE__, __func__, __LINE__)
  #define EBP() __ebp(__FILE__, __func__, __LINE__)
  #define ASSERT(cond) if (!(cond)) {BP();}
#else
  #define BP()
  #define EBP()
  #define ASSERT(cond)
#endif

#include <math.h>

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



//INTERNAL r32
//vec_length_sq(V3 v)
//{
//  r32 result = 0.0f;
//
//  result = vec_dot(v) * vec_dot(v);
//}

// IEEE 754 is in essense a compression algorithm, i.e. compressing all numbers from negative to positive infinity to a finite space of bits
// Therefore, 0.1 + 0.2 != 0.3 (0.300000004) as it can't represent 0.3
// epsilon is an allowable error margin for floating point
//
// this performs an automatic epsilon check
//

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
  V3 colour;
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

int
main(int argc, char *argv[])
{
  puts("Ray tracer");

#if defined(RAY_INTERNAL)
  if (argc > 1 && strcmp(argv[1], "-debugger") == 0)
  {
    __bp = __bp_debugger;
    __ebp = __ebp_debugger;
  }
#endif

  /* TODO(Ryan): Comparison between using uint and r32 here.
     Difference between FPU and SIMD instructions.
  r32 x0 = 5.0f;
  r32 x1 = 10.0f;
  r32 blend = 0.5f;
  r32 lerp = ((x1 - x0) * blend) + x0;
  */

  Material materials[2] = {};
  materials[0].colour = {0, 0, 0};
  materials[1].colour = {1, 0, 0};

  Plane plane = {};
  plane.normal = {0, 0, 1};
  plane.distance = 0;
  plane.material_index = 1;

  World world = {};
  world.material_count = 2;
  world.materials = materials;
  world.plane_count = 1;
  world.planes = &plane;
  world.sphere_count = 0;
  world.spheres = NULL;

  /* rays around the camera. so, want the camera to have a coordinate system, i.e. set of axis
   */
  V3 camera_pos = {0, 10, 1};
  V3 camera_dir = {0, 0, 0};

  uint output_width = 1280;
  uint output_height = 720;
  uint output_pixel_size = output_width * output_height * sizeof(u32);

  u32 *pixels = (u32 *)malloc(output_pixel_size);
  if (pixels != NULL)
  {
    u32 *out = pixels;
    for (uint y = 0; 
         y < output_height;
         ++y)
    {
      for (uint x = 0; 
           x < output_width; 
           ++x)
      {
        *out++ = (y < 32) ? 0xffff0000 : 0xff0000ff;
      }
    }

    BitmapHeader bitmap_header = {};

    bitmap_header.signature = 0x4d42;
    bitmap_header.file_size = sizeof(bitmap_header) + output_pixel_size;
    bitmap_header.data_offset = sizeof(bitmap_header);
    bitmap_header.size = sizeof(bitmap_header) - 14;
    bitmap_header.width = output_width;
    bitmap_header.height = output_height;
    bitmap_header.planes = 1;
    bitmap_header.bits_per_pixel = 32;
    bitmap_header.compression = 3;
    bitmap_header.size_of_bitmap = 0;
    bitmap_header.horz_resolution = 4096;
    bitmap_header.vert_resolution = 4096;
    bitmap_header.colors_used = 0;
    bitmap_header.colors_important = 0;
    bitmap_header.red_mask   = 0x00ff0000;
    bitmap_header.green_mask = 0x0000ff00;
    bitmap_header.blue_mask  = 0x000000ff;

    FILE *out_file = fopen("out.bmp", "wb"); 
    if (out_file != NULL)
    {
      uint header_written = fwrite(&bitmap_header, sizeof(bitmap_header), 1, out_file);
      if (header_written != 1) EBP();

      uint pixels_written = fwrite(pixels, output_pixel_size, 1, out_file);
      if (pixels_written != 1) EBP();

      fclose(out_file);
    }
    else
    {
      EBP();
    }
  } 
  else
  {
    EBP();
  }

  return 0;
}
