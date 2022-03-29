/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

internal b32
IsValid(brain_id ID)
{
    b32 Result = (ID.Value);
    return(Result);
}

inline void
MarkBrainActives(brain *Brain)
{
    u32 BrainFlags = 0;
    for(u32 SlotIndex = 0;
        SlotIndex < MAX_BRAIN_SLOT_COUNT;
        ++SlotIndex)
    {
        entity *Entity = GetEntityInSlot(Brain, SlotIndex);
        if(Entity)
        {
            BrainFlags |= Entity->Flags;
        }
    }

    if(BrainFlags & EntityFlag_Active)
    {
        Brain->Active = true;
        for(u32 SlotIndex = 0;
            SlotIndex < MAX_BRAIN_SLOT_COUNT;
            ++SlotIndex)
        {
            entity *Entity = GetEntityInSlot(Brain, SlotIndex);
            if(Entity)
            {
                Entity->Flags |= EntityFlag_Active;
            }
        }
    }
    else
    {
        Brain->Active = false;
    }
}

internal void
PDC(entity *Entity, v3 TargetP, f32 Kp = 10.0f, f32 Kd = 8.0f)
{
    Entity->ddP = Kp*(TargetP - Entity->P) - Kd*(Entity->dP);
}

internal void
ExecuteBrainHero(game_state *GameState, game_input *Input, sim_region *SimRegion, brain *Brain, r32 dt)
{
    controlled_hero ConHero_ = {};
    controlled_hero *ConHero = &ConHero_;
    brain_hero *Parts = &Brain->Hero;
    entity *Head = Parts->Head;

    v2 dSword = {};
    r32 dZ = 0.0f;
    b32 Exited = false;
    b32 DebugSpawn = false;
    b32 Attacked = false;
    b32 Dodging = false;
    f32 ClutchLevel = 0.0f;

    if(Input)
    {
        u32 ControllerIndex = Brain->ID.Value - ReservedBrainID_FirstHero;
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        ConHero = GameState->ControlledHeroes + ControllerIndex;

        ClutchLevel = Controller->ClutchMax;

        if(Controller->IsAnalog)
        {
            // NOTE(casey): Use analog movement tuning
            ConHero->ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
        }
        else
        {
            // NOTE(casey): Use digital movement tuning
            r32 Recenter = 0.5f;
            if(WasPressed(Controller->MoveUp))
            {
                ConHero->ddP.x = 0.0f;
                ConHero->ddP.y = 1.0f;
            }
            if(WasPressed(Controller->MoveDown))
            {
                ConHero->ddP.x = 0.0f;
                ConHero->ddP.y = -1.0f;
            }
            if(WasPressed(Controller->MoveLeft))
            {
                ConHero->ddP.x = -1.0f;
                ConHero->ddP.y = 0.0f;
            }
            if(WasPressed(Controller->MoveRight))
            {
                ConHero->ddP.x = 1.0f;
                ConHero->ddP.y = 0.0f;
            }

            if(!IsDown(Controller->MoveLeft) &&
               !IsDown(Controller->MoveRight))
            {
                ConHero->ddP.x = 0.0f;
                if(IsDown(Controller->MoveUp))
                {
                    ConHero->ddP.y = 1.0f;
                }
                if(IsDown(Controller->MoveDown))
                {
                    ConHero->ddP.y = -1.0f;
                }
            }

            if(!IsDown(Controller->MoveUp) &&
               !IsDown(Controller->MoveDown))
            {
                ConHero->ddP.y = 0.0f;
                if(IsDown(Controller->MoveLeft))
                {
                    ConHero->ddP.x = -1.0f;
                }
                if(IsDown(Controller->MoveRight))
                {
                    ConHero->ddP.x = 1.0f;
                }
            }

            if(WasPressed(Controller->Start))
            {
                DebugSpawn = true;
            }
        }

        if(Controller->ClutchMax > 0.5f)
        {
            Dodging = true;
        }

        if(Head && WasPressed(Controller->Start))
        {
            entity *ClosestHero = 0;
            real32 ClosestHeroDSq = Square(10.0f); // NOTE(casey): Ten meter maximum search!
            for(entity_iterator Iter = IterateAllEntities(SimRegion);
                Iter.Entity;
                Advance(&Iter))
            {
                entity *TestEntity = Iter.Entity;
                if((TestEntity->BrainID.Value != Head->BrainID.Value) &&
                   TestEntity->BrainID.Value)
                {
                    real32 TestDSq = LengthSq(TestEntity->P - Head->P);
                    if(ClosestHeroDSq > TestDSq)
                    {
                        ClosestHero = TestEntity;
                        ClosestHeroDSq = TestDSq;
                    }
                }
            }

            if(ClosestHero)
            {
                brain_id OldBrainID = Head->BrainID;
                brain_slot OldBrainSlot = Head->BrainSlot;
                Head->BrainID = ClosestHero->BrainID;
                Head->BrainSlot = ClosestHero->BrainSlot;
                ClosestHero->BrainID = OldBrainID;
                ClosestHero->BrainSlot = OldBrainSlot;
            }
        }

        if(Controller->ActionUp.EndedDown)
        {
            Attacked = true;
            dSword = V2(0.0f, 1.0f);
        }
        if(Controller->ActionDown.EndedDown)
        {
            Attacked = true;
            dSword = V2(0.0f, -1.0f);
        }
        if(Controller->ActionLeft.EndedDown)
        {
            Attacked = true;
            dSword = V2(-1.0f, 0.0f);
        }
        if(Controller->ActionRight.EndedDown)
        {
            Attacked = true;
            dSword = V2(1.0f, 0.0f);
        }

        if(WasPressed(Controller->Back))
        {
            Exited = true;
        }
    }

    entity *Glove = Parts->Glove;

    entity *Body = Parts->Body;
    if(Head && Body)
    {
        if(Attacked)
        {
            // TODO(casey): Stop with the StackOverflow "quality" here
            Head->FacingDirection = ATan2(dSword.y, dSword.x);
        }

        v3 ddPNormal = {};
        v3 ddP = V3(ConHero->ddP, 0);
        f32 ddPLength = LengthSq(ddP);
        if(ddPLength > 0.05f)
        {
            f32 NormalizeC = (1.0f / SquareRoot(ddPLength));
            ddPNormal = NormalizeC*ddP;
            if(ddPLength > 1.0f)
            {
                ddP *= NormalizeC;
            }
        }

        f32 InvClutch = (1.0f - ClutchLevel);
        Head->P = Body->P + 0.1f*ddP + V3(0, 0, 0.3f + 0.1f*InvClutch);

        b32x HopRequested =
            (!Dodging &&
             (Body->MovementMode == MovementMode_Planted) &&
             (ddPLength > 0.5f));
        if(HopRequested)
        {
            v3 dTarget = 1.25f*ddPNormal;
            if(AbsoluteValue(dTarget.x) > AbsoluteValue(dTarget.y))
            {
                dTarget.y = 0.0f;
            }
            else
            {
                dTarget.x = 0.0f;
            }

            v3 HopTargetP = Body->P + dTarget;
            traversable_reference Traversable;
            if(GetClosestTraversable(SimRegion, HopTargetP, &Traversable,
                                     TraversableSearch_ClippedZ))
            {
                if(!AreEqual(Traversable, Body->Occupying))
                {
                    Body->CameFrom = Body->Occupying;
                    if(TransactionalOccupy(SimRegion, Body->ID, &Body->Occupying, Traversable))
                    {
                        Body->tMovement = 0.0f;
                        Body->MovementMode = MovementMode_Hopping;
                    }
                }
            }
        }

        Body->FacingDirection = Head->FacingDirection;
        if(Body->MovementMode == MovementMode_Planted)
        {
            if(Head)
            {
                r32 HeadDistance = 0.0f;
                HeadDistance = Length(Head->P - Body->P);

                r32 MaxHeadDistance = 0.5f;
                r32 tHeadDistance = Clamp01MapToRange(0.0f, HeadDistance, MaxHeadDistance);
                Body->ddtBob = -20.0f*tHeadDistance;
            }
        }

        v3 HeadDelta = {};
        if(Head)
        {
            HeadDelta = Head->P - Body->P;
        }
        Body->FloorDisplace = (0.25f*HeadDelta).xy;

        // TODO(casey): Probably want this to be more of an "engaged bob target"
        f32 ClutchBob = 0.25f*InvClutch;
        if(Dodging)
        {
            Body->tBob = ClutchBob;
        }

        if(Glove)
        {
            f32 GloveFloatHeight = 0.5f;
            if(Attacked)
            {
                sound_id BloopID = GetBestMatchSoundFrom(GameState->Assets, GetTagHash(Asset_Audio, Tag_Bloop));
                PlaySound(&GameState->AudioState, BloopID);
            }

            Glove->MovementMode = MovementMode_Floating;
            Glove->FacingDirection = Body->FacingDirection;
            // TODO(casey): GloveDist is probably a upgradable thing
            f32 GloveDist = 2.0f;
            v3 TargetP = Body->P + GloveDist*NOZ(V3(dSword, 0)) + V3(0.6f*Arm2(Body->FacingDirection), GloveFloatHeight);
            PDC(Glove, TargetP, 200.0f, 8.0f);
        }
    }

    if(Exited)
    {
        DeleteEntity(SimRegion, Head);
        DeleteEntity(SimRegion, Body);
        ConHero->BrainID.Value = 0;
    }
}

