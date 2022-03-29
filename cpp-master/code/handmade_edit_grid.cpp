/* ========================================================================
   $File: C:\work\handmade\code\handmade_edit_grid.cpp $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright by Molly Rocket, Inc., All Rights Reserved. $
   ======================================================================== */

inline world_position
ChunkPositionFromTilePosition(world_generator *Gen, s32 AbsTileX, s32 AbsTileY, s32 AbsTileZ,
                              v3 AdditionalOffset = V3(0, 0, 0))
{
    world_position BasePos = {};
    
    v3 TileDim = Gen->TileDim;
    v3 Offset = TileDim*V3((real32)AbsTileX, (real32)AbsTileY, (real32)AbsTileZ);
    world_position Result = MapIntoChunkSpace(Gen->World, BasePos, AdditionalOffset + Offset);
    
    Assert(IsCanonical(Gen->World, Result.Offset_));
    
    return(Result);
}

inline world_position
ChunkPositionFromTilePosition(world_generator *Gen, gen_v3 AbsTile,
                              v3 AdditionalOffset = V3(0, 0, 0))
{
    world_position Result = ChunkPositionFromTilePosition(Gen, AbsTile.x, AbsTile.y, AbsTile.z, AdditionalOffset);;
    return(Result);
}

internal edit_tile_contents *
GetTile(edit_grid *Grid, s32 XIndex, s32 YIndex, s32 ZIndex)
{
    edit_tile_contents *Result = 0;
    gen_v3 Dim = Grid->TileCount;
    
    if((XIndex >= 0) &&
       (YIndex >= 0) &&
       (ZIndex >= 0) &&
       (XIndex < Dim.x) &&
       (YIndex < Dim.y) &&
       (ZIndex < Dim.z))
    {
        Result = Grid->Tiles + (Dim.x*Dim.y)*ZIndex + (Dim.x)*YIndex + XIndex;
    }
    
    return(Result);
}

internal edit_tile_contents *
GetTile(edit_grid *Grid, gen_v3 P)
{
    edit_tile_contents *Result = GetTile(Grid, P.x, P.y, P.z);
    return(Result);
}

internal edit_tile_contents *
GetTile(edit_tile *Tile)
{
    edit_tile_contents *Result = 0;
    if(Tile->Grid)
    {
        Result = GetTile(Tile->Grid, Tile->RelIndex);
    }
    
    return(Result);
}

internal edit_tile_contents *
GetTileAbs(edit_grid *Grid, gen_v3 PAbs)
{
    gen_v3 P = PAbs - Grid->MinTile;
    edit_tile_contents *Result = GetTile(Grid, P.x, P.y, P.z);
    return(Result);
}

internal rectangle3 
GetRoomVolume(edit_grid *Grid)
{
    rectangle3 Result = Grid->RoomDim;
    return(Result);
}

internal world_position
GetRoomMinWorldP(edit_grid *Grid)
{
    world *World = Grid->Gen->World;
    world_position Result = MapIntoChunkSpace(World, Grid->BaseP, Grid->RoomDim.Min);
    return(Result);
}

internal world_position
GetRoomMaxWorldP(edit_grid *Grid)
{
    world *World = Grid->Gen->World;
    world_position Result = MapIntoChunkSpace(World, Grid->BaseP, Grid->RoomDim.Max);
    return(Result);
}

internal edit_grid *
BeginGridEdit(world_generator *Gen, gen_volume Volume)
{
    memory_arena *Memory = &Gen->TempMemory;
    temporary_memory TempMem = BeginTemporaryMemory(Memory);
    
    edit_grid *Grid = PushStruct(Memory, edit_grid);
    
    Grid->Gen = Gen;
    Grid->TempMem = TempMem;
    Grid->TileCount = GetDim(Volume);
    Grid->MinTile = Volume.Min;
    Grid->TileDim = Gen->TileDim;
    Grid->Series = &Gen->World->GameEntropy;
    
    v3 RoomDim =
    {
        Grid->TileCount.x * Grid->TileDim.x,
        Grid->TileCount.y * Grid->TileDim.y,
        Grid->TileCount.z * Grid->TileDim.z,
    };
    
    v3 MinRoomP = {};
    v3 MaxRoomP = MinRoomP + RoomDim;
    Grid->RoomDim = AddRadiusTo(RectMinMax(MinRoomP, MaxRoomP), V3(0, 0, 5.0f));
        
    Grid->BaseP = ChunkPositionFromTilePosition(Gen, Grid->MinTile);
    
    rectangle3 ChangeRect = AddRadiusTo(Grid->RoomDim, 1.0f*Grid->TileDim);
    Grid->Region = BeginWorldChange(Gen->Assets, Memory, Gen->World, Grid->BaseP, ChangeRect, ChangeRect, 0);
    
    Grid->Tiles = PushArray(Memory, GetTotalVolume(Grid->TileCount), edit_tile_contents);
    
    return(Grid);
}

