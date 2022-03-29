/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#if !defined(RECORD_LEAF_BOX)
#define RECORD_LEAF_BOX(...)
#define RECORD_PARTITION_BOX(...)
#define RECORD_RAYCAST_END(...)
#define RECORD_PARTITION_PUSH(...)
#endif

global b32x LightBoxDumpTrigger = false;
global b32x LightDisableThreading = false;

internal void
PushDebugLine(lighting_solution *Solution, v3 FromP, v3 ToP, v4 Color)
{
    if(Solution->UpdateDebugLines)
    {
        Assert(Solution->DebugLineCount < Solution->MaxDebugLineCount);
        debug_line *Line = Solution->DebugLines + Solution->DebugLineCount++;
        Line->FromP = FromP;
        Line->ToP = ToP;
        Line->Color = Color;
    }
}

internal void
PushDebugBox(lighting_solution *Solution, v3 BoxMin, v3 BoxMax, v4 BoxColor)
{
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMin.y, BoxMin.z),
                  V3(BoxMin.x, BoxMax.y, BoxMin.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMax.y, BoxMin.z),
                  V3(BoxMin.x, BoxMax.y, BoxMax.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMax.y, BoxMax.z),
                  V3(BoxMin.x, BoxMin.y, BoxMax.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMin.y, BoxMax.z),
                  V3(BoxMin.x, BoxMin.y, BoxMin.z),
                  BoxColor);

    PushDebugLine(Solution,
                  V3(BoxMax.x, BoxMin.y, BoxMin.z),
                  V3(BoxMax.x, BoxMax.y, BoxMin.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMax.x, BoxMax.y, BoxMin.z),
                  V3(BoxMax.x, BoxMax.y, BoxMax.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMax.x, BoxMax.y, BoxMax.z),
                  V3(BoxMax.x, BoxMin.y, BoxMax.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMax.x, BoxMin.y, BoxMax.z),
                  V3(BoxMax.x, BoxMin.y, BoxMin.z),
                  BoxColor);

    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMin.y, BoxMin.z),
                  V3(BoxMax.x, BoxMin.y, BoxMin.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMax.y, BoxMin.z),
                  V3(BoxMax.x, BoxMax.y, BoxMin.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMax.y, BoxMax.z),
                  V3(BoxMax.x, BoxMax.y, BoxMax.z),
                  BoxColor);
    PushDebugLine(Solution,
                  V3(BoxMin.x, BoxMin.y, BoxMax.z),
                  V3(BoxMax.x, BoxMin.y, BoxMax.z),
                  BoxColor);
}

internal void
PushDebugBox(lighting_solution *Solution, rectangle3 Box, v4 BoxColor)
{
    PushDebugBox(Solution, Box.Min, Box.Max, BoxColor);
}

