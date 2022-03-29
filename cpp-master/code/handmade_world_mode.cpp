/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

internal void
AddPlayer(game_mode_world *WorldMode, sim_region *SimRegion, traversable_reference StandingOn,
          brain_id BrainID)
{
    v3 P = GetSimSpaceTraversable(SimRegion, StandingOn).P;

    entity *Body = AddEntity(SimRegion);
    entity *Head = AddEntity(SimRegion);
//    Head->CollisionVolume = MakeSimpleGroundedCollision(0.5f, 0.5f, 0.5f, 0.7f);

    entity *Glove = AddEntity(SimRegion);
    Glove->MovementMode = MovementMode_Floating;
    Glove->CollisionVolume = RectCenterDim(V3(0, 0, 0), V3(0.5f, 0.5f, 0.5f));
    Glove->Flags |= EntityFlag_SphereCollision;

    //InitHitPoints(Body, 3);

    // TODO(casey): We will probably need a creation-time system for
    // guaranteeing now overlapping occupation.
    Body->Occupying = StandingOn;

    Body->BrainSlot = BrainSlotFor(brain_hero, Body);
    Body->BrainID = BrainID;
    Head->BrainSlot = BrainSlotFor(brain_hero, Head);
    Head->BrainID = BrainID;
    Glove->BrainSlot = BrainSlotFor(brain_hero, Glove);
    Glove->BrainID = BrainID;

    if(WorldMode->Camera.FollowingEntityIndex.Value == 0)
    {
        WorldMode->Camera.FollowingEntityIndex = Body->ID;
    }

    entity_id Result = Head->ID;

    v4 Color = {1, 1, 1, 1};
    f32 HeroSizeC = 1.25f;

    asset_tag_hash HeroTags = GetTagHash(Asset_None, Tag_Hero);
    asset_tag_hash GloveTags = GetTagHash(Asset_None, Tag_Glove, Tag_Fingers);

    entity_visible_piece *BodyPiece = AddPiece(Body, GetTagHash(Asset_Body, HeroTags), HeroSizeC, V3(0, 0, 0), Color, PieceMove_AxesDeform);
    entity_visible_piece *HeadPiece = AddPiece(Head, GetTagHash(Asset_Head, HeroTags), HeroSizeC, V3(0, 0, 0.15f), Color);
    entity_visible_piece *HatPiece = AddPiece(Head, GetTagHash(Asset_Item, HeroTags), HeroSizeC, V3(0, 0, 0.25f), Color);
    entity_visible_piece *GlovePiece = AddPiece(Glove, GetTagHash(Asset_Hand, GloveTags), HeroSizeC, V3(0, 0, 0), Color);

    ConnectPieceToWorld(Body, BodyPiece, HHAAlign_Default);
//    ConnectPiece(Body, BodyPiece, HHAAlign_BaseOfNeck, HeadPiece, HHAAlign_Default);
    ConnectPieceToWorld(Head, HeadPiece, HHAAlign_Default);
    ConnectPiece(Head, HeadPiece, HHAAlign_TopOfHead, HatPiece, HHAAlign_Default);
    ConnectPieceToWorld(Glove, GlovePiece, HHAAlign_Default);

    Glove->P = P + V3(1, 0, 1);
    Head->P = P;
    Body->P = P;
    
    DIAGRAM_SetFilter(Glove->ID.Value);
}

internal void
PlayWorld(game_state *GameState)
{
    SetGameMode(GameState, GameMode_World);

    game_mode_world *WorldMode = PushStruct(&GameState->ModeArena, game_mode_world);
    WorldMode->UpdatingLighting = true;
    WorldMode->FogMin = 6.0f;
    WorldMode->FogSpan = 24.0f;
    WorldMode->AlphaMin = 2.25f;
    WorldMode->AlphaSpan = 0.75f; // 1.0f;
    WorldMode->TimeStepScale = 1.0f;

    WorldMode->ParticleCache = PushStruct(&GameState->ModeArena, particle_cache,
                                          AlignNoClear(16));
    InitParticleCache(WorldMode->ParticleCache, GameState->Assets);

    uint32 TilesPerWidth = 17;
    uint32 TilesPerHeight = 9;

    WorldMode->FundamentalUnit = 1.4f;
    WorldMode->EffectsEntropy = RandomSeed(1234);
    WorldMode->TypicalFloorHeight = 3.0f*WorldMode->FundamentalUnit; // 5.0f;

    // TODO(casey): Remove this!
    real32 PixelsToMeters = 1.0f / 42.0f;
    v3 WorldChunkDimInMeters = {17.0f*WorldMode->FundamentalUnit,
        9.0f*WorldMode->FundamentalUnit,
        WorldMode->TypicalFloorHeight};
    WorldMode->World = CreateWorld(WorldChunkDimInMeters, &GameState->ModeArena);
    GameState->WorldMode = WorldMode;

    CreateWorld(WorldMode, GameState->Assets);
}

