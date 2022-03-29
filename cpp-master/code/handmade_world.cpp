/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

// TODO(casey): Think about what the real safe margin is!
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILES_PER_CHUNK 8

#define TILE_CHUNK_UNINITIALIZED INT32_MAX

internal world_position
NullPosition(void)
{
    world_position Result = {};
    
    Result.ChunkX = TILE_CHUNK_UNINITIALIZED;
    
    return(Result);
}

internal bool32
IsValid(world_position P)
{
    bool32 Result = (P.ChunkX != TILE_CHUNK_UNINITIALIZED);
    return(Result);
}

internal b32x
IsContainedInChunkVolume(world_position MinChunk, world_position TestChunk, world_position MaxChunk)
{
    b32x Result = ((TestChunk.ChunkX >= MinChunk.ChunkX) &&
                   (TestChunk.ChunkY >= MinChunk.ChunkY) &&
                   (TestChunk.ChunkZ >= MinChunk.ChunkZ) &&
                   (TestChunk.ChunkX <= MaxChunk.ChunkX) &&
                   (TestChunk.ChunkY <= MaxChunk.ChunkY) &&
                   (TestChunk.ChunkZ <= MaxChunk.ChunkZ));
    
    return(Result);
}

internal bool32
IsCanonical(real32 ChunkDim, real32 TileRel)
{
    // TODO(casey): Fix floating point math so this can be exact?
    real32 Epsilon = 0.01f;
    bool32 Result = ((TileRel >= -(0.5f*ChunkDim + Epsilon)) &&
                     (TileRel <= (0.5f*ChunkDim + Epsilon)));
    
    return(Result);
}

internal bool32
IsCanonical(world *World, v3 Offset)
{
    bool32 Result = (IsCanonical(World->ChunkDimInMeters.x, Offset.x) &&
                     IsCanonical(World->ChunkDimInMeters.y, Offset.y) &&
                     IsCanonical(World->ChunkDimInMeters.z, Offset.z));
    
    return(Result);
}

internal bool32
AreInSameChunk(world *World, world_position *A, world_position *B)
{
    Assert(IsCanonical(World, A->Offset_));
    Assert(IsCanonical(World, B->Offset_));
    
    bool32 Result = ((A->ChunkX == B->ChunkX) &&
                     (A->ChunkY == B->ChunkY) &&
                     (A->ChunkZ == B->ChunkZ));
    
    return(Result);
}

internal void
ClearWorldEntityBlock(world_entity_block *Block)
{
    Block->Next = 0;
    Block->EntityDataSize = 0;
}

internal world_chunk **
GetWorldChunkInternal(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ)
{
    Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);
    
    // TODO(casey): BETTER HASH FUNCTION!!!!
    uint32 HashValue = 19*ChunkX + 7*ChunkY + 3*ChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->ChunkHash));
    
    world_chunk **Chunk = &World->ChunkHash[HashSlot];
    while(*Chunk &&
          !((ChunkX == (*Chunk)->ChunkX) &&
            (ChunkY == (*Chunk)->ChunkY) &&
            (ChunkZ == (*Chunk)->ChunkZ)))
    {
        Chunk = &(*Chunk)->NextInHash;
    }
    
    return(Chunk);
}

internal world_chunk *
GetWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
              memory_arena *Arena = 0)
{
    world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY, ChunkZ);
    world_chunk *Result = *ChunkPtr;
    if(!Result && Arena)
    {
        if(!World->FirstFreeChunk)
        {
            u32 ChunkCountPerBlock = WORLD_BLOCK_SIZE / sizeof(world_chunk);
            world_chunk *ChunkArray = PushArray(Arena, ChunkCountPerBlock, world_chunk, NoClear());
            for(u32 ChunkIndex = 0;
                ChunkIndex < ChunkCountPerBlock;
                ++ChunkIndex)
            {
                world_chunk *NewChunk = ChunkArray + ChunkIndex;
                NewChunk->NextInHash = World->FirstFreeChunk;
                World->FirstFreeChunk = NewChunk;
            }
        }
        
        Result = World->FirstFreeChunk;
        World->FirstFreeChunk = Result->NextInHash;
        
        Result->FirstBlock = 0;
        Result->ChunkX = ChunkX;
        Result->ChunkY = ChunkY;
        Result->ChunkZ = ChunkZ;
        
        Result->NextInHash = *ChunkPtr;
        *ChunkPtr = Result;
    }
    
    return(Result);
}

