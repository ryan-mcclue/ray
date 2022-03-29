/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

inline brain_id
AddBrain(sim_region *Region)
{
    brain_id Result = AddBrain(Region->World);
    return(Result);
}

inline entity_id
AllocateEntityID(sim_region *Region)
{
    entity_id Result = AllocateEntityID(Region->World);
    return(Result);
}

inline entity_traversable_point *
GetTraversable(entity *Entity, u32 Index)
{
    entity_traversable_point *Result = 0;
    if(Entity)
    {
        Assert(Index < Entity->TraversableCount);
        Result = Entity->Traversables + Index;
    }
    return(Result);
}

inline entity_traversable_point *
GetTraversable(sim_region *SimRegion, traversable_reference Reference)
{
    entity_traversable_point *Result = GetTraversable(GetEntityByID(SimRegion, Reference.Entity), Reference.Index);
    return(Result);
}

inline entity_traversable_point
GetSimSpaceTraversable(entity *Entity, u32 Index)
{
    entity_traversable_point Result = {};
    if(Entity)
    {
        Result.P = Entity->P;

        entity_traversable_point *Point = GetTraversable(Entity, Index);
        if(Point)
        {
            // TODO(casey): This wants to be rotated eventually!
            Result.P += Point->P;
            Result.Occupier = Point->Occupier;
        }
    }

    return(Result);
}

inline entity_traversable_point
GetSimSpaceTraversable(sim_region *SimRegion, traversable_reference Reference)
{
    entity_traversable_point Result =
        GetSimSpaceTraversable(GetEntityByID(SimRegion, Reference.Entity), Reference.Index);
    return(Result);
}

internal void
MarkBit(u64 *Array, umm Index)
{
    umm OccIndex = Index / 64;
    umm BitIndex = Index % 64;
    Array[OccIndex] |= ((u64)1 << BitIndex);
}

internal b32x
IsEmpty(u64 *Array, umm Index)
{
    umm OccIndex = Index / 64;
    umm BitIndex = Index % 64;
    b32x Result = !(Array[OccIndex] & ((u64)1 << BitIndex));
    return(Result);
}

internal void
MarkOccupied(sim_region *SimRegion, entity_hash *Entry)
{
    umm Index = Entry - SimRegion->EntityHash;
    MarkBit(SimRegion->EntityHashOccupancy, Index);
}

internal void
MarkOccupied(sim_region *SimRegion, brain_hash *Entry)
{
    umm Index = Entry - SimRegion->BrainHash;
    MarkBit(SimRegion->BrainHashOccupancy, Index);
}

internal entity_hash *
GetHashFromID(sim_region *SimRegion, entity_id StorageIndex)
{
    Assert(StorageIndex.Value);

    entity_hash *Result = 0;

    uint32 HashValue = StorageIndex.Value;
    for(uint32 Offset = 0;
        Offset < ArrayCount(SimRegion->EntityHash);
        ++Offset)
    {
        uint32 HashMask = (ArrayCount(SimRegion->EntityHash) - 1);
        uint32 HashIndex = ((HashValue + Offset) & HashMask);
        entity_hash *Entry = SimRegion->EntityHash + HashIndex;
        if(IsEmpty(SimRegion->EntityHashOccupancy, HashIndex))
        {
            Result = Entry;
            Result->Ptr = 0;
            break;
        }
        else if(Entry->Ptr->ID.Value == StorageIndex.Value)
        {
            Result = Entry;
            break;
        }
    }

    Assert(Result);
    return(Result);
}

internal brain_hash *
GetHashFromID(sim_region *SimRegion, brain_id StorageIndex)
{
    Assert(StorageIndex.Value);

    brain_hash *Result = 0;

    uint32 HashValue = StorageIndex.Value;
    for(uint32 Offset = 0;
        Offset < ArrayCount(SimRegion->BrainHash);
        ++Offset)
    {
        uint32 HashMask = (ArrayCount(SimRegion->BrainHash) - 1);
        uint32 HashIndex = ((HashValue + Offset) & HashMask);
        brain_hash *Entry = SimRegion->BrainHash + HashIndex;
        if(IsEmpty(SimRegion->BrainHashOccupancy, HashIndex))
        {
            Result = Entry;
            Result->Ptr = 0;
            break;
        }
        else if(Entry->Ptr->ID.Value == StorageIndex.Value)
        {
            Result = Entry;
            break;
        }
    }

    Assert(Result);
    return(Result);
}

inline entity *
GetEntityByID(sim_region *SimRegion, entity_id ID)
{
    entity *Result = 0;
    if(ID.Value)
    {
        entity_hash *Entry = GetHashFromID(SimRegion, ID);
        Result = Entry ? Entry->Ptr : 0;
    }

    return(Result);
}

inline b32x
EntityOverlapsRectangle(v3 P, rectangle3 EntityVolume, rectangle3 Rect)
{
    b32x Result = RectanglesIntersect(Offset(EntityVolume, P), Rect);

    return(Result);
}

