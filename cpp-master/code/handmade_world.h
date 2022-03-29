/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#define MAX_SIM_REGION_ENTITY_COUNT (2*8192)
struct entity;

struct world_position
{
    // TODO(casey): It seems like we have to store ChunkX/Y/Z with each
    // entity because even though the sim region gather doesn't need it
    // at first, and we could get by without it, entity references pull
    // in entities WITHOUT going through their world_chunk, and thus
    // still need to know the ChunkX/Y/Z
    
    s32 ChunkX;
    s32 ChunkY;
    s32 ChunkZ;
    
    // NOTE(casey): These are the offsets from the chunk center
    v3 Offset_;
};

// TODO(casey): Could make this just tile_chunk and then allow multiple tile chunks per X/Y/Z
#define WORLD_BLOCK_SIZE (1 << 16)
struct world_entity_block
{
    world_entity_block *Next;
    u16 EntityDataSize;
    
    u8 EntityData[WORLD_BLOCK_SIZE - 10];
};

struct world_chunk
{
    world_chunk *NextInHash;
    world_entity_block *FirstBlock;
    
    s32 ChunkX;
    s32 ChunkY;
    s32 ChunkZ;
};

struct world_room
{
    world_position MinPos;
    world_position MaxPos;
};

struct world
{
    ticket_mutex ChangeTicket;
    
    v3 ChunkDimInMeters;
    random_series GameEntropy; // NOTE(casey): This is entropy that DOES affect the gameplay
    
    u32 LastUsedEntityStorageIndex; // TODO(casey): Worry about this wrapping - free list for IDs?
    
    world_entity_block *FirstFree;
    
    // TODO(casey): WorldChunkHash should probably switch to pointers IF
    // tile entity blocks continue to be stored en masse directly in the tile chunk!
    // NOTE(casey): A the moment, this must be a power of two!
    world_chunk *ChunkHash[4096];
    
    // TODO(casey): Temporary - eventually these will be spatially partitioned, probably?
    u32 RoomCount;
    world_room Rooms[65536];
    
    memory_arena *Arena;
    
    world_chunk *FirstFreeChunk;
    world_entity_block *FirstFreeBlock;
    
    b32 UnpackIsOpen;
    
    world_position UnpackOrigin;
    u32 MaxUnpackedEntityCount;
    u32 UnpackedEntityCount;
    entity *UnpackedEntities;
    entity *NullEntity;
    
    u32 UnpackedEntityThreshold;
};

internal bool32 AreInSameChunk(world *World, world_position *A, world_position *B);
internal v3 Subtract(world *World, world_position *A, world_position *B);