inline void
GridRayCast(lighting_work *Work, u32 Sy,
            v3 RayOriginSingle,
            light_atlas *SpecAtlas,
            light_atlas *DiffuseAtlas,
            light_atlas_texel SpecTexel,
            u16 InitialGridIndex,
            u8 QuadrantIndex,
#if GRID_RAY_CAST_DEBUGGING
            debug_ray_pick DebugPick = {}
#endif
            )
{
    lighting_solution *Solution = Work->Solution;

#if GRID_RAY_CAST_DEBUGGING
    if(DebugPick.Enabled)
    {
        rectangle3 VoxBox = GetCellBounds(&Solution->SpatialGrid,
                                          DimIndexFrom(&Solution->SpatialGrid, InitialGridIndex));
        PushDebugBox(Solution, AddRadiusTo(VoxBox, 0.25f*V3(1, 1, 1)), V4(0, 1, 1, 1));

        {DEBUG_DATA_BLOCK("Lighting");
            DEBUG_VALUE(SpecTexel.Value[0]);
            DEBUG_VALUE(SpecTexel.Value[1]);
            DEBUG_VALUE(SpecTexel.Value[2]);
            DEBUG_VALUE(SpecTexel.Value[3]);
        }
    }
#endif

    alignas(16) u8 ShuffleTable[4][16] =
    {
        { 0,  1,  2,  3,   0,  1,  2,  3,   0,  1,  2,  3,   0,  1,  2,  3},
        { 4,  5,  6,  7,   4,  5,  6,  7,   4,  5,  6,  7,   4,  5,  6,  7},
        { 8,  9, 10, 11,   8,  9, 10, 11,   8,  9, 10, 11,   8,  9, 10, 11},
        {12, 13, 14, 15,  12, 13, 14, 15,  12, 13, 14, 15,  12, 13, 14, 15},
    };

    alignas(16) f32 NormalTable_[] =
    {
        // X, Y, Z, -,
        0, 0, 1, 0,
        1, 0, 0, 0,
        0, 1, 0, 0,
        1, 0, 0, 0,
    };
    f32_4x *NormalTable = (f32_4x *)NormalTable_;

    f32_4x One = F32_4x(1.0f);
    f32_4x Zero = ZeroF32_4x();

    v3 CellDim = Solution->SpatialGrid.CellDim;
    f32_4x CellDim4 = F32_4x(CellDim.x, CellDim.y, CellDim.z, 0);

    f32 RayWeight = (1.0f / LIGHT_SAMPLING_OCTAHEDRON_COUNT);
    v3 TransferPPS[8] = {};
#if 1
    for(u32 OctahedronIndex = 0; OctahedronIndex < LIGHT_SAMPLING_OCTAHEDRON_COUNT; ++OctahedronIndex)
    {
        u32 DirOffset = OctahedronIndex*LIGHT_SAMPLE_DIRECTIONS_PER_OCTAHEDRON +
            Sy*LIGHTING_OCTAHEDRAL_MAP_DIM;
        light_sample_direction *SampleDir = (Solution->SampleDirectionTable + DirOffset);

        for(u32 RayIndex = 0;
            RayIndex < 8;
            ++RayIndex)
        {
#else
            for(u32 TotalRayIndex = 0; TotalRayIndex < TotalRayCount; ++TotalRayIndex)
            {
                u32 RayIndex = TotalRayIndex & 0x7;

#endif
            v3 RayDSingle = SampleDir[RayIndex].RayD;

            b32 Debugging = false;
            if(DebugPick.Enabled &&
                             (OctahedronIndex == DebugPick.OctahedronIndex) &&
                             (RayIndex == DebugPick.Tx))
            {
                Debugging = true;
            }

            walk_table_entry *WalkTable = Solution->LightSamplingWalkTable[QuadrantIndex] + SampleDir[RayIndex].WalkTableOffset;

            f32_4x tRay = F32_4x(F32Max);

            v3_4x RayD = V3_4x(RayDSingle);
            v3_4x RayOrigin = V3_4x(RayOriginSingle);

            v3_4x InvRayD = V3_4x(1.0f, 1.0f, 1.0f, 1.0f) / RayD;

            f32_4x HitTMinX = ZeroF32_4x();
            f32_4x HitTMinY = ZeroF32_4x();
            v3_4x HitRefColor = V3_4x(1.0f, 1.0f, 1.0f, 1.0f);
            f32_4x HitEmission = ZeroF32_4x();

            __m128i Shuffler = *(__m128i *)ShuffleTable[0];

            f32_4x RayDPacked = F32_4x(RayDSingle.x, RayDSingle.y, RayDSingle.z, 0);
            f32_4x SignRayD = InverseSignBitFrom(RayDPacked);
            f32_4x RayOriginPacked = F32_4x(RayOriginSingle.x, RayOriginSingle.y, RayOriginSingle.z, 0);
            f32_4x ProbeSampleNSingle = RayDPacked;
            f32_4x ProbeSamplePSingle = RayOriginPacked;
            f32_4x OcclusionD = -RayDPacked;

            f32_4x tTerminateVerify = ZeroF32_4x();
            f32_4x tTerminate = ZeroF32_4x();
            v3 At = 0.5f*CellDim;
            f32_4x At4 = F32_4x(At.x, At.y, At.z, 0);
            u16 GridIndex = InitialGridIndex;
            s32 CostMetric = Solution->MaxCostPerRay;
            // CostMetric = 0;
            u16 SomethingHit = 0;
            for(;;)
            {
                lighting_spatial_grid_node Node = Solution->SpatialGridNodes[GridIndex];
                if((CostMetric <= 0) || (Node.StartIndex == SPATIAL_GRID_NODE_TERMINATOR))
                {
                    ProbeSamplePSingle = RayOriginPacked + tTerminate*RayDPacked;
                    break;
                }

#if GRID_RAY_CAST_DEBUGGING
                if(Debugging)
                {
                    v3 BoxColor = (Node.StartIndex == Node.OnePastLastIndex) ?
                        V3(0, 0.25f, 0.25f) : V3(0.75f, 0.0f, 0.75f);

                    rectangle3 VoxBox = GetCellBounds(&Solution->SpatialGrid,
                                                      DimIndexFrom(&Solution->SpatialGrid, GridIndex));
                    PushDebugBox(Solution, VoxBox, V4(BoxColor, 1));

                    for(u16 LeafIndexIndex = Node.StartIndex;
                        LeafIndexIndex < Node.OnePastLastIndex;
                        ++LeafIndexIndex)
                    {
                        lighting_spatial_grid_leaf Leaf = Solution->SpatialGridLeaves[LeafIndexIndex];

                        v3_4x BoxMin = Leaf.BoxMin;
                        v3_4x BoxMax = Leaf.BoxMax;
                        for(u32 Index = 0;
                            Index < 4;
                            ++Index)
                        {
                            rectangle3 OccluderBox = RectMinMax(
                                                                GetComponent(BoxMin, Index),
                                                                GetComponent(BoxMax, Index));
                            //OccluderBox = AddRadiusTo(OccluderBox, 0.1f*V3(1, 1, 1));
                            PushDebugBox(Solution, OccluderBox, V4(1, 1, 1, 1));
                        }
                    }
                }
#endif

                // TODO(casey): We need to also try a version that is single-leaf spatial
                // structures that we then interleave in a pre-walk, because that would not
                // suffer from the underfull leaf case like this one does.
                for(u16 LeafIndexIndex = Node.StartIndex;
                    LeafIndexIndex < Node.OnePastLastIndex;
                    ++LeafIndexIndex)
                {
                    lighting_spatial_grid_leaf Leaf = Solution->SpatialGridLeaves[LeafIndexIndex];

                    v3_4x BoxMin = Leaf.BoxMin;
                    v3_4x BoxMax = Leaf.BoxMax;

                    v3_4x tBoxMin = Hadamard((BoxMin - RayOrigin), InvRayD);
                    v3_4x tBoxMax = Hadamard((BoxMax - RayOrigin), InvRayD);

                    v3_4x tMin3 = Min(tBoxMin, tBoxMax);
                    v3_4x tMax3 = Max(tBoxMin, tBoxMax);

                    f32_4x tMin = Max(tMin3.x, Max(tMin3.y, tMin3.z));
                    f32_4x tMax = Min(tMax3.x, Min(tMax3.y, tMax3.z));

                    // TODO(casey): Do we actually need (tMin < tMax) here?
                    f32_4x tValid = (tMin > ZeroF32_4x()) & (tMin < tMax);
                    f32_4x Mask = tValid & (tMin < tRay);

                    tRay = Select(tRay, Mask, tMin);
                    HitTMinX = Select(HitTMinX, Mask, tMin3.x);
                    HitTMinY = Select(HitTMinY, Mask, tMin3.y);

                    HitRefColor = Select(HitRefColor, Mask, Leaf.RefColor);
                    HitEmission = Select(HitEmission, Mask, Leaf.IsEmission);

                    SomethingHit |= _mm_movemask_ps(Mask.P);
                }

                if(SomethingHit)
                {
#if 0
                    __m128 tRayTo16 = {};
                    __m128 tRayScaled = _mm_mul_ps(tRay.P, tRayTo16);
                    __m128i tRayInt = _mm_cvtps_epi32(tRayScaled);
                    __m128i HComp = _mm_packs_epi32(tRayInt, tRayInt);
#else
                    // TODO(casey): I'm not sure this is actually going to be accurate
                    // enough for our purposes.  It may be the case that we need the full
                    // 32-bit compare in order to determine which hit is best, and we
                    // may not want the inaccuracy of sometimes picking the further hit
                    // when two hits are very close.
                    alignas(16) u8 HCompShuffler[16] = {2, 3, 6, 7, 10, 11, 14, 15, 2, 3, 2, 3, 2, 3, 2, 3};
                    __m128i HComp = _mm_shuffle_epi8(_mm_castps_si128(tRay.P), *(__m128i *)HCompShuffler);
#endif

                    __m128i MinTest = _mm_minpos_epu16(HComp);
                    int ShuffleIndex = _mm_extract_epi16(MinTest, 1);
                    //Assert(ShuffleIndex < 4);
                    Shuffler = *(__m128i *)ShuffleTable[ShuffleIndex];

                    tRay = PShufB(tRay, Shuffler);

#if 0
                    v3_4x ProbeSampleP = RayOrigin + tRay*RayD;ProbeSamplePSingle = F32_4x(ConvertF32(ProbeSampleP.x, 0), ConvertF32(ProbeSampleP.y, 0), ConvertF32(ProbeSampleP.z, 0), 0);

                    f32_4x SignX = InverseSignBitFrom(RayD.x);
                    f32_4x SignY = InverseSignBitFrom(RayD.y);
                    f32_4x SignZ = InverseSignBitFrom(RayD.z);

                    f32_4x XMask = (PShufB(HitTMinX, Shuffler) == tRay);
                    f32_4x YMask = AndNot(PShufB(HitTMinY, Shuffler) == tRay, XMask);
                    f32_4x ZMask = (XMask | YMask);

                    f32_4x NormalX = SignX | (One & XMask);
                    f32_4x NormalY = SignY | (One & YMask);
                    f32_4x NormalZ = SignZ | AndNot(One, ZMask);

                    ProbeSampleNSingle = F32_4x(ConvertF32(NormalX, 0), ConvertF32(NormalY, 0), ConvertF32(NormalZ, 0), 0);
#else
                    ProbeSamplePSingle = RayOriginPacked + tRay*RayDPacked;

                    // TODO(casey): This has not been tested, and is VERY hard to perceive
                    // Needs to be stress-tested to make sure we always get the right
                    // normal, there are probably bugs in here:
                    f32_4x XCmp = (HitTMinX == tRay);
                    f32_4x YCmp = (HitTMinY == tRay);
                    u32 NormalIndex = (_mm_movemask_ps(XCmp.P) >> ShuffleIndex) & 0x1;
                    NormalIndex |= ((_mm_movemask_ps(YCmp.P) >> ShuffleIndex) & 0x1) << 1;
                    ProbeSampleNSingle = SignRayD | NormalTable[NormalIndex];
#endif
                    OcclusionD = ProbeSampleNSingle;

                    break;
                }

                CostMetric -= (Node.OnePastLastIndex - Node.StartIndex) + 1;

#if 1
                s16 dGrid = WalkTable->dGrid;
                GridIndex += dGrid;
                tTerminate = F32_4x(WalkTable->tTerminate);
                ++WalkTable;
#endif

#if 0
                f32_4x t4 = (CellDim4 - At4) * AbsInvRayDPacked;
                f32_4x t4s = Shuffle4x(t4, 1, 2, t4, 0, 3);
                f32_4x MinComp = (t4 > t4s);
                u32 CompMask = _mm_movemask_ps(MinComp.P);
                u32 BestDim = tBestTable[CompMask];
                f32_4x tBest = PShufB(t4, *(__m128i *)tShuffleTable[CompMask]);
                At4 += tBest*AbsRayDPacked;
                At4 -= (CellDim4 & LoadF32_4X(tMaskTable[CompMask]));

                s16 dGridResult = Step[BestDim];
#if 1
                if((ConvertF32(tTerminateVerify, 0) != ConvertF32(tTerminate, 0)) ||
                   (dGrid != dGridResult))
                {
                    *(volatile int *)0 = 0;
                }
#endif

                //GridIndex += dGridResult;
                //tTerminate = tTerminateVerify;
                tTerminateVerify += tBest;
#endif
            }

            voxel_grid *Grid = &Solution->AtlasGrid;

            f32_4x R = ProbeSampleNSingle;
            f32_4x P = ProbeSamplePSingle;

            f32_4x AbsR = AbsoluteValue(R);
            f32_4x OneNorm = Broadcast4x(AbsR, 0) + Broadcast4x(AbsR, 1) + Broadcast4x(AbsR, 2);
            f32_4x iUV = (One / OneNorm)*R;
            f32_4x Oxy = SignOf(iUV) * (One - AbsoluteValue(Shuffle4x(iUV, 1, 0, iUV, 1, 0)));

            f32_4x ZMask = (Broadcast4x(R, 2) < ZeroF32_4x());
            f32_4x UV = Select(iUV, ZMask, Oxy);

            UV = F32_4x(0.5f)*(One + UV);

            f32_4x I = F32_4x(DiffuseAtlas->OctDimCoefficient.x, DiffuseAtlas->OctDimCoefficient.y, 0, 0)*UV + F32_4x(1.5f);
            f32_4x Txy = Floor(I);

            f32_4x InvCellDim = F32_4x(Grid->InvCellDim.x,
                                       Grid->InvCellDim.y,
                                       Grid->InvCellDim.z,
                                       0);

            f32_4x TotalBounds = F32_4x(Grid->TotalBounds.Min.x,
                                        Grid->TotalBounds.Min.y,
                                        Grid->TotalBounds.Min.z,
                                        0);

            f32_4x FCoord = InvCellDim*(P - TotalBounds);
            f32_4x BCoord = Floor(FCoord);
            f32_4x uvw = FCoord - BCoord;

            f32_4x StrideXYZ = LoadF32_4X(DiffuseAtlas->StrideXYZ_4x);
            f32_4x VoxelDim = LoadF32_4X(DiffuseAtlas->VoxelDim_4x);
            f32_4x VoxelMax = VoxelDim - One;
            f32_4x MinXYZ = Clamp(Zero, BCoord, VoxelMax);
            f32_4x MaxXYZ = Clamp(Zero, BCoord + One, VoxelMax);
            f32_4x DeltaXYZ = MaxXYZ - MinXYZ;
            f32_4x BaseXYZ = MinXYZ*StrideXYZ + F32_4x(1.0f, (f32)DiffuseAtlas->TileRowStride, 0, 0)*Txy;
            f32_4x dXYZ = StrideXYZ*DeltaXYZ;

            u32 BaseOffset = ConvertS32(BaseXYZ, 0) + ConvertS32(BaseXYZ, 1) + ConvertS32(BaseXYZ, 2);

            u32 XOffset = ConvertS32(dXYZ, 0);
            u32 YOffset = ConvertS32(dXYZ, 1);
            u32 ZOffset = ConvertS32(dXYZ, 2);
            u32 XYOffset = XOffset + YOffset;

            v3 *TexelPtr = (v3 *)GetLightAtlasTexels(DiffuseAtlas) + BaseOffset;

            f32_4x Tile0 = LoadF32_4X(TexelPtr);
            f32_4x Tile1 = LoadF32_4X(TexelPtr + XOffset);
            f32_4x Tile2 = LoadF32_4X(TexelPtr + YOffset);
            f32_4x Tile3 = LoadF32_4X(TexelPtr + XYOffset);
            TexelPtr += ZOffset;
            f32_4x Tile4 = LoadF32_4X(TexelPtr);
            f32_4x Tile5 = LoadF32_4X(TexelPtr + XOffset);
            f32_4x Tile6 = LoadF32_4X(TexelPtr + YOffset);
            f32_4x Tile7 = LoadF32_4X(TexelPtr + XYOffset);

            f32_4x u = Broadcast4x(uvw, 0);
            f32_4x v = Broadcast4x(uvw, 1);
            f32_4x w = Broadcast4x(uvw, 2);

            // TODO(casey): We can probably eliminate some of these
            // broadcasts by welding this into the calling routine
            // where they were already separated?
            f32_4x ONx = Broadcast4x(OcclusionD, 0);
            f32_4x ONy = Broadcast4x(OcclusionD, 1);
            f32_4x ONz = Broadcast4x(OcclusionD, 2);

            f32_4x Pat0101 = F32_4x(0, 1, 0, 1);
            f32_4x Pat0011 = F32_4x(0, 0, 1, 1);

            f32_4x C03 = (u - Pat0101)*ONx + (v - Pat0011)*ONy + w*ONz;
            f32_4x C47 = C03 - ONz;

            C03 = Clamp01(C03);
            C47 = Clamp01(C47);

            f32_4x nUVW = One - uvw;
            f32_4x XTemp = Shuffle4x(nUVW, 0, 0, uvw, 0, 0);
            f32_4x NxPx = Shuffle4x(XTemp, 0, 2, XTemp, 0, 2);
            f32_4x NyPy = Shuffle4x(nUVW, 1, 1, uvw, 1, 1);
            f32_4x Nz = Broadcast4x(nUVW, 2);
            f32_4x Pz = Broadcast4x(uvw, 2);

            f32_4x W03 = NxPx*NyPy*Nz*C03;
            f32_4x W47 = NxPx*NyPy*Pz*C47;

            f32_4x W0 = Broadcast4x(W03, 0);
            f32_4x W1 = Broadcast4x(W03, 1);
            f32_4x W2 = Broadcast4x(W03, 2);
            f32_4x W3 = Broadcast4x(W03, 3);
            f32_4x W4 = Broadcast4x(W47, 0);
            f32_4x W5 = Broadcast4x(W47, 1);
            f32_4x W6 = Broadcast4x(W47, 2);
            f32_4x W7 = Broadcast4x(W47, 3);

            f32_4x Sum01 = W0 + W1;
            f32_4x Sum23 = W2 + W3;
            f32_4x Sum45 = W4 + W5;
            f32_4x Sum67 = W6 + W7;

            f32_4x Sum02 = Sum01 + Sum23;
            f32_4x Sum46 = Sum45 + Sum67;
            f32_4x Sum0 = Sum02 + Sum46;
            f32_4x InvSum0 = ApproxOneOver(Sum0);

            f32_4x ReflectLevel = InvSum0*(Tile0*W0 +
                                           Tile1*W1 +
                                           Tile2*W2 +
                                           Tile3*W3 +
                                           Tile4*W4 +
                                           Tile5*W5 +
                                           Tile6*W6 +
                                           Tile7*W7);


            //f32 ReflectFalloff = Clamp01(Inner(ProbeSampleNSingle, EmissionDirection));
            //ReflectColorFoo *= ReflectFalloff;

            f32_4x EmissionLevel = PShufB(HitEmission, Shuffler);
            f32_4x TransmissionLevel = Max(ReflectLevel, EmissionLevel);

            //TransferPPS[RayIndex] = Clamp01(Inner(MoonDir, -RayDSingle))*MoonColor;

            // TODO(casey): Why isn't this just a wide multiply?  Change this to just keeping TransferPPS as wide.
            TransferPPS[RayIndex].r += RayWeight*ConvertF32(TransmissionLevel, 0)*ConvertF32(PShufB(HitRefColor.r, Shuffler), 0);
            TransferPPS[RayIndex].g += RayWeight*ConvertF32(TransmissionLevel, 1)*ConvertF32(PShufB(HitRefColor.g, Shuffler), 0);
            TransferPPS[RayIndex].b += RayWeight*ConvertF32(TransmissionLevel, 2)*ConvertF32(PShufB(HitRefColor.b, Shuffler), 0);

#if GRID_RAY_CAST_DEBUGGING
            if(Debugging)
            {
                v3 SP = V3(ProbeSamplePSingle.E[0], ProbeSamplePSingle.E[1], ProbeSamplePSingle.E[2]);
                v3 SN = V3(ProbeSampleNSingle.E[0], ProbeSampleNSingle.E[1], ProbeSampleNSingle.E[2]);
                PushDebugBox(Solution, RectCenterDim(SP, .125f*V3(1, 1, 1)), V4(1, 1, 1, 1));
                PushDebugLine(Solution, RayOriginSingle, SP, V4(1, 1, 0, 1));
                PushDebugLine(Solution, SP, RayOriginSingle + 50.0f*RayDSingle, V4(.25f, .125f, 0, 1));

                PushDebugLine(Solution, SP, SP + 1.0f*SN, V4(1.0f, 0.5f, 0, 1));

                Debugging = false;
            }
#endif
        }
    }

    f32_4x S0 = LoadF32_4X((f32_4x *)SpecTexel.Value + 0);
    f32_4x S1 = LoadF32_4X((f32_4x *)SpecTexel.Value + 1);
    f32_4x S2 = LoadF32_4X((f32_4x *)SpecTexel.Value + 2);
    f32_4x S3 = LoadF32_4X((f32_4x *)SpecTexel.Value + 3);
    f32_4x S4 = LoadF32_4X((f32_4x *)SpecTexel.Value + 4);
    f32_4x S5 = LoadF32_4X((f32_4x *)SpecTexel.Value + 5);

    f32_4x T0 = LoadF32_4X((f32_4x *)TransferPPS + 0);
    f32_4x T1 = LoadF32_4X((f32_4x *)TransferPPS + 1);
    f32_4x T2 = LoadF32_4X((f32_4x *)TransferPPS + 2);
    f32_4x T3 = LoadF32_4X((f32_4x *)TransferPPS + 3);
    f32_4x T4 = LoadF32_4X((f32_4x *)TransferPPS + 4);
    f32_4x T5 = LoadF32_4X((f32_4x *)TransferPPS + 5);

    f32_4x Blend = F32_4x(Solution->tUpdateBlend);
    f32_4x InvBlend = One - Blend;

    S0 = InvBlend*S0 + Blend*T0;
    S1 = InvBlend*S1 + Blend*T1;
    S2 = InvBlend*S2 + Blend*T2;
    S3 = InvBlend*S3 + Blend*T3;
    S4 = InvBlend*S4 + Blend*T4;
    S5 = InvBlend*S5 + Blend*T5;

    StoreF32_4X(S0, (f32_4x *)SpecTexel.Value + 0);
    StoreF32_4X(S1, (f32_4x *)SpecTexel.Value + 1);
    StoreF32_4X(S2, (f32_4x *)SpecTexel.Value + 2);
    StoreF32_4X(S3, (f32_4x *)SpecTexel.Value + 3);
    StoreF32_4X(S4, (f32_4x *)SpecTexel.Value + 4);
    StoreF32_4X(S5, (f32_4x *)SpecTexel.Value + 5);

    RECORD_RAYCAST_END();
}

internal u8
GetQuadrantFor(v3s Remainder)
{
    Assert(Remainder.z == 0);
    u8 Result = SafeTruncateToU8(Remainder.x | (Remainder.y << 1));
    return(Result);
}

internal void
FullCast(lighting_work *Work, light_atlas_texel Tile, v3 VoxCenterP, v3s AtlasIndex)
{
    lighting_solution *Solution = Work->Solution;
    light_atlas *SpecAtlas = Work->SpecularLightAtlas;
    light_atlas *DiffuseAtlas = Work->DiffuseLightAtlas;

    v3 LightProbeP = VoxCenterP;
    v3_4x RayOrigin = V3_4x(LightProbeP, LightProbeP, LightProbeP, LightProbeP);

    v3s HalfAtlasIndex = (AtlasIndex/2);
    v3s Remainder = AtlasIndex - (2*HalfAtlasIndex);
    Remainder.z = 0;
    u8 QuadrantIndex = GetQuadrantFor(Remainder);
    u16 InitialGridIndex =
        SafeTruncateToU16(FlatIndexFrom(&Solution->SpatialGrid,
                                        HalfAtlasIndex + Solution->AtlasToSpatialGridIndexOffset));

    f32_4x IsInside = ZeroF32_4x();
    lighting_spatial_grid_node Node = Solution->SpatialGridNodes[InitialGridIndex];
    for(u16 LeafIndexIndex = Node.StartIndex;
        LeafIndexIndex < Node.OnePastLastIndex;
        ++LeafIndexIndex)
    {
        lighting_spatial_grid_leaf Leaf = Solution->SpatialGridLeaves[LeafIndexIndex];

        v3_4x BoxMin = Leaf.BoxMin;
        v3_4x BoxMax = Leaf.BoxMax;

        f32_4x InBoundsX = (RayOrigin.x > BoxMin.x) & (RayOrigin.x < BoxMax.x);
        f32_4x InBoundsY = (RayOrigin.y > BoxMin.y) & (RayOrigin.y < BoxMax.y);
        f32_4x InBoundsZ = (RayOrigin.z > BoxMin.z) & (RayOrigin.z < BoxMax.z);
        f32_4x Mask = InBoundsX & InBoundsY & InBoundsZ;

        IsInside |= Mask;
    }

    if(AnyTrue(IsInside))
    {
        // TODO(casey): This is buggy because you can be inside a light source, and in
        // that case, clearly the right answer is not that you don't have light.  You
        // should at least set your value to the emission value.

        // TODO(casey): Handle lerping towards zero in case we ever have moving occluders.

#if 1
        for(u32 Sy = 0;
            Sy < (SpecAtlas->TileDim.y - 2);
            ++Sy)
        {
            light_atlas_texel SpecTexel = OffsetFromTexel(SpecAtlas, Tile, 1, Sy+1);

#if 0
            f32_4x S0 = F32_4x(1.0f, 0.0f, 1.0f,  1.0f);
            f32_4x S1 = F32_4x(0.0f, 1.0f,  1.0f, 0.0f);
            f32_4x S2 = F32_4x(1.0f,  1.0f, 0.0f, 1.0f);
            f32_4x S3 = F32_4x(1.0f, 0.0f, 1.0f,  1.0f);
            f32_4x S4 = F32_4x(0.0f, 1.0f,  1.0f, 0.0f);
            f32_4x S5 = F32_4x(1.0f,  1.0f, 0.0f, 1.0f);
#else
            f32_4x S0, S1, S2, S3, S4, S5;
            S0 = S1 = S2 = S3 = S4 = S5 = {};
#endif

            StoreF32_4X(S0, (f32_4x *)SpecTexel.Value + 0);
            StoreF32_4X(S1, (f32_4x *)SpecTexel.Value + 1);
            StoreF32_4X(S2, (f32_4x *)SpecTexel.Value + 2);
            StoreF32_4X(S3, (f32_4x *)SpecTexel.Value + 3);
            StoreF32_4X(S4, (f32_4x *)SpecTexel.Value + 4);
            StoreF32_4X(S5, (f32_4x *)SpecTexel.Value + 5);

        }
#endif
    }
    else
    {
        for(u32 Sy = 0;
            Sy < (SpecAtlas->TileDim.y - 2);
            ++Sy)
        {
            light_atlas_texel SpecTexelA = OffsetFromTexel(SpecAtlas, Tile, 1, Sy+1);
            GridRayCast(Work, Sy, LightProbeP, SpecAtlas, DiffuseAtlas, SpecTexelA, InitialGridIndex, QuadrantIndex);
            Work->TotalCastsInitiated += 8*LIGHT_SAMPLING_OCTAHEDRON_COUNT;
        }

#if GRID_RAY_CAST_DEBUGGING
        if(AreEqual(Solution->DebugPick.AtlasIndex, AtlasIndex))
        {
            Solution->DebugPick.AtlasIndex = Clamp({}, Solution->DebugPick.AtlasIndex, Solution->AtlasGrid.CellCount - V3S(1, 1, 1));
            Solution->DebugPick.Tx = Clamp(0, Solution->DebugPick.Tx, 7);
            Solution->DebugPick.Ty = Clamp(0, Solution->DebugPick.Ty, 7);
            Solution->DebugPick.OctahedronIndex = Clamp(0, Solution->DebugPick.OctahedronIndex,
                                                        LIGHT_SAMPLING_OCTAHEDRON_COUNT);
            v3 ExpectedDirection = DirectionFromTxTy(SpecAtlas->OxyCoefficient,
                                                     Solution->DebugPick.Tx + 1,
                                                     Solution->DebugPick.Ty + 1);

            v3 Ignored[16];
            light_atlas_texel SpecTexelA = {Ignored};
            GridRayCast(Work, Solution->DebugPick.Ty, LightProbeP, SpecAtlas, DiffuseAtlas,
                        SpecTexelA, InitialGridIndex, QuadrantIndex, Solution->DebugPick);

            PushDebugLine(Solution, LightProbeP, LightProbeP + ExpectedDirection, V4(1, 0, 1, 1));
        }
#endif
    }
}

internal void
FillLightAtlasBorder(light_atlas *Atlas, u32 X, u32 Y, u32 Z)
{
    u32 N;

    v3 *DiffuseTexels = (v3 *)GetLightAtlasTexels(Atlas);

    N = Atlas->TileDim.x - 1;
    u32 Row0Start = LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, 0);
    u32 Row1Start = LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, 1);
    u32 RowNStart = LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, N);
    u32 RowN1Start = LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, N - 1);
    for(u32 Tx = 1;
        Tx < N;
        ++Tx)
    {
        v3 C0 = DiffuseTexels[Row1Start + N - Tx];
        v3 C1 = DiffuseTexels[RowN1Start + N - Tx];

#if EDGE_COLORING
        C0 = V3(1, 0, 0);
        C1 = V3(0, 1, 0);
#endif

        DiffuseTexels[Row0Start + Tx] = C0;
        DiffuseTexels[RowNStart + Tx] = C1;
    }

    N = Atlas->TileDim.y - 1;
    for(u32 Ty = 1;
        Ty < N;
        ++Ty)
    {
        v3 C0 = DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 1, N - Ty)];
        v3 C1 = DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, N - 1, N - Ty)];