inline b32x
EntityOverlapsEntity(entity *A, entity *B)
{
    b32x Result = RectanglesIntersect(Offset(A->CollisionVolume, A->P),
                                      Offset(B->CollisionVolume, B->P));

    return(Result);
}

internal brain *
GetOrAddBrain(sim_region *SimRegion, brain_id ID, brain_type Type)
{
    brain *Result = 0;

    brain_hash *Hash = GetHashFromID(SimRegion, ID);
    Result = Hash->Ptr;

    if(!Result)
    {
        Assert(SimRegion->BrainCount < SimRegion->MaxBrainCount);
        Assert(IsEmpty(SimRegion->BrainHashOccupancy, Hash - SimRegion->BrainHash));
        Result = SimRegion->Brains + SimRegion->BrainCount++;
        ZeroStruct(*Result);
        Result->ID = ID;
        Result->Type = Type;

        Hash->Ptr = Result;

        MarkOccupied(SimRegion, Hash);
    }

    return(Result);
}

inline void
AddEntityToHash(sim_region *Region, entity *Entity)
{
    entity_hash *Entry = GetHashFromID(Region, Entity->ID);
    Assert(IsEmpty(Region->EntityHashOccupancy, Entry - Region->EntityHash));
    Entry->Ptr = Entity;
    MarkOccupied(Region, Entry);
}

inline v3
MapIntoSimSpace(sim_region *SimRegion, world_position WorldPos)
{
    v3 Result = Subtract(SimRegion->World, &WorldPos, &SimRegion->Origin);
    return(Result);
}

internal void
RegisterEntity(sim_region *SimRegion, entity *Entity)
{
    if(EntityOverlapsRectangle(Entity->P, Entity->CollisionVolume, SimRegion->Bounds))
    {
        AddEntityToHash(SimRegion, Entity);

        if(EntityOverlapsRectangle(Entity->P, Entity->CollisionVolume, SimRegion->UpdatableBounds))
        {
            Entity->Flags |= EntityFlag_Active;
        }
        else
        {
            Entity->Flags &= ~EntityFlag_Active;
        }

        if(Entity->BrainID.Value)
        {
            brain *Brain = GetOrAddBrain(SimRegion, Entity->BrainID, (brain_type)Entity->BrainSlot.Type);
            u8 *Ptr = (u8 *)&Brain->Array;
            Ptr += sizeof(entity *)*Entity->BrainSlot.Index;
            Assert(Ptr <= ((u8 *)Brain + sizeof(brain) - sizeof(entity *)));
            *((entity **)Ptr) = Entity;
        }
    }
}

internal sim_region *
BeginWorldChange(game_assets *Assets, memory_arena *SimArena, world *World, world_position Origin, rectangle3 SimBounds, rectangle3 ApronBounds, real32 dt)
{
    TIMED_FUNCTION();

    BEGIN_BLOCK("SimArenaAlloc");
    sim_region *SimRegion = PushStruct(SimArena, sim_region, NoClear());
    END_BLOCK();

    BEGIN_BLOCK("SimArenaClear");
    ZeroStruct(SimRegion->EntityHashOccupancy);
    ZeroStruct(SimRegion->BrainHashOccupancy);
    END_BLOCK();

    SimRegion->World = World;

    SimRegion->Origin = Origin;
    SimRegion->Bounds = ApronBounds;
    SimRegion->UpdatableBounds = SimBounds;

    SimRegion->MaxBrainCount = 512;
    SimRegion->BrainCount = 0;
    SimRegion->Brains = PushArray(SimArena, SimRegion->MaxBrainCount, brain, NoClear());

    SimRegion->DEBUGPreventMovement = false;

    world_position MinChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));

    DEBUG_VALUE(SimRegion->Origin.ChunkX);
    DEBUG_VALUE(SimRegion->Origin.ChunkY);
    DEBUG_VALUE(SimRegion->Origin.ChunkZ);
    DEBUG_VALUE(SimRegion->Origin.Offset_);

    EnsureRegionIsUnpacked(Assets, World, MinChunkP, MaxChunkP, SimRegion);

    return(SimRegion);
}

