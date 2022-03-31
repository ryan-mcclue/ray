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

#include "math.h"

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

INTERNAL V3
cast_ray(World *world, V3 ray_origin, V3 ray_direction)
{
  V3 result = world->materials[0].colour;

  // closest hit
  r32 hit_distance = R32_MAX;
  // NOTE(Ryan): Ad-hoc value
  r32 tolerance = 0.0001f;

  for (u32 plane_index = 0;
       plane_index < world->plane_count;
       ++plane_index)
  {
    Plane plane = world->planes[plane_index];

    // for ray line: ray_origin + scale_factor·ray_direction
    // substitute this in for point in plane equation and solve for scale_factor (in this case 't')
    r32 denom = vec_dot(plane.normal, ray_direction);
    // zero if perpendicular to normal, a.k.a will never intersect plane
    if (denom < -tolerance || denom > tolerance)
    {
      r32 t = (-plane.distance - vec_dot(plane.normal, ray_origin)) / denom;
      if (t > 0 && t < hit_distance)
      {
        hit_distance = t;
        result = world->materials[plane.material_index].colour;
      }
    }

  }

  for (u32 sphere_index = 0;
       sphere_index < world->sphere_count;
       ++sphere_index)
  {
    Sphere sphere = world->spheres[sphere_index];

    // to account for the sphere's origin
    V3 sphere_relative_ray_origin = ray_origin - sphere.position;

    // for sphere: x² + y² + z² - r² = 0
    // we see that this contains the dot product of itself: pᵗp - r² = 0
    // substituting ray line equation we get a quadratic equation in terms of t
    // so, use quadratic formula to solve
    r32 a = vec_dot(ray_direction, ray_direction);
    r32 b = 2 * vec_dot(ray_direction, sphere_relative_ray_origin);
    r32 c = vec_dot(sphere_relative_ray_origin, sphere_relative_ray_origin) - (sphere.radius * sphere.radius);

    r32 denom = 2 * a;
    r32 root_term = square_root(b * b - 4.0f * a * c);
    if (root_term > tolerance)
    {
      r32 t_pos = (-b + root_term) / denom;
      r32 t_neg = (-b - root_term) / denom;

      r32 t = t_pos;
      // check if t_neg is a better hit
      if (t_neg > 0 && t_neg < t_pos)
      {
        t = t_neg;
      }
      
      if (t > 0 && t < hit_distance)
      {
        hit_distance = t;
        result = world->materials[sphere.material_index].colour;
      }
    }
  }

  return result;
}

int
main(int argc, char *argv[])
{
  printf("Ray tracing...\n");

#if defined(RAY_INTERNAL)
  if (argc > 1 && strcmp(argv[1], "-debugger") == 0)
  {
    __bp = __bp_debugger;
    __ebp = __ebp_debugger;
  }
#endif

  /* 
  TODO(Ryan): Comparison between using uint and r32 here.
  Difference between FPU and SIMD instructions.

  r32 val0 = 12.0f;
  r32 val1 = 45.0f;
  r32 test_val = 23.0f;
  r32 norm_val = (test_val - val0) / (val1 - val0);
  
  r32 x0 = 5.0f;
  r32 x1 = 10.0f;
  r32 blend = 0.5f;
  r32 lerp = ((x1 - x0) * blend) + x0;

  map = norm + lerp;
  */

  Material materials[3] = {};
  materials[0].colour = {0.1f, 0.1f, 0.1f};
  materials[1].colour = {1, 0, 0};
  materials[2].colour = {0, 0, 1};

  Plane plane = {};
  plane.normal = {0, 0, 1};
  plane.distance = 0;
  plane.material_index = 1;

  Sphere sphere = {};
  sphere.position = {0, 0, 0};
  sphere.radius = 1.0f;
  sphere.material_index = 2;

  World world = {};
  world.material_count = 3;
  world.materials = materials;
  world.plane_count = 1;
  world.planes = &plane;
  world.sphere_count = 1;
  world.spheres = &sphere;

  // right hand rule here to derive these?
  /* rays around the camera. so, want the camera to have a coordinate system, i.e. set of axis
   */
  V3 camera_pos = {0, -10, 1};
  // we are looking through -'z', i.e. opposite direction to what our camera z axis is
  V3 camera_z = vec_noz(camera_pos);
  // cross our z with universal z
  V3 camera_x = vec_noz(vec_cross({0, 0, 1}, camera_z));
  V3 camera_y = vec_noz(vec_cross(camera_z, camera_x));

  u32 output_width = 1280;
  u32 output_height = 720;
  u32 output_pixel_size = output_width * output_height * sizeof(u32);

  r32 film_dist = 1.0f;
  r32 film_w = 1.0f;
  r32 film_h = 1.0f;
  // aspect ratio correction
  if (output_width > output_height)
  {
    film_h = film_w * ((r32)output_height / (r32)output_width);  
  }
  if (output_height > output_width)
  {
    film_w = film_h * ((r32)output_width / (r32)output_height);  
  }
  r32 half_film_w = 0.5f * film_w;
  r32 half_film_h = 0.5f * film_h;
  V3 film_centre = camera_pos - (film_dist * camera_z);


  u32 *pixels = (u32 *)malloc(output_pixel_size);
  if (pixels != NULL)
  {
    u32 *out = pixels;
    for (uint y = 0; 
         y < output_height;
         ++y)
    {
      // for camera, z axis is looking from, x and y determine plane aperture
      r32 film_y = -1.0f + 2.0f * ((r32)y / (r32)output_height);
      for (uint x = 0; 
           x < output_width; 
           ++x)
      {
        r32 film_x = -1.0f + 2.0f * ((r32)x / (r32)output_width);

        // need to do half width as from centre
        V3 film_p = film_centre + (film_x * half_film_w * camera_x) + (film_y * half_film_h * camera_y);

        V3 ray_origin = camera_pos;
        // why is this not the other way round?
        V3 ray_direction = vec_noz(film_p - camera_pos);

        V3 colour = cast_ray(&world, ray_origin, ray_direction);

        V4 bmp_colour = {255.0f, 255.0f*colour.r, 255.0f*colour.g, 255.0f*colour.b};
        u32 bmp_value = pack_4x8(bmp_colour);
        
        *out++ = bmp_value;

        printf("\rRaycasting %d%%...    ", (y * 100 / output_height));
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

  printf("\nDone\n");

  return 0;
}