#if EDGE_COLORING
        C0 = V3(1, 1, 0);
        C1 = V3(0, 1, 1);
#endif

        DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, Ty)] = C0;
        DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, N, Ty)] = C1;
    }

    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, 0)] =
        DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, N - 1, N - 1)];
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, N, 0)] =
        DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 1, N - 1)];
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 0, N)] =
        DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, N - 1, 1)];
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, N, N)] =
        DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 1, 1)];

#if 0
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 3, 3)] = V3(1, 0, 0);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 4, 3)] = V3(0, 1, 0);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 5, 3)] = V3(0, 0, 1);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 3, 4)] = V3(1, 1, 0);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 4, 4)] = V3(0, 0, 0);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 5, 4)] = V3(1, 0, 1);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 3, 5)] = V3(0.5f, 1, 0);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 4, 5)] = V3(1, 0.5f, 0);
    DiffuseTexels[LIGHT_ATLAS_OFFSET(Atlas, X, Y, Z, 5, 5)] = V3(0, 0.5f, 1);
#endif
}

internal PLATFORM_WORK_QUEUE_CALLBACK(ComputeLightPropagationWork)
{
    TIMED_FUNCTION();

    SetDefaultFPBehavior();

    lighting_work *Work = (lighting_work *)Data;
    lighting_solution *Solution = Work->Solution;
    light_atlas *DiffuseAtlas = Work->DiffuseLightAtlas;
    light_atlas *SpecAtlas = Work->SpecularLightAtlas;

#define EDGE_COLORING 0

    s32 Y = Work->VoxelY;
    v3s VoxelDim = SpecAtlas->VoxelDim;
    //u32 XOdd = (X&1) ^ Solution->FrameOdd;

    for(s32 Z = 0;
        Z < VoxelDim.z;
        ++Z)
    {
        for(s32 X = 0;
            X < VoxelDim.x;
            ++X)
        {
            v3s AtlasIndex = {X, Y, Z};

            b32x ShouldCompute = true; // (XOdd ^ ((Y&1) ^ (Z&1)));
            if(ShouldCompute)
            {
                light_atlas_texel SpecTile = GetTileClamped(SpecAtlas, X, Y, Z);
                v3 BoxCenterP = GetCellCenterP(&Solution->AtlasGrid, AtlasIndex);

                FullCast(Work, SpecTile, BoxCenterP, AtlasIndex);

                light_atlas_texel DiffuseTile = GetTileClamped(DiffuseAtlas, X, Y, Z);

                // 8
#if 1
                for(u32 Ty = 1;
                    Ty < (DiffuseAtlas->TileDim.y - 1);
                    ++Ty)
                {
                    light_atlas_texel DiffuseTexel = OffsetFromTexel(DiffuseAtlas, DiffuseTile, 1, Ty);

                    v3_4x *DestC = (v3_4x *)DiffuseTexel.Value;

                    // 2
                    for(u32 Tx4 = 0;
                        Tx4 < ((DiffuseAtlas->TileDim.x - 2) / 4);
                        ++Tx4)
                    {
                        v3_4x LightC = {};
                        diffuse_weight_map *Weights = &Solution->DiffuseWeightMap[Ty-1][Tx4];

                        // 8
                        for(u32 Sy = 0;
                            Sy < (SpecAtlas->TileDim.y - 2);
                            ++Sy)
                        {
                            light_atlas_texel SpecTexel = OffsetFromTexel(SpecAtlas, SpecTile, 1, Sy+1);

                            f32_4x S0 = LoadF32_4X((f32_4x *)SpecTexel.Value + 0);
                            f32_4x S1 = LoadF32_4X((f32_4x *)SpecTexel.Value + 1);
                            f32_4x S2 = LoadF32_4X((f32_4x *)SpecTexel.Value + 2);

                            f32_4x S3 = LoadF32_4X((f32_4x *)SpecTexel.Value + 3);
                            f32_4x S4 = LoadF32_4X((f32_4x *)SpecTexel.Value + 4);
                            f32_4x S5 = LoadF32_4X((f32_4x *)SpecTexel.Value + 5);

                            LightC.r += Weights->E[Sy][0] * Broadcast4x(S0, 0);
                            LightC.g += Weights->E[Sy][0] * Broadcast4x(S0, 1);
                            LightC.b += Weights->E[Sy][0] * Broadcast4x(S0, 2);

                            LightC.r += Weights->E[Sy][1] * Broadcast4x(S0, 3);
                            LightC.g += Weights->E[Sy][1] * Broadcast4x(S1, 0);
                            LightC.b += Weights->E[Sy][1] * Broadcast4x(S1, 1);

                            LightC.r += Weights->E[Sy][2] * Broadcast4x(S1, 2);
                            LightC.g += Weights->E[Sy][2] * Broadcast4x(S1, 3);
                            LightC.b += Weights->E[Sy][2] * Broadcast4x(S2, 0);

                            LightC.r += Weights->E[Sy][3] * Broadcast4x(S2, 1);
                            LightC.g += Weights->E[Sy][3] * Broadcast4x(S2, 2);
                            LightC.b += Weights->E[Sy][3] * Broadcast4x(S2, 3);

                            LightC.r += Weights->E[Sy][4] * Broadcast4x(S3, 0);
                            LightC.g += Weights->E[Sy][4] * Broadcast4x(S3, 1);
                            LightC.b += Weights->E[Sy][4] * Broadcast4x(S3, 2);

                            LightC.r += Weights->E[Sy][5] * Broadcast4x(S3, 3);
                            LightC.g += Weights->E[Sy][5] * Broadcast4x(S4, 0);
                            LightC.b += Weights->E[Sy][5] * Broadcast4x(S4, 1);

                            LightC.r += Weights->E[Sy][6] * Broadcast4x(S4, 2);
                            LightC.g += Weights->E[Sy][6] * Broadcast4x(S4, 3);
                            LightC.b += Weights->E[Sy][6] * Broadcast4x(S5, 0);

                            LightC.r += Weights->E[Sy][7] * Broadcast4x(S5, 1);
                            LightC.g += Weights->E[Sy][7] * Broadcast4x(S5, 2);
                            LightC.b += Weights->E[Sy][7] * Broadcast4x(S5, 3);
                        }

                        LightC = Transpose(LightC);

                        _mm_storeu_ps((f32 *)&DestC[Tx4].x, LightC.x.P);
                        _mm_storeu_ps((f32 *)&DestC[Tx4].y, LightC.y.P);
                        _mm_storeu_ps((f32 *)&DestC[Tx4].z, LightC.z.P);
                    }
                }

                FillLightAtlasBorder(DiffuseAtlas, X, Y, Z);
                FillLightAtlasBorder(SpecAtlas, X, Y, Z);
#else
#endif
            }
        }
    }
}