internal void
EndWorldChange(sim_region *Region)
{
    world *World = Region->World;

    // TODO(casey): Maybe use the new expected camera position here instead?
    world_position MinChunkP = MapIntoChunkSpace(World, Region->Origin, GetMinCorner(Region->Bounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, Region->Origin, GetMaxCorner(Region->Bounds));
    RepackEntitiesAsNecessary(World, MinChunkP, MaxChunkP);
}

inline entity *
CreateEntity(sim_region *Region, entity_id ID)
{
    entity *Result = AcquireUnpackedEntitySlot(Region->World);
    ZeroStruct(*Result);

    Result->ID = ID;
    AddEntityToHash(Region, Result);

    return(Result);
}

inline void
DeleteEntity(sim_region *Region, entity *Entity)
{
    if(Entity)
    {
        Entity->Flags |= EntityFlag_Deleted;
    }
}

internal b32x
IsRoom(entity *Entity)
{
    b32x Result = (Entity->BrainSlot.Type == Type_brain_room);
    return(Result);
}

internal void
UpdateCameraForEntityMovement(game_camera *Camera, sim_region *Region, world *World, entity *Entity, f32 dt, 
                              v3 CameraZ)
{
    Assert(Entity->ID.Value == Camera->FollowingEntityIndex.Value);

    // TODO(casey): Probably don't want to loop over all entities - maintain a
    // separate list of room entities during unpack?
    entity *InRoom = 0;
    entity *SpecialCamera = 0;
    for(entity_iterator Iter = IterateAllEntities(Region);
        Iter.Entity;
        Advance(&Iter))
    {
        entity *TestEntity = Iter.Entity;

        if(IsRoom(TestEntity))
        {
            if(EntityOverlapsEntity(Entity, TestEntity))
            {
                InRoom = TestEntity;
            }
        }

        if(TestEntity->CameraBehavior)
        {
            b32x Pass = EntityOverlapsEntity(Entity, TestEntity);
            if(TestEntity->CameraBehavior & Camera_GeneralVelocityConstraint)
            {
                Pass = Pass && IsInRange(TestEntity->CameraMinVelocity,
                                         Length(Entity->dP),
                                         TestEntity->CameraMaxVelocity);
            }

            if(TestEntity->CameraBehavior & Camera_DirectionalVelocityConstraint)
            {
                Pass = Pass && IsInRange(TestEntity->CameraMinVelocity,
                                         Inner(TestEntity->CameraVelocityDirection, Entity->dP),
                                         TestEntity->CameraMaxVelocity);
            }

            if(Pass)
            {
                if((TestEntity != InRoom) ||
                   (!SpecialCamera))
                {
                    SpecialCamera = TestEntity;
                }
            }
        }
    }

    if(SpecialCamera)
    {
        if(AreEqual(Camera->InSpecial, SpecialCamera->ID))
        {
            Camera->tInSpecial += dt;
        }
        else
        {
            Camera->InSpecial = SpecialCamera->ID;
            Camera->tInSpecial = 0.0f;
        }
    }
    else
    {
        Clear(&Camera->InSpecial);
    }

    v3 P = Subtract(World, &Camera->P, &Region->Origin);
    v3 TargetP = Subtract(World, &Camera->TargetP, &Region->Origin);
    v3 NewTargetP = TargetP;

    f32 EntityFocusZ = 8.0f;
    f32 RoomFocusZ = EntityFocusZ;
    if(InRoom)
    {
        rectangle3 RoomVolume = Offset(InRoom->CollisionVolume, InRoom->P);

        //v3 SimulationCenter = V3(GetCenter(RoomVolume).xy, RoomVolume.Min.z);
        v3 SimulationCenter = V3(GetCenter(RoomVolume).xy, 0);
        SimulationCenter = AlignToMovementVoxel(SimulationCenter);

        Camera->SimulationCenter = MapIntoChunkSpace(World, Region->Origin, SimulationCenter);

        NewTargetP = SimulationCenter;
        f32 TargetOffsetZ = InRoom->CameraOffset.z;

        if(SpecialCamera && (Camera->tInSpecial > SpecialCamera->CameraMinTime))
        {
            if(SpecialCamera->CameraBehavior & Camera_Inspect)
            {
                NewTargetP = SpecialCamera->P;
            }

            if(SpecialCamera->CameraBehavior & Camera_ViewPlayerX)
            {
                NewTargetP.x = Entity->P.x;
            }

            if(SpecialCamera->CameraBehavior & Camera_ViewPlayerY)
            {
                NewTargetP.y = Entity->P.y;
            }

            if(SpecialCamera->CameraBehavior & Camera_Offset)
            {
                NewTargetP = SpecialCamera->P;
                NewTargetP.xy += SpecialCamera->CameraOffset.xy;

                TargetOffsetZ = SpecialCamera->CameraOffset.z;

                Camera->MovementMask = V3(1, 1, 1);
            }
        }

        v3 CameraArm = TargetOffsetZ*CameraZ;
        NewTargetP += CameraArm;
    }

    f32 A = 10.0f;
    f32 B = 5.25f;

    TargetP = NewTargetP;

    v3 Threshold = {4.0f, 2.0f, 1.0f};

    v3 TotalDeltaP = TargetP - P;

    if(AbsoluteValue(TotalDeltaP.E[0]) > Threshold.E[0]) {Camera->MovementMask.E[0] = 1.0f;}
    if(AbsoluteValue(TotalDeltaP.E[1]) > Threshold.E[1]) {Camera->MovementMask.E[1] = 1.0f;}
    if(AbsoluteValue(TotalDeltaP.E[2]) > Threshold.E[2]) {Camera->MovementMask.E[2] = 1.0f;}

    {DEBUG_DATA_BLOCK("Renderer/Pre");
        DEBUG_VALUE(P);
        DEBUG_VALUE(TargetP);
        DEBUG_VALUE(TotalDeltaP);
        DEBUG_VALUE(Camera->MovementMask);
    }

    TotalDeltaP = TotalDeltaP*Camera->MovementMask;

    v3 dPTarget = V3(0, 0, 0);
    v3 TotalDeltadP = (dPTarget - Camera->dP);
    TotalDeltadP = TotalDeltadP*Camera->MovementMask;

    v3 ddP = A*TotalDeltaP + B*TotalDeltadP;
    v3 dP = dt*ddP + Camera->dP;

    v3 DeltaP = 0.5f*dt*dt*ddP + dt*Camera->dP;
    if((LengthSq(DeltaP) < LengthSq(TotalDeltaP)) ||
           (LengthSq((dPTarget - dP)*Camera->MovementMask) < Square(0.001f)))
    {
        P += DeltaP;
    }
    else
    {
        P += TotalDeltaP;
        dP = dPTarget;
        ddP = V3(0, 0, 0);
        Camera->MovementMask = V3(0, 0, 0);
    }

    {DEBUG_DATA_BLOCK("Renderer/Post");
        DEBUG_VALUE(TotalDeltaP);
        DEBUG_VALUE(DeltaP);
    }


    Camera->P = MapIntoChunkSpace(World, Region->Origin, P);
    Camera->dP = dP;
    Camera->TargetP = MapIntoChunkSpace(World, Region->Origin, TargetP);
}

struct test_wall
{
    real32 X;
    real32 RelX;
    real32 RelY;
    real32 DeltaX;
    real32 DeltaY;
    real32 MinY;
    real32 MaxY;
    v3 Normal;
};
internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY,
         real32 *tMin, real32 MinY, real32 MaxY)
{
    bool32 Hit = false;

    real32 tEpsilon = 0.001f;
    if(PlayerDeltaX != 0.0f)
    {
        real32 tResult = (WallX - RelX) / PlayerDeltaX;
        real32 Y = RelY + tResult*PlayerDeltaY;
        if((tResult >= 0.0f) && (*tMin > tResult))
        {
            if((Y >= MinY) && (Y <= MaxY))
            {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                Hit = true;
            }
        }
    }

    return(Hit);
}

