/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

struct debug_table;
#define DEBUG_GAME_FRAME_END(name) void name(game_memory *Memory, game_input *Input, game_render_commands *RenderCommands)
typedef DEBUG_GAME_FRAME_END(debug_game_frame_end);

#if HANDMADE_INTERNAL
enum debug_type
{
    DebugType_Unknown,
    
    DebugType_Name,
    
    DebugType_FrameMarker,
    DebugType_BeginBlock,
    DebugType_EndBlock,
    
    DebugType_OpenDataBlock,
    DebugType_CloseDataBlock,
    DebugType_SetHUD,
    
    DebugType_ArenaSetName,
    DebugType_ArenaBlockFree,
    DebugType_ArenaBlockTruncate,
    DebugType_ArenaBlockAllocate,
    DebugType_ArenaAllocate,
    
    //    DebugType_MarkDebugValue,
    
    DebugType_string_ptr,
    DebugType_b32,
    DebugType_r32,
    DebugType_u32,
    DebugType_umm,
    DebugType_s32,
    DebugType_v2,
    DebugType_v3,
    DebugType_v4,
    DebugType_rectangle2,
    DebugType_rectangle3,
    DebugType_bitmap_id,
    DebugType_sound_id,
    DebugType_font_id,
    
    DebugType_ThreadIntervalGraph,
    DebugType_FrameBarGraph,
    // DebugType_CounterFunctionList,
    DebugType_LastFrameInfo,
    DebugType_FrameSlider,
    DebugType_TopClocksList,
    DebugType_FunctionSummary,
    DebugType_MemoryByArena,
    DebugType_MemoryByFrame,
    DebugType_MemoryBySize,
};

struct debug_memory_block_op
{
    platform_memory_block *ArenaLookupBlock;
    platform_memory_block *Block;
    umm AllocatedSize;
};

struct debug_memory_op
{
    platform_memory_block *Block;
    umm AllocatedSize;
    umm OffsetInBlock;
};

struct debug_event
{
    u64 Clock;
    char *GUID;
    u16 ThreadID;
    u16 CoreIndex;
    u8 Type;
    char *Name;
    
    union
    {
        dev_id DebugID;
        debug_event *Value_debug_event;
        
        char *Value_string_ptr;
        b32 Value_b32;
        s32 Value_s32;
        u32 Value_u32;
        umm Value_umm;
        r32 Value_r32;
        v2 Value_v2;
        v3 Value_v3;
        v4 Value_v4;
        rectangle2 Value_rectangle2;
        rectangle3 Value_rectangle3;
        bitmap_id Value_bitmap_id;
        sound_id Value_sound_id;
        font_id Value_font_id;
        debug_memory_op Value_debug_memory_op;
        debug_memory_block_op Value_debug_memory_block_op;
    };
};

struct debug_table
{
    debug_event EditEvent;
    v2 MouseP;
    u32 RecordIncrement;
    
    // TODO(casey): No attempt is currently made to ensure that the final
    // debug records being written to the event array actually complete
    // their output prior to the swap of the event array index.
    u32 CurrentEventArrayIndex;
    // TODO(casey): This could actually be a u32 atomic now, since we
    // only need 1 bit to store which array we're using...
    u64 volatile EventArrayIndex_EventIndex;
    debug_event Events[2][16*65536];
};

extern debug_table *GlobalDebugTable;

#define DEBUG_NAME__(A, B, C) A "|" #B "|" #C
#define DEBUG_NAME_(A, B, C) DEBUG_NAME__(A, B, C)
#define DEBUG_NAME(Name) DEBUG_NAME_(__FILE__, __LINE__, __COUNTER__)

#define DEBUGSetEventRecording(Enabled) (GlobalDebugTable->RecordIncrement = (Enabled) ? 1 : 0)

// NOTE(casey): Name is recorded separately now to work around the fact that
// GCC/Clang don't define __FUNCTION__ as a constant string so it can't be
// compile-time welded along with the File/Line/Counter.
#define RecordDebugEvent(EventType, GUIDInit, NameInit)           \
u64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, GlobalDebugTable->RecordIncrement); \
u32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;            \
Assert(EventIndex < ArrayCount(GlobalDebugTable->Events[0]));   \
debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32] + EventIndex; \
Event->Clock = __rdtsc();                       \
Event->Type = (u8)EventType;                                    \
Event->CoreIndex = 0;                                           \
Event->ThreadID = (u16)GetThreadID();                         \
Event->GUID = GUIDInit;                                       \
Event->Name = NameInit

#define FRAME_MARKER(SecondsElapsedInit) \
{RecordDebugEvent(DebugType_FrameMarker, DEBUG_NAME("Frame Marker"), "Frame Marker"); \
    Event->Value_r32 = SecondsElapsedInit;}

#if HANDMADE_SLOW

