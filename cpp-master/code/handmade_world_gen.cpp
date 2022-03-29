/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

internal u32x
GetDirMaskFromRoom(gen_connection *Connection, gen_room *From)
{
    u32x DirMask = Connection->DirMaskFromA;
    if(Connection->B == From)
    {
        DirMask = GetBoxMaskComplement(DirMask);
    }
    else
    {
        Assert(Connection->A == From);
    }

    return(DirMask);
}

internal b32x
CouldGoDirection(gen_connection *Connection, gen_room *From, u32x TestMask)
{
    u32x DirMask = GetDirMaskFromRoom(Connection, From);
    b32x Result = (DirMask & TestMask);
    return(Result);
}

internal b32x
CouldGoDirection(gen_connection *Connection, gen_room *From, u32x Dim, u32x Side)
{
    b32x Result = CouldGoDirection(Connection, From, GetSurfaceMask(Dim, Side));
    return(Result);
}

internal gen_apron_spec *
GenApronSpec(world_generator *Gen)
{
    gen_apron_spec *Spec = PushStruct(&Gen->Memory, gen_apron_spec);
    return(Spec);
}

internal gen_room_spec *
GenSpec(world_generator *Gen, gen_apron_spec *ApronSpec = 0)
{
    gen_room_spec *Spec = PushStruct(&Gen->Memory, gen_room_spec);
    Spec->Apron = ApronSpec;

    return(Spec);
}

internal gen_apron *
GenApron(world_generator *Gen, gen_apron_spec *Spec)
{
    gen_apron *Apron = PushStruct(&Gen->Memory, gen_apron);

    Apron->Spec = Spec;
    Apron->GlobalNext = Gen->FirstApron;
    Gen->FirstApron = Apron;

    return(Apron);
}

#if HANDMADE_INTERNAL
#define GenRoom(Gen, Spec, Label) GenRoom_(Gen, Spec, Label)
internal gen_room *
GenRoom_(world_generator *Gen, gen_room_spec *Spec, char *Label)
#else
#define GenRoom(Gen, Spec, Label) GenRoom_(Gen, Spec)
internal gen_room *
GenRoom_(world_generator *Gen, gen_room_spec *Spec)
#endif
{
    gen_room *Room = PushStruct(&Gen->Memory, gen_room);

    Room->Spec = Spec;

    Room->GlobalNext = Gen->FirstRoom;
    Gen->FirstRoom = Room;

#if HANDMADE_INTERNAL
    Room->DebugLabel = Label;
#endif

    return(Room);
}

internal gen_room_connection *
AddRoomConnection(world_generator *Gen, gen_room *Room, gen_connection *Connection)
{
    gen_room_connection *RoomCon = PushStruct(&Gen->Memory, gen_room_connection);

    RoomCon->Connection = Connection;
    RoomCon->Next = Room->FirstConnection;

    Room->FirstConnection = RoomCon;

    return(RoomCon);
}

internal gen_connection *
Connect(world_generator *Gen, gen_room *A, gen_room *B, u32 DirMaskFromA = BoxMask_Planar)
{
    gen_connection *Connection = PushStruct(&Gen->Memory, gen_connection);

    Connection->DirMaskFromA = DirMaskFromA;
    Connection->A = A;
    Connection->B = B;

    Connection->GlobalNext = Gen->FirstConnection;
    Gen->FirstConnection = Connection;

    Connection->AToB = AddRoomConnection(Gen, A, Connection);
    Connection->BToA = AddRoomConnection(Gen, B, Connection);

    return(Connection);
}

internal gen_connection *
Connect(world_generator *Gen, gen_room *A, box_surface_index Direction, gen_room *B)
{
    gen_connection *Result = Connect(Gen, A, B, GetSurfaceMask(Direction));
    return(Result);
}

internal void
SetSize(world_generator *Gen, gen_room_spec *Spec, s32 DimX, s32 DimY, s32 DimZ = 1)
{
    Spec->RequiredDim.x = DimX;
    Spec->RequiredDim.y = DimY;
    Spec->RequiredDim.z = DimZ;
}

internal gen_option_iterator
IterateOptions(world_generator *Gen, gen_option_type Type)
{
    gen_option_iterator Result = {};

    // TODO(casey): Not yet implemented for reals
    gen_option_array *Array = Gen->OptionArrays + Type;
    if(Array->OptionCount)
    {
        Result.Room = Array->Options[--Array->OptionCount].Room;
    }

    return(Result);
}

internal b32x
IsValid(gen_option_iterator *Iter)
{
    // TODO(casey): Not yet implemented for reals
    b32x Result = (Iter->Room != 0);
    return(Result);
}

internal void
Advance(gen_option_iterator *Iter)
{
    // TODO(casey): Not yet implemented for reals
    Iter->Room = 0;
}