internal bool32
CanCollide(entity *A, entity *B)
{
    b32 Result = false;

    if(A != B)
    {
        if(A->ID.Value > B->ID.Value)
        {
            entity *Temp = A;
            A = B;
            B = Temp;
        }

//        if(IsSet(A, EntityFlag_Collides) && IsSet(B, EntityFlag_Collides))
        {
            if(HasArea(A->CollisionVolume) && HasArea(B->CollisionVolume))
            {
                Result = true;
            }
        }
    }

    return(Result);
}

inline b32
IsOccupied(sim_region *SimRegion, traversable_reference Ref)
{
    b32 Result = true;

    entity_traversable_point *Dest = GetTraversable(SimRegion, Ref);
    if(Dest)
    {
        Result = IsValid(Dest->Occupier);
    }

    return(Result);
}

internal b32
TransactionalOccupy(sim_region *SimRegion, entity_id EntityID, traversable_reference *MoverRef, traversable_reference DesiredRef)
{
    b32 Result = false;

    entity_traversable_point *Desired = GetTraversable(SimRegion, DesiredRef);
    if(Desired)
    {
        if(!IsValid(Desired->Occupier))
        {
            Desired->PrevOccupier = Desired->Occupier;
            Desired->Occupier = EntityID;
            Result = true;
        }
    }
    else
    {
        Result = true;
    }

    if(Result)
    {
        entity_traversable_point *Mover = GetTraversable(SimRegion, *MoverRef);
        if(Mover)
        {
            Mover->PrevOccupier = Mover->Occupier;
            Clear(&Mover->Occupier);
        }

        *MoverRef = DesiredRef;
    }

    return(Result);
}

#define MOTION_DISPLACEMENT_SIZE 0.125f

internal v3s GetMovementVoxelIndex(v3 A)
{
    v3 CellDim = MOTION_DISPLACEMENT_SIZE*V3(1, 1, 1);
    v3s Result = RoundToV3S(A / MOTION_DISPLACEMENT_SIZE);

    return Result;
}

internal v3 AlignToMovementVoxel(v3 A)
{
    v3 CellDim = MOTION_DISPLACEMENT_SIZE*V3(1, 1, 1);
    v3 Result = MOTION_DISPLACEMENT_SIZE*(Round(A / MOTION_DISPLACEMENT_SIZE));

    return Result;
}

internal v3 GetVoxelCenterP(voxel_stack *Stack, v3s I)
{
    v3 CellDim = MOTION_DISPLACEMENT_SIZE*V3(1, 1, 1);
    v3 Result = Stack->MinCenterP + V3(I)*CellDim;

    return Result;
}

internal rectangle3 GetVoxelBounds(voxel_stack *Stack, v3s I)
{
    v3 CellDim = MOTION_DISPLACEMENT_SIZE*V3(1, 1, 1);
    rectangle3 Result = RectCenterDim(GetVoxelCenterP(Stack, I), CellDim);
    return Result;
}

