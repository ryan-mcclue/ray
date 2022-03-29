/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

inline rectangle3
MakeSimpleGroundedCollision(f32 DimX, f32 DimY, f32 DimZ, f32 OffsetZ)
{
    rectangle3 Result = RectCenterDim(V3(0, 0, 0.5f*DimZ + OffsetZ),
                                      V3(DimX, DimY, DimZ));
    return(Result);
}

internal entity_visible_piece *
AddPiece(entity *Entity, asset_tag_hash Asset, v3 Dim, v3 Offset, v4 Color, u32 Flags)
{
    Assert(Entity->PieceCount < ArrayCount(Entity->Pieces));
    entity_visible_piece *Piece = Entity->Pieces + Entity->PieceCount++;
    Piece->Asset = Asset;
    Piece->Dim = Dim;
    Piece->Offset = Offset;
    Piece->Color = Color;
    Piece->Flags = Flags;

    return(Piece);
}

internal entity_visible_piece *
AddPiece(entity *Entity, asset_tag_hash Asset, f32 Height, v3 Offset, v4 Color, u32 Flags)
{
    entity_visible_piece *Result =
        AddPiece(Entity, Asset, V3(0, Height, 0), Offset, Color, Flags);
    return(Result);
}

internal void
ConnectPiece(entity *Entity,
             entity_visible_piece *Parent, hha_align_point_type ParentType,
             entity_visible_piece *Child, hha_align_point_type ChildType)
{
    Assert(IsBitmap(Child));
    bitmap_piece *Bitmap = &Child->Bitmap;
    Bitmap->ParentPiece = SafeTruncateToU8(Parent - Entity->Pieces);
    Bitmap->ParentAlignType = SafeTruncateToU8(ParentType);
    Bitmap->ChildAlignType = SafeTruncateToU8(ChildType);

    Assert(Bitmap->ParentPiece < (Child - Entity->Pieces));
}

internal void
ConnectPieceToWorld(entity *Entity, entity_visible_piece *Child, hha_align_point_type ChildType)
{
    Assert(IsBitmap(Child));
    bitmap_piece *Bitmap = &Child->Bitmap;
    Bitmap->ParentPiece = 0;
    Bitmap->ParentAlignType = SafeTruncateToU8(HHAAlign_None);
    Bitmap->ChildAlignType = SafeTruncateToU8(ChildType);
}

internal entity_visible_piece *
AddPieceLight(entity *Entity, f32 Radius, v3 Offset, f32 Emission, v3 Color)
{
    entity_visible_piece *Result =
        AddPiece(Entity, {}, V3(Radius, Radius, Radius), Offset, V4(Color, Emission), PieceType_Light);
    return(Result);
}

internal entity_visible_piece *
AddPieceOccluder(entity *Entity, v3 Offset, v3 Dim, v3 Color)
{
    entity_visible_piece *Result =
        AddPiece(Entity, {}, Dim, Offset, V4(Color, 1.0f), PieceType_Occluder);
    return(Result);
}

internal entity *
AddEntity(sim_region *Region)
{
    entity *Entity = CreateEntity(Region, AllocateEntityID(Region->World));

    Entity->XAxis = V2(1, 0);
    Entity->YAxis = V2(0, 1);

    return(Entity);
}

internal void
InitHitPoints(entity *Entity, uint32 HitPointCount)
{
    Assert(HitPointCount <= ArrayCount(Entity->HitPoint));
    Entity->HitPointMax = HitPointCount;
    for(uint32 HitPointIndex = 0;
        HitPointIndex < Entity->HitPointMax;
        ++HitPointIndex)
    {
        hit_point *HitPoint = Entity->HitPoint + HitPointIndex;
        HitPoint->Flags = 0;
        HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;
    }
}

