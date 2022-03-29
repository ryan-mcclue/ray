/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

internal f32
GetCameraOffsetZForCloseup(void)
{
    f32 Result = 6.0f;
    return(Result);
}

internal f32
GetCameraOffsetZForDim(gen_v3 Dim, u32 *CameraBehavior)
{
    s32 XCount = Dim.x;
    s32 YCount = Dim.y;

    f32 XDist = 13.0f;
    if(XCount == 12)
    {
        XDist = 14.0f;
    }
    else if(XCount == 13)
    {
        XDist = 15.0f;
    }
    else if(XCount == 14)
    {
        XDist = 16.0f;
        *CameraBehavior |= Camera_ViewPlayerX;
    }
    else if(XCount >= 15)
    {
        XDist = 17.0f;
        *CameraBehavior |= Camera_ViewPlayerX;
    }

    f32 YDist = 13.0f;
    if(YCount == 10)
    {
        YDist = 15.0f;
    }
    else if(YCount == 11)
    {
        YDist = 17.0f;
    }
    else if(YCount == 12)
    {
        YDist = 19.0f;
        *CameraBehavior |= Camera_ViewPlayerY;
    }
    else if(YCount >= 13)
    {
        YDist = 21.0f;
        *CameraBehavior |= Camera_ViewPlayerY;
    }

    f32 Result = Maximum(XDist, YDist);

    return(Result);
}

internal void
GenerateRoom(world_generator *Gen, gen_room *Room)
{
    gen_room_spec *Spec = Room->Spec;

    f32 FullWallHeight = 2.0f;

    edit_grid *Grid = BeginGridEdit(Gen, Room->Vol);
    for(edit_tile Tile = IterateAsPlanarTiles(Grid);
        IsValid(Tile);
        Advance(&Tile))
    {
        gen_v3 AbsIndex = GetAbsoluteTileIndex(&Tile);
        v3 P = GetMinZCenterP(&Tile);

        edit_tile_contents *Contents = GetTile(&Tile);
        Assert(Contents);

        b32x Traversable = true;
        b32x OnEdge = IsOnEdge(&Tile);
        b32 OnBoundary = OnEdge;
        f32 tStair = 0.0f;
        b32 OnConnection = false;
        b32 Stairwell = false;
        if(Spec->Outdoors)
        {
            OnBoundary = false;
        }

        for(gen_room_connection *RoomCon = Room->FirstConnection;
            RoomCon;
            RoomCon = RoomCon->Next)
        {
            gen_connection *Con = RoomCon->Connection;
            if(IsInVolume(&Con->Vol, AbsIndex))
            {
                if(RoomCon->PlacedDirection == BoxIndex_Down)
                {
                    Stairwell = true;
                    tStair = (f32)(AbsIndex.y - Con->Vol.Min.y + 1) /
                        (f32)(Con->Vol.Max.y - Con->Vol.Min.y + 2);
                }
                else if(RoomCon->PlacedDirection == BoxIndex_Up)
                {
                    Traversable = false;
                }

                OnConnection = true;
            }
        }

        entity *Entity = AddEntity(Grid->Region);

        v4 Color = sRGBLinearize(0.31f, 0.49f, 0.32f, 1.0f);
        if(OnConnection)
        {
            Color = sRGBLinearize(0.21f, 0.29f, 0.42f, 1.0f);
        }

        asset_tag_id TileType = Tag_Floor;
        asset_tag_id TileMaterial = Tag_Wood;

        f32 WallHeight = 0.5f;

        b32x OnWall = (OnBoundary && !OnConnection);
        b32x PlaceTree = (Spec->Outdoors && !OnConnection && OnEdge);
        b32x RandomizeTop = false;
        if(OnWall)
        {
            WallHeight = FullWallHeight;
            Color = sRGBLinearize(0.5f, 0.2f, 0.2f, 1.0f);
            TileType = Tag_Wall;
            TileMaterial = Tag_Wood;
            Traversable = false;
        }
        else
        {
            TileType = Tag_Floor;
            if(Spec->Outdoors)
            {
                TileMaterial = Tag_Grass;
                Entity->GroundCoverSpecs[0].Type = CoverType_ThickGrass;
                Entity->GroundCoverSpecs[0].Density = 64;
            }
            else
            {
                TileMaterial = Spec->StoneFloor ? Tag_Stone : Tag_Wood;
            }
            RandomizeTop = true;
        }

        rectangle3 Vol = MakeRelative(GetVolumeFromMinZ(&Tile, WallHeight), P);

        if(Traversable)
        {
            Entity->TraversableCount = 1;
            Entity->Traversables[0].P = GetMaxZCenterP(Vol);
            Clear(&Entity->Traversables[0].Occupier);
        }

        if(Stairwell)
        {
            P.z -= tStair*Grid->TileDim.z;
        }
        v3 BasisP = P;

        P.x += 0.0f;
        P.y += 0.0f;
        //P.z += 0.5f*RandomUnilateral(Grid->Series);

        Color = sRGBLinearize(0.8f, 0.8f, 0.8f, 1.0f);
        entity_visible_piece *Piece =
            AddPiece(Entity, GetTagHash(Asset_Block, TileType, TileMaterial), GetRadius(Vol), GetCenter(Vol), Color, PieceType_Cube);
        Piece->Variant = RandomUnilateral(Gen->Entropy);

        if(RandomizeTop)
        {
            Piece->CubeUVLayout = EncodeCubeUVLayout(0, 0, 0, 0, 0, 0,
                                                     RandomChoice(Gen->Entropy, 4),
                                                     RandomChoice(Gen->Entropy, 4));
        }

        rectangle3 OccluderVol = OnWall ? MakeRelative(GetTotalVolume(&Tile), P) : Vol;
        AddPieceOccluder(Entity, GetCenter(OccluderVol), GetRadius(OccluderVol), Color.rgb);
        if(!OnWall && !Spec->Outdoors)
        {
            rectangle3 CeilingVol = MakeRelative(GetTotalVolume(&Tile), P);
            CeilingVol.Min.z = CeilingVol.Max.z - 0.1f;
            AddPieceOccluder(Entity, GetCenter(CeilingVol), GetRadius(CeilingVol), Color.rgb);
        }

        Entity->P = P;
        Entity->CollisionVolume = Vol;
        if(OnWall)
        {
            Entity->CollisionVolume.Max.z += FullWallHeight;;
        }

        Contents->Structural = Entity;
        b32x Open = (!Stairwell && !OnConnection &&
                         (Entity->TraversableCount == 1));

        if(Open)
        {
            traversable_reference Ref = {};
            Ref.Entity = Contents->Structural->ID;
            v3 GroundP = GetSimSpaceTraversable(Grid->Region, Ref).P;
            if(PlaceTree)
            {
                entity *Tree = AddTree(Gen, Grid->Region);
                PlaceEntityOnTraversable(Grid->Region, Tree, Ref);

                rectangle3 TreeVol = MakeRelative(GetTotalVolume(&Tile), P);
                TreeVol = AddRadiusTo(TreeVol, -V3(0.25f, 0.25f, 1.0f));
                AddPieceOccluder(Tree, GetCenter(TreeVol), GetRadius(TreeVol), Color.rgb);
            }
        }
    }

    u32 DoorIndex = 0;
    for(gen_room_connection *RoomCon = Room->FirstConnection;
        RoomCon;
        RoomCon = RoomCon->Next)
    {
        gen_connection *Con = RoomCon->Connection;
        if(IsValid(RoomCon->DoorBrainID))
        {
            Assert(DoorIndex < ArrayCount(brain_switches::Unlocks));

            gen_volume DoorVol = Intersect(&Con->Vol, &Room->Vol);
            edit_tile_contents *DoorTile = GetTileAbs(Grid, DoorVol.Min);
            v3 DoorDim = Grid->TileDim;
            DoorDim.z = FullWallHeight;

            traversable_reference Ref = {};
            Ref.Entity = DoorTile->Structural->ID;
            entity *Door = AddBlock(Grid->Region, GetTagHash(Asset_Block, Tag_Floor, Tag_Stone), DoorDim, V4(1, 1, 1, 1));
            PlaceEntityOnTraversable(Grid->Region, Door, Ref);
            Door->BrainSlot = IndexedBrainSlotFor(brain_switches, Unlocks, DoorIndex);
            Door->BrainID = RoomCon->DoorBrainID;

            ++DoorIndex;
        }
    }

    for(gen_entity_pattern *Pattern = Room->FirstPattern;
            Pattern;
            Pattern = Pattern->Next)
    {
        Pattern->Creator(Grid, Pattern);
    }

    entity *CamRoom = AddEntity(Grid->Region);
    CamRoom->Flags |= EntityFlag_AllowsMotionOnCollision;
    CamRoom->P = GetCenter(GetRoomVolume(Grid));
    CamRoom->CollisionVolume = MakeRelative(GetRoomVolume(Grid), CamRoom->P);
    CamRoom->BrainSlot = SpecialBrainSlot(Type_brain_room);
    CamRoom->CameraOffset.z = GetCameraOffsetZForDim(Grid->TileCount, &CamRoom->CameraBehavior);

    world_room *WorldRoom = AddWorldRoom(Gen->World, GetRoomMinWorldP(Grid), GetRoomMaxWorldP(Grid));

    EndGridEdit(Grid);

    if(Spec->Apron)
    {
        gen_apron *Apron = GenApron(Gen, Spec->Apron);
        Apron->Vol = AddRadiusTo(&Room->Vol, GenV3(8, 8, 0));
    }
}