internal b32 ShapesCollide(b32 AIsSphere, rectangle3 A,
                           b32 BIsSphere, rectangle3 B)
{
    // TODO(casey): We can probably fold a bunch of these
    // together to make even less operations here.

    v3 RadiusA = 0.5f*GetDim(A);
    v3 RadiusB = 0.5f*GetDim(B);
    v3 Radius = RadiusA + RadiusB;

    v3 CenterA = GetCenter(A);
    v3 CenterB = GetCenter(B);

    v3 CenterDelta = CenterA - CenterB;

    v3 ClosestA = Min(A.Max, Max(A.Min, CenterB));
    v3 ClosestB = Min(B.Max, Max(B.Min, CenterA));

    b32 Spheres = LengthSq(CenterDelta) < Square(Radius.x);
    b32 ARectBSphere = LengthSq(CenterB - ClosestA) < Square(RadiusB.x);
    b32 BRectASphere = LengthSq(CenterA - ClosestB) < Square(RadiusA.x);
    b32 Rects = ((Square(CenterDelta.x) < Square(Radius.x)) &&
                 (Square(CenterDelta.y) < Square(Radius.y)) &&
                 (Square(CenterDelta.z) < Square(Radius.z)));

    b32 Result = ((AIsSphere && BIsSphere && Spheres) ||
                  (AIsSphere && !BIsSphere && BRectASphere) ||
                  (!AIsSphere && BIsSphere && ARectBSphere) ||
                  (!AIsSphere && !BIsSphere && Rects));

    if(DIAGRAM_IsOn() && Result)
    {
        DIAGRAM_Text("%s", __FUNCTION__);
        DIAGRAM_Begin();

        if(AIsSphere && BIsSphere)
        {
            DIAGRAM_Text("SphereA");
            DIAGRAM_Sphere(CenterA, RadiusA.x);

            DIAGRAM_Text("SphereB");
            DIAGRAM_Sphere(CenterB, RadiusB.x);

            DIAGRAM_Line(CenterA, CenterB);
        }

        if(AIsSphere && !BIsSphere)
        {
            DIAGRAM_Text("SphereA");
            DIAGRAM_Sphere(CenterA, RadiusA.x);

            DIAGRAM_Text("BoxB");
            DIAGRAM_Box(B);
            DIAGRAM_Line(ClosestB, CenterA);

            Assert(HasArea(B));
        }

        if(!AIsSphere && BIsSphere)
        {
            DIAGRAM_Text("BoxA");
            DIAGRAM_Box(A);
            DIAGRAM_Text("SphereB");
            DIAGRAM_Sphere(CenterB, RadiusB.x);
            DIAGRAM_Line(ClosestA, CenterB);

            Assert(HasArea(A));
        }

        if(!AIsSphere && !BIsSphere)
        {
            DIAGRAM_Text("BoxA");
            DIAGRAM_Box(A);
            DIAGRAM_Text("BoxB");
            DIAGRAM_Box(B);
        }

        DIAGRAM_Text("Collides: %s", Result ? "yes" : "no");
        DIAGRAM_Overlay();

        DIAGRAM_End();
    }

    return Result;
}

internal collision_field
CollidesAtP(sim_region *SimRegion, entity *Entity, v3 P)
{
    collision_field Result = {};

    rectangle3 MoverRect = Offset(Entity->CollisionVolume, P);
    b32 MoverIsSphere = Entity->Flags & EntityFlag_SphereCollision;

    // TODO(casey): Replace with spatial partition
    int AvgCount = 0;
    for(entity_iterator Iter = IterateAllEntities(SimRegion);
        Iter.Entity;
        Advance(&Iter))
    {
        entity *TestEntity = Iter.Entity;
        if((TestEntity != Entity) && !(TestEntity->Flags & EntityFlag_AllowsMotionOnCollision) &&
               HasArea(TestEntity->CollisionVolume))
        {
            b32 TestIsSphere = TestEntity->Flags & EntityFlag_SphereCollision;

            // NOTE(casey): To maintain collision integrity, always test entities aligned
            // to voxel centers.
            v3 AlignedTestEntityP = AlignToMovementVoxel(TestEntity->P);
            
            // TODO(casey): RectanglesIntersect should probably be changed
            // to be properly >= and < instead of >= and <= ??
            rectangle3 TestRect = Offset(TestEntity->CollisionVolume, AlignedTestEntityP);
            if(ShapesCollide(MoverIsSphere, MoverRect, TestIsSphere, TestRect))
            {
                // TODO(casey): ShapesCollide now has much better
                // unembed direction computation for various cases...
                // We should try to get that out into here?
                v3 UnembedD = GetCenter(MoverRect) - GetCenter(TestRect);
                ++AvgCount;
                Result.Repulsion += UnembedD;
                Result.Occupied = true;
            }
        }
    }

    if(AvgCount)
    {
        Result.Repulsion *= 1.0f / (f32)AvgCount;
    }

    return Result;
}