internal void
ComputeWalkTable(memory_arena *Memory, v3s VoxelDim, u32 MaxCostPerRay, v3 CellDim,
                 u32 SampleDirectionCount, light_sample_direction *SourceSamp,
                 light_sample_direction *DestSamp, walk_table_entry **DestTable)
{
    u32 PaddedMaxCostPerRay = MaxCostPerRay + 2;
    u32 MaxDestCount = SampleDirectionCount*PaddedMaxCostPerRay;

    for(u32 Y = 0;
        Y < 2;
        ++Y)
    {
        for(u32 X = 0;
            X < 2;
            ++X)
        {
            v3s Remainder = V3S(X, Y, 0);
            u8 QuadrantIndex = GetQuadrantFor(Remainder);
            walk_table_entry *Dest = PushArray(Memory, MaxDestCount, walk_table_entry);
            DestTable[QuadrantIndex] = Dest;

            u32 DestIndex = 0;
            v3 StartingLocation = 0.25f*CellDim + V3(Remainder)*(0.5f*CellDim);
            for(u32 SampleDirIndex = 0;
                SampleDirIndex < SampleDirectionCount;
                ++SampleDirIndex)
            {
                v3 RayD = SourceSamp[SampleDirIndex].RayD;
                light_sample_direction *Dir = DestSamp + SampleDirIndex;

                u32 Cost = 0;
                Dir->RayD = RayD;
                Dir->WalkTableOffset = DestIndex;
                s16 Step[3];

                s16 DimStep = 1;
                for(u32 DimIndex = 0;
                    DimIndex < 2;
                    ++DimIndex)
                {
                    if(RayD.E[DimIndex] >= 0)
                    {
                        Step[DimIndex] = DimStep;
                    }
                    else
                    {
                        Step[DimIndex] = -DimStep;
                    }

                    RayD.E[DimIndex] = AbsoluteValue(RayD.E[DimIndex]);
                    DimStep *= SafeTruncateToU16(VoxelDim.E[DimIndex]);
                }

#if 0
                v3 InvRayD =
                {
                    (RayD.x > 0) ? (1.0f / RayD.x) : 1000.0f,
                    (RayD.y > 0) ? (1.0f / RayD.y) : 1000.0f,
                    (RayD.z > 0) ? (1.0f / RayD.z) : 1000.0f,
                };
#else
                v3 InvRayD = 1.0f / RayD;
#endif

                v3 At = StartingLocation;
                f32 tTerminate = 0;
                while(Cost < PaddedMaxCostPerRay)
                {
                    if(DestIndex == 6912)
                    {
                        int FruFru = 1;
                    }

                    f32 tBest = F32Max;
                    u32 BestDim = 0;

                    f32 tThis[3];
                    for(u32 DimIndex = 0;
                        DimIndex < 2;
                        ++DimIndex)
                    {
                        tThis[DimIndex] = (CellDim.E[DimIndex] - At.E[DimIndex]) * InvRayD.E[DimIndex];
                    }

                    for(u32 DimIndex = 0;
                        DimIndex < 2;
                        ++DimIndex)
                    {
                        if(tBest > tThis[DimIndex])
                        {
                            tBest = tThis[DimIndex];
                            BestDim = DimIndex;
                        }
                    }

                    At += tBest*RayD;
                    At.E[BestDim] -= CellDim.E[BestDim];

                    if(DestIndex == 6913)
                    {
                        int FruFru = 1;
                    }

                    Assert((BestDim == 0) || (BestDim == 1));
                    Assert(DestIndex < MaxDestCount);
                    Dest[DestIndex].tTerminate = tTerminate;
                    Dest[DestIndex].dGrid = Step[BestDim];
                    ++DestIndex;

                    tTerminate += tBest;

                    ++Cost;
                }
            }
        }
    }
}