internal world_chunk *
RemoveWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ)
{
    TIMED_FUNCTION();
    
    BeginTicketMutex(&World->ChangeTicket);
    
    world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY, ChunkZ);
    world_chunk *Result = *ChunkPtr;
    if(Result)
    {
        *ChunkPtr = Result->NextInHash;
    }
    
    EndTicketMutex(&World->ChangeTicket);
    
    return(Result);
}

internal world *
CreateWorld(v3 ChunkDimInMeters, memory_arena *ParentArena)
{
    world *World = PushStruct(ParentArena, world);
    
    World->ChunkDimInMeters = ChunkDimInMeters;
    World->FirstFree = 0;
    World->Arena = ParentArena;
    World->GameEntropy = RandomSeed(1234);
    World->LastUsedEntityStorageIndex = ReservedBrainID_FirstFree;
    
    World->MaxUnpackedEntityCount = MAX_SIM_REGION_ENTITY_COUNT; // 4*MAX_SIM_REGION_ENTITY_COUNT;
    World->UnpackedEntityThreshold = (World->MaxUnpackedEntityCount - MAX_SIM_REGION_ENTITY_COUNT);
    
    World->UnpackedEntities = PushArray(World->Arena, World->MaxUnpackedEntityCount, entity);
    World->UnpackedEntityCount = 0;
    
    World->NullEntity = PushStruct(World->Arena, entity);
    
    return(World);
}

internal void
RecanonicalizeCoord(real32 ChunkDim, int32 *Tile, real32 *TileRel)
{
    // TODO(casey): Need to do something that doesn't use the divide/multiply method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.
    
    // NOTE(casey): Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO(casey): Assert that we are nowhere near the edges of the world.
    
    int32 Offset = RoundReal32ToInt32(*TileRel / ChunkDim);
    *Tile += Offset;
    *TileRel -= (r32)Offset*ChunkDim;
    
    Assert(IsCanonical(ChunkDim, *TileRel));
}

internal world_position
MapIntoChunkSpace(world *World, world_position BasePos, v3 Offset)
{
    world_position Result = BasePos;
    
    Result.Offset_ += Offset;
    
    RecanonicalizeCoord(World->ChunkDimInMeters.x, &Result.ChunkX, &Result.Offset_.x);
    RecanonicalizeCoord(World->ChunkDimInMeters.y, &Result.ChunkY, &Result.Offset_.y);
    RecanonicalizeCoord(World->ChunkDimInMeters.z, &Result.ChunkZ, &Result.Offset_.z);
    
    return(Result);
}

internal v3
Subtract(world *World, world_position *A, world_position *B)
{
    v3 dTile = {(real32)A->ChunkX - (real32)B->ChunkX,
        (real32)A->ChunkY - (real32)B->ChunkY,
        (real32)A->ChunkZ - (real32)B->ChunkZ};
    
    v3 Result = World->ChunkDimInMeters*dTile + (A->Offset_ - B->Offset_);
    
    return(Result);
}

internal b32
HasRoomFor(world_entity_block *Block, u32 Size)
{
    b32 Result = ((Block->EntityDataSize + Size) <= sizeof(Block->EntityData));
    return(Result);
}

internal void *
UseChunkSpace(world *World, u32 Size, world_chunk *Chunk)
{
    if(!Chunk->FirstBlock || !HasRoomFor(Chunk->FirstBlock, Size))
    {
        if(!World->FirstFreeBlock)
        {
            World->FirstFreeBlock = PushStruct(World->Arena, world_entity_block);
            World->FirstFreeBlock->Next = 0;
        }
        
        world_entity_block *NewBlock = World->FirstFreeBlock;
        World->FirstFreeBlock = NewBlock->Next;
        
        ClearWorldEntityBlock(NewBlock);
        
        NewBlock->Next = Chunk->FirstBlock;
        Chunk->FirstBlock = NewBlock;
    }
    
    world_entity_block *Block = Chunk->FirstBlock;
    
    Assert(HasRoomFor(Block, Size));
    u8 *Dest = (Block->EntityData + Block->EntityDataSize);
    Block->EntityDataSize += SafeTruncateToU16(Size);
    
    return(Dest);
}