internal v3
GetClosestPointInBoxConservative(rectangle3 Box, v3 P)
{
    // TODO(casey): Really, we should be doing these tests
    // the other way around, so that boxes are always in a fixed
    // domain, like 0->1.
    f32 Eps = 0.0001f;
    v3 Result = Min(Box.Max - V3(Eps, Eps, Eps), Max(Box.Min, P));
    return(Result);
}

internal v3
RefineVoxelPlacement(rectangle3 Cell, sim_region *SimRegion, entity *Entity, v3 FromP, v3 ToP)
{
    // NOTE(casey): It is expected that FromP/ToP have already been mapped into the cell

    v3 ResultP = FromP;

    f32 tTest = 0.5f;
    f32 tStep = 0.25f;

    // TODO(casey): Probably this can be passed in.
    b32 StartingState = CollidesAtP(SimRegion, Entity, FromP).Occupied;
    if(StartingState)
    {
        tStep = -tStep;
    }

    for(int Refine = 0; Refine < 8; ++Refine)
    {
        Assert(tTest >= 0);
        Assert(tTest <= 1);
        v3 TestP = Lerp(FromP, tTest, ToP);
        b32 Collides = CollidesAtP(SimRegion, Entity, TestP).Occupied;
        DIAGRAM_Color(Collides ? V3(1, 0, 1) : V3(0, 1, 1));
        DIAGRAM_Box(RectCenterDim(TestP, 0.01f*V3(1, 1, 1)));

        f32 tCollides = tTest - tStep;
        f32 tSafe = tTest + tStep;
        tTest = Collides ? tCollides : tSafe;
        ResultP = Collides ? ResultP : TestP;

        tStep *= 0.5f;
    }

    return ResultP;
}

internal u8
GetOccupyCode(voxel_stack *Stack, v3s I)
{
    u8 Result = Stack->OccupyCode[I.z][I.y][I.x];
    return Result;
}

internal void 
PushVoxelStack(voxel_stack *Stack, u8 OriginatorCode, v3s I, sim_region *SimRegion, entity *Entity)
{
    // TODO(casey): There should be a closing bounds here that prevents
    // searches from proceeding outside the space where you could ever
    // possibly produce a closer point.
    collision_field Result = {};

    if(IsInArrayBounds(V3S(VOXEL_STACK_DIM, VOXEL_STACK_DIM, VOXEL_STACK_DIM), I) &&
           (GetOccupyCode(Stack, I) == Occupy_Untested))
    {
        Result = CollidesAtP(SimRegion, Entity, GetVoxelCenterP(Stack, I));
        u8 ThisCode = (u8)(Result.Occupied ? Occupy_Blocked : Occupy_Open);
        Stack->OccupyCode[I.z][I.y][I.x] = ThisCode;
        if(ThisCode >= OriginatorCode)
        {
            Assert(Stack->Depth < ArrayCount(Stack->CellI));
            Stack->CellI[Stack->Depth++] = I;
        }
    }
}

internal b32
StackNotEmpty(voxel_stack *Stack)
{
    b32 Result = (Stack->Depth > 0);
    return Result;
}

internal v3s
PopVoxelStack(voxel_stack *Stack)
{
    Assert(StackNotEmpty(Stack));
    v3s Result = Stack->CellI[--Stack->Depth];
    return Result;
}