internal void
ExecuteBrainSwitches(game_state *GameState, sim_region *SimRegion, brain *Brain, r32 dt)
{
    brain_switches *Switches = &Brain->Switches;

    b32 SwitchesSet = true;
    for(u32 TileIndex = 0;
            TileIndex < ArrayCount(Switches->Tiles);
            ++TileIndex)
    {
        entity *Tile = Switches->Tiles[TileIndex];
        if(Tile)
        {
            entity_traversable_point *Point = &Tile->Traversables[0];
            if(!AreEqual(Point->PrevOccupier, Point->Occupier))
            {
                if(IsValid(Point->Occupier))
                {
                    Tile->SwitchState = 1;
                    Tile->Pieces[0].Color = V4(0, 1, 1, 1);
                    for(u32 PieceIndex = 0;
                            PieceIndex < Tile->PieceCount;
                            ++PieceIndex)
                    {
                        if(Tile->Pieces[PieceIndex].Flags & PieceType_Light)
                        {
                            Tile->Pieces[PieceIndex].Color = V4(0, 1, 0, 10);
                        }
                    }
                }

                if(IsValid(Point->Occupier) &&
                   !IsValid(Point->PrevOccupier))
                {
                    Tile->P.z -= 0.25f;
                }

                if(!IsValid(Point->Occupier) &&
                    IsValid(Point->PrevOccupier))
                {
                    Tile->P.z += 0.25f;
                }

                // TODO(casey): Should this happen outside
                // in a final sweep, or... ?
                Point->PrevOccupier = Point->Occupier;
            }

            if(Tile->SwitchState == 0)
            {
                SwitchesSet = false;
            }
        }
    }

    if(SwitchesSet)
    {
        for(u32 UnlockIndex = 0;
            UnlockIndex < ArrayCount(Switches->Unlocks);
            ++UnlockIndex)
        {
            entity *Unlock = Switches->Unlocks[UnlockIndex];
            if(Unlock && IsValid(Unlock->Occupying))
            {
                traversable_reference NullSpace = {};
                TransactionalOccupy(SimRegion, Unlock->ID, &Unlock->Occupying, NullSpace);
                Unlock->P.z -= 2.0f;
            }
        }
    }
}

