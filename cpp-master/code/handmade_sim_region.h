/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

/* TODO(casey):

  Traversables should be their own entities, so they can also store GroundP and be interpolated
  Entities should have parents that control their movement
  Pieces should go away, and we should just use multiple entities for objects that have multiple parts

  When we do all this, maybe make GroundP a first-class citizen and set it in the
  generator so we don't get artifacts from jumping on uneven terrain vis a vis the alpha/fog

*/

introspect(sim_region) struct entity_hash
{
    entity *Ptr;
};

introspect(sim_region) struct brain_hash
{
    brain *Ptr;
};

introspect(sim_region) struct sim_region
{
    // TODO(casey): Need a hash table here to map stored entity indices
    // to sim entities!

    world *World;

    world_position Origin;
    rectangle3 Bounds;
    rectangle3 UpdatableBounds;

    u32 MaxBrainCount;
    u32 BrainCount;
    brain *Brains;

    // TODO(casey): Do I really want a hash for this??
    // NOTE(casey): Must be a power of two!
    entity_hash EntityHash[MAX_SIM_REGION_ENTITY_COUNT];
    brain_hash BrainHash[256];

    u64 EntityHashOccupancy[MAX_SIM_REGION_ENTITY_COUNT/64];
    u64 BrainHashOccupancy[256/64];

    b32 DEBUGPreventMovement;
};

internal entity_hash *GetHashFromID(sim_region *SimRegion, entity_id StorageIndex);

struct entity_iterator
{
    entity *Entity;
    sim_region *SimRegion;
    u32 HashIndex;
};
internal entity_iterator IterateAllEntities(sim_region *SimRegion);
internal void Advance(entity_iterator *Iter);

internal void RegisterEntity(sim_region *SimRegion, entity *Entity);
inline void PackTraversableReference(sim_region *SimRegion, traversable_reference *Ref);
inline entity *GetEntityByID(sim_region *SimRegion, entity_id ID);

internal v3 AlignToMovementVoxel(v3 A);

struct voxel_center
{
    rectangle3 Cell;
    v3 Repulsion;
    b32 Occupied;
    b32 Embedded;
};

struct collision_field
{
    v3 Repulsion;
    b32 Occupied;
};

enum occupy_code
{
    Occupy_Untested = 0,
    Occupy_Blocked = 1,
    Occupy_Open = 2,
};

#define VOXEL_STACK_DIM 8
struct voxel_stack
{
    v3 MinCenterP;
    
    v3 Repulsion[VOXEL_STACK_DIM][VOXEL_STACK_DIM][VOXEL_STACK_DIM];
    u8 OccupyCode[VOXEL_STACK_DIM][VOXEL_STACK_DIM][VOXEL_STACK_DIM];

    // TODO(casey): We can encode this in a 16-bit value, eventually
    v3s CellI[VOXEL_STACK_DIM*VOXEL_STACK_DIM*6];

    u32 Depth;
};