int DontCareAboutThat = 1;
internal void
MoveEntity(sim_region *SimRegion, entity *Entity, real32 dt, v3 ddP)
{
    world *World = SimRegion->World;
    if(DontCareAboutThat) return;

    DIAGRAM_Filter(Entity->ID.Value);

    DIAGRAM_Text("Move Entity");
    DIAGRAM_Begin();

    // TODO(casey): This should probably move out of this routine
    // and be in some "requested physics" stuff, and we just take
    // the desired point here.
    v3 DeltaP = (0.5f*ddP*Square(dt) + Entity->dP*dt);
    Entity->dP = ddP*dt + Entity->dP;

    v3 ToP = Entity->P + DeltaP;
    v3 FromP = Entity->P;

    // TODO(casey): We want to reverse-solve for the acceleration
    // and velocity here, probably

    v3s MaxDeltaDim = V3S(VOXEL_STACK_DIM - 2, VOXEL_STACK_DIM - 2, VOXEL_STACK_DIM - 2);

    v3 VoxelStartP = AlignToMovementVoxel(FromP);
    if(CollidesAtP(SimRegion, Entity, VoxelStartP).Occupied)
    {
        ToP = FromP;
    }
    
    
    v3 CellDim = MOTION_DISPLACEMENT_SIZE*V3(1, 1, 1);
    v3s DeltaPI = FloorToV3S(DeltaP / MOTION_DISPLACEMENT_SIZE);
    DeltaPI.x = Clamp(-MaxDeltaDim.x, DeltaPI.x, MaxDeltaDim.x);
    DeltaPI.y = Clamp(-MaxDeltaDim.y, DeltaPI.y, MaxDeltaDim.y);
    DeltaPI.z = Clamp(-MaxDeltaDim.z, DeltaPI.z, MaxDeltaDim.z);
    v3 DeltaPClamp = MOTION_DISPLACEMENT_SIZE*V3(DeltaPI);
    v3 VoxelEndP = VoxelStartP + DeltaPClamp;

    v3 VoxelMinCorner = Min(VoxelStartP, VoxelEndP) - CellDim;
    v3 VoxelMaxCorner = Max(VoxelStartP, VoxelEndP) + CellDim;
    v3 VoxelSpan = (VoxelMaxCorner - VoxelMinCorner) / CellDim;
    Assert(VoxelSpan.x <= VOXEL_STACK_DIM);
    Assert(VoxelSpan.y <= VOXEL_STACK_DIM);
    Assert(VoxelSpan.z <= VOXEL_STACK_DIM);

    voxel_stack Stack = {};
    Stack.MinCenterP = VoxelMinCorner;

    f32 BestDistanceSq = F32Max;
    v3s InitialCellI = GetMovementVoxelIndex(VoxelStartP - VoxelMinCorner);
    v3s TargetCellI = GetMovementVoxelIndex(VoxelStartP - VoxelMaxCorner);
    v3s BestCellI = InitialCellI;
    PushVoxelStack(&Stack, Occupy_Untested, InitialCellI, SimRegion, Entity);
    
    DIAGRAM_Color(0.5, 0.5, 0.5);
    DIAGRAM_Box(RectMinMax(VoxelMinCorner, VoxelMaxCorner));
    DIAGRAM_Color(0.5, 0.5, 0.5);
    DIAGRAM_Line(FromP, ToP);

    DIAGRAM_Color(1, 1, 1);
    DIAGRAM_Box(RectCenterDim(ToP, 0.1f*V3(1, 1, 1)));

    // TODO(casey): Do initial check to see if we're embedded, and use that to
    // allow or disallow stack.  Probably need the stack to store embedded/not.
    b32 FoundBest = false;
    while(!FoundBest && StackNotEmpty(&Stack))
    {
        v3s CurrentCellI = PopVoxelStack(&Stack);
        v3 CellCenterP = GetVoxelCenterP(&Stack, CurrentCellI);
        rectangle3 Cell = GetVoxelBounds(&Stack, CurrentCellI);
        u8 OccupyCode = GetOccupyCode(&Stack, CurrentCellI);

        // TODO(casey): We should probably store corners and only test when they are untested
        DIAGRAM_Text("Collision Voxel");
        DIAGRAM_Begin();

        DIAGRAM_Color(0.0, 0.0, 0.0);
        if(OccupyCode == Occupy_Open)
        {
            f32 DistanceSq = LengthSq(ToP - GetClosestPointInBoxConservative(Cell, ToP));
            if(BestDistanceSq > DistanceSq)
            {
                BestDistanceSq = DistanceSq;
                BestCellI = CurrentCellI;
                if(AreEqual(BestCellI, TargetCellI))
                {
                    FoundBest = true;
                }
                DIAGRAM_Color(1.0, 1.0, 0.0);
            }
        }
#if 0
        DIAGRAM_Box(Corners.Cell);
#endif

        if(!FoundBest)
        {
            int SideX = (DeltaP.x < 0) ? -1 : 1;
            int SideY = (DeltaP.y < 0) ? -1 : 1;
            int SideZ = (DeltaP.z < 0) ? -1 : 1;

            PushVoxelStack(&Stack, OccupyCode, CurrentCellI + V3S(SideX, 0, 0), SimRegion, Entity);
            PushVoxelStack(&Stack, OccupyCode, CurrentCellI + V3S(0, SideY, 0), SimRegion, Entity);
            PushVoxelStack(&Stack, OccupyCode, CurrentCellI + V3S(0, 0, SideZ), SimRegion, Entity);
            PushVoxelStack(&Stack, OccupyCode, CurrentCellI + V3S(-SideX, 0, 0), SimRegion, Entity);
            PushVoxelStack(&Stack, OccupyCode, CurrentCellI + V3S(0, -SideY, 0), SimRegion, Entity);
            PushVoxelStack(&Stack, OccupyCode, CurrentCellI + V3S(0, 0, -SideZ), SimRegion, Entity);
        }

        DIAGRAM_End();
    }

    rectangle3 BestCell = GetVoxelBounds(&Stack, BestCellI);

    // TODO(casey): Can this be improved?  We don't know where we "came from"
    // since we might move along a bended path, so starting at FromP may be
    // very wrong?
    v3 LocalToP = GetClosestPointInBoxConservative(BestCell, ToP);

    DIAGRAM_Text("Refinement");
    DIAGRAM_Begin();
    v3 NewP = RefineVoxelPlacement(BestCell, SimRegion, Entity,
                                   GetVoxelCenterP(&Stack, BestCellI), LocalToP);
    DIAGRAM_End();

    DIAGRAM_End();

#if 0
    v3s NewPIndex = GetIndexForP(&Stack.Grid, NewP);
    Assert(AreEqual(NewPIndex, BestCellI));
    if(SawEmbedding)
    {
        DIAGRAM_Capture(true);
    }
#endif

    if(!SimRegion->DEBUGPreventMovement)
    {
        Entity->P = NewP;
    }
}

