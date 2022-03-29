/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#define DEFAULT_CAMERA_PITCH 0.125f*Pi32
#define DEFAULT_CAMERA_UP V3(0, 0.9238795f, 0.38268346f)

struct game_mode_world;

struct pairwise_collision_rule
{
    bool32 CanCollide;
    uint32 IDA;
    uint32 IDB;
    
    pairwise_collision_rule *NextInHash;
};
struct game_state;
internal void AddCollisionRule(game_mode_world *WorldMode, uint32 StorageIndexA, uint32 StorageIndexB, bool32 ShouldCollide);
internal void ClearCollisionRulesFor(game_mode_world *WorldMode, uint32 StorageIndex);

#if 0
struct particle_cel
{
    real32 Density;
    v3 VelocityTimesDensity;
};
struct particle
{
    bitmap_id BitmapID;
    v3 P;
    v3 dP;
    v3 ddP;
    v4 Color;
    v4 dColor;
};
#endif

struct game_camera
{
    entity_id FollowingEntityIndex;
    
    world_position SimulationCenter;
    
    world_position P;
    world_position TargetP;
    
    v3 dP;
    v3 MovementMask;
    
    entity_id InSpecial;
    f32 tInSpecial;
};

struct particle_cache;
struct game_mode_world
{
    world *World;
    game_camera Camera;
    
    v3 StandardRoomDimension;
    v3 StandardApronRadius;
    f32 FundamentalUnit;
    f32 TypicalFloorHeight;
    
    random_series EffectsEntropy; // NOTE(casey): This is entropy that doesn't affect the gameplay
    particle_cache *ParticleCache;
    
    v2 LastMouseP;
    b32 UseDebugCamera;
    f32 DebugCameraPitch;
    f32 DebugCameraOrbit;
    f32 DebugCameraDolly;
    v3 DebugCameraPan;
    v3 DebugLightP;
    
    f32 CameraPitch;
    f32 CameraOrbit;
    f32 CameraDolly;
    
#if 0
#define PARTICLE_CEL_DIM 32
    u32 NextParticle;
    particle Particles[256];
    particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
#endif
    
#if 1
    b32x UpdatingLighting;
    u32x LightingPattern;
    lighting_solution TestLighting;
#endif
    
    f32 FogMin;
    f32 FogSpan;
    f32 AlphaMin;
    f32 AlphaSpan;
    
    f32 TimeStepScale;
    b32 SingleStep;
    b32 StepLatch;
};

struct world_sim
{
    sim_region *SimRegion;
    temporary_memory SimMemory;
};

struct world_sim_work
{
    world_position SimCenterP;
    rectangle3 SimBounds;
    game_mode_world *WorldMode;
    f32 dt;
    game_state *GameState;
};

internal void PlayWorld(game_state *GameState);
