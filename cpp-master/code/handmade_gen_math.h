/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

typedef v3s gen_v3;

union gen_volume
{
    // NOTE(casey): Volumes _include_ their min _and_ their max.  They
    // are inclusive on both ends of the interval.
    struct
    {
        gen_v3 Min;
        gen_v3 Max;
    };
    s32 E[2][3];
};

internal b32x IsInArrayBounds(gen_v3 Bounds, gen_v3 P);