internal void *
UseChunkSpace(world *World, u32 Size, world_position At)
{
    BeginTicketMutex(&World->ChangeTicket);
    
    world_chunk *Chunk = GetWorldChunk(World, At.ChunkX, At.ChunkY, At.ChunkZ, World->Arena);
    Assert(Chunk);
    void *Result = UseChunkSpace(World, Size, Chunk);
    
    EndTicketMutex(&World->ChangeTicket);
    
    return(Result);
}

internal void
AddToFreeList(world *World, world_chunk *Old, world_entity_block *FirstBlock, world_entity_block *LastBlock)
{
    BeginTicketMutex(&World->ChangeTicket);
    
    Old->NextInHash = World->FirstFreeChunk;
    World->FirstFreeChunk = Old;
    
    if(FirstBlock)
    {
        Assert(LastBlock);
        LastBlock->Next = World->FirstFreeBlock;
        World->FirstFreeBlock = FirstBlock;
    }
    
    EndTicketMutex(&World->ChangeTicket);
}

internal rectangle3
GetWorldChunkBounds(world *World, s32 ChunkX, s32 ChunkY, s32 ChunkZ)
{
    v3 ChunkCenter = World->ChunkDimInMeters*V3((f32)ChunkX, (f32)ChunkY, (f32)ChunkZ);
    rectangle3 Result = RectCenterDim(ChunkCenter, World->ChunkDimInMeters);
    
    return(Result);
}

internal world_room *
AddWorldRoom(world *World, 
             world_position MinPos,
             world_position MaxPos)
{
    Assert(World->RoomCount < ArrayCount(World->Rooms));
    world_room *Room = World->Rooms + World->RoomCount++;
    Room->MinPos = MinPos;
    Room->MaxPos = MaxPos;
    
    return(Room);
}

internal brain_id
AddBrain(world *World)
{
    brain_id ID = {++World->LastUsedEntityStorageIndex};
    return(ID);
}

internal entity_id
AllocateEntityID(world *World)
{
    entity_id Result = {++World->LastUsedEntityStorageIndex};
    return(Result);
}

internal entity *
AcquireUnpackedEntitySlot(world *World)
{
    entity *Result = 0;
    
    if(World->UnpackedEntityCount < World->MaxUnpackedEntityCount)
    {
        Result = World->UnpackedEntities + World->UnpackedEntityCount++;
    }
    else
    {
        InvalidCodePath;
        Result = World->NullEntity;
    }
    
    return(Result);
}

internal void
PackEntity(world *World, world_position EntityP, entity *Entity)
{
    u16 PackedEntitySize = OffsetOf(entity, DiscardEverythingAfter);
    
    world_position ChunkP = EntityP;
    ChunkP.Offset_ = V3(0, 0, 0);
    v3 ChunkDelta = EntityP.Offset_ - Entity->P;
    v3 OldEntityP = Entity->P;
    Entity->P += ChunkDelta;
    
    void *DestE = (void *)UseChunkSpace(World, PackedEntitySize, ChunkP);
    Copy(PackedEntitySize, Entity, DestE);
}