internal void
GridBuildSpatialPartition(lighting_solution *Solution, memory_arena *TempArena,
                          u32 InputBoxCount, lighting_box *InputBoxes)
{
    u16 GridCount = SafeTruncateToU16(GetTotalCellCount(&Solution->SpatialGrid));
    Solution->SpatialGridNodes = PushArray(TempArena, GridCount, lighting_spatial_grid_node);

    rectangle3 ClippingRegion = Solution->SpatialGrid.TotalBounds;
    v3 ClipShrink = -1.0f*Solution->SpatialGrid.CellDim;
    ClipShrink.z = 0;
    ClippingRegion = AddRadiusTo(ClippingRegion, ClipShrink);

    for(u16 BoxIndex = 0;
        BoxIndex < InputBoxCount;
        ++BoxIndex)
    {
        lighting_box *Box = InputBoxes + BoxIndex;

        rectangle3 BoxBounds = Intersect(ClippingRegion, RectMinMax(Box->BoxMin, Box->BoxMax));
        if(HasArea(BoxBounds))
        {
            v3s MinNodeIndex = GetIndexForP(&Solution->SpatialGrid, BoxBounds.Min);
            v3s MaxNodeIndex = GetIndexForP(&Solution->SpatialGrid, BoxBounds.Max);
            for(s32 Z = MinNodeIndex.z;
                Z <= MaxNodeIndex.z;
                ++Z)
            {
                for(s32 Y = MinNodeIndex.y;
                    Y<= MaxNodeIndex.y;
                    ++Y)
                {
                    for(s32 X = MinNodeIndex.x;
                        X <= MaxNodeIndex.x;
                        ++X)
                    {
                        v3s SpatialIndex = V3S(X, Y, Z);
                        lighting_spatial_grid_node *Node = Solution->SpatialGridNodes + FlatIndexFrom(&Solution->SpatialGrid, SpatialIndex);
                        ++Node->OnePastLastIndex;
                    }
                }
            }
        }
    }

    u32 TotalLeafCount = 0;
    for(u32 GridIndex = 0;
        GridIndex < GridCount;
        ++GridIndex)
    {
        lighting_spatial_grid_node *Node = Solution->SpatialGridNodes + GridIndex;
        Node->StartIndex = SafeTruncateToU16(TotalLeafCount);
        TotalLeafCount = Align4(TotalLeafCount + Node->OnePastLastIndex);
        Node->OnePastLastIndex = 0;
    }

    // NOTE(casey): Some folks had compiler problems with the compiler producing movaps's,
    // which would crash, on SpatialGridLeaves, so we align to 16 here just to ensure
    // that it won't matter which load is used.
    u32 SpatialGridLeafCount = (TotalLeafCount / 4);
    Solution->SpatialGridLeaves = PushArray(TempArena, SpatialGridLeafCount, lighting_spatial_grid_leaf, Align(16, true));

    for(u16 BoxIndex = 0;
        BoxIndex < InputBoxCount;
        ++BoxIndex)
    {
        lighting_box *Box = InputBoxes + BoxIndex;
        rectangle3 BoxBounds = Intersect(ClippingRegion, RectMinMax(Box->BoxMin, Box->BoxMax));
        if(HasArea(BoxBounds))
        {
            v3s MinNodeIndex = GetIndexForP(&Solution->SpatialGrid, BoxBounds.Min);
            v3s MaxNodeIndex = GetIndexForP(&Solution->SpatialGrid, BoxBounds.Max);
            for(s32 Z = MinNodeIndex.z;
                Z <= MaxNodeIndex.z;
                ++Z)
            {
                for(s32 Y = MinNodeIndex.y;
                    Y<= MaxNodeIndex.y;
                    ++Y)
                {
                    for(s32 X = MinNodeIndex.x;
                        X <= MaxNodeIndex.x;
                        ++X)
                    {
                        Assert(Z == 0);
                        v3s SpatialIndex = V3S(X, Y, Z);
                        lighting_spatial_grid_node *Node = Solution->SpatialGridNodes + FlatIndexFrom(&Solution->SpatialGrid, SpatialIndex);
                        rectangle3 CellBounds = GetCellBounds(&Solution->SpatialGrid, V3S(X, Y, Z));

                        u32 LeafIndex = (Node->StartIndex + Node->OnePastLastIndex);
                        u32 PrimaryIndex = LeafIndex / 4;
                        u32 LaneIndex = LeafIndex % 4;

                        lighting_spatial_grid_leaf *Leaf = Solution->SpatialGridLeaves + PrimaryIndex;

                        // NOTE(casey): We need to use Intersect here because we
                        // cannot allow boxes to extend outside their node region,
                        // otherwise early termination of the raycaster would not
                        // work since earlier t's could be _farther_ hits compared
                        // to later t's.
                        rectangle3 LeafBox = Intersect(CellBounds, BoxBounds);

                        Leaf->BoxMin.x.E[LaneIndex] = LeafBox.Min.x;
                        Leaf->BoxMin.y.E[LaneIndex] = LeafBox.Min.y;
                        Leaf->BoxMin.z.E[LaneIndex] = LeafBox.Min.z;

                        Leaf->BoxMax.x.E[LaneIndex] = LeafBox.Max.x;
                        Leaf->BoxMax.y.E[LaneIndex] = LeafBox.Max.y;
                        Leaf->BoxMax.z.E[LaneIndex] = LeafBox.Max.z;

                        Leaf->RefColor.x.E[LaneIndex] = Box->RefC.x;
                        Leaf->RefColor.y.E[LaneIndex] = Box->RefC.y;
                        Leaf->RefColor.z.E[LaneIndex] = Box->RefC.z;

                        Leaf->IsEmission.E[LaneIndex] = (Box->IsLight ? 1.0f : 0.0f);

                        ++Node->OnePastLastIndex;
                    }
                }
            }
        }
    }

    v3s CellCount = Solution->SpatialGrid.CellCount;
    for(s32 Z = 0;
        Z < CellCount.z;
        ++Z)
    {
        for(s32 Y = 0;
            Y < CellCount.y;
            ++Y)
        {
            for(s32 X = 0;
                X < CellCount.x;
                ++X)
            {
                Assert(Z == 0);
                v3s SpatialIndex = V3S(X, Y, Z);
                lighting_spatial_grid_node *Node = Solution->SpatialGridNodes + FlatIndexFrom(&Solution->SpatialGrid, SpatialIndex);

                Node->StartIndex /= 4;
                u16 Count4 = ((Node->OnePastLastIndex + 3) / 4);
                Node->OnePastLastIndex = Node->StartIndex + Count4;
                if((Y == 0) ||
                   (X == 0) ||
                   (Y == (CellCount.y - 1)) ||
                   (X == (CellCount.x - 1)))
                {
                    Node->StartIndex = SPATIAL_GRID_NODE_TERMINATOR;
                }
                else
                {
                    Assert(Node->OnePastLastIndex <= SpatialGridLeafCount);
                }
            }
        }
    }
}

