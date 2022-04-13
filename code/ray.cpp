// SPDX-License-Identifier: zlib-acknowledgement

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sched.h>
#include <sys/sysinfo.h>

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
typedef u32          b32;

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
#include "ray.h"

INTERNAL ImageU32
create_image_u32(u32 width, u32 height)
{
  ImageU32 result = {};

  result.width = width;
  result.height = height;

  result.pixels = (u32 *)malloc(width * height * sizeof(u32));
  if (result.pixels == NULL) EBP();

  return result;
}

INTERNAL void
write_image_u32_to_bmp(ImageU32 *image, char const *file_name)
{
  BitmapHeader bitmap_header = {};

  u32 output_pixel_size = image->width * image->height * sizeof(u32);

  bitmap_header.signature = 0x4d42;
  bitmap_header.file_size = sizeof(bitmap_header) + output_pixel_size;
  bitmap_header.data_offset = sizeof(bitmap_header);
  bitmap_header.size = sizeof(bitmap_header) - 14;
  bitmap_header.width = image->width;
  bitmap_header.height = image->height;
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

  FILE *out_file = fopen(file_name, "wb"); 
  if (out_file != NULL)
  {
    uint header_written = fwrite(&bitmap_header, sizeof(bitmap_header), 1, out_file);
    if (header_written != 1) EBP();

    uint pixels_written = fwrite(image->pixels, output_pixel_size, 1, out_file);
    if (pixels_written != 1) EBP();

    fclose(out_file);
  }
}

INTERNAL u64
get_wall_clock(void)
{
  u64 result = 0;

  struct timespec time_spec = {};

  clock_gettime(CLOCK_MONOTONIC_RAW, &time_spec);

  result = time_spec.tv_sec * 1000 + time_spec.tv_nsec / 1000000;

  return result;
}

INTERNAL r32
random_unilateral(void)
{
  LOCAL_PERSIST u32 seed = time(NULL);

  r32 result = 0.0f;

  // rand() implements mutexes so each thread has to run serially
  result = (r32)rand_r(&seed) / (r32)RAND_MAX;

  return result;
}

INTERNAL r32
random_bilateral(void)
{
  r32 result = 0.0f;

  result = -1.0f + 2.0f * random_unilateral();

  return result;
}

// IEEE 754 is in essense a compression algorithm, i.e. compressing all numbers from negative to positive infinity to a finite space of bits
// Therefore, 0.1 + 0.2 != 0.3 (0.300000004) as it can't represent 0.3
// epsilon is an allowable error margin for floating point
//
// this performs an automatic epsilon check
//

INTERNAL u64
locked_add_and_return_previous_value(u64 volatile *previous, u64 add)
{
  // for msdn interlocked, gcc will be __sync_*
  u64 result = __sync_fetch_and_add(previous, add);

  return result;
}