internal void
CheckForJoiningPlayers(game_input *Input, game_state *GameState,
                       game_mode_world *WorldMode, sim_region *SimRegion)
{

    //
    // NOTE(casey): Look to see if any players are trying to join
    //

    if(Input)
    {
        for(u32 ControllerIndex = 0;
            ControllerIndex < ArrayCount(Input->Controllers);
            ++ControllerIndex)
        {
            game_controller_input *Controller = GetController(Input, ControllerIndex);
            controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
            if(ConHero->BrainID.Value == 0)
            {
                if(WasPressed(Controller->Start))
                {
                    *ConHero = {};
                    traversable_reference Traversable;
                    if(GetClosestTraversable(SimRegion, V3(0, 0, 0), &Traversable,
                                             TraversableSearch_Unoccupied))
                    {
                        ConHero->BrainID = {ReservedBrainID_FirstHero + ControllerIndex};
                        AddPlayer(WorldMode, SimRegion, Traversable, ConHero->BrainID);
                    }
                    else
                    {
                        // TODO(casey): GameUI that tells you there's no safe place...
                        // maybe keep trying on subsequent frames?
                    }
                }
            }
        }
    }
}

internal world_sim
BeginSim(game_assets *Assets, memory_arena *TempArena, world *World, world_position SimCenterP, rectangle3 SimBounds, rectangle3 ApronBounds, r32 dt)
{
    world_sim Result = {};

    // TODO(casey): How big do we actually want to expand here?
    // TODO(casey): Do we want to simulate upper floors, etc.?
    temporary_memory SimMemory = BeginTemporaryMemory(TempArena);

    sim_region *SimRegion = BeginWorldChange(Assets, TempArena, World, SimCenterP, SimBounds, ApronBounds, dt);

    Result.SimRegion = SimRegion;
    Result.SimMemory = SimMemory;

    return(Result);
}

internal void
Simulate(world_sim *WorldSim, f32 TypicalFloorHeight, random_series *GameEntropy, r32 dt,
         // NOTE(casey): Optional...
         v4 BackgroundColor, game_state *GameState,
         game_assets *Assets, game_input *Input, render_group *RenderGroup,
         particle_cache *ParticleCache, editable_hit_test *HitTest)
{
    sim_region *SimRegion = WorldSim->SimRegion;

    // NOTE(casey): Run all brains
    BEGIN_BLOCK("ExecuteBrains");
    for(u32 BrainIndex = 0;
        BrainIndex < SimRegion->BrainCount;
        ++BrainIndex)
    {
        brain *Brain = SimRegion->Brains + BrainIndex;
        MarkBrainActives(Brain);
    }

    for(u32 BrainIndex = 0;
        BrainIndex < SimRegion->BrainCount;
        ++BrainIndex)
    {
        brain *Brain = SimRegion->Brains + BrainIndex;
        if(Brain->Active)
        {
            ExecuteBrain(GameState, GameEntropy, Input, SimRegion, Brain, dt);
        }
    }
    END_BLOCK();

    UpdateAndRenderEntities(TypicalFloorHeight, SimRegion, dt,
                            RenderGroup, BackgroundColor, ParticleCache, Assets,
                            HitTest);
}