internal u64
IrradiancePack(v3 LightI)
{
    LightI += V3(LIGHT_FLOOR_VALUE, LIGHT_FLOOR_VALUE, LIGHT_FLOOR_VALUE);
    if(LightI.r < 0) {LightI.r = 0.0f;}
    if(LightI.g < 0) {LightI.g = 0.0f;}
    if(LightI.b < 0) {LightI.b = 0.0f;}

    u64 Result = 0;
    f32 MaxV = Maximum(LightI.x, Maximum(LightI.y, LightI.z));
    if(MaxV > 0)
    {
        f32 InvMaxV = 1.0f / MaxV;
        f32 A = Clamp01(MaxV / (f32)MAX_LIGHT_INTENSITY);
        Result = BGRAPack4x16(65535.0f*V4(InvMaxV*LightI.x, InvMaxV*LightI.y, InvMaxV*LightI.z, A));
    }

    return(Result);
}

internal v3
BinormalToNormal(v3 N)
{
    v3 Result = 0.5f*N + V3(0.5f, 0.5f, 0.5f);
    return(Result);
}

internal void
BuildDiffuseLightMaps(lighting_solution *Solution, light_atlas *DiffuseAtlas, light_atlas *SpecAtlas)
{
    for(u32 Sy = 0;
        Sy < (SpecAtlas->TileDim.y - 2);
        ++Sy)
    {
        for(u32 Sx = 0;
            Sx < (SpecAtlas->TileDim.x - 2);
            ++Sx)
        {
            f32 WTotal = 0;

            // TODO(casey): If we really cared, this total weight distribution could
            // probably be shown to be the same for all texels, so we could just
            // compute it once rather than doing it for each spec texel location.
            for(u32 Ty = 1;
                Ty < (DiffuseAtlas->TileDim.y - 1);
                ++Ty)
            {
                for(u32 Tx = 1;
                    Tx < (DiffuseAtlas->TileDim.x - 1);
                    ++Tx)
                {
                    v3 Outgoing = DirectionFromTxTy(DiffuseAtlas->OxyCoefficient, Tx, Ty);
                    v3 Incoming = DirectionFromTxTy(SpecAtlas->OxyCoefficient, Sx + 1, Sy + 1);
                    f32 W = Clamp01(Inner(Outgoing, Incoming));
                    WTotal += W;
                }
            }

            for(u32 Ty = 1;
                Ty < (DiffuseAtlas->TileDim.y - 1);
                ++Ty)
            {
                for(u32 Tx = 1;
                    Tx < (DiffuseAtlas->TileDim.x - 1);
                    ++Tx)
                {
                    v3 Outgoing = DirectionFromTxTy(DiffuseAtlas->OxyCoefficient, Tx, Ty);
                    v3 Incoming = DirectionFromTxTy(SpecAtlas->OxyCoefficient, Sx + 1, Sy + 1);
                    f32 W = Clamp01(Inner(Outgoing, Incoming));
                    diffuse_weight_map *Weights = &Solution->DiffuseWeightMap[Ty - 1][(Tx - 1)/4];
                    Weights->E[Sy][Sx].E[(Tx - 1) % 4] = W / WTotal;
                }
            }
        }
    }
}