internal void
Finish(gen_option_iterator *Iter)
{
    // TODO(casey): Not yet implemented for reals
    Iter->Room = 0;
}

internal gen_option *
AddOption(world_generator *Gen, gen_room *Room, gen_option_type OptionType)
{
    gen_option_array *Array = Gen->OptionArrays + OptionType;
    Assert(Array->OptionCount <= Array->MaxOptionCount);

    if(Array->OptionCount == Array->MaxOptionCount)
    {
        // TODO(casey): Very wasteful here (doesn't free the old option lock).  Do we care?
        Array->MaxOptionCount += 100;
        gen_option *NewOptions = PushArray(&Gen->Memory, Array->MaxOptionCount, gen_option);
        CopyArray(Array->OptionCount, Array->Options, NewOptions);
        Array->Options = NewOptions;
    }

    gen_option *Result = Array->Options + Array->OptionCount++;
    Result->Room = Room;

    return(Result);
}

internal world_generator *
BeginWorldGen(world *World, game_assets *Assets)
{
    world_generator *Gen = BootstrapPushStruct(world_generator, Memory);
    Gen->World = World;
    Gen->Assets = Assets;

    f32 TileSideInMeters = 1.4f;
    f32 TileDepthInMeters = World->ChunkDimInMeters.z; // TOOD(casey): Probably this will not be true, maybe?  We shall see.
    Gen->TileDim = V3(TileSideInMeters, TileSideInMeters, TileDepthInMeters);

    return(Gen);
}

internal gen_room *
GetOtherRoom(gen_connection *Connection, gen_room *FromRoom)
{
    gen_room *Result = Connection->A;
    if(Connection->A == FromRoom)
    {
        Assert(Connection->B != FromRoom); // NOTE(casey): Should only fire on connections that are from and to the same room!
        Result = Connection->B;
    }
    else
    {
        Assert(Connection->B == FromRoom);
    }

    return(Result);
}

internal gen_room_connection *
GetRoomConnectionTo(gen_room *FromRoom, gen_room *ToRoom)
{
    gen_room_connection *Result = 0;

    for(gen_room_connection *Test = FromRoom->FirstConnection;
        Test;
        Test = Test->Next)
    {
        if(GetOtherRoom(Test->Connection, FromRoom) == ToRoom)
        {
            Result = Test;
            break;
        }
    }

    return(Result);
}

internal void
PushRoom(gen_room_stack *Stack, gen_room *Room)
{
    Assert(Room);

    if(!Stack->FirstFree)
    {
        Stack->FirstFree = PushStruct(Stack->Memory, gen_room_stack_entry);
    }

    gen_room_stack_entry *Entry = Stack->FirstFree;
    Stack->FirstFree = Entry->Prev;

    Entry->Room = Room;
    Entry->Prev = Stack->Top;
    Stack->Top = Entry;
}

internal b32x
HasEntries(gen_room_stack *Stack)
{
    b32x Result = (Stack->Top != 0);
    return(Result);
}

internal gen_room *
PopRoom(gen_room_stack *Stack)
{
    gen_room *Result = 0;

    gen_room_stack_entry *Popped = Stack->Top;
    if(Popped)
    {
        Result = Popped->Room;
        Assert(Result);

        Stack->Top = Popped->Prev;

        Popped->Prev = Stack->FirstFree;
        Stack->FirstFree = Popped;

        // NOTE(casey): Just for safety's sake
        Popped->Room = 0;
    }

    return(Result);
}

internal void
PushConnectedRooms(gen_room_stack *Stack, gen_room *Room, u32 GenerationIndex)
{
    for(gen_room_connection *RoomCon = Room->FirstConnection;
        RoomCon;
        RoomCon = RoomCon->Next)
    {
        gen_connection *Connection = RoomCon->Connection;
        gen_room *OtherRoom = GetOtherRoom(Connection, Room);
        if(OtherRoom->GenerationIndex != GenerationIndex)
        {
            PushRoom(Stack, OtherRoom);
        }
    }
}

internal void
PlaceRoomInVolume(gen_room *Room, gen_volume Vol)
{
    Room->Vol = Vol;
}