internal void
EnsureRegionIsUnpacked(game_assets *Assets, world *World, world_position MinChunkP, world_position MaxChunkP,
                       sim_region *SimRegion)
{
    TIMED_FUNCTION();
    
    Assert(!World->UnpackIsOpen);
    World->UnpackIsOpen = true;
    
    // TODO(casey): Since we're making this pass here, it does seem like we
    // would want to just keep an updatable hash table, perhaps, and not
    // have to do so many passes over all the entities?
    
    u16 PackedEntitySize = OffsetOf(entity, DiscardEverythingAfter);
    
    v3 dUnpackOrigin = Subtract(World, &World->UnpackOrigin, &SimRegion->Origin);
    for(u32 EntityIndex = 0;
        EntityIndex < World->UnpackedEntityCount;
        )
    {
        entity *Entity = World->UnpackedEntities + EntityIndex;
        b32x RemovedFromUnpacked = false;
        
        if(Entity->Flags & EntityFlag_Deleted)
        {
            RemovedFromUnpacked = true;
        }
        else
        {
            // TODO(casey): Think about what we actually are OK with this value being...
            f32 MaxAllowedDistanceSq = Square(1000.0f);
            f32 DistanceFromOrigin = LengthSq(Entity->P + dUnpackOrigin);
            b32x TooFarForPrecision = (DistanceFromOrigin > MaxAllowedDistanceSq);
            
            world_position EntityP = MapIntoChunkSpace(World, World->UnpackOrigin, Entity->P);
            b32x IsOutsideVolume = !IsContainedInChunkVolume(MinChunkP, EntityP, MaxChunkP);
            b32x CountExceeded = (World->UnpackedEntityCount > World->UnpackedEntityThreshold);
            
            if(TooFarForPrecision || (CountExceeded && IsOutsideVolume))
            {
                Assert(IsOutsideVolume);
                PackEntity(World, EntityP, Entity);
                
                RemovedFromUnpacked = true;
            }
            else
            {
                Entity->P += dUnpackOrigin;
                RegisterEntity(SimRegion, Entity);
            }
        }
        
        if(RemovedFromUnpacked)
        {
            *Entity = World->UnpackedEntities[--World->UnpackedEntityCount];
        }
        else
        {
            ++EntityIndex;
        }
    }
    World->UnpackOrigin = SimRegion->Origin;
    
    for(int32 ChunkZ = MinChunkP.ChunkZ;
        ChunkZ <= MaxChunkP.ChunkZ;
        ++ChunkZ)
    {
        for(int32 ChunkY = MinChunkP.ChunkY;
            ChunkY <= MaxChunkP.ChunkY;
            ++ChunkY)
        {
            for(int32 ChunkX = MinChunkP.ChunkX;
                ChunkX <= MaxChunkP.ChunkX;
                ++ChunkX)
            {
                world_chunk *Chunk = RemoveWorldChunk(World, ChunkX, ChunkY, ChunkZ);
                if(Chunk)
                {
                    Assert(Chunk->ChunkX == ChunkX);
                    Assert(Chunk->ChunkY == ChunkY);
                    Assert(Chunk->ChunkZ == ChunkZ);
                    world_position ChunkPosition = {ChunkX, ChunkY, ChunkZ};
                    v3 ChunkDelta = Subtract(World, &ChunkPosition, &World->UnpackOrigin);
                    world_entity_block *FirstBlock = Chunk->FirstBlock;
                    world_entity_block *LastBlock = FirstBlock;
                    for(world_entity_block *Block = FirstBlock;
                        Block;
                        Block = Block->Next)
                    {
                        LastBlock = Block;
                        
                        for(u16 At = 0;
                            At < Block->EntityDataSize;
                            )
                        {
                            entity *Dest = AcquireUnpackedEntitySlot(World);
                            Copy(PackedEntitySize, Block->EntityData + At, Dest);
                            ZeroSize(sizeof(entity) - PackedEntitySize,
                                     (u8 *)Dest + PackedEntitySize);
                            At += PackedEntitySize;
                            
                            Dest->P += ChunkDelta;
                            
                            FillUnpackedEntity(Assets, SimRegion, Dest);
                            RegisterEntity(SimRegion, Dest);
                        }
                    }
                    
                    AddToFreeList(World, Chunk, FirstBlock, LastBlock);
                }
            }
        }
    }
    
    DEBUG_VALUE(World->UnpackedEntityCount);
}

internal void
RepackEntitiesAsNecessary(world *World, world_position ExpectedMinChunkP, world_position ExpectedMaxChunkP)
{
    TIMED_FUNCTION();
    
    Assert(World->UnpackIsOpen);
    World->UnpackIsOpen = false;
}

internal void
ClearUnpackedEntityCache(world *World)
{
    for(u32 EntityIndex = 0;
        EntityIndex < World->UnpackedEntityCount;
        ++EntityIndex)
    {
        entity *Entity = World->UnpackedEntities + EntityIndex;
        world_position EntityP = MapIntoChunkSpace(World, World->UnpackOrigin, Entity->P);
        PackEntity(World, EntityP, Entity);
    }
    
    World->UnpackedEntityCount = 0;
}