internal lighting_stats
InternalLightingCore(lighting_solution *Solution, light_atlas *SpecAtlas, light_atlas *DiffuseAtlas, platform_work_queue *LightingQueue, memory_arena *TempArena,
                     u16 InputBoxCount, lighting_box *InputBoxes, u32 DebugRepeatCount = 1)
{
    // TODO(casey): Shouldn't I be able to not use debug/ here?  Maybe we need to
    // make the way this works be more intuitive?
    if(LightBoxDumpTrigger)
    {
        DEBUGDumpData("debug/source_lighting.dump", sizeof(*Solution), Solution);
        DEBUGDumpData("debug/source_lightboxes.dump", InputBoxCount*sizeof(*InputBoxes), InputBoxes);
        DEBUGDumpData("debug/source_specatlas.dump", GetLightAtlasSize(SpecAtlas), GetLightAtlasTexels(SpecAtlas));
        DEBUGDumpData("debug/source_diffuseatlas.dump", GetLightAtlasSize(DiffuseAtlas), GetLightAtlasTexels(DiffuseAtlas));
    }

    GridBuildSpatialPartition(Solution, TempArena, InputBoxCount, InputBoxes);

    u32 WorkCount = SpecAtlas->VoxelDim.y;
    lighting_work *Works = PushArray(TempArena, WorkCount, lighting_work, Align(64, false));

    while(DebugRepeatCount--)
    {
        for(u32 WorkIndex = 0;
            WorkIndex < WorkCount;
            ++WorkIndex)
        {
            lighting_work *Work = Works + WorkIndex;
            Assert(((memory_index)Work & 63) == 0);

            Work->Solution = Solution;

            Work->DiffuseLightAtlas = DiffuseAtlas;
            Work->SpecularLightAtlas = SpecAtlas;
            Work->VoxelY = WorkIndex;
            Work->SamplePointEntropy = RandomNextU32(&Solution->Series);

            Work->TotalCastsInitiated = 0;
            Work->TotalPartitionsTested = 0;
            Work->TotalPartitionLeavesUsed = 0;
            Work->TotalLeavesTested = 0;

            if(LightingQueue)
            {
                Platform.AddEntry(LightingQueue, ComputeLightPropagationWork, Work);
            }
            else
            {
                ComputeLightPropagationWork(LightingQueue, Work);
            }
        }

    }

    if(LightingQueue)
    {
        Platform.CompleteAllWork(LightingQueue);
    }

    lighting_stats Stats = {};
    for(u32 WorkIndex = 0;
        WorkIndex < WorkCount;
        ++WorkIndex)
    {
        lighting_work *Work = Works + WorkIndex;
        Stats.TotalCastsInitiated += Work->TotalCastsInitiated;
        Stats.TotalPartitionsTested += Work->TotalPartitionsTested;
        Stats.TotalLeavesTested += Work->TotalLeavesTested;
        Stats.TotalPartitionLeavesUsed += Work->TotalPartitionLeavesUsed;
    }

    if(LightBoxDumpTrigger)
    {
        DEBUGDumpData("debug/result_specatlas.dump", GetLightAtlasSize(SpecAtlas), GetLightAtlasTexels(SpecAtlas));
        DEBUGDumpData("debug/result_diffuseatlas.dump", GetLightAtlasSize(DiffuseAtlas), GetLightAtlasTexels(DiffuseAtlas));

        LightBoxDumpTrigger = false;
    }

    return(Stats);
}

internal void
DebugDrawColorDir(lighting_solution *Solution, light_atlas *Atlas, v3 CenterP,
                  s32 X, s32 Y, s32 Z, u32 Tx, u32 Ty)
{
    v3 Dir = DirectionFromTxTy(Atlas->OxyCoefficient, Tx, Ty);

    light_atlas_texel Texel = GetTileUnclamped(Atlas, X, Y, Z, Tx, Ty);
    v3 Value = *Texel.Value;

    f32 Intensity = 0.5f + 0.5f*Clamp01(Value.r + Value.g + Value.b);
    f32 Scale = 0.25f;

    PushDebugLine(Solution, CenterP, CenterP + Scale*Intensity*Dir, V4(Value, 1.0f));
}

internal void
DebugDrawOctahedralValues(lighting_solution *Solution, light_atlas *SpecAtlas, light_atlas *DiffuseAtlas)
{
    v3s DebugDim = SpecAtlas->VoxelDim;// / 2;
    DebugDim.z = SpecAtlas->VoxelDim.z;
    v3s VoxStart = (SpecAtlas->VoxelDim - DebugDim) / 2;

    for(s32 Z = 0;
        Z < DebugDim.z;
        ++Z)
    {
        for(s32 Y = 0;
            Y< DebugDim.y;
            ++Y)
        {
            for(s32 X = 0;
                X < DebugDim.x;
                ++X)
            {
                v3s VoxCoord = VoxStart + V3S(X, Y, Z);
                v3 CenterP = GetCellCenterP(&Solution->AtlasGrid, VoxCoord);
                rectangle3 Bounds = GetCellBounds(&Solution->AtlasGrid, VoxCoord);

                // PushDebugBox(Solution, Bounds, V4(0.5f, 0, 0, 1.0f));

#if 1
                for(u32 Ty = 1;
                    Ty < 9;
                    ++Ty)
                {
                    for(u32 Tx = 1;
                        Tx < 9;
                        ++Tx)
                    {
                        DebugDrawColorDir(Solution, DiffuseAtlas, CenterP,
                                          VoxCoord.x, VoxCoord.y, VoxCoord.z, Tx, Ty);
                        //DebugDrawColorDir(Solution, SpecAtlas, CenterP, VoxCoord.x, VoxCoord.y, VoxCoord.z, Tx, Ty);
                    }
                }
#endif

                PushDebugBox(Solution, RectCenterDim(CenterP, V3(0.25f, 0.25f, 0.25f)),
                             V4(1.0f, 0, 0, 1.0f));
            }
        }
    }
}

internal void
DebugDrawSpatialGrid(lighting_solution *Solution)
{
    voxel_grid *SpatialGrid = &Solution->SpatialGrid;
    Assert(SpatialGrid->CellCount.z == 1);

    for(s32 Y = 0;
        Y< SpatialGrid->CellCount.y;
        ++Y)
    {
        for(s32 X = 0;
            X < SpatialGrid->CellCount.x;
            ++X)
        {
            v3s NodeIndex = V3S(X, Y, 0);
            v4 Color = ((X&1) ^ (Y&1)) ? V4(1, 0, 1, 1) : V4(0, 1, 1, 1);
            rectangle3 CellBounds = AddRadiusTo(GetCellBounds(SpatialGrid, NodeIndex), -V3(0.1f, 0.1f, 0.1f));
            // PushDebugBox(Solution, CellBounds, Color);
#if 1
            lighting_spatial_grid_node Node = Solution->SpatialGridNodes[FlatIndexFrom(SpatialGrid, NodeIndex)];
            for(u16 LeafIndexIndex = Node.StartIndex;
                LeafIndexIndex < Node.OnePastLastIndex;
                ++LeafIndexIndex)
            {
                lighting_spatial_grid_leaf Leaf = Solution->SpatialGridLeaves[LeafIndexIndex];
                for(s32 E = 0; E < ArrayCount(Leaf.BoxMin.x.E); ++E)
                {
                    rectangle3 BoxBounds = RectMinMax(GetComponent(Leaf.BoxMin, E),
                                                   GetComponent(Leaf.BoxMax, E));
                    PushDebugBox(Solution, BoxBounds, Color);
                }
            }
#endif
        }
    }
}