internal b32x
PlaceRoom(world_generator *Gen, world *World, gen_room *Room,
          gen_volume *MinVol, gen_volume *MaxVol,
          gen_room_connection *RoomCon)
{
    b32x Result = false;

    while(RoomCon)
    {
        gen_connection *Connection = RoomCon->Connection;
        gen_room *OtherRoom = GetOtherRoom(Connection, Room);
        if(OtherRoom->GenerationIndex == Room->GenerationIndex)
        {
            break;
        }

        RoomCon = RoomCon->Next;
    }

    if(RoomCon)
    {
        gen_connection *Connection = RoomCon->Connection;
        gen_room *OtherRoom = GetOtherRoom(Connection, Room);
        gen_room_connection *OtherRoomCon = GetRoomConnectionTo(Room, OtherRoom);

        // TODO(casey): Use the "box" code to turn this into one loop.
        for(u32 Dim = 0;
            !Result && (Dim < 3);
            ++Dim)
        {
            for(u32 Side = 0;
                !Result && (Side < 2);
                ++Side)
            {
                // TODO(casey): Make sure this coincides with the actual
                // way I'm using Side etc. down below
                if(CouldGoDirection(Connection, OtherRoom, Dim, Side))
                {
                    gen_volume NewMinVol = *MinVol;
                    gen_volume NewMaxVol = *MaxVol;

                    if(Side)
                    {
                        ClipMin(&NewMinVol, Dim, OtherRoom->Vol.Max.E[Dim] + 1);
                        ClipMax(&NewMinVol, Dim, OtherRoom->Vol.Max.E[Dim] + 1);

                        ClipMin(&NewMaxVol, Dim, OtherRoom->Vol.Max.E[Dim] + 1);
                    }
                    else
                    {
                        ClipMax(&NewMinVol, Dim, OtherRoom->Vol.Min.E[Dim] - 1);

                        ClipMin(&NewMaxVol, Dim, OtherRoom->Vol.Min.E[Dim] - 1);
                        ClipMax(&NewMaxVol, Dim, OtherRoom->Vol.Min.E[Dim] - 1);
                    }

                    for(u32 OtherDim = 0;
                        OtherDim < 3;
                        ++OtherDim)
                    {
                        if(OtherDim != Dim)
                        {
                            // TODO(casey): We probably want to push this _in_ slightly,
                            // so that doors won't occur on the last place, etc.
                            s32 InteriorApron = ((OtherDim == 2) ? 0 : 4);
                            ClipMax(&NewMinVol, OtherDim, OtherRoom->Vol.Max.E[OtherDim] - InteriorApron);
                            ClipMin(&NewMaxVol, OtherDim, OtherRoom->Vol.Min.E[OtherDim] + InteriorApron);
                        }
                    }

                    gen_volume TestVol = GetMaximumVolumeFor(NewMinVol, NewMaxVol);
                    if(IsMinimumDimensionsForRoom(TestVol))
                    {
                        Result = PlaceRoom(Gen, World, Room, &NewMinVol, &NewMaxVol, RoomCon->Next);
                        if(Result)
                        {
                            gen_volume Door = Intersect(&Room->Vol, &OtherRoom->Vol);
                            s32 DoorAt = (Side ? Room->Vol.Min.E[Dim] : OtherRoom->Vol.Min.E[Dim]);
                            Door.Min.E[Dim] = DoorAt - 1;
                            Door.Max.E[Dim] = DoorAt;
                            Connection->Vol = Door;

                            RoomCon->PlacedDirection = GetSurfaceIndex(Dim, GetOtherSide(Side));
                            OtherRoomCon->PlacedDirection = GetSurfaceIndex(Dim, Side);
                        }
                    }
                }
            }
        }
    }
    else
    {
        s32 MaxAllowedDim[3] =
        {
            16,//*3,
            9,//*3,
            1,
        };

        Result = true;

        gen_volume FinalVol;
        for(u32 Dim = 0;
            Dim < 3;
            ++Dim)
        {
            s32 Min = MinVol->Min.E[Dim];
            s32 Max = MaxVol->Max.E[Dim];

            if(((Max - Min) + 1) > MaxAllowedDim[Dim])
            {
                Max = Min + MaxAllowedDim[Dim] - 1;
            }

            if(Max < MaxVol->Min.E[Dim])
            {
                Max = MaxVol->Min.E[Dim];
                Min = Max - MaxAllowedDim[Dim] + 1;
            }

            if(Min > MinVol->Max.E[Dim])
            {
                Result = false;
            }

            FinalVol.Min.E[Dim] = Min;
            FinalVol.Max.E[Dim] = Max;
        }

        if(Result)
        {
            PlaceRoomInVolume(Room, FinalVol);
        }
    }

    return(Result);
}

internal s32
GetDeltaAlongAxisForCleanPlacement(world_generator *Gen, gen_volume *Vol, s32 EdgeAxis,
                                   u32 GenerationIndex)
{
    s32 Result = 0;

    for(gen_room *Room = Gen->FirstRoom;
        Room;
        Room = Room->GlobalNext)
    {
        if(Room->GenerationIndex == GenerationIndex)
        {
            gen_volume Inter = Intersect(&Room->Vol, Vol);
            if(HasVolume(Inter))
            {
                // TODO(casey): Actually return the amount to move
                Result = 1;
                break;
            }
        }
    }

    return(Result);
}