internal void
EndGridEdit(edit_grid *Grid)
{
    EndWorldChange(Grid->Region);
    EndTemporaryMemory(Grid->TempMem);
}

internal gen_v3
GetAbsoluteTileIndex(edit_tile *Tile)
{
    edit_grid *Grid = Tile->Grid;
    gen_v3 Result = Grid->MinTile + Tile->RelIndex;
    return(Result);
}

internal b32x
IsOnEdge(edit_tile *Tile)
{
    gen_v3 P = Tile->RelIndex;
    edit_grid *Grid = Tile->Grid;
    b32x Result = ((P.x == 0) ||
                   (P.x == (Grid->TileCount.x - 1)) ||
                   (P.y == 0) ||
                   (P.y == (Grid->TileCount.y - 1)));
    return(Result);
}

internal rectangle3
GetTotalVolume(edit_tile *Tile)
{
    edit_grid *Grid = Tile->Grid;
    v3 MinP = {(f32)Tile->RelIndex.x, (f32)Tile->RelIndex.y, 0};
    MinP = (Grid->TileDim*MinP) - 0.5f*Grid->TileDim;
    
    v3 MaxP = MinP + Grid->TileDim;
    
    rectangle3 Result = RectMinMax(MinP, MaxP);
    return(Result);
}

internal v3
GetMinZCenterP(edit_tile *Tile)
{
    v3 Result = GetMinZCenterP(GetTotalVolume(Tile));
    return(Result);
}

internal rectangle3
GetVolumeFromMinZ(edit_tile *Tile, f32 Height)
{
    rectangle3 Result = GetTotalVolume(Tile);
    Result.Max.z = (Result.Min.z + Height);
    return(Result);
}

internal edit_tile
IterateAsPlanarTiles(edit_grid *Grid, v3s BaseRelIndex = {0, 0, 0})
{
    edit_tile Tile = {};
    if(Grid->TileCount.x && Grid->TileCount.y && Grid->TileCount.z)
    {
        Tile.Grid = Grid;
        Tile.RelIndex = BaseRelIndex;
    }
    
    return(Tile);
}

internal b32x
IsValid(edit_tile Tile)
{
    b32x Result = (Tile.Grid != 0);
    return(Result);
}

internal void
Advance(edit_tile *Tile, v3s Base = {0, 0, 0}, v3s dGrid = {1, 1, 1})
{
    edit_grid *Grid = Tile->Grid;
    Tile->RelIndex.x += dGrid.x;
    if(Tile->RelIndex.x >= Grid->TileCount.x)
    {
        Tile->RelIndex.x = Base.x;
        Tile->RelIndex.y += dGrid.y;
        if(Tile->RelIndex.y >= Grid->TileCount.y)
        {
            Tile->Grid = 0;
        }
    }
}

internal b32
TraversableIsOpen(edit_tile Tile)
{
    b32 Result = false;
    
    edit_tile_contents *Contents = GetTile(&Tile);
    if(Contents)
    {
        traversable_reference Ref = {};
        if(Contents->Structural && Contents->Structural->TraversableCount)
        {
            Ref.Entity = Contents->Structural->ID;
            Result = !IsOccupied(Tile.Grid->Region, Ref);
        }
    }
        
    return(Result);
}

internal edit_tile
FindRandomOpenTile(edit_grid *Grid, v2 StartUV = V2(0.5f, 0.5f))
{
    edit_tile Result = {};
    f32 BestDistSq = F32Max;
    
    v2 StartP = Hadamard(StartUV, V3(Grid->TileCount).xy);
                
    for(edit_tile Tile = IterateAsPlanarTiles(Grid);
        IsValid(Tile);
        Advance(&Tile))
    {
        if(TraversableIsOpen(Tile))
        {
            f32 DistSq = LengthSq(V3(Tile.RelIndex).xy - StartP);
            if(BestDistSq > DistSq)
            {
                BestDistSq = DistSq;
                Result = Tile;
            }
        }
    }
    
    return(Result);
}

internal edit_tile 
FindAdjacentOpenTile(edit_tile BaseTile, u32 DirMask)
{
    // TODO(casey): This should probably use a stochastic selection
    // for the direction, or we pass in an array of ordering, or
    // something, so it's not always biased towards one direction.
    edit_tile Result = {};
    
    for(u32 Dir = 0;
        Dir < BoxIndex_Count;
        ++Dir)
    {
        box_surface_index DirIndex = (box_surface_index)Dir;
        u32 Mask = GetSurfaceMask(DirIndex);
        if(DirMask & Mask)
        {
            gen_v3 dTileP = GetDirection(DirIndex);
            edit_tile CheckTile = BaseTile;
            CheckTile.RelIndex = CheckTile.RelIndex + dTileP;
            if(TraversableIsOpen(CheckTile))
            {
                Result = CheckTile;
                break;
            }
        }
    }
    
    return(Result);
}
