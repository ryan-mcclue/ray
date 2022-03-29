/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

inline move_spec
DefaultMoveSpec(void)
{
    move_spec Result;

    Result.UnitMaxAccelVector = false;
    Result.Speed = 1.0f;
    Result.Drag = 0.0f;

    return(Result);
}

internal void
DrawHitpoints(entity *Entity, render_group *PieceGroup, v3 GroundP)
{
    if(Entity->HitPointMax >= 1)
    {
        v2 HealthDim = {0.2f, 0.2f};
        real32 SpacingX = 1.5f*HealthDim.x;
        v2 HitP = {-0.5f*(Entity->HitPointMax - 1)*SpacingX, -0.25f};
        v2 dHitP = {SpacingX, 0.0f};
        for(uint32 HealthIndex = 0;
            HealthIndex < Entity->HitPointMax;
            ++HealthIndex)
        {
            hit_point *HitPoint = Entity->HitPoint + HealthIndex;
            v4 Color = {1.0f, 0.0f, 0.0f, 1.0f};
            if(HitPoint->FilledAmount == 0)
            {
                Color = V4(0.2f, 0.2f, 0.2f, 1.0f);
            }

            PushRect(PieceGroup, GroundP + V3(HitP, 0.1f), HealthDim, Color);
            HitP += dHitP;
        }
    }
}

inline s32
ConvertToLayerRelative(game_mode_world *WorldMode, r32 *Z)
{
    s32 RelativeIndex = 0;
    RecanonicalizeCoord(WorldMode->TypicalFloorHeight, &RelativeIndex, Z);
    return(RelativeIndex);
}

internal f32
RayIntersectsBox(v3 RayOrigin, v3 RayD, v3 BoxP, v3 BoxRadius)
{
    v3 BoxMin = BoxP - BoxRadius;
    v3 BoxMax = BoxP + BoxRadius;

    v3 InvRayD = 1.0f / RayD;

    v3 tBoxMin = (BoxMin - RayOrigin)*InvRayD;
    v3 tBoxMax = (BoxMax - RayOrigin)*InvRayD;

    v3 tMin3 = Min(tBoxMin, tBoxMax);
    v3 tMax3 = Max(tBoxMin, tBoxMax);

    f32 tMin = Maximum(tMin3.x, Maximum(tMin3.y, tMin3.z));
    f32 tMax = Minimum(tMax3.x, Minimum(tMax3.y, tMax3.z));

    f32 Result = F32Max;
    if((tMin > 0.0f) && (tMin < tMax))
    {
        Result = tMin;
    }

    return(Result);
}

internal void
DrawGroundCover(game_assets *Assets, render_group *RenderGroup, entity *Entity)
{
    indexed_vertex_output Out = OutputQuads(RenderGroup, Entity->GroundCoverCount);
    for(u32 CoverIndex = 0;
        CoverIndex < Entity->GroundCoverCount;
        ++CoverIndex)
    {
        ground_cover *Cover = Entity->GroundCover + CoverIndex;
        renderer_texture TextureHandle = GetBitmap(Assets, Cover->Bitmap);

        if(IsValid(TextureHandle))
        {
            u16 TextureIndex = SafeTruncateToU16(TextureIndexFrom(TextureHandle));
            v2 UV0 = {0, 0};
            v2 UV1 = {Cover->UV.x, 0};
            v2 UV2 = {Cover->UV.x, Cover->UV.y};
            v2 UV3 = {0, Cover->UV.y};
            VertexOut(&Out, 0, Cover->P[0] + Entity->P, Cover->N, UV0, Cover->Color, TextureIndex);
            VertexOut(&Out, 1, Cover->P[1] + Entity->P, Cover->N, UV1, Cover->Color, TextureIndex);
            VertexOut(&Out, 2, Cover->P[2] + Entity->P, Cover->N, UV2, Cover->Color, TextureIndex);
            VertexOut(&Out, 3, Cover->P[3] + Entity->P, Cover->N, UV3, Cover->Color, TextureIndex);
            QuadIndexOut(&Out, 0);
            Advance(&Out, 1);
        }
        else
        {
            LoadBitmap(Assets, Cover->Bitmap);
            ++RenderGroup->MissingResourceCount;
        }
    }
}