internal b32
PlaceRoomAlongEdge(world_generator *Gen, gen_room *BaseRoom, gen_connection *Connection,
                   box_surface_index SurfaceIndex, u32 GenerationIndex)
{
    Assert(CouldGoDirection(Connection, BaseRoom, GetSurfaceMask(SurfaceIndex)));

    gen_v3 AddRadius = {};
    u32x RelXAxis = 0;
    u32x RelYAxis = 0;
    u32x RelZAxis = 0;
    b32x RelZAxisMin = false;
    switch(SurfaceIndex)
    {
        case BoxIndex_West:
        {
            RelXAxis = 1;
            RelYAxis = 2;
            RelZAxis = 0;
            RelZAxisMin = true;
        } break;

        case BoxIndex_East:
        {
            RelXAxis = 1;
            RelYAxis = 2;
            RelZAxis = 0;
            RelZAxisMin = false;
        } break;

        case BoxIndex_South:
        {
            RelXAxis = 0;
            RelYAxis = 2;
            RelZAxis = 1;
            RelZAxisMin = true;
        } break;

        case BoxIndex_North:
        {
            RelXAxis = 0;
            RelYAxis = 2;
            RelZAxis = 1;
            RelZAxisMin = false;
        } break;

        case BoxIndex_Down:
        {
            RelXAxis = 0;
            RelYAxis = 1;
            RelZAxis = 2;
            RelZAxisMin = true;
            AddRadius = GenV3(1, 1, 0);
        } break;

        case BoxIndex_Up:
        {
            RelXAxis = 0;
            RelYAxis = 1;
            RelZAxis = 2;
            RelZAxisMin = false;
            AddRadius = GenV3(1, 1, 0);
        } break;

        InvalidDefaultCase;
    }

    gen_room *Room = GetOtherRoom(Connection, BaseRoom);
    Assert(Room->GenerationIndex != GenerationIndex);
    gen_room_spec *Spec = Room->Spec;

    gen_volume TestVol;
    if(RelZAxisMin)
    {
        TestVol.Max.E[RelZAxis] = BaseRoom->Vol.Min.E[RelZAxis] - 1;
        TestVol.Min.E[RelZAxis] = TestVol.Max.E[RelZAxis] - Spec->RequiredDim.E[RelZAxis] + 1;
    }
    else
    {
        TestVol.Min.E[RelZAxis] = BaseRoom->Vol.Max.E[RelZAxis] + 1;
        TestVol.Max.E[RelZAxis] = TestVol.Min.E[RelZAxis] + Spec->RequiredDim.E[RelZAxis] - 1;
    }

    // TODO(casey): These ranges really want to be done more specifically.
    s32 MinRelX = BaseRoom->Vol.Min.E[RelXAxis];
    s32 MaxRelX = BaseRoom->Vol.Max.E[RelXAxis];

    s32 MinRelY = BaseRoom->Vol.Min.E[RelYAxis];
    s32 MaxRelY = BaseRoom->Vol.Max.E[RelYAxis];

    s32 RelY = MinRelY;
    while((Room->GenerationIndex != GenerationIndex) &&
          (RelY <= MaxRelY))
    {
        TestVol.Min.E[RelYAxis] = RelY;
        TestVol.Max.E[RelYAxis] = RelY + Spec->RequiredDim.E[RelYAxis] - 1;

        s32 RelX = MinRelX;
        while(RelX < MaxRelX)
        {
            TestVol.Min.E[RelXAxis] = RelX;
            TestVol.Max.E[RelXAxis] = RelX + Spec->RequiredDim.E[RelXAxis] - 1;

            s32 DeltaX = GetDeltaAlongAxisForCleanPlacement(Gen, &TestVol, RelXAxis,
                                                            GenerationIndex);
            if(DeltaX == 0)
            {
                Room->GenerationIndex = GenerationIndex;
                Room->Vol = TestVol;

                gen_volume Door = Intersect(&BaseRoom->Vol, &Room->Vol);

                s32 DoorEdgeX = (Door.Min.E[RelXAxis] + Door.Max.E[RelXAxis])/2;
                Door.Min.E[RelXAxis] = Door.Max.E[RelXAxis] = DoorEdgeX;

                s32 DoorEdgeY = (Door.Min.E[RelYAxis] + Door.Max.E[RelYAxis])/2;
                Door.Min.E[RelYAxis] = Door.Max.E[RelYAxis] = DoorEdgeY;

                s32 MinDoor = Door.Min.E[RelZAxis];
                s32 MaxDoor = Door.Max.E[RelZAxis];
                Door.Min.E[RelZAxis] = MaxDoor;
                Door.Max.E[RelZAxis] = MinDoor;

                Door = AddRadiusTo(&Door, AddRadius);

                Connection->Vol = Door;

                gen_room_connection *RoomCon = GetRoomConnectionTo(Room, BaseRoom);
                gen_room_connection *OtherRoomCon = GetRoomConnectionTo(BaseRoom, Room);

                RoomCon->PlacedDirection = GetSurfaceIndex(RelZAxis, RelZAxisMin);
                OtherRoomCon->PlacedDirection = GetSurfaceIndex(RelZAxis, GetOtherSide(RelZAxisMin));

                break;
            }
            else
            {
                RelX += DeltaX;
            }
        }

        ++RelY;
    }

    b32 Result = (Room->GenerationIndex == GenerationIndex);
    return(Result);
}