#define TIMED_BLOCK__(GUID, Name) timed_block TimedBlock_##Number(GUID, Name)
#define TIMED_BLOCK_(GUID, Name) TIMED_BLOCK__(GUID, Name)
#define TIMED_BLOCK(Name) TIMED_BLOCK_(DEBUG_NAME(Name), Name)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(DEBUG_NAME(__FUNCTION__), (char *)__FUNCTION__)
#define HUD_TIMED_FUNCTION_(Name, ...) GlobalDebugTable->HUDFunction = Name; TIMED_BLOCK_(Name, (char *)__FUNCTION__)
#define HUD_TIMED_FUNCTION(...) HUD_TIMED_FUNCTION_(DEBUG_NAME(__FUNCTION__), ## __VA_ARGS__)

#define BEGIN_BLOCK_(GUID, Name) {RecordDebugEvent(DebugType_BeginBlock, GUID, Name);}
#define END_BLOCK_(GUID, Name) {RecordDebugEvent(DebugType_EndBlock, GUID, Name);}

#define BEGIN_BLOCK(Name) BEGIN_BLOCK_(DEBUG_NAME(Name), Name)
#define END_BLOCK() END_BLOCK_(DEBUG_NAME("END_BLOCK_"), "END_BLOCK_")

struct timed_block
{
    timed_block(char *GUID, char *Name)
    {
        BEGIN_BLOCK_(GUID, Name);
    }
    
    ~timed_block()
    {
        END_BLOCK();
    }
};

#else

#define TIMED_BLOCK(...)
#define TIMED_FUNCTION(...)
#define BEGIN_BLOCK(...)
#define END_BLOCK(...)
#define HUD_TIMED_FUNCTION(...)

#endif

#else

#define DEBUGSetEventRecording(...)
#define FRAME_MARKER(...)

#endif

#ifdef __cplusplus
}
#endif


#if defined(__cplusplus) && HANDMADE_INTERNAL

extern debug_event *DEBUGGlobalEditEvent;

#define DEBUGValueSetEventData_(type) \
inline void \
DEBUGValueSetEventData(debug_event *Event, type Ignored, void *Value) \
{ \
Event->Type = DebugType_##type; \
if(GlobalDebugTable->EditEvent.GUID == Event->GUID) \
{ \
    *(type *)Value = GlobalDebugTable->EditEvent.Value_##type; \
} \
\
Event->Value_##type = *(type *)Value; \
}

DEBUGValueSetEventData_(r32);
DEBUGValueSetEventData_(u32);
DEBUGValueSetEventData_(umm);
DEBUGValueSetEventData_(s32);
DEBUGValueSetEventData_(v2);
DEBUGValueSetEventData_(v3);
DEBUGValueSetEventData_(v4);
DEBUGValueSetEventData_(rectangle2);
DEBUGValueSetEventData_(rectangle3);
DEBUGValueSetEventData_(bitmap_id);
DEBUGValueSetEventData_(sound_id);
DEBUGValueSetEventData_(font_id);

#define DEBUGValueSetEventData2_(as_type,type) \
inline void \
DEBUGValueSetEventData(debug_event *Event, type Ignored, void *Value) \
{ \
Event->Type = DebugType_##as_type; \
if(GlobalDebugTable->EditEvent.GUID == Event->GUID) \
{ \
    *(type *)Value = (type)GlobalDebugTable->EditEvent.Value_##as_type; \
} \
\
Event->Value_##as_type = *(type *)Value; \
}
DEBUGValueSetEventData2_(u32,u16);

struct debug_data_block
{
debug_data_block(char *GUID, char *Name)
{
    RecordDebugEvent(DebugType_OpenDataBlock, GUID, Name);
    //Event->DebugID = ID;
}

~debug_data_block(void)
{
    RecordDebugEvent(DebugType_CloseDataBlock, DEBUG_NAME("End Data Block"), "End Data Block");
}
};

#define DEBUG_DATA_BLOCK(Name) debug_data_block DataBlock__(DEBUG_NAME(Name), Name)
#define DEBUG_BEGIN_DATA_BLOCK(Name) RecordDebugEvent(DebugType_OpenDataBlock, DEBUG_NAME(Name), Name)
#define DEBUG_END_DATA_BLOCK(Name) RecordDebugEvent(DebugType_CloseDataBlock, DEBUG_NAME("End Data Block"), "End Data Block")

internal void DEBUGEditEventData(char *GUID, debug_event *Event);

#define GET_DEBUG_MOUSE_P() GlobalDebugTable->MouseP
#define SET_DEBUG_MOUSE_P(P) GlobalDebugTable->MouseP = (P)

#define DEBUG_VALUE(Value)  \
{ \
RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value), #Value);                              \
DEBUGValueSetEventData(Event, Value, (void *)&(Value)); \
}