internal entity *
AddInanimate(sim_region *Region, asset_tag_hash BaseAsset, f32 Variant = 0)
{
    entity *Entity = AddEntity(Region);
    entity_visible_piece *Body = AddPiece(Entity, GetTagHash(Asset_Scenery, BaseAsset), 1.0f, V3(0, 0, 0), V4(1, 1, 1, 1));
    Body->Variant = Variant;
    ConnectPieceToWorld(Entity, Body, HHAAlign_Default);
    return(Entity);
}

internal entity *
AddObstacle(sim_region *Region, asset_tag_hash BaseAsset, f32 Variant = 0)
{
    entity *Entity = AddInanimate(Region, BaseAsset, Variant);
    return(Entity);
}

internal entity *
AddBlock(sim_region *Region, asset_tag_hash BaseAsset, v3 Dim, v4 Color, f32 Variant = 0)
{
    entity *Entity = AddEntity(Region);
    entity_visible_piece *Piece =
        AddPiece(Entity, GetTagHash(Asset_Block, BaseAsset), 0.5f*Dim, V3(0, 0, 0.5f*Dim.z), Color, PieceType_Cube);
    Piece->Variant = Variant;

    return(Entity);
}

internal entity *
AddConversation(sim_region *Region)
{
    entity *Entity = AddEntity(Region);
    v3 Dim = {0.5f, 0.5f, 0.5f};
    Entity->Flags |= EntityFlag_AllowsMotionOnCollision;
    Entity->CollisionVolume = AddRadiusTo(RectCenterDim(V3(0, 0, 0.5f*Dim.z), Dim),
                                          V3(0.1f, 0.1f, 0.1f));
    Entity->CameraBehavior = Camera_Offset;
    Entity->CameraOffset.z = GetCameraOffsetZForCloseup();
    Entity->CameraOffset.y = 2.0f;

    return(Entity);
}

internal entity *
AddTree(world_generator *Gen, sim_region *Region)
{
    asset_tag_hash AssetHash = GetTagHash(Asset_None, Tag_Tree, RandomChoice(Gen->Entropy, 2) ? Tag_Winter : Tag_None);
    entity *Entity = AddObstacle(Region, AssetHash, RandomUnilateral(Gen->Entropy));
    return(Entity);
}

#if 0
internal void
AddMonstar(sim_region *Region, entity_piece_asset BaseAsset, v3 P, traversable_reference StandingOn)
{
    entity *Entity = AddEntity(Region);
    AddFlags(Entity, EntityFlag_Collides);

    Entity->BrainSlot = BrainSlotFor(brain_monstar, Body);
    Entity->BrainID = AddBrain(Region);
    Entity->Occupying = StandingOn;

    InitHitPoints(Entity, 3);

    AddPiece(Entity, ChangeHash(Asset_Shadow, BaseAsset), 4.5f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));
    AddPiece(Entity, ChangeHash(Asset_Body, BaseAsset), 4.5f, V3(0, 0, 0), V4(1, 1, 1, 1));

    Entity->P = P;
}
#endif

internal entity *
PlaceEntityAtP(entity *Entity, v3 P)
{
    Entity->P = P;
    return(Entity);
}

internal entity *
PlaceEntityAtTraversable(sim_region *Region, entity *Entity, traversable_reference StandingOn)
{
    Entity->P = GetSimSpaceTraversable(Region, StandingOn).P;
    return(Entity);
}

internal entity *
PlaceEntityOnTraversable(sim_region *Region, entity *Entity, traversable_reference StandingOn)
{
    PlaceEntityAtTraversable(Region, Entity, StandingOn);
    TransactionalOccupy(Region, Entity->ID, &Entity->Occupying, StandingOn);

    return(Entity);
}

internal entity *
PlaceEntityAtP(entity *Entity, edit_tile Tile)
{
    edit_tile_contents *Contents = GetTile(&Tile);
    Assert(Contents);
    if(Contents->Structural)
    {
        Entity->P = Contents->Structural->P;
    }
    else
    {
        InvalidCodePath;
    }

    return(Entity);
}