internal void
EndSim(world_sim *WorldSim)
{
    // TODO(casey): Make sure we hoist the camera update out to a place where the renderer
    // can know about the location of the camera at the end of the frame so there isn't
    // a frame of lag in camera updating compared to the hero.
    EndWorldChange(WorldSim->SimRegion);
    EndTemporaryMemory(WorldSim->SimMemory);
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoWorldSim)
{
    TIMED_FUNCTION();

    // TODO(casey): It is inefficient to reallocate every time - this should be
    // something that is passed in as a property of the worker thread.
    memory_arena Arena = {};

    world_sim_work *Work = (world_sim_work *)Data;

    // TODO(casey): This is probably much too heavyweight - lock only the world
    // data structures when we use them?
    world *World = Work->WorldMode->World;

    editable_hit_test NullHitTest = {};

    rectangle3 ApronBounds = AddRadiusTo(Work->SimBounds, Work->WorldMode->StandardApronRadius);
    
    world_sim WorldSim = BeginSim(Work->GameState->Assets, &Arena, World, Work->SimCenterP, Work->SimBounds, ApronBounds, Work->dt);
    Simulate(&WorldSim, Work->WorldMode->TypicalFloorHeight, &World->GameEntropy,
             Work->dt, V4(0, 0, 0, 0), Work->GameState, 0, 0, 0, 0, &NullHitTest);
    EndSim(&WorldSim);

    Clear(&Arena);
}

#if 0
internal void
DEBUGLighting(game_mode_world *WorldMode)
{
    DEBUG_B32(WorldMode->ShowLighting);
    DEBUG_B32(WorldMode->TestLighting.UpdateDebugLines);

    if(DEBUG_B32(WorldMode->TestLighting.Accumulating))
    {
        if(!WorldMode->TestLighting.Accumulating)
        {
            WorldMode->TestLighting.AccumulationCount = 0;
        }
    }

    DEBUG_U32(WorldMode->TestLighting.DebugBoxDrawDepth);
    DEBUG_B32(WorldMode->UpdatingLighting);

    if(DEBUG_BUTTON("Lighting Pattern"))
    {
        ++WorldMode->LightingPattern;
        GenerateLightingPattern(&WorldMode->TestLighting,
                                WorldMode->LightingPattern);
    }
}
#endif