internal b32x
OverlappingEntitiesExist(sim_region *SimRegion, rectangle3 Bounds)
{
    b32x Result = false;

    for(entity_iterator Iter = IterateAllEntities(SimRegion);
        Iter.Entity;
        Advance(&Iter))
    {
        entity *TestEntity = Iter.Entity;
        if(EntityOverlapsRectangle(TestEntity->P, TestEntity->CollisionVolume, Bounds))
        {
            Result = true;
            break;
        }
    }

    return(Result);
}

enum traversable_search_flag
{
    TraversableSearch_Unoccupied = 0x1,
    TraversableSearch_ClippedZ = 0x2,
};
// TODO(casey): Why doesn't this just return a traversable_reference
internal b32
GetClosestTraversable(sim_region *SimRegion, v3 FromP, traversable_reference *Result,
                      u32 Flags = 0)
{
    TIMED_FUNCTION();

    b32 Found = false;

    // TODO(casey): Make spatial queries easy for things!
    r32 ClosestDistanceSq = Square(1000.0f);

    for(entity_iterator Iter = IterateAllEntities(SimRegion);
        Iter.Entity;
        Advance(&Iter))
    {
        entity *TestEntity = Iter.Entity;
        for(u32 PIndex = 0;
            PIndex < TestEntity->TraversableCount;
            ++PIndex)
        {
            entity_traversable_point P =
                GetSimSpaceTraversable(TestEntity, PIndex);
            if(!(Flags & TraversableSearch_Unoccupied) || !IsValid(P.Occupier))
            {
                v3 ToPoint = P.P - FromP;
                if(!(Flags & TraversableSearch_ClippedZ) ||
                   ((ToPoint.z >= -2.0f) &&
                    (ToPoint.z <= 2.0f)))
                {
                    if(Flags & TraversableSearch_ClippedZ)
                    {
                        ToPoint.z = 0.0f;
                    }

                    real32 TestDSq = LengthSq(ToPoint);
                    if(ClosestDistanceSq > TestDSq)
                    {
                        // P.P;
                        Result->Entity = TestEntity->ID;
                        Result->Index = PIndex;
                        ClosestDistanceSq = TestDSq;
                        Found = true;
                    }
                }
            }
        }
    }

    if(!Found)
    {
        Result->Entity.Value = 0;
        Result->Index = 0;
    }

    return(Found);
}

internal b32
GetClosestTraversableAlongRay(sim_region *SimRegion, v3 FromP, v3 Dir,
                              traversable_reference Skip, traversable_reference *Result,
                              u32 Flags = 0)
{
    TIMED_FUNCTION();

    b32 Found = false;

    for(u32 ProbeIndex = 0;
        ProbeIndex < 5;
        ++ProbeIndex)
    {
        v3 SampleP = FromP + 0.5f*(r32)ProbeIndex*Dir;
        if(GetClosestTraversable(SimRegion, SampleP, Result, Flags))
        {
            if(!AreEqual(Skip, *Result))
            {
                Found = true;
                break;
            }
        }
    }

    return(Found);
}

struct closest_entity
{
    entity *Entity;
    v3 Delta;
    r32 DistanceSq;
};
internal closest_entity
GetClosestEntityWithBrain(sim_region *SimRegion, v3 P, brain_type Type, r32 MaxRadius = 20.0f)
{
    closest_entity Result = {};
    Result.DistanceSq = Square(MaxRadius);

    for(entity_iterator Iter = IterateAllEntities(SimRegion);
        Iter.Entity;
        Advance(&Iter))
    {
        entity *TestEntity = Iter.Entity;
        if(IsType(TestEntity->BrainSlot, Type))
        {
            v3 TestDelta = TestEntity->P - P;
            real32 TestDSq = LengthSq(TestDelta);
            if(Result.DistanceSq > TestDSq)
            {
                Result.Entity = TestEntity;
                Result.DistanceSq = TestDSq;
                Result.Delta = TestDelta;
            }
        }
    }

    return(Result);
}

internal void
FindNextEntity(entity_iterator *Iter)
{
    Iter->Entity = 0;

    while(Iter->HashIndex < ArrayCount(Iter->SimRegion->EntityHash))
    {
        if(!IsEmpty(Iter->SimRegion->EntityHashOccupancy, Iter->HashIndex))
        {
            Iter->Entity = Iter->SimRegion->EntityHash[Iter->HashIndex].Ptr;
            Assert(Iter->Entity);
            break;
        }
        ++Iter->HashIndex;
    }
}

internal entity_iterator
IterateAllEntities(sim_region *SimRegion)
{
    entity_iterator Iter;
    Iter.SimRegion = SimRegion;
    Iter.HashIndex = 0;
    FindNextEntity(&Iter);

    return(Iter);
}

internal void
Advance(entity_iterator *Iter)
{
    Assert(Iter->Entity);
    ++Iter->HashIndex;
    FindNextEntity(Iter);
}