internal entity *
PlaceEntityAtTraversable(edit_grid *Grid, entity *Entity, edit_tile Tile)
{
    edit_tile_contents *Contents = GetTile(&Tile);
    Assert(Contents);

    traversable_reference Ref = {};
    if(Contents->Structural)
    {
        Ref.Entity = Contents->Structural->ID;
    }
    else
    {
        InvalidCodePath;
    }

    PlaceEntityAtTraversable(Grid->Region, Entity, Ref);

    return(Entity);
}

internal entity *
PlaceEntityOnTraversable(edit_grid *Grid, entity *Entity, edit_tile Tile)
{
    edit_tile_contents *Contents = GetTile(&Tile);
    Assert(Contents);

    traversable_reference Ref = {};
    if(Contents->Structural)
    {
        Ref.Entity = Contents->Structural->ID;
    }
    else
    {
        InvalidCodePath;
    }

    PlaceEntityOnTraversable(Grid->Region, Entity, Ref);

    return(Entity);
}

GEN_CREATE_ENTITY_PATTERN(SingleEnemyPattern)
{
    edit_tile HeadTile = FindRandomOpenTile(Grid, V2(0.5f, 0.5f));
    if(IsValid(HeadTile))
    {
        entity *Entity = AddEntity(Grid->Region);
        
        Entity->BrainSlot = BrainSlotFor(brain_familiar, Head);
        Entity->BrainID = AddBrain(Grid->Region);
        Entity->CollisionVolume = MakeSimpleGroundedCollision(1.0f, 1.0f, 1.0f, 0.25f);
        
        PlaceEntityOnTraversable(Grid, Entity, HeadTile);

        entity_visible_piece *Piece = AddPiece(Entity, GetTagHash(Asset_Head, Pattern->BaseAsset), 2.5f, V3(0, 0, 0), V4(1, 1, 1, 1), PieceMove_BobOffset);
        
        ConnectPieceToWorld(Entity, Piece, HHAAlign_Default);
    }
}

GEN_CREATE_ENTITY_PATTERN(SnakePattern)
{
    edit_tile HeadTile = FindRandomOpenTile(Grid, V2(0.5f, 0.5f));
    if(IsValid(HeadTile))
    {
        asset_tag_hash BaseAsset = Pattern->BaseAsset;
        
        entity *Head = AddEntity(Grid->Region);
        Head->CollisionVolume = MakeSimpleGroundedCollision(0.75f, 0.75f, 0.75f, 0.0f);
        entity_visible_piece *HeadPiece = AddPiece(Head, GetTagHash(Asset_Head, BaseAsset), 1.5f, V3(0, 0, 0.5f), V4(1, 1, 1, 1));
        AddPieceLight(Head, 0.5f, V3(0, 0, 1.0f), 5.0f, V3(1, 1, 0));
        ConnectPieceToWorld(Head, HeadPiece, HHAAlign_Default);
        InitHitPoints(Head, 3);
        Head->BrainSlot = IndexedBrainSlotFor(brain_snake, Segments, 0);
        Head->BrainID = AddBrain(Grid->Region);

        PlaceEntityOnTraversable(Grid, Head, HeadTile);

        edit_tile PrevTile = HeadTile;
        u32 SegmentCount = 4; // RandomBetween(Gen->Entropy, 1, 3);
        for(u32 SegmentIndex = 0;
            SegmentIndex < SegmentCount;
            ++SegmentIndex)
        {
            edit_tile SegTile = FindAdjacentOpenTile(PrevTile, BoxMask_Planar);
            if(IsValid(SegTile))
            {
                entity *Seg = AddEntity(Grid->Region);
                Seg->CollisionVolume = Head->CollisionVolume;

                entity_visible_piece *SegPiece = AddPiece(Seg, GetTagHash(Asset_Body, BaseAsset), 1.5f, V3(0, 0, 0.5f), V4(1, 1, 1, 1));
//                AddPieceLight(Seg, 0.5f, V3(0, 0, 1.0f), 5.0f, V3(1, 1, 0));
                ConnectPieceToWorld(Seg, SegPiece, HHAAlign_Default);
                InitHitPoints(Seg, 3);

                Seg->BrainSlot = IndexedBrainSlotFor(brain_snake, Segments, SegmentIndex + 1);
                Seg->BrainID = Head->BrainID;
                PlaceEntityOnTraversable(Grid, Seg, SegTile);

                PrevTile = SegTile;
            }
            else
            {
                break;
            }
        }
    }
}