internal box_surface_index
GetRandomDirectionFromMask(world_generator *Gen, u32 DirMask)
{
    // TODO(casey): This is super-stoopid!  Maybe try to make the bit-twiddly version
    // of this sometime for fun?
    u32 DirCount = 0;
    box_surface_index Directions[6];
    for(u32 DirIndex = 0;
        DirIndex < ArrayCount(Directions);
        ++DirIndex)
    {
        if(DirMask & GetSurfaceMask((box_surface_index)DirIndex))
        {
            Directions[DirCount++] = (box_surface_index)DirIndex;
        }
    }

    Assert(DirCount);
    box_surface_index Result = Directions[RandomChoice(Gen->Entropy, DirCount)];
    return(Result);
}

internal void
Layout(world_generator *Gen, gen_room *StartAtRoom)
{
    world *World = Gen->World;
    random_series *Series = &World->GameEntropy;

    temporary_memory Temp = BeginTemporaryMemory(&Gen->TempMemory);
    gen_room_stack Stack = {&Gen->TempMemory};
    u32 GenerationIndex = 1;

    gen_room *FirstRoom = StartAtRoom;
    gen_volume Vol;
    Vol.Min.x = 0;
    Vol.Min.y = 0;
    Vol.Min.z = 0;
    Vol.Max.x = Vol.Min.x + FirstRoom->Spec->RequiredDim.x - 1;
    Vol.Max.y = Vol.Min.y + FirstRoom->Spec->RequiredDim.y - 1;
    Vol.Max.z = Vol.Min.z + FirstRoom->Spec->RequiredDim.z - 1;
    PlaceRoomInVolume(FirstRoom, Vol);
    FirstRoom->GenerationIndex = GenerationIndex;
    PushConnectedRooms(&Stack, FirstRoom, GenerationIndex);
    while(HasEntries(&Stack))
    {
        gen_room *Room = PopRoom(&Stack);
        if(Room->GenerationIndex != GenerationIndex)
        {
            for(gen_room_connection *RoomCon = Room->FirstConnection;
                RoomCon;
                RoomCon = RoomCon->Next)
            {
                gen_connection *Connection = RoomCon->Connection;
                gen_room *OtherRoom = GetOtherRoom(Connection, Room);
                if(OtherRoom->GenerationIndex == GenerationIndex)
                {
                    if(Room->GenerationIndex != GenerationIndex)
                    {
                        u32 DirMask = GetDirMaskFromRoom(Connection, OtherRoom);
                        while(DirMask)
                        {
                            box_surface_index Direction =
                                GetRandomDirectionFromMask(Gen, DirMask);

                            if(PlaceRoomAlongEdge(Gen, OtherRoom, Connection,
                                                  Direction, GenerationIndex))
                            {
                                break;
                            }
                            else
                            {
                                DirMask &= ~GetSurfaceMask(Direction);
                            }
                        }

                        Assert(Room->GenerationIndex == GenerationIndex);
                    }
                }
                else
                {
                    PushRoom(&Stack, OtherRoom);
                }
            }
        }
    }

    EndTemporaryMemory(Temp);
}

internal void
GenerateWorld(world_generator *Gen)
{
    for(gen_room *Room = Gen->FirstRoom;
        Room;
        Room = Room->GlobalNext)
    {
        GenerateRoom(Gen, Room);
    }

    for(gen_apron *Apron = Gen->FirstApron;
        Apron;
        Apron = Apron->GlobalNext)
    {
        GenerateApron(Gen, Apron);
    }
}

internal void
EndWorldGen(world_generator *Gen)
{
    ClearUnpackedEntityCache(Gen->World);
    Clear(&Gen->TempMemory);
    Clear(&Gen->Memory);
}

internal gen_entity_pattern *
AddPattern(world_generator *Gen, gen_room *Room, gen_create_pattern *Creator)
{
    gen_entity_pattern *Result = PushStruct(&Gen->Memory, gen_entity_pattern);
    Result->Next = Room->FirstPattern;
    Result->Creator = Creator;
    
    Room->FirstPattern = Result;
    
    return(Result);
}