#define DEBUG_B32(Value)  \
{ \
RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value), #Value);                              \
DEBUGValueSetEventData(Event, (s32)0, (void *)&Value); \
Event->Type = DebugType_b32; \
}

#define DEBUG_STRING(Value)  \
{ \
RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value), #Value);                              \
Event->Value_string_ptr = Value; \
Event->Type = DebugType_string_ptr; \
}

#define DEBUG_UI_ELEMENT(Type, Name) \
{ \
RecordDebugEvent(Type, #Name, #Name);                   \
}

#define DEBUG_UI_HUD(Value)  \
{ \
RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value), #Value);                              \
Event->Value_u32 = Value; \
Event->Type = DebugType_SetHUD; \
}

#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)

inline dev_id DEBUG_POINTER_ID(void *Pointer)
{
dev_id ID = {Pointer};

return(ID);
}

#define DEBUG_UI_ENABLED 1

internal void DEBUG_HIT(dev_id ID, r32 ZValue);
internal b32 DEBUG_HIGHLIGHTED(dev_id ID, v4 *Color);
internal b32 DEBUG_REQUESTED(dev_id ID);

#define DEBUG_ARENA_NAME(Ar, Name) \
{ \
RecordDebugEvent(DebugType_ArenaSetName, DEBUG_NAME("ArenaSetName"), Name); \
Event->Value_debug_memory_op.Block = (Ar)->CurrentBlock; \
Event->Value_debug_memory_op.AllocatedSize = 0; \
}

#define DEBUG_ARENA_SUPPRESS(Ar, Name) \
{ \
RecordDebugEvent(DebugType_ArenaSetName, DEBUG_NAME("ArenaSuppress"), Name); \
Event->Value_debug_memory_op.Block = (Ar)->CurrentBlock; \
Event->Value_debug_memory_op.AllocatedSize = 1; \
}

#define DEBUG_RECORD_BLOCK_FREE(Bl) \
{ \
RecordDebugEvent(DebugType_ArenaBlockFree, DEBUG_NAME("ArenaBlockFree"), "BlockFree"); \
Event->Value_debug_memory_op.Block = (Bl); \
}

#define DEBUG_RECORD_BLOCK_TRUNCATE(Bl) \
{ \
RecordDebugEvent(DebugType_ArenaBlockTruncate, DEBUG_NAME("ArenaBlockTruncate"), "Truncation"); \
Event->Value_debug_memory_op.Block = (Bl); \
Event->Value_debug_memory_op.AllocatedSize = (Bl)->Used; \
}

#define DEBUG_RECORD_BLOCK_ALLOCATION(Bl) \
{ \
RecordDebugEvent(DebugType_ArenaBlockAllocate, DEBUG_NAME("ArenaBlockAllocate"), "BlockAlloc"); \
Event->Value_debug_memory_block_op.ArenaLookupBlock = (Bl)->ArenaPrev; \
Event->Value_debug_memory_block_op.Block = (Bl); \
Event->Value_debug_memory_block_op.AllocatedSize = (Bl)->Size; \
}

#define DEBUG_RECORD_ALLOCATION(Bl, GUID, ASize, USize, BlockOffset) \
{ \
RecordDebugEvent(DebugType_ArenaAllocate, GUID, "Allocation"); \
Event->Value_debug_memory_op.Block = (Bl); \
Event->Value_debug_memory_op.AllocatedSize = (ASize); \
Event->Value_debug_memory_op.OffsetInBlock = (BlockOffset); \
}

#else

inline dev_id DEBUG_POINTER_ID(void *Pointer) {dev_id NullID = {}; return(NullID);}

#define GET_DEBUG_MOUSE_P(...)
#define SET_DEBUG_MOUSE_P(...)

#define DEBUG_DATA_BLOCK(...)
#define DEBUG_VALUE(...)
#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)
#define DEBUG_UI_ENABLED 0
#define DEBUG_HIT(...)
#define DEBUG_HIGHLIGHTED(...) 0
#define DEBUG_REQUESTED(...) 0
#define DEBUG_BEGIN_DATA_BLOCK(...)
#define DEBUG_END_DATA_BLOCK(...)
#define DEBUG_B32(...)
#define DEBUG_STRING(...)
#define DEBUG_UI_HUD(...)
#define DEBUG_UI_ELEMENT(...)

#define DEBUG_ARENA_NAME(...)
#define DEBUG_RECORD_BLOCK_ALLOCATION(...)
#define DEBUG_RECORD_BLOCK_FREE(...)
#define DEBUG_RECORD_BLOCK_TRUNCATE(...)
#define DEBUG_RECORD_ALLOCATION(...)
#define DEBUG_ARENA_SUPPRESS(...)

#define TIMED_BLOCK(...)
#define TIMED_FUNCTION(...)
#define BEGIN_BLOCK(...)
#define END_BLOCK(...)
#define HUD_TIMED_FUNCTION(...)

#endif