internal b32x
TileAlreadyExists(tile_pattern *Tiles, v3s I)
{
    b32x Result = false;

    for(u32 TileIndex = 0;
            TileIndex < Tiles->Count;
            ++TileIndex)
    {
        if(AreEqual(Tiles->AtI[TileIndex], I))
        {
            Result = true;
            break;
        }
    }

    return Result;
}

internal u32
AppendTile(tile_pattern *Tiles, gen_v3 I)
{
    Assert(Tiles->Count < Tiles->MaxCount);
    Assert(Tiles->Count < ArrayCount(Tiles->AtI));

    u32 Result = Tiles->Count++;
    Tiles->AtI[Result] = I;

    return(Result);
}

GEN_CREATE_ENTITY_PATTERN(TileSwitchPattern)
{
    random_series *Series = Grid->Series;
    f32 D = Pattern->Difficulty;

    tile_pattern Tiles = {};

    if(D < 0.25f)
    {
        // Tiles.MaxCount = RandomBetween(Series, 1, 4);
        Tiles.MaxCount = 16;

        Tiles.Type = TilePattern_Scatter;
        Tiles.RequiredEmptyNeighborCount = 4;
    }
    else if(D < 0.75f)
    {
        Tiles.MaxCount = RandomBetween(Series, 4, 12);
    }
    else
    {
        Tiles.MaxCount = RandomBetween(Series, 8, 16);
    }

    for(u32 AttemptIndex = 0;
            (AttemptIndex < 100) && (Tiles.Count < Tiles.MaxCount);
            ++AttemptIndex)
    {
        edit_tile TileAt = {};

        gen_v3 Dir[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

        if(Tiles.Count)
        {
            TileAt = {Grid, Tiles.AtI[Tiles.Count - 1]};
            switch(Tiles.Type)
            {
                case TilePattern_Horizontal:
                {
                    ++TileAt.RelIndex.x;
                } break;

                case TilePattern_Vertical:
                {
                    ++TileAt.RelIndex.y;
                } break;

                case TilePattern_Snake:
                {
                    TileAt.RelIndex += Dir[RandomChoice(Series, ArrayCount(Dir))];
                } break;

                case TilePattern_Branching:
                {
                    TileAt.RelIndex = Tiles.AtI[RandomChoice(Series, Tiles.Count)];
                    TileAt.RelIndex += Dir[RandomChoice(Series, ArrayCount(Dir))];
                } break;

                case TilePattern_Scatter:
                {
                    TileAt.RelIndex = Tiles.AtI[RandomChoice(Series, Tiles.Count)];
                    TileAt.RelIndex.x += RandomBetween(Series, -3, 3);
                    TileAt.RelIndex.y += RandomBetween(Series, -3, 3);
                } break;
            }
        }
        else
        {
            TileAt = FindRandomOpenTile(Grid, V2(RandomUnilateral(Series), RandomUnilateral(Series)));
        }

        if(IsValid(TileAt) && TraversableIsOpen(TileAt) &&
               !TileAlreadyExists(&Tiles, TileAt.RelIndex))
        {
            u32 EmptyNeighborCount = 0;
            for(u32 DirIndex = 0;
                    DirIndex < ArrayCount(Dir);
                    ++DirIndex)
            {
                if(!TileAlreadyExists(&Tiles, TileAt.RelIndex + Dir[DirIndex]))
                {
                    ++EmptyNeighborCount;
                }
            }

            if(EmptyNeighborCount >= Tiles.RequiredEmptyNeighborCount)
            {
                AppendTile(&Tiles, TileAt.RelIndex);
            }
        }
    }

    for(u32 TileIndex = 0;
            TileIndex < Tiles.Count;
            ++TileIndex)
    {
        edit_tile_contents *Contents = GetTile(Grid, Tiles.AtI[TileIndex]);
        Assert(Contents);

        entity *Tile = Contents->Structural;
        Tile->BrainSlot = IndexedBrainSlotFor(brain_switches, Tiles, TileIndex);
        Tile->BrainID = Pattern->BrainID;
        Tile->Pieces[0].Color = {0.1f, 0.1f, 0.1f, 1.0f};
        AddPieceLight(Tile, 0.5f, V3(0, 0, 1.0f), 0.0f, V3(1, 1, 1));
    }
}

GEN_CREATE_ENTITY_PATTERN(NPCPattern)
{
    edit_tile CharTile = FindRandomOpenTile(Grid, V2(0.5f, 0.75f));
    edit_tile TalkTile = FindAdjacentOpenTile(CharTile, BoxMask_South);
    if(IsValid(CharTile) && IsValid(TalkTile))
    {
        entity *Entity = AddEntity(Grid->Region);
        Entity->FacingDirection = 0.75f*Tau32;

        entity_visible_piece *Body = AddPiece(Entity, GetTagHash(Asset_Body, Pattern->BaseAsset), 1.0f, V3(0, 0, 0), V4(1, 1, 1, 1));
        entity_visible_piece *Head = AddPiece(Entity, GetTagHash(Asset_Head, Pattern->BaseAsset), 1.0f, V3(0, 0, 0.1f), V4(1, 1, 1, 1));

        ConnectPieceToWorld(Entity, Body, HHAAlign_Default);
        ConnectPiece(Entity, Body, HHAAlign_BaseOfNeck, Head, HHAAlign_Default);
        PlaceEntityOnTraversable(Grid, Entity, CharTile);

        entity *Conversation = AddConversation(Grid->Region);
        PlaceEntityAtTraversable(Grid, Conversation, TalkTile);
    }
}

internal gen_v3 CalcBasePForOffset(edit_grid *Grid, v3s dStep)
{
    // TODO(casey): Do this next weekend.
    gen_v3 Result = {1, 1, 0};
    return(Result);
}

GEN_CREATE_ENTITY_PATTERN(StandardLightingPattern)
{
    random_series *Entropy = Grid->Gen->Entropy;

    gen_v3 LightSpacing = {5, 5, 1};
    gen_v3 BaseP = CalcBasePForOffset(Grid, LightSpacing);
    for(edit_tile Tile = IterateAsPlanarTiles(Grid, BaseP);
            IsValid(Tile);
            Advance(&Tile, BaseP, LightSpacing))
    {
        entity *Light = AddEntity(Grid->Region);
        v3 LampLight = V3(RandomBetween(Entropy, 0.4f, 0.7f),
                          RandomBetween(Entropy, 0.4f, 0.7f),
                          0.5f);
        AddPieceLight(Light, 1.0f, V3(0, 0, 2.5f), 10.0f, LampLight);

        PlaceEntityAtP(Light, Tile);
    }

#if 0
        entity *Lamp = AddObstacle(Grid->Region, GetTagHash(Asset_Scenery, Tag_Orphan, Tag_Lamp));
        PlaceEntityOnTraversable(Grid->Region, Lamp, Ref);
        v3 LampLight = V3(RandomBetween(Gen->Entropy, 0.4f, 0.7f),
                          RandomBetween(Gen->Entropy, 0.4f, 0.7f),
                          0.5f);
        AddPieceLight(Lamp, 0.5f, V3(0, 0, 2.5f), 20.0f, LampLight);
#endif
}