internal gen_entity_pattern *
PlaceNPC(world_generator *Gen, asset_tag_id NameTag)
{
    gen_entity_pattern *Result = 0;

    for(gen_option_iterator Iter = IterateOptions(Gen, GenOption_Orphan);
        IsValid(&Iter);
        Advance(&Iter))
    {
        gen_room *Room = Iter.Room;

        // TODO(casey): Check this room to see if it meets our other criteria
        // (whatever those happen to be)
        if(1)
        {
            Result = AddPattern(Gen, Room, NPCPattern);
            Result->BaseAsset = GetTagHash(Asset_None, NameTag);
            Finish(&Iter);
        }
    }

    return(Result);
}

internal gen_dungeon
CreateDungeon(world_generator *Gen, s32 FloorCount)
{
    gen_dungeon Result = {};

    gen_room_spec *DungeonSpec = GenSpec(Gen);
    SetSize(Gen, DungeonSpec, 17, 9, 1);

    FloorCount = 4;

    gen_room *RoomAbove = 0;
    for(s32 FloorIndex = 0;
        FloorIndex < FloorCount;
        ++FloorIndex)
    {
        temporary_memory Temp = BeginTemporaryMemory(&Gen->TempMemory);

        gen_room *FloorEntranceRoom = GenRoom(Gen, DungeonSpec, "Floor Entrance");
        if(RoomAbove)
        {
            Connect(Gen, RoomAbove, BoxIndex_Down, FloorEntranceRoom);
        }
        else
        {
            Result.EntranceRoom = FloorEntranceRoom;
        }

        gen_room *PrevRoom = FloorEntranceRoom;
        s32 PathCount = RandomBetween(Gen->Entropy, 4 + FloorIndex/2, 6 + FloorIndex);

        gen_room **Chain = PushArray(&Gen->TempMemory, PathCount, gen_room *);
        for(s32 PathIndex = 0;
            PathIndex < PathCount;
            ++PathIndex)
        {
            gen_room *Room = GenRoom(Gen, DungeonSpec, "Dungeon Path");
            Chain[PathIndex] = Room;

            gen_connection *Connection = Connect(Gen, PrevRoom, Room, BoxMask_Planar);
            Connection->AToB->DoorBrainID = AddBrain(Gen->World);
            
            #if 0
            gen_entity_pattern *Switches = AddPattern(Gen, PrevRoom, TileSwitchPattern);
            Switches->BrainID = Connection->AToB->DoorBrainID;
            #endif
            
            gen_entity_pattern *Snake = AddPattern(Gen, Room, SnakePattern);
            Snake->BaseAsset = GetTagHash(Asset_None, Tag_Undead, Tag_Bones);
            
            gen_entity_pattern *Enemy = AddPattern(Gen, Room, SingleEnemyPattern);
            Enemy->BaseAsset = GetTagHash(Asset_None, Tag_Undead, Tag_Bones);
            
            //AddPattern(Gen, Room, StandardLightingPattern);
            
            PrevRoom = Room;
        }

        // TODO(casey): Need a utility here that removes path rooms when they are
        // chosen, to avoid over-connecting a room with special rooms.
        gen_room *Shop = GenRoom(Gen, DungeonSpec, "Shop");
        Connect(Gen, Chain[RandomChoice(Gen->Entropy, PathCount)], Shop, BoxMask_Planar);
        AddPattern(Gen, Shop, StandardLightingPattern);
        
        gen_room *ItemRoom = GenRoom(Gen, DungeonSpec, "Item Room");
        Connect(Gen, Chain[RandomChoice(Gen->Entropy, PathCount)], ItemRoom, BoxMask_Planar);
        AddPattern(Gen, ItemRoom, StandardLightingPattern);
        
        gen_room *FloorExitRoom = GenRoom(Gen, DungeonSpec, "Floor Exit");
        Connect(Gen, PrevRoom, FloorExitRoom, BoxMask_Planar);
        AddPattern(Gen, FloorExitRoom, StandardLightingPattern);
        
        RoomAbove = FloorExitRoom;

        EndTemporaryMemory(Temp);
    }

    Result.ExitRoom = RoomAbove;

    return(Result);
}