internal void
UpdateLighting(lighting_solution *Solution, f32 FundamentalUnit,
               light_atlas *SpecAtlas, light_atlas *DiffuseAtlas,
               world *World, world_position SimOriginP, world_position OriginP,
               u16 OccluderCount, lighting_box *Occluders,
               platform_work_queue *LightingQueue, memory_arena *TempArena)
{
    TIMED_FUNCTION();

    Assert(SimOriginP.ChunkZ == 0);
    Assert(SimOriginP.Offset_.z == 0);
    Assert(OriginP.ChunkZ == 0);
    Assert(OriginP.Offset_.z == 0);

    temporary_memory TempMem = BeginTemporaryMemory(TempArena);

    //
    // NOTE(casey): See if any settings have changed (or if we've never been run before),
    // and initialize all our tables.
    //

    v3s AtlasCellCount = SpecAtlas->VoxelDim;
    if((Solution->FundamentalUnit != FundamentalUnit) ||
       !AreEqual(AtlasCellCount, Solution->AtlasGrid.CellCount))
    {
        Clear(&Solution->TableMemory);
        Solution->MaxDebugLineCount = 2*65536;
        Solution->DebugLines = PushArray(&Solution->TableMemory, Solution->MaxDebugLineCount, debug_line);

        Solution->SamplingSpheres = LightSamplingSphereTable;
        Solution->FundamentalUnit = FundamentalUnit;

        Solution->Series = RandomSeed(1234);
        Solution->UpdateDebugLines = true;
        Solution->AtlasToSpatialGridIndexOffset = V3S(1, 1, 0);

        v3 VoxCellDim = FundamentalUnit*V3(1, 1, 1);
        v3s SpatialCellCount = (AtlasCellCount/2) + 2*Solution->AtlasToSpatialGridIndexOffset;

        // TODO(casey): Let's tune this value in the future - maybe it should be more than 8?
        // Also, we may want to just have this be number of cells, and stop counting leaves,
        // because we may always want light gather to be a particular distance?
        Solution->MaxCostPerRay = 8;
        Solution->AtlasGrid = MakeVoxelGrid(AtlasCellCount, VoxCellDim);
        v3 SpatialCellDim = 2.0f*VoxCellDim;
        SpatialCellDim.z *= 3.0f;
        Solution->SpatialGrid = MakeVoxelGrid(SpatialCellCount, SpatialCellDim);

        Solution->SampleDirectionTable = PushArray(&Solution->TableMemory, ArrayCount(SampleDirectionTable), light_sample_direction);
        ComputeWalkTable(&Solution->TableMemory, Solution->SpatialGrid.CellCount,
                         Solution->MaxCostPerRay, Solution->SpatialGrid.CellDim,
                         ArrayCount(SampleDirectionTable), SampleDirectionTable,
                         Solution->SampleDirectionTable,
                         Solution->LightSamplingWalkTable);

        // TODO(casey): This should probably dynamically allocate the map just like
        // ComputeWalkTable!
        BuildDiffuseLightMaps(Solution, DiffuseAtlas, SpecAtlas);

        // 14889, 36
        // 16317, 42
        // 17912, 155
        // 17788, 325
        // 13586, 335
        // 717, 531
        Solution->DebugPick.Enabled = GRID_RAY_CAST_DEBUGGING;
        Solution->DebugPick.AtlasIndex = {10, 4, 1};
        Solution->DebugPick.OctahedronIndex = 0;
        Solution->DebugPick.Tx = 0;
        Solution->DebugPick.Ty = 0;
    }

    //
    // NOTE(casey): Update the alignment of the lighting voxels to the view
    //

    v3 VoxCellDim = Solution->AtlasGrid.CellDim;
    v3 InvVoxCellDim = Solution->AtlasGrid.InvCellDim;

    v3s VoxCameraOffset = RoundToV3S(InvVoxCellDim*OriginP.Offset_);
    VoxCameraOffset.y += 2;
    v3 CameraOffset = VoxCellDim*V3(VoxCameraOffset);
    OriginP.Offset_ = V3(0, 0, 0);

    v3s dVoxel = VoxCameraOffset - Solution->VoxCameraOffset;
    Solution->VoxCameraOffset = VoxCameraOffset;

    b32 CenterMoved = !AreInSameChunk(World, &Solution->LastOriginP, &OriginP);
    if(CenterMoved)
    {
        v3s VoxelCellsPerChunk =
        {
            RoundReal32ToInt32(World->ChunkDimInMeters.x * InvVoxCellDim.x),
            RoundReal32ToInt32(World->ChunkDimInMeters.y * InvVoxCellDim.y),
            RoundReal32ToInt32(World->ChunkDimInMeters.z * InvVoxCellDim.z),
        };

        v3s dChunk =
        {
            OriginP.ChunkX - Solution->LastOriginP.ChunkX,
            OriginP.ChunkY - Solution->LastOriginP.ChunkY,
            OriginP.ChunkZ - Solution->LastOriginP.ChunkZ,
        };

        dVoxel = dVoxel + VoxelCellsPerChunk*dChunk;
    }
    Solution->LastOriginP = OriginP;

    if(dVoxel.x || dVoxel.y || dVoxel.z)
    {
        BlockCopyAtlas(DiffuseAtlas, dVoxel);
        BlockCopyAtlas(SpecAtlas, dVoxel);
    }

    v3s HotCellCount = DiffuseAtlas->VoxelDim;

    v3 HotDim = V3(HotCellCount)*VoxCellDim;
    v3 AtlasMinCorner = Subtract(World, &OriginP, &SimOriginP) + CameraOffset - 0.5f*HotDim;
    AtlasMinCorner.xy += 0.5f*VoxCellDim.xy;

    // NOTE(casey): The spatial grid always has one of _its_ cells as an apron
    // around the actual atlas grid, which is a different resolution.
    v3 SpatialGridOffset = Solution->SpatialGrid.CellDim;
    SpatialGridOffset.z /= 3.0f;
    v3 SpatialMinCorner = (AtlasMinCorner - SpatialGridOffset);

    SetMinCorner(&Solution->AtlasGrid, AtlasMinCorner);
    SetMinCorner(&Solution->SpatialGrid, SpatialMinCorner);

    //
    // NOTE(casey): Determine the blend setup
    //

    if(Solution->UpdateDebugLines)
    {
        Solution->DebugLineCount = 0;
    }

    // NOTE(casey): From the linear blend coefficient tIrradiancePreservation
    // between the two successive frames of irradiance data, we "derive" two
    // coefficients, one for the multiplication at the end that renormalizes,
    // and one for the new ray contributions.  This is to avoid pre-multiplying
    // the existing probes in a first pass.
    Solution->tUpdateBlend = 15.0 / 60.0f;

    // NOTE(casey): When doing long-term accumulation for testing, we turn
    // off the linear blend and instead just set the coefficients for
    // long-term averaging.
    if(Solution->Accumulating)
    {
        if(Solution->AccumulationCount < LIGHT_TEST_ACCUMULATION_COUNT)
        {
            Solution->tUpdateBlend = 1.0f / LIGHT_TEST_ACCUMULATION_COUNT;
        }
        else
        {
            Solution->tUpdateBlend = 0.0f;
        }
    }

    //
    // NOTE(casey): Update voxel values
    //

    lighting_stats Stats = InternalLightingCore(Solution, SpecAtlas, DiffuseAtlas, LightDisableThreading ? 0 : LightingQueue, TempArena, OccluderCount, Occluders);

    // DebugDrawOctahedralValues(Solution, SpecAtlas, DiffuseAtlas);
    // DebugDrawSpatialGrid(Solution);

    //
    // NOTE(casey): Record statistics
    //

    f32 PartitionsPerCast = (f32)((f64)Stats.TotalPartitionsTested / (f64)Stats.TotalCastsInitiated);
    f32 LeavesPerCast = (f32)((f64)Stats.TotalLeavesTested / (f64)Stats.TotalCastsInitiated);
    f32 PartitionsPerLeaf = (f32)((f64)Stats.TotalPartitionsTested / (f64)Stats.TotalLeavesTested);

    u32 RaysPerProbe = 4*ArrayCount(Solution->SamplingSpheres[0].SampleDirection);

    VALIDATE_TEXEL_ATLAS(SpecAtlas);
    VALIDATE_TEXEL_ATLAS(DiffuseAtlas);

    {DEBUG_DATA_BLOCK("Lighting");
        DEBUG_UI_HUD(DevMode_lighting);

        DEBUG_B32(Global_Lighting_ShowProbes);

        DEBUG_VALUE(Stats.TotalCastsInitiated);
        DEBUG_VALUE(Stats.TotalPartitionsTested);
        DEBUG_VALUE(Stats.TotalPartitionLeavesUsed);
        DEBUG_VALUE(Stats.TotalLeavesTested);
        DEBUG_VALUE(PartitionsPerCast);
        DEBUG_VALUE(LeavesPerCast);
        DEBUG_VALUE(PartitionsPerLeaf);

        DEBUG_VALUE(RaysPerProbe);

        DEBUG_B32(LightBoxDumpTrigger);
    }

    Solution->FrameOdd = (Solution->FrameOdd ^ 1);

    EndTemporaryMemory(TempMem);
}

internal void
PushLightingRenderValues(lighting_solution *Solution, render_group *Group)
{
    {DEBUG_DATA_BLOCK("Lighting");
        DEBUG_UI_HUD(DevMode_lighting);

        DEBUG_VALUE(Solution->DebugPick.AtlasIndex.x);
        DEBUG_VALUE(Solution->DebugPick.AtlasIndex.y);
        DEBUG_VALUE(Solution->DebugPick.AtlasIndex.z);
        DEBUG_VALUE(Solution->DebugPick.OctahedronIndex);
        DEBUG_VALUE(Solution->DebugPick.Tx);
        DEBUG_VALUE(Solution->DebugPick.Ty);
            }

    rectangle3 HotVoxelRect = Solution->AtlasGrid.TotalBounds;
    v3 InvHotDim = 1.0f / GetDim(HotVoxelRect);

#if 1
    PushVolumeOutline(Group, HotVoxelRect, V4(0, 1, 1, 1));
    PushVolumeOutline(Group, Solution->SpatialGrid.TotalBounds, V4(0, 1, 0, 1));
#endif

    SetLightBounds(Group, HotVoxelRect.Min, InvHotDim);

    renderer_texture WhiteTexture = Group->WhiteTexture;

#if 0
    for(u32 SphereIndex = 0;
        SphereIndex < LIGHT_SAMPLING_SPHERE_COUNT;
        ++SphereIndex)
    {
        for(u32 BundleIndex = 0;
            BundleIndex < LIGHT_SAMPLING_RAY_BUNDLES_PER_SPHERE;
            ++BundleIndex)
        {
            for(u32 RayIndex = 0;
                RayIndex < 4;
                ++RayIndex)
            {
                v3 P = GetComponent(Solution->SamplingSpheres[SphereIndex].SampleDirection[BundleIndex], RayIndex);
                P.z += 3.0f;
                v3 Radius = {0.01f, 0.01f, 0.01f};
                PushCube(Group, WhiteTexture, P, Radius, GetDebugColor4(4*BundleIndex + RayIndex));
            }
        }
    }
#endif

    u32 DebugLineIndex = 0;
    while(DebugLineIndex < Solution->DebugLineCount)
    {
        u32 DebugLinesLeft = Solution->DebugLineCount - DebugLineIndex;
        u32 DebugLineBatch = Minimum(DebugLinesLeft, 4096);
        GetCurrentQuads(Group, DebugLineBatch, WhiteTexture);
        for(u32 DebugLineSubIndex = 0;
            DebugLineSubIndex < DebugLineBatch;
            ++DebugLineSubIndex)
        {
            debug_line *Line = Solution->DebugLines + DebugLineIndex + DebugLineSubIndex;
            v4 Color = Clamp01(Line->Color);
            PushLineSegment(Group, WhiteTexture, Line->FromP, Color, Line->ToP, Color, 0.01f);
        }
        DebugLineIndex += DebugLineBatch;
    }
}