INTERNAL V3
cast_ray(WorkQueue *queue, World *world, V3 ray_origin, V3 ray_direction)
{
  u64 bounces_computed = 0;

  V3 result = {};
  // starts as 1 as we have not attenuated the light at all
  // i.e. when we initially cast a ray, there is no light absorption at all
  V3 attenuation = {1, 1, 1};

  r32 min_hit_distance = 0.001f; // as oppose to using 0?
  // NOTE(Ryan): Ad-hoc value
  r32 tolerance = 0.0001f;

  u32 max_bounce_count = queue->max_bounce_count;
  for (u32 bounce_count = 0;
       bounce_count < max_bounce_count;
       ++bounce_count)
  {
    bounces_computed++;

    // closest hit
    r32 hit_distance = R32_MAX;

    // this is the sky material index, i.e. emitter of light
    u32 hit_material_index = 0;
    V3 next_origin = {};
    V3 next_normal = {};

    for (u32 plane_index = 0;
        plane_index < world->plane_count;
        ++plane_index)
    {
      Plane plane = world->planes[plane_index];

      // for ray line: ray_origin + scale_factor·ray_direction
      // substitute this in for point in plane equation and solve for scale_factor (in this case 't')
      // (in this sense, is it more appropriate to say check for intersection?)
      r32 denom = vec_dot(plane.normal, ray_direction);
      // zero if perpendicular to normal, a.k.a will never intersect plane
      if (denom < -tolerance || denom > tolerance)
      {
        r32 t = (-plane.distance - vec_dot(plane.normal, ray_origin)) / denom;
        if (t > min_hit_distance && t < hit_distance)
        {
          hit_distance = t;
          hit_material_index = plane.material_index;
          next_origin = ray_origin + t * ray_direction;
          next_normal = plane.normal;
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
        if (t_neg > min_hit_distance && t_neg < t_pos)
        {
          t = t_neg;
        }
        
        if (t > min_hit_distance && t < hit_distance)
        {
          hit_distance = t;
          hit_material_index = sphere.material_index;
          next_origin = ray_origin + t * ray_direction;
          next_normal = vec_noz(next_origin - sphere.position);
        }
      }
    }

    if (hit_material_index > 0)
    {
      Material hit_material = world->materials[hit_material_index];

      result += vec_hadamard(attenuation, hit_material.emitted_colour);

      r32 cos_attenuation = 1.0f;
#if 1
      cos_attenuation = vec_dot(-ray_direction, next_normal);
      if (cos_attenuation < 0) cos_attenuation = 0.0f;
#endif
      attenuation = vec_hadamard(attenuation, cos_attenuation * hit_material.reflected_colour);

      ray_origin = next_origin;
      // basic reflection here
      V3 pure_bounce = ray_direction - 2.0f * vec_dot(ray_direction, next_normal) * next_normal;
      V3 random_bounce = vec_noz(next_normal + v3(random_bilateral(), random_bilateral(), random_bilateral()));
      ray_direction = vec_noz(lerp(random_bounce, pure_bounce, hit_material.scatter));

      // black dots are if reflected and never hit sky
      // could also be black say if purely red object attenuates the ray such that it does not reflect any green light so a green object will appear black as the red object as knocked out all the green
    }
    else
    {
      Material hit_material = world->materials[hit_material_index];

      result += vec_hadamard(attenuation, hit_material.emitted_colour);

      break;
    }
  }

  locked_add_and_return_previous_value(&queue->bounces_computed, bounces_computed);

  return result;
}

INTERNAL u32 *
get_pixel_pointer(ImageU32 *image, u32 x, u32 y)
{
  u32 *result = NULL;
  
  result = image->pixels + y * image->width + x;

  return result;
}

INTERNAL b32
render_tile(WorkQueue *queue)
{
  u64 work_order_index = locked_add_and_return_previous_value(&queue->next_work_order_index, 1);
  if (work_order_index >= queue->work_order_count)
  {
    return false;
  }

  WorkOrder *order = queue->work_orders + work_order_index;
  World *world = order->world; 
  ImageU32 *image = order->image;
  u32 x_min = order->x_min;
  u32 y_min = order->y_min;
  u32 one_past_x_max = order->one_past_x_max; 
  u32 one_past_y_max = order->one_past_y_max;

  // right hand rule here to derive these?
  /* rays around the camera. so, want the camera to have a coordinate system, i.e. set of axis
   */
  V3 camera_pos = {0, -10, 1};
  // we are looking through -'z', i.e. opposite direction to what our camera z axis is
  V3 camera_z = vec_noz(camera_pos);
  // cross our z with universal z
  V3 camera_x = vec_noz(vec_cross({0, 0, 1}, camera_z));
  V3 camera_y = vec_noz(vec_cross(camera_z, camera_x));

  r32 film_dist = 1.0f;
  r32 film_w = 1.0f;
  r32 film_h = 1.0f;
  // aspect ratio correction
  if (image->width > image->height)
  {
    film_h = film_w * ((r32)image->height / (r32)image->width);  
  }
  if (image->height > image->width)
  {
    film_w = film_h * ((r32)image->width / (r32)image->height);  
  }
  r32 half_film_w = 0.5f * film_w;
  r32 half_film_h = 0.5f * film_h;
  V3 film_centre = camera_pos - (film_dist * camera_z);

  r32 half_pix_w = 0.5f / image->width;
  r32 half_pix_h = 0.5f / image->height;

  u32 rays_per_pixel = queue->rays_per_pixel;
  for (u32 y = y_min; 
       y < one_past_y_max;
       ++y)
  {
    u32 *out = get_pixel_pointer(image, x_min, y);

    // for camera, z axis is looking from, x and y determine plane aperture 
    r32 film_y = -1.0f + 2.0f * ((r32)y / (r32)image->height);
    for (u32 x = x_min; 
         x < one_past_x_max; 
         ++x)
    {
      r32 film_x = -1.0f + 2.0f * ((r32)x / (r32)image->width);

      V3 colour = {};
      r32 contrib = 1.0f / (r32)rays_per_pixel;
      for (u32 ray_index = 0;
          ray_index < rays_per_pixel;
          ++ray_index)
      {
        // we can get some anti-aliasing here
        r32 jitter_offx = film_x + random_bilateral() * half_pix_w;
        r32 jitter_offy = film_y + random_bilateral() * half_pix_h;

        // need to do half width as from centre
        V3 film_p = film_centre + (jitter_offx * half_film_w * camera_x) + (jitter_offy * half_film_h * camera_y);

        V3 ray_origin = camera_pos;
        V3 ray_direction = vec_noz(film_p - camera_pos);

        // colour is a sum of a series of ray casts
        colour += contrib * cast_ray(queue, world, ray_origin, ray_direction);
      }

      V4 bmp_srgb255 = { 
        255.0f, 
        255.0f * exact_linear1_to_srgb1(colour.r),
        255.0f * exact_linear1_to_srgb1(colour.g),
        255.0f * exact_linear1_to_srgb1(colour.b)
      };

      u32 bmp_value = pack_4x8(bmp_srgb255);

      *out++ = bmp_value;

      //printf("\rRaycasting %d%%...    ", (y * 100 / output_height));
    }
  }
  
  locked_add_and_return_previous_value(&queue->tiles_retired_count, 1);

  return true;
}

INTERNAL int
worker_thread(void *arg)
{
  WorkQueue *work_queue = (WorkQueue *)arg;
  // keep calling if there is work to be done
  while (render_tile(work_queue)) {}

  return 0;
}

// IMPORTANT(Ryan):
// The Linux kernel sees threads and processes as the same, i.e. concurrent task structs
// In userspace, a thread is a task that shares its memory space
//
// cpu atomic instructions ensure that you have exclusive access to a variable, i.e. read and write to it with no other interference
// mutexes are programming constructs built on top of this to create locks to ensure some code waits before another executes. say, atomic add it to 1 to indicate code should wait.
INTERNAL void
create_thread(void *parameter)
{
#if 1
  // windows create_thread defaults to 1M
  u32 stack_size = MEGABYTES(1); 
  char *stack = (char *)malloc(stack_size);
  if (stack == NULL)
  {
    EBP();
    return;
  }

  // NOTE(Ryan): Stack grows down
  // passing SIGCHLD as a flag would allow us to use wait()
  //int clone_flags = (0 | CLONE_VM | 
  //                   CLONE_FS | 
  //                   CLONE_FILES | 
  //                   CLONE_SYSVSEM | 
  //                   CLONE_SIGHAND | 
  //                   CLONE_PARENT_SETTID | 
  //                   CLONE_CHILD_CLEARTID |
  //                   CLONE_THREAD ); 
  //                   //CLONE_SETTLS | 

  // only see 1.6x speed up? 
  int thread_id = clone(worker_thread, stack + stack_size, CLONE_VM, parameter);
  if (thread_id == -1)
  {
    EBP();
    return;
  }
#else
  pthread_attr_t attr;
  pthread_t tid;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int result = pthread_create(&tid, &attr, worker_thread, parameter);
  pthread_attr_destroy(&attr);
#endif

}

int
main(int argc, char *argv[])
{
  // microsecond resolution; even millisecond would be fine as we are timing large loops
  // printf("Clocks per second: %ld\n", CLOCKS_PER_SEC);

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
  // in the final scene, the sphere and plane will reflect the sky's colour attenuated to a certain level
  Material materials[6] = {};
  materials[0].emitted_colour = {0.3f, 0.4f, 0.5f};
  materials[1].reflected_colour = {0.5f, 0.5f, 0.5f};
  materials[2].reflected_colour = {0.7f, 0.5f, 0.3f};
  materials[3].emitted_colour = {4.0f, 0.0f, 0.0f};
  materials[4].reflected_colour = {0.2f, 0.8f, 0.2f};
  materials[4].scatter = 0.7f; 
  materials[5].reflected_colour = {0.4f, 0.8f, 0.9f};
  materials[5].scatter = 0.85f; 

  Plane planes[1] = {};
  planes[0].normal = {0, 0, 1};
  planes[0].distance = 0;
  planes[0].material_index = 1;

  Sphere spheres[4] = {};
  spheres[0].position = {0, 0, 0};
  spheres[0].radius = 1.0f;
  spheres[0].material_index = 2;
  spheres[1].position = {3, -2, 0};
  spheres[1].radius = 1.0f;
  spheres[1].material_index = 3;
  spheres[2].position = {-2, -1, 2};
  spheres[2].radius = 1.0f;
  spheres[2].material_index = 4;
  spheres[3].position = {1, -1, 3};
  spheres[3].radius = 1.0f;
  spheres[3].material_index = 5;

  World world = {};
  world.material_count = ARRAY_COUNT(materials);
  world.materials = materials;
  world.plane_count = ARRAY_COUNT(planes);
  world.planes = planes;
  world.sphere_count = ARRAY_COUNT(spheres);
  world.spheres = spheres;


  ImageU32 image = create_image_u32(1280, 720);
  if (image.pixels != NULL)
  {

    u64 start_clock = get_wall_clock();
    u64 end_clock;
    
    u32 core_count = (u32)get_nprocs(); // logical cores
    // increasing the number to 16, 32, 64, 128 keep increasing speed?
    //u32 core_count = 16 // logical cores

    // for an uneven divisor, we want too many, not too few
    // i.e. want to always be able to get to the end of a row
    u32 tile_width = image.width / core_count;
    u32 tile_height = tile_width;

    //tile_width = tile_height = 64;

    u32 tile_count_x = (image.width + tile_width - 1) / tile_width;
    u32 tile_count_y = (image.height + tile_height - 1) / tile_height;
    u32 total_tile_count = tile_count_x * tile_count_y;

    // from k/tile we can say if it will fit into L1 cache
    printf("Configuration: %d cores with %d tiles, %dx%d (%ldk/tile) tiles\n", 
        core_count, total_tile_count, tile_width, tile_height, tile_width * tile_height * sizeof(u32) / 1024);

    WorkQueue work_queue = {};
    work_queue.max_bounce_count = 8;
    work_queue.rays_per_pixel = 32;
    work_queue.work_orders = (WorkOrder *)malloc(sizeof(WorkOrder) * work_queue.work_order_count);
    WorkOrder *work_order = work_queue.work_orders;

    for (u32 tile_y = 0;
         tile_y < tile_count_y;
         ++tile_y)
    {
      u32 min_y = tile_y * tile_height;
      u32 max_y = min_y + tile_height;
      if (max_y > image.height)
      {
        max_y = image.height;
      }

      for (u32 tile_x = 0;
          tile_x < tile_count_x;
          ++tile_x)
      {
        u32 min_x = tile_x * tile_width;
        u32 max_x = min_x + tile_width;
        if (max_x > image.width)
        {
          max_x = image.width;
        }

        work_order->world = &world;
        work_order->image = &image;
        work_order->x_min = min_x;
        work_order->y_min = min_y; 
        work_order->one_past_x_max = max_x; 
        work_order->one_past_y_max = max_y;

        work_order++;
      }

    }

    // IMPORTANT(Ryan): Although not strictly necessary as quite sure linux creating threads will fence itself,
    // do this to create a memory fence (ensure core 0 flushes its values for other cores)
    locked_add_and_return_previous_value(&work_queue.next_work_order_index, 0);

#if 1
    for (u32 core_index = 1;
         core_index < core_count;
         ++core_index)
    {
      create_thread(&work_queue);
    }
#endif

    while (work_queue.tiles_retired_count < total_tile_count)
    {
      if (render_tile(&work_queue))
      {
        // only show if we render it, to reduce output
        printf("\rRaycasting %d%%    ", (u32)work_queue.tiles_retired_count * 100 / total_tile_count);
        fflush(stdout);
      }
    }

    end_clock = get_wall_clock();
    u64 time_elapsed_ms = end_clock - start_clock;

    // this time is still wrong?
    //r64 time_elapsed_ms = 1000.0 * (r64)time_elapsed / (CLOCKS_PER_SEC * core_count);
    // r64 time_elapsed_ms = 1000.0 * (r64)time_elapsed / (CLOCKS_PER_SEC);
    printf("\n");
    printf("Raycasting time: %ldms\n", time_elapsed_ms);
    printf("Bounces computed: %lu\n", work_queue.bounces_computed);
    // we want to generate a metric that is constant across runs so that we can ascertain if we have made performace improvements  
    // currently: 0.000054ms/bounce
    printf("Performance: %fms/bounce\n", (r64)time_elapsed_ms / work_queue.bounces_computed);

    write_image_u32_to_bmp(&image, "output.bmp");

    printf("\nDone\n");
  }

  return 0;
}