internal void
ExecuteBrainSnake(sim_region *SimRegion, random_series *Entropy, brain *Brain, r32 dt)
{
    brain_snake *Parts = &Brain->Snake;

    entity *Head = Parts->Segments[0];
    if(Head)
    {
        v3 Delta = {RandomBilateral(Entropy),
            RandomBilateral(Entropy),
            0.0f};
        if(AbsoluteValue(Delta.x) > AbsoluteValue(Delta.y))
        {
            Delta.y = 0.0f;
        }
        else
        {
            Delta.x = 0.0f;
        }
        Delta = NOZ(Delta);

        traversable_reference Traversable;
        if(GetClosestTraversable(SimRegion, Head->P + Delta, &Traversable))
        {
            if(Head->MovementMode == MovementMode_Planted)
            {
                if(!AreEqual(Traversable, Head->Occupying))
                {
                    traversable_reference LastOccupying = Head->Occupying;
                    Head->CameFrom = Head->Occupying;
                    if(TransactionalOccupy(SimRegion, Head->ID, &Head->Occupying, Traversable))
                    {
                        // TODO(casey): Blowing an ATan2 for these
                        // is nuts.  Make better!
                        Head->FacingDirection = ATan2(Delta.y, Delta.x);
                        Head->tMovement = 0.0f;
                        Head->MovementMode = MovementMode_Hopping;

                        for(u32 SegmentIndex = 1;
                            SegmentIndex < ArrayCount(Parts->Segments);
                            ++SegmentIndex)
                        {
                            entity *Segment = Parts->Segments[SegmentIndex];
                            if(Segment)
                            {
                                v3 SegDelta =
                                    GetSimSpaceTraversable(SimRegion, LastOccupying).P -
                                    GetSimSpaceTraversable(SimRegion, Segment->Occupying).P;

                                Segment->CameFrom = Segment->Occupying;
                                TransactionalOccupy(SimRegion, Segment->ID, &Segment->Occupying, LastOccupying);
                                LastOccupying = Segment->CameFrom;

                                Segment->FacingDirection = ATan2(SegDelta.y, SegDelta.x);
                                Segment->tMovement = 0.0f;
                                Segment->MovementMode = MovementMode_Hopping;
                            }
                        }
                    }
                }
            }
        }
    }
}

