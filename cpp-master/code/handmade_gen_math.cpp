/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */


internal gen_v3
GenV3(s32 X, s32 Y, s32 Z)
{
    gen_v3 Result = {X, Y, Z};
    return(Result);
}

internal gen_v3
GetDirection(box_surface_index Dir)
{
    gen_v3 Result = {};
    box_surface_params Params = GetBoxSurfaceParams(Dir);
    Result.E[Params.AxisIndex] = (Params.Positive ? 1 : -1);
    return(Result);
}

internal gen_volume
InfinityVolume(void)
{
    gen_volume Result;
    
    Result.Min.x = Result.Min.y = Result.Min.z = S32Min/4;
    Result.Max.x = Result.Max.y = Result.Max.z = S32Max/4;
    
    return(Result);
}

internal gen_volume
InvertedInfinityVolume(void)
{
    gen_volume Result;
    
    Result.Min.x = Result.Min.y = Result.Min.z = S32Max/4;
    Result.Max.x = Result.Max.y = Result.Max.z = S32Min/4;
    
    return(Result);
}

internal b32x
IsInVolume(gen_volume *Vol, s32 X, s32 Y, s32 Z)
{
    b32x Result = ((X >= Vol->Min.x) && (X <= Vol->Max.x) &&
                   (Y >= Vol->Min.y) && (Y <= Vol->Max.y) &&
                   (Z >= Vol->Min.z) && (Z <= Vol->Max.z));
    
    return(Result);
}

internal b32x
IsInVolume(gen_volume *Vol, gen_v3 P)
{
    b32x Result = IsInVolume(Vol, P.x, P.y, P.z);
    return(Result);
}

internal b32x
IsInArrayBounds(gen_v3 Bounds, gen_v3 P)
{
    b32x Result = ((P.x >= 0) && (P.x < Bounds.x) &&
                   (P.y >= 0) && (P.y < Bounds.y) &&
                   (P.z >= 0) && (P.z < Bounds.z));
    
    return(Result);
}

internal gen_v3
GetDim(gen_volume Vol)
{
    gen_v3 Result =
    {
        (Vol.Max.x - Vol.Min.x) + 1,
        (Vol.Max.y - Vol.Min.y) + 1,
        (Vol.Max.z - Vol.Min.z) + 1,
    };
    
    return(Result);
}

internal b32x
IsMinimumDimensionsForRoom(gen_volume Vol)
{
    gen_v3 Dim = GetDim(Vol);
    b32x Result = ((Dim.x >= 4) &&
                   (Dim.y >= 4) &&
                   (Dim.z >= 1));
    
    return(Result);
}

internal b32x
HasVolume(gen_volume Vol)
{
    gen_v3 Dim = GetDim(Vol);
    b32x Result = ((Dim.x > 0) &&
                   (Dim.y > 0) &&
                   (Dim.z > 0));
    
    return(Result);
}

internal s32
GetTotalVolume(gen_v3 Dim)
{
    s32 Result = (Dim.x*Dim.y*Dim.z);
    return(Result);
}

internal gen_volume
GetMaximumVolumeFor(gen_volume MinVol, gen_volume MaxVol)
{
    gen_volume Result;
    
    Result.Min.x = MinVol.Min.x;
    Result.Min.y = MinVol.Min.y;
    Result.Min.z = MinVol.Min.z;
    
    Result.Max.x = MaxVol.Max.x;
    Result.Max.y = MaxVol.Max.y;
    Result.Max.z = MaxVol.Max.z;
    
    return(Result);
}

internal void
ClipMin(gen_volume *Vol, u32 Dim, s32 Val)
{
    if(Vol->Min.E[Dim] < Val)
    {
        Vol->Min.E[Dim] = Val;
    }
}

internal void
ClipMax(gen_volume *Vol, u32 Dim, s32 Val)
{
    if(Vol->Max.E[Dim] > Val)
    {
        Vol->Max.E[Dim] = Val;
    }
}

internal gen_volume
Union(gen_volume *A, gen_volume *B)
{
    gen_volume Result;
    
    for(u32 Dim = 0;
        Dim < 3;
        ++Dim)
    {
        Result.Min.E[Dim] = Minimum(A->Min.E[Dim], B->Min.E[Dim]);
        Result.Max.E[Dim] = Maximum(A->Max.E[Dim], B->Max.E[Dim]);
    }
    
    return(Result);
}

internal gen_volume
Intersect(gen_volume *A, gen_volume *B)
{
    gen_volume Result;
    
    for(u32 Dim = 0;
        Dim < 3;
        ++Dim)
    {
        Result.Min.E[Dim] = Maximum(A->Min.E[Dim], B->Min.E[Dim]);
        Result.Max.E[Dim] = Minimum(A->Max.E[Dim], B->Max.E[Dim]);
    }
    
    return(Result);
}

internal gen_volume
AddRadiusTo(gen_volume *A, gen_v3 Radius)
{
    gen_volume Result = *A;
    
    Result.Min.E[0] -= Radius.E[0];
    Result.Min.E[1] -= Radius.E[1];
    Result.Min.E[2] -= Radius.E[2];
    
    Result.Max.E[0] += Radius.E[0];
    Result.Max.E[1] += Radius.E[1];
    Result.Max.E[2] += Radius.E[2];
    
    return(Result);
}