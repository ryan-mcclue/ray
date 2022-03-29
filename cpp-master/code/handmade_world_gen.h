/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

struct gen_apron_spec;
struct gen_apron;
struct gen_room_spec;
struct gen_room;
struct gen_room_connection;
struct gen_connection;
struct gen_option;

enum gen_option_type
{
    GenOption_None, 
    
    GenOption_Cat,
    GenOption_Orphan,
    
    GenOption_Count,
};

struct gen_option_iterator
{
    gen_room *Room;
};

struct gen_option_array
{
    gen_option *Options;
    
    u32 MaxOptionCount;
    u32 OptionCount;
};

struct gen_option
{
    gen_room *Room;
};

struct world_generator
{
    memory_arena Memory;
    memory_arena TempMemory;
    
    world *World;
    v3 TileDim;
    
    gen_apron *FirstApron;
    gen_room *FirstRoom;
    gen_connection *FirstConnection;
    
    gen_option_array OptionArrays[GenOption_Count];
    
    random_series *Entropy;
    game_assets *Assets;
};

struct gen_apron_spec
{
    // TODO(casey): Fill this in later
    b32 Trees;
};

struct gen_room_spec
{
    // TODO(casey): Fill this in later
    gen_v3 RequiredDim;
    b32 StoneFloor;
    b32 Outdoors;
    
    gen_apron_spec *Apron;
};

struct gen_apron
{
    gen_apron *GlobalNext;
    gen_apron_spec *Spec;
    gen_volume Vol;
};

struct gen_room
{
    gen_room_connection *FirstConnection;
    gen_room *GlobalNext;
    
    gen_room_spec *Spec;
    u64 OptionsPicked;
    
    gen_volume Vol;
    u32 GenerationIndex;
    
    gen_entity_pattern *FirstPattern;
    
#if HANDMADE_INTERNAL
    char *DebugLabel;
#endif
};

struct gen_room_connection
{
    gen_connection *Connection;
    gen_room_connection *Next;
    
    box_surface_index PlacedDirection;
    
    brain_id DoorBrainID;
};

struct gen_connection
{
    u32 DirMaskFromA; // NOTE(casey): Masks the connection direction _relative to room A_
    
    gen_room *A;
    gen_room *B;
    
    gen_room_connection *AToB;
    gen_room_connection *BToA;
    
    gen_connection *GlobalNext;
    
    gen_volume Vol;
};

struct gen_dungeon
{
    gen_room *EntranceRoom;
    gen_room *ExitRoom;
};

struct gen_forest
{
    gen_room *Exits[4];
};

struct gen_orphanage
{
    gen_room *ForestEntrance;
    gen_room *HeroBedroom;
};

struct gen_result
{
    world_position InitialCameraP;
};

struct gen_room_stack_entry
{
    gen_room *Room;
    gen_room_stack_entry *Prev;
};

struct gen_room_stack
{
    memory_arena *Memory;
    
    gen_room_stack_entry *FirstFree;
    gen_room_stack_entry *Top;
};

internal void CreateWorld(game_mode_world *WorldMode, game_assets *Assets);
