/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

struct entity;
struct sim_region;

struct entity_id
{
    u32 Value;
};
inline b32x AreEqual(entity_id A, entity_id B)
{
    b32x Result = (A.Value == B.Value);

    return(Result);
}
inline void Clear(entity_id *A)
{
    A->Value = 0;
}
internal b32x IsValid(entity_id A)
{
    b32x Result = (A.Value != 0);
    return(Result);
}


#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
    // TODO(casey): Bake this down into one variable
    uint8 Flags;
    uint8 FilledAmount;
};

struct traversable_reference
{
    entity_id Entity;
    u32 Index;
};
internal b32 IsValid(traversable_reference A)
{
    b32 Result = IsValid(A.Entity);

    return(Result);
}
internal b32 AreEqual(traversable_reference A, traversable_reference B)
{
    b32 Result = (AreEqual(A.Entity, B.Entity) &&
                  (A.Index == B.Index));

    return(Result);
}

enum entity_flags
{
    EntityFlag_Deleted = (1 << 0),
    EntityFlag_Active = (1 << 1),
    EntityFlag_EventOnCollision = (1 << 2),
    EntityFlag_AllowsMotionOnCollision = (1 << 3),
    EntityFlag_SphereCollision = (1 << 4),
};

struct entity_traversable_point
{
    v3 P;
    entity_id PrevOccupier;
    entity_id Occupier;
};

struct move_spec
{
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

enum entity_movement_mode
{
    MovementMode_Planted,
    MovementMode_Hopping,
    MovementMode_Floating,

    MovementMode_AngleOffset,
    MovementMode_AngleAttackSwipe,
};

struct bitmap_piece
{
    // NOTE(casey): Parents must ALWAYS come before children
    u8 ParentPiece;
    enum8(hha_align_point_type) ParentAlignType;
    enum8(hha_align_point_type) ChildAlignType;
    u8 Reserved;
};

enum entity_visible_piece_flag
{
    PieceMove_AxesDeform = 0x1,
    PieceMove_BobOffset = 0x2,
    PieceType_Cube = 0x4,
    PieceType_Light = 0x8,
    PieceType_Occluder = 0x10,
};
struct entity_visible_piece
{
    asset_tag_hash Asset;

    v4 Color;
    v3 Offset;
    v3 Dim;
    f32 Variant;

    u32 Flags;

    union
    {
        bitmap_piece Bitmap;
        cube_uv_layout CubeUVLayout;
    };
};
internal b32x
IsBitmap(entity_visible_piece *Piece)
{
    b32x Result = !(Piece->Flags & (PieceType_Cube | PieceType_Light));
    return(Result);
}

enum camera_behavior
{
    Camera_Inspect = 0x1,
    Camera_Offset = 0x2,
    Camera_ViewPlayerX = 0x4,
    Camera_ViewPlayerY = 0x8,
    Camera_GeneralVelocityConstraint = 0x10,
    Camera_DirectionalVelocityConstraint = 0x20,
};

struct ground_cover
{
    bitmap_id Bitmap;

#if 0
    v3 P;
    v3 Color;
    f32 Scale;
#else
    u32 Color;
    v3 N;
    v2 UV;
    v3 P[4];
#endif
};

enum ground_cover_type
{
    CoverType_None,

    CoverType_Rocks,
    CoverType_Flowers,
    CoverType_ThickGrass,
    CoverType_SplotchGrass,

    CoverType_Count,
};

struct ground_cover_spec
{
    u8 Type;
    u8 Density;
    u8 ReservedA;
    u8 ReservedB;
};

#define ENTITY_MAX_GROUND_COVER 128
#define ENTITY_MAX_PIECE_COUNT 4 // TODO(casey): This should go away once we're packing
struct entity
{
    //
    // NOTE(casey): Things we've thought about
    //

    entity_id ID;
    brain_slot BrainSlot;
    brain_id BrainID;

    u32 CameraBehavior;
    f32 CameraMinVelocity;
    f32 CameraMaxVelocity;
    f32 CameraMinTime;
    v3 CameraOffset;
    v3 CameraVelocityDirection;

    //
    // NOTE(casey): Everything below here is NOT worked out yet
    //

    u32 SwitchProperties;
    u32 SwitchState;

    u32 Flags;

    v3 P;
    v3 dP;

    // r32 DistanceLimit;

    rectangle3 CollisionVolume;

    f32 FacingDirection;
    f32 tBob;
    f32 dtBob;

    s32 dAbsTileZ;

    // TODO(casey): Should hitpoints themselves be entities?
    u32 HitPointMax;
    hit_point HitPoint[16];

    // TODO(casey): Only for stairwells!
    v2 WalkableDim;
    f32 WalkableHeight;

    entity_movement_mode MovementMode;
    f32 tMovement;
    traversable_reference Occupying;
    traversable_reference CameFrom;

    v3 AngleBase;
    f32 AngleCurrent;
    f32 AngleStart;
    f32 AngleTarget;
    f32 AngleCurrentDistance;
    f32 AngleBaseDistance;
    f32 AngleSwipeDistance;

    v2 XAxis;
    v2 YAxis;

    v2 FloorDisplace;

    u32 TraversableCount;
    entity_traversable_point Traversables[16];

    u32 PieceCount;
    entity_visible_piece Pieces[ENTITY_MAX_PIECE_COUNT];

    // TODO(casey): Generation index so we know how "up to date" this entity is.

    traversable_reference AutoBoostTo;

    ground_cover_spec GroundCoverSpecs[4];

    //
    // NOTE(casey): Everything below this line isn't actually stored in long-term storage
    //

    u64 DiscardEverythingAfter;

    r32 ddtBob;
    v3 ddP;
    v3 GroundP;

    u32 GroundCoverCount;
    ground_cover GroundCover[ENTITY_MAX_GROUND_COVER];
};

#define InvalidP V3(100000.0f, 100000.0f, 100000.0f)

inline bool32
IsSet(entity *Entity, uint32 Flag)
{
    bool32 Result = Entity->Flags & Flag;

    return(Result);
}

inline b32 IsDeleted(entity *E) {b32 Result = IsSet(E, EntityFlag_Deleted); return(Result);}

inline void
AddFlags(entity *Entity, uint32 Flag)
{
    Entity->Flags |= Flag;
}

inline void
ClearFlags(entity *Entity, uint32 Flag)
{
    Entity->Flags &= ~Flag;
}

inline void
MakeEntitySpatial(entity *Entity, v3 P, v3 dP)
{
    Entity->P = P;
    Entity->dP = dP;
}

inline real32
GetStairGround(entity *Entity, v3 AtGroundPoint)
{
    rectangle2 RegionRect = RectCenterDim(Entity->P.xy, Entity->WalkableDim);
    v2 Bary = Clamp01(GetBarycentric(RegionRect, AtGroundPoint.xy));
    real32 Result = Entity->P.z + Bary.y*Entity->WalkableHeight;

    return(Result);
}

internal void FillUnpackedEntity(game_assets *Assets, sim_region *SimRegion, entity *Dest);