internal b32
UpdateAndRenderWorld(game_state *GameState, game_mode_world *WorldMode,
                     game_input *Input, game_render_commands *RenderCommands,
                     editable_hit_test *HitTest)
{
    TIMED_FUNCTION();

    b32 Result = false;
    b32 ResetDebugCam = false;

    world *World = WorldMode->World;

    f32 FocalLength = 1.5f;

    WorldMode->CameraPitch = DEFAULT_CAMERA_PITCH;
    WorldMode->CameraOrbit = 0;

    v4 BackgroundColor = V4(0.15f, 0.15f, 0.15f, 0.0f);
    render_group RenderGroup_ = BeginRenderGroup(GameState->Assets, RenderCommands,
                                                 Render_HandleTransparency|
                                                 Render_ClearColor|
                                                 Render_ClearDepth,
                                                 BackgroundColor);
    render_group *RenderGroup = &RenderGroup_;

    f32 NearClipPlane = 3.0f;
    f32 FarClipPlane = 100.0f;
    v3 DebugLightP = WorldMode->DebugLightP;
    if(WorldMode->UseDebugCamera)
    {
        NearClipPlane = 0.2f;
        FarClipPlane = (1000.0f+2.0f*WorldMode->DebugCameraDolly);
    }

    m4x4 CameraO = ZRotation(WorldMode->CameraOrbit)*XRotation(WorldMode->CameraPitch);
    v3 DeltaFromSim = Subtract(World, &WorldMode->Camera.P, &WorldMode->Camera.SimulationCenter);
    v3 CameraOt = DeltaFromSim;
    v3 CameraX = GetColumn(CameraO, 0);
    v3 CameraY = GetColumn(CameraO, 1);
    v3 CameraZ = GetColumn(CameraO, 2);
    
    f32 ExpectedFocusZ = DeltaFromSim.z / CameraZ.z;
    f32 FocusMinZ = ExpectedFocusZ - 2.0f;
    f32 FocusMaxZ = ExpectedFocusZ + 2.0f;
    fog_params Fog =
    {
        {0, 0, -1},
        FocusMaxZ + (WorldMode->FogMin),
        FocusMaxZ + (WorldMode->FogMin + WorldMode->FogSpan),
    };

    alpha_clip_params AlphaClip =
    {
        FocusMinZ - (WorldMode->AlphaMin + WorldMode->AlphaSpan),
        FocusMinZ - (WorldMode->AlphaMin),
    };

    v2 MouseP = {};
    v2 dMouseP = {};
    if(Input)
    {
        MouseP = Input->ClipSpaceMouseP.xy;
        dMouseP = MouseP - WorldMode->LastMouseP;

        if(WorldMode->UseDebugCamera)
        {
            if(Input->AltDown && IsDown(Input->MouseButtons[PlatformMouseButton_Left]))
            {
                f32 RotationSpeed = 0.8f*Pi32;
                WorldMode->DebugCameraOrbit -= RotationSpeed*dMouseP.x;
                WorldMode->DebugCameraPitch += RotationSpeed*dMouseP.y;
            }
            else if(IsDown(Input->MouseButtons[PlatformMouseButton_Middle]))
            {
                if(Input->AltDown)
                {
                    f32 ZoomSpeed = 3.0f*WorldMode->DebugCameraDolly;
                    WorldMode->DebugCameraDolly -= ZoomSpeed*dMouseP.y;
                }
            }
        }

        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
        {
            WorldMode->UseDebugCamera = !WorldMode->UseDebugCamera;
            ResetDebugCam = WorldMode->UseDebugCamera;
        }

        WorldMode->LastMouseP = MouseP;
    }
    SET_DEBUG_MOUSE_P(MouseP);

    SetCameraTransform(RenderGroup, 0, FocalLength, CameraX, CameraY, CameraZ,
                       CameraOt, NearClipPlane, FarClipPlane, &Fog, &AlphaClip);
    if(WorldMode->UseDebugCamera)
    {
        if(ResetDebugCam)
        {
            WorldMode->DebugCameraDolly = Inner(CameraOt, CameraZ);
            v3 ToCam = WorldMode->DebugCameraDolly * CameraZ;
            WorldMode->DebugCameraPan = CameraOt - ToCam;
            v3 CamArm = CameraZ;
            v2 PlanarArm = CamArm.xy;

            WorldMode->DebugCameraOrbit = ATan2(PlanarArm.y, PlanarArm.x) + (0.5f*Pi32);
            WorldMode->DebugCameraPitch = ATan2(Length(PlanarArm), CameraZ.z);
        }
    }

    m4x4 DebugCameraO = ZRotation(WorldMode->DebugCameraOrbit)*XRotation(WorldMode->DebugCameraPitch);
    v3 ToCam = DebugCameraO*V3(0, 0, WorldMode->DebugCameraDolly);

    if(WorldMode->UseDebugCamera)
    {
        if(Input)
        {
            if(!Input->AltDown && IsDown(Input->MouseButtons[PlatformMouseButton_Middle]))
            {
                v3 DebugCameraX = GetColumn(DebugCameraO, 0);
                v3 DebugCameraY = GetColumn(DebugCameraO, 1);

                f32 PanSpeed = 0.5f*WorldMode->DebugCameraDolly;
                v3 dPan = dMouseP.x*DebugCameraX + dMouseP.y*DebugCameraY;
                WorldMode->DebugCameraPan -= PanSpeed*dPan;
            }
        }

        v3 DebugCameraOt = WorldMode->DebugCameraPan + ToCam;
        SetCameraTransform(RenderGroup, Camera_IsDebug, FocalLength, GetColumn(DebugCameraO, 0),
                           GetColumn(DebugCameraO, 1),
                           GetColumn(DebugCameraO, 2),
                           DebugCameraOt, NearClipPlane, FarClipPlane);
    }

    rectangle3 SimBounds = RectCenterDim(WorldMode->Camera.SimulationCenter.Offset_,
                                         4.0f*WorldMode->StandardRoomDimension);
    rectangle3 ApronBounds = AddRadiusTo(SimBounds, WorldMode->StandardApronRadius);
        
    rectangle3 WorldCameraRect = GetCameraRectangleAtTarget(RenderGroup, ExpectedFocusZ);

#if 0
    rectangle2 ScreenBounds = RectCenterDim(V2(0, 0), V2(WorldCameraRect.Max.x - WorldCameraRect.Min.x,
                                                         WorldCameraRect.Max.y - WorldCameraRect.Min.y));
    rectangle3 CameraBoundsInMeters = RectMinMax(V3(ScreenBounds.Min, 0.0f), V3(ScreenBounds.Max, 0.0f));
    CameraBoundsInMeters.Min.z = -3.0f*WorldMode->TypicalFloorHeight;
    CameraBoundsInMeters.Max.z =  1.0f*WorldMode->TypicalFloorHeight;

    SimBounds = Union(AddRadiusTo(WorldCameraRect, V3(5, 5, 0)), SimBounds);

    rectangle3 LightBounds = WorldCameraRect;
    LightBounds.Min.z = SimBounds.Min.z;
    LightBounds.Max.z = SimBounds.Max.z;
#if 0
    LightBounds = AddRadiusTo(LightBounds, V3(7.0f, 6.0f, 0.0f));
#endif
    f32 AdjustmentY = (GetDim(LightBounds).x - GetDim(LightBounds).y);
    LightBounds.Min.y -= 0.5f*AdjustmentY;
    LightBounds.Max.y += 0.5f*AdjustmentY;
#endif

#if 0
    PushVolumeOutline(RenderGroup, SimBounds, V4(1, 1, 0, 1));
    PushVolumeOutline(RenderGroup, WorldCameraRect, V4(1, 1, 0, 1));
#endif

    EnableLighting(RenderGroup, true);
    
    DIAGRAM_SetOrigin(World, WorldMode->Camera.SimulationCenter);
    
    world_position LightingCenter = MapIntoChunkSpace(World,
                                                      WorldMode->Camera.SimulationCenter,
                                                      GetCenter(WorldCameraRect));
    LightingCenter.ChunkZ = 0;
    LightingCenter.Offset_.z = 0;

    world_position LightingSimCenter = WorldMode->Camera.SimulationCenter;

#if 0
    world_sim_work SimWork[16];
    u32 SimIndex = 0;
    for(u32 SimY = 0;
        SimY < 4;
        ++SimY)
    {
        for(u32 SimX = 0;
            SimX < 4;
            ++SimX)
        {
            world_sim_work *Work = SimWork + SimIndex++;

            world_position CenterP = WorldMode->Camera.P;
            CenterP.ChunkX += -70*(SimX + 1);
            CenterP.ChunkY += -70*(SimY + 1);

            Work->SimCenterP = CenterP;
            Work->SimBounds = SimBounds;
            Work->WorldMode = WorldMode;
            Work->dt = Input->dtForFrame;
            Work->GameState = GameState;
#if 1
            // NOTE(casey): This is the multi-threaded path
            Platform.AddEntry(TranState->HighPriorityQueue, DoWorldSim, Work);
#else
            // NOTE(casey): This is the single-threaded path
            DoWorldSim(TranState->HighPriorityQueue, Work);
#endif
        }
    }

    Platform.CompleteAllWork(TranState->HighPriorityQueue);
#endif

    f32 dt = Input->dtForFrame*WorldMode->TimeStepScale;

    // NOTE(casey): Simulating the primary region
    world_sim WorldSim = BeginSim(GameState->Assets, GameState->FrameArena, World,
                                  WorldMode->Camera.SimulationCenter, SimBounds, ApronBounds,
                                  dt);
    {
        CheckForJoiningPlayers(Input, GameState, WorldMode, WorldSim.SimRegion);

        WorldSim.SimRegion->DEBUGPreventMovement = (WorldMode->SingleStep && !WorldMode->StepLatch);
        WorldMode->StepLatch = false;
        
        
        Simulate(&WorldSim, WorldMode->TypicalFloorHeight, &World->GameEntropy, dt, BackgroundColor, GameState,
                 GameState->Assets, Input, RenderGroup, WorldMode->ParticleCache, HitTest);

        // TODO(casey): Can we go ahead and merge the camera update down into the
        // simulation so that we correctly update the camera for the current frame?

        world_position LastP = WorldMode->Camera.SimulationCenter;
        entity *CameraEntity =
            GetEntityByID(WorldSim.SimRegion, WorldMode->Camera.FollowingEntityIndex);
        if(CameraEntity)
        {
            UpdateCameraForEntityMovement(&WorldMode->Camera, WorldSim.SimRegion, World, CameraEntity, dt, CameraZ);
            WorldMode->DebugLightP = CameraEntity->GroundP + V3(0.0f, 0, 2.0f);
        }

        PushLight(RenderGroup, WorldMode->DebugLightP, V3(0.5f, 0.5f, 0.5f), 10.0f*V3(1, 1, 1));

        v3 FrameToFrameCameraDeltaP = Subtract(World, &WorldMode->Camera.SimulationCenter, &LastP);
        UpdateAndRenderParticleSystems(WorldMode->ParticleCache, dt, RenderGroup,
                                       -FrameToFrameCameraDeltaP);

        world_position MinChunkP = WorldMode->Camera.P;
        MinChunkP.Offset_ = V3(0, 0, 0);
        world_position MaxChunkP = MinChunkP;
        MaxChunkP.ChunkX += 1;
        MaxChunkP.ChunkY += 1;
        MaxChunkP.ChunkZ += 1;
        rectangle3 ChunkBoundary = RectMinMax(MapIntoSimSpace(WorldSim.SimRegion, MinChunkP),
                                              MapIntoSimSpace(WorldSim.SimRegion, MaxChunkP));

#if 0
        //        PushVolumeOutline(RenderGroup, RectMinMax(V3(-1, -1, -1), V3(1, 1, 1)), V4(1.0f, 1.0f, 0.0f, 1), 0.01f);
        PushVolumeOutline(RenderGroup, ChunkBoundary, V4(1, 0.5f, 0.0f, 1));
        PushRectOutline(RenderGroup, V3(0.0f, 0.0f, 0.005f), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1));
        //    PushRectOutline(RenderGroup, V3(0.0f, 0.0f, 0.0f), GetDim(CameraBoundsInMeters).xy, V4(1.0f, 1.0f, 1.0f, 1));
        PushRectOutline(RenderGroup, V3(0.0f, 0.0f, 0.005f), GetDim(LightBounds).xy, V4(0.0f, 1.0f, 1.0f, 1));
        PushRectOutline(RenderGroup, V3(0.0f, 0.0f, 0.005f), GetDim(WorldSim.SimRegion->Bounds).xy, V4(1.0f, 0.0f, 1.0f, 1));
#endif
        PushVolumeOutline(RenderGroup, WorldCameraRect, V4(1, 1, 1, 1));
        
        PushVolumeOutline(RenderGroup, RectMinMax(V3(-1, -1, -1), V3(1, 1, 1)), V4(1, 1, 0, 1));
        
#if 0
        for(u32 RoomIndex = 0;
            RoomIndex < World->RoomCount;
            ++RoomIndex)
        {
            world_room *Room = World->Rooms + RoomIndex;
            PushVolumeOutline(RenderGroup, &WorldTransform,
                              RectMinMax(MapIntoSimSpace(WorldSim.SimRegion, Room->MinPos),
                                         MapIntoSimSpace(WorldSim.SimRegion, Room->MaxPos)),
                              V4(1.0f, 1.0f, 0.0f, 1), 0.01f);

        }
#endif
    }

    WorldMode->TestLighting.DebugLightP = WorldMode->DebugLightP;

    game_render_commands *Commands = RenderGroup->Commands;
    if(Input->FKeyPressed[9] || 1)
    {
        UpdateLighting(&WorldMode->TestLighting, WorldMode->FundamentalUnit,
                       &Commands->SpecularLightAtlas, &Commands->DiffuseLightAtlas, World, LightingSimCenter,
                       LightingCenter, SafeTruncateToU16(Commands->LightOccluderCount),
                       Commands->LightOccluders,
                       GameState->HighPriorityQueue, GameState->FrameArena);
    }
    PushLightingRenderValues(&WorldMode->TestLighting, RenderGroup);

    RenderDiagrams(RenderGroup);
    EndRenderGroup(RenderGroup);

    EndSim(&WorldSim);

    EnableLighting(RenderGroup, false);

    b32 HeroesExist = false;
    for(u32 ConHeroIndex = 0;
        ConHeroIndex < ArrayCount(GameState->ControlledHeroes);
        ++ConHeroIndex)
    {
        if(GameState->ControlledHeroes[ConHeroIndex].BrainID.Value)
        {
            HeroesExist = true;
            break;
        }
    }
    if(!HeroesExist)
    {
        PlayTitleScreen(GameState);
    }

    return(Result);
}