internal gen_orphanage
CreateOrphanage(world_generator *Gen)
{
    gen_orphanage Result = {};

    // TODO(casey): Fix 2-high room bug - camera seems to get confused?

    gen_apron_spec *ApronSpec = GenApronSpec(Gen);

    gen_room_spec *GardenSpec = GenSpec(Gen, ApronSpec);
    GardenSpec->Outdoors = true;

    gen_room_spec *BasicForestSpec = GenSpec(Gen, ApronSpec);
    BasicForestSpec->Outdoors = true;

    gen_room_spec *BedroomSpec = GenSpec(Gen, ApronSpec);
    BedroomSpec->StoneFloor = true;

    gen_room_spec *SaveSlotSpec = GenSpec(Gen, ApronSpec);
    gen_room_spec *MainRoomSpec = GenSpec(Gen, ApronSpec);
    gen_room_spec *TailorSpec = GenSpec(Gen, ApronSpec);
    gen_room_spec *KitchenSpec = GenSpec(Gen, ApronSpec);
    gen_room_spec *VerticalHallwaySpec = GenSpec(Gen, ApronSpec);
    gen_room_spec *HorizontalHallwaySpec = GenSpec(Gen, ApronSpec);

    gen_room *MainRoom = GenRoom(Gen, MainRoomSpec, "Orphanage Main Room");
    gen_room *HeroSaveSlotA = GenRoom(Gen, SaveSlotSpec, "Save Slot A");
    gen_room *HeroSaveSlotB = GenRoom(Gen, SaveSlotSpec, "Save Slot B");
    gen_room *HeroSaveSlotC = GenRoom(Gen, SaveSlotSpec, "Save Slot C");
    gen_room *FrontHall = GenRoom(Gen, VerticalHallwaySpec, "Orphanage Front Hallway");
    gen_room *BackHall = GenRoom(Gen, HorizontalHallwaySpec, "Orphanage Back Hallway");
    gen_room *BedroomA = GenRoom(Gen, BedroomSpec, "Orphanage Bedroom A");
    gen_room *BedroomB = GenRoom(Gen, BedroomSpec, "Orphanage Bedroom B");
    gen_room *BedroomC = GenRoom(Gen, BedroomSpec, "Orphanage Bedroom C");
    gen_room *BedroomD = GenRoom(Gen, BedroomSpec, "Orphanage Bedroom D");
    gen_room *TailorRoom = GenRoom(Gen, TailorSpec, "Orphanage Tailor's Room");
    gen_room *Kitchen = GenRoom(Gen, KitchenSpec, "Orphanage Kitchen");
    gen_room *Garden = GenRoom(Gen, GardenSpec, "Orphanage Garden");
    gen_room *ForestPath = GenRoom(Gen, BasicForestSpec, "Orphanage Forest Path");
    gen_room *ForestEntrance = GenRoom(Gen, BasicForestSpec, "Orphanage Forest Entrance");
#if 0
    gen_room *SideAlley = GenRoom(Gen, BasicForestSpec, "Orphange Side Alley");
#endif
    AddPattern(Gen, MainRoom, StandardLightingPattern);
    AddPattern(Gen, HeroSaveSlotA, StandardLightingPattern);
    AddPattern(Gen, HeroSaveSlotB, StandardLightingPattern);
    AddPattern(Gen, HeroSaveSlotC, StandardLightingPattern);
    AddPattern(Gen, FrontHall, StandardLightingPattern);
    AddPattern(Gen, BackHall, StandardLightingPattern);
    AddPattern(Gen, BedroomA, StandardLightingPattern);
    AddPattern(Gen, BedroomB, StandardLightingPattern);
    AddPattern(Gen, BedroomC, StandardLightingPattern);
    AddPattern(Gen, BedroomD, StandardLightingPattern);
    AddPattern(Gen, TailorRoom, StandardLightingPattern);
    AddPattern(Gen, Kitchen, StandardLightingPattern);
    AddPattern(Gen, Garden, StandardLightingPattern);
    AddPattern(Gen, ForestPath, StandardLightingPattern);
    AddPattern(Gen, ForestEntrance, StandardLightingPattern);
    
    AddOption(Gen, MainRoom, GenOption_Cat);
    //AddOption(Gen, MainRoom, GenOption_Orphan);
    AddOption(Gen, BedroomA, GenOption_Cat);
    AddOption(Gen, BedroomA, GenOption_Orphan);
    AddOption(Gen, BedroomB, GenOption_Cat);
    AddOption(Gen, BedroomB, GenOption_Orphan);
    AddOption(Gen, BedroomC, GenOption_Cat);
    AddOption(Gen, BedroomC, GenOption_Orphan);
    AddOption(Gen, BedroomD, GenOption_Cat);
    AddOption(Gen, BedroomD, GenOption_Orphan);
    AddOption(Gen, TailorRoom, GenOption_Cat);
    AddOption(Gen, TailorRoom, GenOption_Orphan);
    AddOption(Gen, Kitchen, GenOption_Cat);
    AddOption(Gen, Kitchen, GenOption_Orphan);
    AddOption(Gen, Garden, GenOption_Orphan);

    SetSize(Gen, MainRoomSpec, 13, 13);
    SetSize(Gen, TailorSpec, 8, 6);
    SetSize(Gen, KitchenSpec, 8, 6);
    SetSize(Gen, VerticalHallwaySpec, 5, 13);
    SetSize(Gen, BedroomSpec, 8, 6);
    SetSize(Gen, HorizontalHallwaySpec, 13, 5);
    SetSize(Gen, SaveSlotSpec, 5, 6);
    SetSize(Gen, GardenSpec, 13, 13);
    SetSize(Gen, BasicForestSpec, 13, 13);
#if 0
    SetSize(Gen, SideAlley);
#endif

    Connect(Gen, MainRoom, BoxIndex_North, ForestPath);
    Connect(Gen, MainRoom, BoxIndex_West, TailorRoom);
    Connect(Gen, MainRoom, BoxIndex_West, Kitchen);
    Connect(Gen, MainRoom, BoxIndex_South, FrontHall);

    Connect(Gen, FrontHall, BoxIndex_East, BedroomD);
    Connect(Gen, FrontHall, BoxIndex_East, BedroomB);
    Connect(Gen, FrontHall, BoxIndex_West, BedroomC);
    Connect(Gen, FrontHall, BoxIndex_West, BedroomA);
    Connect(Gen, FrontHall, BoxIndex_South, BackHall);

    Connect(Gen, BackHall, BoxIndex_South, HeroSaveSlotA);
    Connect(Gen, BackHall, BoxIndex_South, HeroSaveSlotB);
    Connect(Gen, BackHall, BoxIndex_South, HeroSaveSlotC);
    Connect(Gen, BackHall, BoxIndex_East, Garden);

#if 0
    Connect(Gen, Garden, BoxIndex_North, SideAlley);
    Connect(Gen, SideAlley, BoxIndex_North, ForestPath);
#endif
    Connect(Gen, ForestPath, BoxIndex_North, ForestEntrance);

    Result.ForestEntrance = ForestEntrance;
    Result.HeroBedroom = HeroSaveSlotA;

    return(Result);
}