inline void
ExecuteBrain(game_state *GameState, random_series *Entropy, game_input *Input,
             sim_region *SimRegion, brain *Brain, r32 dt)
{
    switch(Brain->Type)
    {
        case Type_brain_hero:
        {
            ExecuteBrainHero(GameState, Input, SimRegion, Brain, dt);
        } break;

        case Type_brain_snake:
        {
            ExecuteBrainSnake(SimRegion, Entropy, Brain, dt);
        } break;

        case Type_brain_familiar:
        {
            brain_familiar *Parts = &Brain->Familiar;
            entity *Head = Parts->Head;
            if(Head)
            {
                b32 Blocked = true;
                Head->MovementMode = MovementMode_Floating;

                traversable_reference Traversable;
                if(GetClosestTraversable(SimRegion, Head->P, &Traversable))
                {
                    if(AreEqual(Traversable, Head->Occupying))
                    {
                        Blocked = false;
                    }
                    else
                    {
                        if(TransactionalOccupy(SimRegion, Head->ID, &Head->Occupying, Traversable))
                        {
                            Blocked = false;
                        }
                    }
                }

                v3 TargetP = GetSimSpaceTraversable(SimRegion, Head->Occupying).P;
                if(!Blocked)
                {
                    closest_entity Closest =
                        GetClosestEntityWithBrain(SimRegion, Head->P, Type_brain_hero);
                    if(Closest.Entity) //  && (ClosestHeroDSq > Square(3.0f)))
                    {
                        traversable_reference TargetTraversable;
                        if(GetClosestTraversableAlongRay(SimRegion, Head->P, NOZ(Closest.Delta),
                                                         Head->Occupying, &TargetTraversable))
                        {
                            if(!IsOccupied(SimRegion, TargetTraversable))
                            {
                                TargetP = Closest.Entity->P;
                            }
                        }
                    }
                }

                Head->ddP = 10.0f*(TargetP - Head->P) - 8.0f*(Head->dP);
            }
        } break;

        case Type_brain_floaty_thing_for_now:
        {
            // TODO(casey): Think about what this stuff actually should mean,
            // or does mean, or will mean?
            //Entity->P.z += 0.05f*Cos(Entity->tBob);
            //Entity->tBob += dt;
        } break;

        case Type_brain_monstar:
        {
            brain_monstar *Parts = &Brain->Monstar;
            entity *Body = Parts->Body;
            if(Body)
            {
                v3 Delta = {RandomBilateral(Entropy),
                    RandomBilateral(Entropy),
                    0.0f};
                traversable_reference Traversable;
                if(GetClosestTraversable(SimRegion, Body->P + Delta, &Traversable))
                {
                    if(Body->MovementMode == MovementMode_Planted)
                    {
                        if(!AreEqual(Traversable, Body->Occupying))
                        {
                            Body->CameFrom = Body->Occupying;
                            if(TransactionalOccupy(SimRegion, Body->ID, &Body->Occupying, Traversable))
                            {
                                Body->tMovement = 0.0f;
                                Body->MovementMode = MovementMode_Hopping;
                            }
                        }
                    }
                }
            }
        } break;

        case Type_brain_switches:
        {
            ExecuteBrainSwitches(GameState, SimRegion, Brain, dt);
        } break;

        InvalidDefaultCase;
    }
}