internal void
GenerateApron(world_generator *Gen,
              gen_apron *Apron)
{
    gen_apron_spec *Spec = Apron->Spec;

    edit_grid *Grid = BeginGridEdit(Gen, Apron->Vol);
    for(edit_tile Tile = IterateAsPlanarTiles(Grid);
        IsValid(Tile);
        Advance(&Tile))
    {
        f32 Eps = 0.001f;
        if(!OverlappingEntitiesExist(Grid->Region, AddRadiusTo(GetTotalVolume(&Tile), V3(-Eps, -Eps, -Eps))))
        {
            v3 P = GetMinZCenterP(&Tile);
            f32 Height = RandomBetween(Grid->Series, 0.5f, 1.0f);
            rectangle3 Vol = MakeRelative(GetVolumeFromMinZ(&Tile, Height), P);

            entity *Entity = AddEntity(Grid->Region);
            Entity->P = P;
            Entity->CollisionVolume = Vol;

            v4 Color = sRGBLinearize(0.5f, 0.5f, 0.5f, 1.0f);

            entity_visible_piece *Piece = AddPiece(Entity, GetTagHash(Asset_Block, Tag_Floor, Tag_Grass), GetRadius(Vol), GetCenter(Vol), Color, PieceType_Cube);

            Entity->GroundCoverSpecs[0].Type = CoverType_ThickGrass;
            Entity->GroundCoverSpecs[0].Density = 64;

            v3 GroundP = P + GetMaxZCenterP(Vol);
            if(RandomChoice(Grid->Series, 3))
            {
                entity *Tree = AddTree(Gen, Grid->Region);
                PlaceEntityAtP(Tree, GroundP);
            }
        }
    }
    EndGridEdit(Grid);
}