internal gen_result
CreateWorld(world *World, game_assets *Assets)
{
    gen_result Result = {};

    world_generator *Gen = BeginWorldGen(World, Assets);

    Gen->Entropy = &World->GameEntropy;

    gen_orphanage Orphanage = CreateOrphanage(Gen);
    gen_dungeon Dungeon = CreateDungeon(Gen, 7);
    Connect(Gen, Orphanage.ForestEntrance, BoxIndex_Down, Dungeon.EntranceRoom);

    gen_room *StartRoom = Orphanage.HeroBedroom;

    PlaceNPC(Gen, Tag_Hannah);
    PlaceNPC(Gen, Tag_Fred);
    PlaceNPC(Gen, Tag_Molly);
    
    PlaceNPC(Gen, Tag_Baby);
    PlaceNPC(Gen, Tag_Brahm);
    PlaceNPC(Gen, Tag_Carla);
    PlaceNPC(Gen, Tag_Cassidy);
    PlaceNPC(Gen, Tag_Drew);
    PlaceNPC(Gen, Tag_Dylan);
    PlaceNPC(Gen, Tag_Giles);
    PlaceNPC(Gen, Tag_Kline);
    PlaceNPC(Gen, Tag_Laird);
    PlaceNPC(Gen, Tag_Lambert);
    PlaceNPC(Gen, Tag_Rhoda);
    PlaceNPC(Gen, Tag_Slade);
    PlaceNPC(Gen, Tag_Sunny);
    PlaceNPC(Gen, Tag_Viva);

    Layout(Gen, StartRoom);
    GenerateWorld(Gen);

    gen_volume HeroRoom = Orphanage.HeroBedroom->Vol;
    // HeroRoom = Orphanage.ForestEntrance->Vol;
    //HeroRoom = Dungeon.EntranceRoom->Vol;

    Result.InitialCameraP = ChunkPositionFromTilePosition(
        Gen,
        (HeroRoom.Min.x + HeroRoom.Max.x)/2,
        (HeroRoom.Min.y + HeroRoom.Max.y)/2,
        (HeroRoom.Min.z + HeroRoom.Max.z)/2);

    EndWorldGen(Gen);

    return(Result);
}

internal void
CreateWorld(game_mode_world *WorldMode, game_assets *Assets)
{
    // TODO(casey): If we _do_ want to go with transient-memory-bounds,
    // we _could_ just use that as the arena in the generator
    gen_result Generated = CreateWorld(WorldMode->World, Assets);
    WorldMode->Camera.P = WorldMode->Camera.SimulationCenter = Generated.InitialCameraP;

    WorldMode->StandardRoomDimension = V3(17.0f*1.4f,
                                          9.0f*1.4f,
                                          WorldMode->TypicalFloorHeight);
    WorldMode->StandardApronRadius = 2.0f*WorldMode->StandardRoomDimension;
}