internal void
StompOnEntity(game_assets *Assets, sim_region *SimRegion, particle_cache *ParticleCache,
              entity *Stomper, entity_id StompeeID)
{
    entity *Stompee = GetEntityByID(SimRegion, StompeeID);
    if(Stompee)
    {
#if 0
        Stompee->GroundCoverSpecs[1].Type = CoverType_Flowers;
        Stompee->GroundCoverSpecs[1].Density += 8;
        FillUnpackedEntity(Assets, SimRegion, Stompee);
#endif
    }

    SpawnFire(ParticleCache, Stomper->P);
}

internal void
UpdateAndRenderEntities(f32 TypicalFloorHeight, sim_region *SimRegion, r32 dt,
                        // NOTE(casey): Optional...
                        render_group *RenderGroup,
                        v4 BackgroundColor,
                        particle_cache *ParticleCache,
                        game_assets *Assets,
                        editable_hit_test *HitTest)
{
    TIMED_FUNCTION();

    v3 PickingOrigin = {};
    v3 PickingRay = {};
    if(ShouldHitTest(HitTest) && RenderGroup)
    {
        render_transform *XForm = &RenderGroup->DebugXForm;
        v3 CursorInWorld = Unproject(RenderGroup, XForm, HitTest->ClipSpaceMouseP, 1.0f);

        PickingOrigin = XForm->P;
        PickingRay = NOZ(CursorInWorld - PickingOrigin);
    }

    for(entity_iterator Iter = IterateAllEntities(SimRegion);
        Iter.Entity;
        Advance(&Iter))
    {
        entity *Entity = Iter.Entity;
        if(Entity->Flags & EntityFlag_Active)
        {
            // TODO(casey): Should non-active entities NOT do simmy stuff?
            entity_traversable_point *BoostTo = GetTraversable(SimRegion, Entity->AutoBoostTo);
            if(BoostTo)
            {
                for(uint32 TraversableIndex = 0;
                    TraversableIndex < Entity->TraversableCount;
                    ++TraversableIndex)
                {
                    entity_traversable_point *Traversable =
                        Entity->Traversables + TraversableIndex;
                    entity *Occupier = GetEntityByID(SimRegion, Traversable->Occupier);
                    if(Occupier && (Occupier->MovementMode == MovementMode_Planted))
                    {
                        Occupier->CameFrom = Occupier->Occupying;
                        if(TransactionalOccupy(SimRegion, Occupier->ID,
                                               &Occupier->Occupying,
                                               Entity->AutoBoostTo))
                        {
                            Occupier->tMovement = 0.0f;
                            Occupier->MovementMode = MovementMode_Hopping;
                        }
                    }
                }
            }

            //
            // NOTE(casey): "Physics"
            //

            Entity->GroundP = Entity->P;
            v3 StartP = Entity->P;
            switch(Entity->MovementMode)
            {
                case MovementMode_Planted:
                {
                } break;

                case MovementMode_Hopping:
                {
                    v3 MovementTo = GetSimSpaceTraversable(SimRegion, Entity->Occupying).P;
                    v3 MovementFrom = GetSimSpaceTraversable(SimRegion, Entity->CameFrom).P;

                    r32 tJump = 0.1f;
                    r32 tThrust = 0.2f;
                    r32 tLand = 0.9f;

                    if(Entity->tMovement < tThrust)
                    {
                        Entity->ddtBob = 30.0f;
                    }

                    if(Entity->tMovement < tLand)
                    {
                        r32 t = Clamp01MapToRange(tJump, Entity->tMovement, tLand);
                        v3 a = V3(0, 0.0f, -2.0f);
                        v3 b = (MovementTo - MovementFrom) - a;
                        Entity->P = a*t*t + b*t + MovementFrom;
                        Entity->GroundP = Lerp(MovementFrom, t, MovementTo);
                    }

                    if(Entity->tMovement >= 1.0f)
                    {
                        Entity->P = MovementTo;
                        Entity->CameFrom = Entity->Occupying;
                        Entity->MovementMode = MovementMode_Planted;
                        Entity->dtBob = -2.0f;

                        StompOnEntity(Assets, SimRegion, ParticleCache,
                                      Entity, Entity->Occupying.Entity);
                    }

                    Entity->tMovement += 4.0f*dt;
                    if(Entity->tMovement > 1.0f)
                    {
                        Entity->tMovement = 1.0f;
                    }
                } break;

                case MovementMode_AngleAttackSwipe:
                {
                    if(Entity->tMovement < 1.0f)
                    {
                        Entity->AngleCurrent = Lerp(Entity->AngleStart,
                                                    Entity->tMovement,
                                                    Entity->AngleTarget);

                        Entity->AngleCurrentDistance = Lerp(Entity->AngleBaseDistance,
                                                            Triangle01(Entity->tMovement),
                                                            Entity->AngleSwipeDistance);
                    }
                    else
                    {
                        Entity->MovementMode = MovementMode_AngleOffset;
                        Entity->AngleCurrent = Entity->AngleTarget;
                        Entity->AngleCurrentDistance = Entity->AngleBaseDistance;
                    }

                    Entity->tMovement += 5.0f*dt;
                    if(Entity->tMovement > 1.0f)
                    {
                        Entity->tMovement = 1.0f;
                    }
                }
                case MovementMode_AngleOffset:
                {
                    v2 Arm = Entity->AngleCurrentDistance*Arm2(Entity->AngleCurrent + Entity->FacingDirection);
                    Entity->P = Entity->AngleBase + V3(Arm.x, Arm.y + 0.5f, 0.0f);
                } break;

                case MovementMode_Floating:
                {
                    if((LengthSq(Entity->dP) > 0) || (LengthSq(Entity->ddP) > 0))
                    {
                        MoveEntity(SimRegion, Entity, dt, Entity->ddP);
                    }
                } break;
            }

            r32 Cp = 100.0f;
            r32 Cv = 10.0f;
            Entity->ddtBob += Cp*(0.0f - Entity->tBob) + Cv*(0.0f - Entity->dtBob);
            Entity->tBob += Entity->ddtBob*dt*dt + Entity->dtBob*dt;
            Entity->dtBob += Entity->ddtBob*dt;

            v3 EndP = Entity->P;

            // TODO(casey): Feed things into the collision detector here.

            if(RenderGroup)
            {
                v3 EntityGroundP = Entity->P;

                //
                // NOTE(casey): Rendering
                //

                asset_match_vector MatchVector = {};
                f32 Facing = (1.0f / Tau32) * Entity->FacingDirection;
                if(Facing < 0.0)
                {
                    Facing = 1.0f + Facing;
                }
                MatchVector.E[MatchElement_FacingDirection] = Facing;

                hha_bitmap *BitmapInfos[ENTITY_MAX_PIECE_COUNT] = {};
                sprite_values PieceSprites[ENTITY_MAX_PIECE_COUNT];
                for(u32 PieceIndex = 0;
                    PieceIndex < Entity->PieceCount;
                    ++PieceIndex)
                {
                    entity_visible_piece *Piece = Entity->Pieces + PieceIndex;
                    MatchVector.E[MatchElement_Variant] = Piece->Variant;
                    bitmap_id BitmapID =
                        GetBestMatchBitmapFrom(Assets, Piece->Asset, MatchVector);

                    v3 WorldRadius = Piece->Dim;

                    v2 XAxis = {1, 0};
                    v2 YAxis = {0, 1};
                    if(Piece->Flags & PieceMove_AxesDeform)
                    {
                        XAxis = Entity->XAxis;
                        YAxis = Entity->YAxis;
                    }

                    r32 tBob = 0.0f;
                    v3 Offset = {};
                    if(Piece->Flags & PieceMove_BobOffset)
                    {
                        tBob = Entity->tBob;
                        Offset = V3(Entity->FloorDisplace, 0.0f);
                        Offset.y += Entity->tBob;
                    }

                    v4 Color = Piece->Color;
#if 0
                    // TODO(casey): WTF doesn't this produce
                    // a out-of-bounds array access?
                    Color *= (1.0f - 0.5f*MatchVector.E[Tag_Ghost]);
#endif

                    dev_id DevID = DevIDFromU32s(Entity->ID.Value, PieceIndex);
                    b32x Highlighted = AreEqual(DevID, HitTest->HighlightID);

                    if(Piece->Flags & PieceType_Light)
                    {
                        // TODO(casey): Probably can just use RGB for emission, and encode that directly
                        // when we create the entity, right?
                        PushLight(RenderGroup, EntityGroundP + Piece->Offset, Piece->Dim, Color.a*Color.rgb);
#if 0
                        PushVolumeOutline(RenderGroup, RectCenterDim(EntityGroundP + Piece->Offset,Piece->Dim),
                                              V4(Color.rgb, 1.0f), 0.01f);
#endif
                    }
                    else if(Piece->Flags & PieceType_Cube)
                    {
                        v3 CubeP = EntityGroundP + Piece->Offset;
                        v3 CubeR = Piece->Dim;
                        PushCube(RenderGroup, BitmapID, CubeP, CubeR, Color, Piece->CubeUVLayout, 0.0f);
                    }
                    else if(Piece->Flags & PieceType_Occluder)
                    {
                        v3 CubeP = EntityGroundP + Piece->Offset;
                        v3 CubeR = Piece->Dim;
                        //PushVolumeOutline(RenderGroup, RectCenterHalfDim(CubeP, CubeR), Color, 0.025f);
                        PushOccluder(RenderGroup, CubeP, CubeR, Color);
                        //PushOccluder(RenderGroup, CubeP, CubeR, V4(1, 0, 0, 1));
                    }
                    else
                    {
                        bitmap_piece BitmapPiece = Piece->Bitmap;

                        hha_bitmap *BitmapInfo = GetBitmapInfo(Assets, BitmapID);
                        hha_bitmap *ParentBitmapInfo = BitmapInfos[BitmapPiece.ParentPiece];
                        f32 HeightRatio = (f32)BitmapInfo->OrigDim[1] / 1024.0f;

                        hha_align_point ChildAlign = FindAlign(BitmapInfo, BitmapPiece.ChildAlignType|HHAAlign_ToParent);
                        HeightRatio *= GetSize(ChildAlign);

                        b32x Visible = true;
                        v3 InitialP = EntityGroundP + Offset;
                        if(BitmapPiece.ParentAlignType)
                        {
                            Assert(PieceIndex > 0);

                            hha_align_point ParentAlign = FindAlign(ParentBitmapInfo, BitmapPiece.ParentAlignType);
                            InitialP = WorldPFromAlignP(PieceSprites + BitmapPiece.ParentPiece,
                                                        GetPPercent(ParentAlign));
                            HeightRatio *= GetSize(ParentAlign);

                            dev_id ParentDevID = DevIDFromU32s(Entity->ID.Value, BitmapPiece.ParentPiece);
                            CopyType(DevID, &ParentDevID);
                            Visible = ShouldDrawChildren(HitTest, ParentDevID);
                        }
                        BitmapInfos[PieceIndex] = BitmapInfo;
                        v2 AlignP = GetPPercent(ChildAlign);

                        v2 WorldDim = WorldDimFromWorldHeight(BitmapInfo, Piece->Dim.y*HeightRatio);

                        WorldRadius.x = WorldDim.x;
                        WorldRadius.y = WorldDim.y;
                        WorldRadius.z = 0.1f;

                        v3 CameraZ = RenderGroup->GameXForm.Z;

                        sprite_values Sprite =
                            SpriteValuesForUpright(RenderGroup, WorldDim, AlignP, XAxis, YAxis);
                        f32 h = Sprite.ZDisplacement;
                        f32 Cz = CameraZ.z;
                        if(Cz > 0.1f) // NOTE(casey): We don't support head-on cameras
                        {
                            f32 t = h / Cz;
                            Sprite.MinP += InitialP + t*CameraZ; // AlignP.y*V3(0, WorldDim.y, 0);
                            Sprite.MinP += Piece->Offset;
                            PieceSprites[PieceIndex] = Sprite;

                            // TODO(casey): We really want to clean up how we do
                            // asset access and loading now.
                            renderer_texture TextureHandle = GetBitmap(Assets, BitmapID);
                            if(IsValid(TextureHandle))
                            {
                                if(Visible)
                                {
                                    PushSprite(RenderGroup, TextureHandle,
                                               Sprite.MinP, Sprite.ScaledXAxis, Sprite.ScaledYAxis,
                                               Color);
                                }

                                if(Highlighted)
                                {
                                    for(u32 APIndex = 0;
                                        APIndex < ArrayCount(BitmapInfo->AlignPoints);
                                        ++APIndex)
                                    {
                                        hha_align_point *AP = BitmapInfo->AlignPoints + APIndex;
                                        if(ShouldDrawAlignPoint(HitTest, APIndex) &&
                                           (GetType(*AP) != HHAAlign_None))
                                        {
                                            PushCube(RenderGroup, RenderGroup->WhiteTexture,
                                                     WorldPFromAlignP(&Sprite, GetPPercent(*AP)), V3(0.04f, 0.04f, 0.04f),
                                                     GetDebugColor4(APIndex),
                                                     DefaultCubeUVLayout(), 0, 3.0f);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                LoadBitmap(Assets, BitmapID);
                                ++RenderGroup->MissingResourceCount;
                            }
                        }

                        if(ShouldHitTest(HitTest))
                        {
                            v3 WorldP = EntityGroundP + Piece->Offset;

                            f32 tHit = RayIntersectsBox(PickingOrigin, PickingRay, WorldP, WorldRadius);
                            if(tHit < F32Max)
                            {
                                AddHit(HitTest, DevID, BitmapID.Value, tHit);
                            }
                        }

                        if(Highlighted)
                        {
                            PushVolumeOutline(RenderGroup,
                                              RectCenterHalfDim(EntityGroundP + Piece->Offset, WorldRadius),
                                              HitTest->HighlightColor, 0.001f);

                            PushCube(RenderGroup, RenderGroup->WhiteTexture,
                                     Entity->P, V3(0.06f, 0.06f, 0.06f),
                                     V4(1, 1, 1, 1),
                                     DefaultCubeUVLayout(), 0, 3.0f);
                        }
                    }
                }

                DrawGroundCover(Assets, RenderGroup, Entity);
                DrawHitpoints(Entity, RenderGroup, EntityGroundP);

#if 0
                if(HasArea(Entity->CollisionVolume))
                {
                    v4 Color = V4(0, 0.5f, 1.0f, 1.0f);
                    if(Entity->Flags & EntityFlag_SphereCollision)
                    {
                        Color = V4(1.0f, 0.5f, 0.0f, 1.0f);
                    }
                    PushVolumeOutline(RenderGroup, Offset(Entity->CollisionVolume, EntityGroundP),
                                      Color, 0.01f);
                }

#endif

#if 0
                for(uint32 TraversableIndex = 0;
                    TraversableIndex < Entity->TraversableCount;
                    ++TraversableIndex)
                {
                    entity_traversable_point *Traversable =
                        Entity->Traversables + TraversableIndex;
                    v4 Color = V4(0.05f, 0.25f, 0.05f, 1.0f);
#if 0
                    if(GetTraversable(Entity->AutoBoostTo))
                    {
                        Color = V4(1.0f, 0.0f, 1.0f, 1.0f);
                    }
#endif
                    if(IsValid(Traversable->Occupier))
                    {
                        Color = V4(1.0, 0.5f, 0.0f, 1.0f);
                    }

                    PushCube(RenderGroup, RenderGroup->WhiteTexture,
                             EntityGroundP + Traversable->P,
                                 V3(0.04f, 0.04f, 0.04f),
                                 Color,
                             DefaultCubeUVLayout(), 1.0f);
                }
#endif
            }
        }
    }
}

internal void
FillUnpackedEntity(game_assets *Assets, sim_region *SimRegion, entity *Entity)
{
    TIMED_FUNCTION();

    random_series CoverSeries = RandomSeedOffset(Entity->ID.Value);

    u32 CoverIndex = 0;
    for(u32 CoverSpecIndex = 0;
        CoverSpecIndex < ArrayCount(Entity->GroundCoverSpecs);
        ++CoverSpecIndex)
    {
        ground_cover_spec *Spec = Entity->GroundCoverSpecs + CoverSpecIndex;

        if(Spec->Type)
        {
            asset_tag_hash TagHash = {};
            asset_match_vector MatchVector = {};

            switch(Spec->Type)
            {
                case CoverType_Rocks:
                {
                    TagHash = GetTagHash(Asset_Particle, Tag_Stone, Tag_Cover);
                } break;

                case CoverType_Flowers:
                {
                    TagHash = GetTagHash(Asset_Particle, Tag_Flowers);
                } break;

                case CoverType_ThickGrass:
                {
                    TagHash = GetTagHash(Asset_Particle, Tag_Cover, Tag_Large);
                } break;

                case CoverType_SplotchGrass:
                {
                    TagHash = GetTagHash(Asset_Particle, Tag_Cover);
                } break;
            }

            u32 DensityCount = (ENTITY_MAX_GROUND_COVER - CoverIndex);
            if(DensityCount > Spec->Density)
            {
                DensityCount = Spec->Density;
            }

            for(u32 DensityIndex = 0;
                DensityIndex < DensityCount;
                ++DensityIndex)
            {
                ground_cover *Cover = Entity->GroundCover + CoverIndex;

                v3 RandomUVW =
                {
                    RandomUnilateral(&CoverSeries),
                    RandomUnilateral(&CoverSeries),
                    1.0f,
                };

                MatchVector.E[MatchElement_Variant] = RandomUnilateral(&CoverSeries);

                v3 Color = {1, 1, 1};

                Cover->Bitmap = GetBestMatchBitmapFrom(Assets, TagHash, MatchVector);
                Cover->Color = FinalizeColor(Color);
                Cover->N = NOZ(V3(0, 0, 1)); // TODO(casey): Adjust this once lighting is working

                v3 BaseP = PointFromUVW(Entity->CollisionVolume, RandomUVW);
                f32 Scale = 0.3f;

                hha_bitmap *BitmapInfo = GetBitmapInfo(Assets, Cover->Bitmap);
                v2 WorldDim = WorldDimFromWorldHeight(BitmapInfo, Scale);

                v2 XAxis = {1, 0};
                v3 WorldUp = {0, 0, 1};
                v3 XAxisH = {1, 0, 0};
                v2 YAxis = {0, 1};
                v2 AlignP = {0.5f, 0.0f};
                v3 CameraUp = DEFAULT_CAMERA_UP;

                sprite_values Sprite =
                    SpriteValuesForUpright(WorldUp, CameraUp, XAxisH, WorldDim, AlignP, XAxis, YAxis);

                v3 MinP = Sprite.MinP + BaseP;
                Cover->P[0] = MinP;
                Cover->P[1] = MinP + Sprite.ScaledXAxis;
                Cover->P[2] = MinP + Sprite.ScaledXAxis + Sprite.ScaledYAxis;
                Cover->P[3] = MinP + Sprite.ScaledYAxis;
                Cover->UV = GetUVScaleForBitmap(Assets, BitmapInfo->Dim[0], BitmapInfo->Dim[1]);

                ++CoverIndex;
            }
        }
    }

    Entity->GroundCoverCount = CoverIndex;
}
