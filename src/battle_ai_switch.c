#include "global.h"
#include "battle.h"
#include "battle_ai_engine.h"
#include "battle_ai_main.h"
#include "battle_ai_script_commands.h"
#include "battle_ai_switch.h"
#include "battle_ai_util.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_util.h"
#include "pokemon.h"
#include "random.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"
#if AI_DEBUG_CHECKS
#include "gba/isagbprint.h"
#endif

// Switching ported from pokefirered-expansion's battle_ai_switch.c, trimmed to
// Gen 1-3 moves, abilities and items and to Gen 3 mechanics.
//
// Two decisions:
// - Should the mon on the field switch out (AI_TrySwitchWithLogic)? Level 5
//   uses the expansion AI's basic checks, level 6+ its smart switching.
// - Which party mon comes in (AI_GetMostSuitableMonToSwitchInto)? Level 5+ sends
//   in the mon that wins the matchup in single battles.
//
// The player's mon is judged by what the AI can see (AI_LOGIC_DATA and
// AI_GetMovesArray), never by the move or switch the player picked this turn.
//
// From level 7 the same checks also run in the player's place, to predict the
// player's switches (AI_WouldBattlerSwitch). The player's side then only uses
// the party mons, moves and abilities the AI knows of.

#define AI_DATA (AI_LOGIC_DATA)

// Tuning, same values as expansion's include/config/ai.h.
#define SHOULD_SWITCH_WONDER_GUARD_PERCENTAGE                   100
#define SHOULD_SWITCH_TRUANT_PERCENTAGE                         100
#define SHOULD_SWITCH_ALL_MOVES_BAD_PERCENTAGE                  100
#define STAY_IN_STATS_RAISED                                    2  // stat stages raised before the AI stays in
#define SHOULD_SWITCH_ABSORBS_MOVE_PERCENTAGE                   100
#define SHOULD_SWITCH_ABSORBS_HIDDEN_POWER_PERCENTAGE           50
#define SHOULD_SWITCH_TRAPPER_PERCENTAGE                        100
#define SHOULD_SWITCH_FREE_TURN_PERCENTAGE                      50
#define STAY_IN_ABSORBING_PERCENTAGE                            66 // chance to stay in with a super effective move instead of switching to an absorber
#define SHOULD_SWITCH_HASBADODDS_PERCENTAGE                     50
#define SHOULD_SWITCH_ENCORE_STATUS_PERCENTAGE                  100
#define SHOULD_SWITCH_ENCORE_DAMAGE_PERCENTAGE                  50
#define SHOULD_SWITCH_CHOICE_LOCKED_PERCENTAGE                  100
#define SHOULD_SWITCH_ATTACKING_STAT_MINUS_TWO_PERCENTAGE       50
#define SHOULD_SWITCH_ATTACKING_STAT_MINUS_THREE_PLUS_PERCENTAGE 100
#define SHOULD_SWITCH_ALL_SCORES_BAD_PERCENTAGE                 100
#define SHOULD_SWITCH_PERISH_SONG_PERCENTAGE                    100
#define SHOULD_SWITCH_YAWN_PERCENTAGE                           100
#define SHOULD_SWITCH_BADLY_POISONED_PERCENTAGE                 50
#define SHOULD_SWITCH_BADLY_POISONED_STATS_RAISED_PERCENTAGE    20
#define SHOULD_SWITCH_CURSED_PERCENTAGE                         50
#define SHOULD_SWITCH_CURSED_STATS_RAISED_PERCENTAGE            20
#define SHOULD_SWITCH_NIGHTMARE_PERCENTAGE                      33
#define SHOULD_SWITCH_NIGHTMARE_STATS_RAISED_PERCENTAGE         15
#define SHOULD_SWITCH_SEEDED_PERCENTAGE                         25
#define SHOULD_SWITCH_SEEDED_STATS_RAISED_PERCENTAGE            10
#define SHOULD_SWITCH_INFATUATION_PERCENTAGE                    100
#define SHOULD_SWITCH_NATURAL_CURE_STRONG_PERCENTAGE            66
#define SHOULD_SWITCH_NATURAL_CURE_STRONG_STATS_RAISED_PERCENTAGE 10
#define SHOULD_SWITCH_NATURAL_CURE_WEAK_PERCENTAGE              25
#define SHOULD_SWITCH_NATURAL_CURE_WEAK_STATS_RAISED_PERCENTAGE 10
#define AI_BAD_SCORE_THRESHOLD                                  90  // move scores at or below this are bad
#define AI_GOOD_SCORE_THRESHOLD                                 100 // move scores above this are good
#define AI_SWITCHIN_DAMAGE_THRESHOLD                            0   // damage a switch-in must exceed to count as a fallback
#define AI_DEFENSIVE_KO_THRESHOLD                               3   // hits a switch-in must take to count as defensive
#define AI_TYPE_MATCHUP_THRESHOLD                               AI_EFFECTIVENESS_2 // 2x is a neutral matchup in GetBattlerTypeMatchup
#define AI_WISH_HEAL_THRESHOLD                                  4   // Wish must heal at least 1/4 of max HP
#define CONFUSE_BERRY_HP_FRACTION                               2   // Gen 3 Figy Berry etc. trigger at 1/2 HP

struct IncomingHealInfo
{
    u16 healAmount;
    u8 wishCounter;
    bool8 hasHealing;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool32 HasSwitchFlag(u8 flag)
{
    return (BattleAI_GetSwitchFlags() & flag) != 0;
}

static u8 *GetAiMonToSwitchInto(u8 battler)
{
    return &gBattleStruct->AI_monToSwitchIntoId[GetBattlerPosition(battler) >> 1];
}

static bool32 SetSwitchinAndSwitch(u8 battler, u8 switchinId)
{
    // While predicting the player, only note the mon it expects to come in.
    if (AI_DATA->predictionInProgress)
    {
        if (switchinId != PARTY_SIZE)
            AI_DATA->mostSuitableMonId[battler] = switchinId;
        return TRUE;
    }
    *GetAiMonToSwitchInto(battler) = switchinId;
    return TRUE;
}

static bool32 IsValidForBattle(struct Pokemon *mon)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES_OR_EGG);

    return species != SPECIES_NONE && species != SPECIES_EGG && GetMonData(mon, MON_DATA_HP) != 0;
}

// A party mon that can come in and that the AI knows of (it has been on the
// field, for the player's mons below level 9).
static bool32 IsValidSwitchin(u8 battler, u8 monIndex)
{
    return IsValidForBattle(&AI_GetBattlerParty(battler)[monIndex]) && AI_CanSeePartyMon(battler, monIndex);
}

// The AI's mons on the field (both the same in single battles) and the
// opposing battler it plays against.
static u8 GetActiveBattlerIds(u8 battler, u8 *battlerIn1, u8 *battlerIn2)
{
    u8 opposingBattler;

    if (IsDoubleBattle())
    {
        *battlerIn1 = battler;
        if (gAbsentBattlerFlags & gBitTable[BATTLE_PARTNER(battler)])
            *battlerIn2 = battler;
        else
            *battlerIn2 = BATTLE_PARTNER(battler);
        opposingBattler = BATTLE_OPPOSITE(battler);
        if (gAbsentBattlerFlags & gBitTable[opposingBattler])
            opposingBattler ^= BIT_FLANK;
    }
    else
    {
        *battlerIn1 = battler;
        *battlerIn2 = battler;
        opposingBattler = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(battler)));
    }
    return opposingBattler;
}

static u8 GetOppositeBattler(u8 battler)
{
    u8 in1, in2;

    return GetActiveBattlerIds(battler, &in1, &in2);
}

static bool32 IsPartyMonOnFieldOrChosenToSwitch(u8 partyIndex, u8 battlerIn1, u8 battlerIn2)
{
    if (partyIndex == gBattlerPartyIndexes[battlerIn1] || partyIndex == gBattlerPartyIndexes[battlerIn2])
        return TRUE;
    // A switch the AI's partner already chose. The player's choice this turn
    // is never looked at.
    if (GetBattlerSide(battlerIn1) == B_SIDE_PLAYER)
        return FALSE;
    if (partyIndex == gBattleStruct->monToSwitchIntoId[battlerIn1] || partyIndex == gBattleStruct->monToSwitchIntoId[battlerIn2])
        return TRUE;
    return FALSE;
}

bool32 IsAceMon(u8 battler, u8 monPartyId)
{
    return HasSwitchFlag(AI_SWITCH_ACE_POKEMON) && GetBattlerSide(battler) == B_SIDE_OPPONENT
        && monPartyId == CalculateEnemyPartyCount() - 1;
}

static bool32 AreStatsRaised(u8 battler)
{
    u8 buffedStatsValue = 0;
    u8 i;

    for (i = 0; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[battler].statStages[i] > DEFAULT_STAT_STAGE)
            buffedStatsValue += gBattleMons[battler].statStages[i] - DEFAULT_STAT_STAGE;
    }
    return buffedStatsValue > STAY_IN_STATS_RAISED;
}

static bool32 AnyUsefulStatIsRaised(u8 battler)
{
    u8 i;

    for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[battler].statStages[i] <= DEFAULT_STAT_STAGE)
            continue;
        switch (i)
        {
        case STAT_ATK:
            if (HasMoveWithCategory(battler, DAMAGE_CATEGORY_PHYSICAL))
                return TRUE;
            break;
        case STAT_SPATK:
            if (HasMoveWithCategory(battler, DAMAGE_CATEGORY_SPECIAL))
                return TRUE;
            break;
        case STAT_SPEED:
            return TRUE;
        }
    }
    return FALSE;
}

// Choice Band locks the battler into the move it used last.
static bool32 AI_DoesChoiceEffectBlockMove(u8 battler, u16 move)
{
    return AI_DATA->lastUsedMove[battler] != MOVE_NONE
        && AI_DATA->lastUsedMove[battler] != move
        && AI_DATA->holdEffects[battler] == HOLD_EFFECT_CHOICE_BAND;
}

static bool32 IsAbilityPreventingEscape(u8 battler)
{
    u8 opposing = BATTLE_OPPOSITE(battler);

    if (AI_IsAbilityOnSide(opposing, ABILITY_SHADOW_TAG))
        return TRUE;
    if (AI_IsAbilityOnSide(opposing, ABILITY_ARENA_TRAP)
     && !IS_BATTLER_OF_TYPE(battler, TYPE_FLYING) && AI_DATA->abilities[battler] != ABILITY_LEVITATE)
        return TRUE;
    if (AI_IsAbilityOnSide(opposing, ABILITY_MAGNET_PULL) && IS_BATTLER_OF_TYPE(battler, TYPE_STEEL))
        return TRUE;
    return FALSE;
}

static bool32 CanBattlerConsiderSwitch(u8 battler)
{
    if (gBattleMons[battler].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION))
        return FALSE;
    if (gStatuses3[battler] & STATUS3_ROOTED)
        return FALSE;
    if (IsAbilityPreventingEscape(battler))
        return FALSE;
    return TRUE;
}

// Does the AI expect to KO the target this turn before it moves?
static bool32 AiExpectsToFaintPlayer(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    u16 *moves = AI_GetMovesArray(battler);
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || !AI_MoveSlotHasPP(battler, i))
            continue;
        if (CanIndexMoveFaintTarget(battler, opposingBattler, i, AI_ATTACKING)
         && AI_IsFaster(battler, opposingBattler, moves[i], GetIncomingMoveSpeedCheck(battler, opposingBattler), CONSIDER_PRIORITY))
            return TRUE;
    }
    return FALSE;
}

// Type effectiveness of a move on a party mon (AI_TypeCalc with abilities).
static uq4_12_t GetPartyMonTypeEffectiveness(u16 move, u8 moveType, u16 species, u8 ability)
{
    u32 effectiveness = AI_EFFECTIVENESS_1;
    u8 type1 = gSpeciesInfo[species].types[0];
    u8 type2 = gSpeciesInfo[species].types[1];
    s32 i = 0;

    if (ability == ABILITY_LEVITATE && moveType == TYPE_GROUND)
        return AI_EFFECTIVENESS_0;
    while (TYPE_EFFECT_ATK_TYPE(i) != TYPE_ENDTABLE)
    {
        if (TYPE_EFFECT_ATK_TYPE(i) == TYPE_FORESIGHT)
        {
            i += 3;
            continue;
        }
        if (TYPE_EFFECT_ATK_TYPE(i) == moveType)
        {
            if (TYPE_EFFECT_DEF_TYPE(i) == type1)
                effectiveness = effectiveness * TYPE_EFFECT_MULTIPLIER(i) / 10;
            if (TYPE_EFFECT_DEF_TYPE(i) == type2 && type1 != type2)
                effectiveness = effectiveness * TYPE_EFFECT_MULTIPLIER(i) / 10;
        }
        i += 3;
    }
    if (ability == ABILITY_WONDER_GUARD && gBattleMoves[move].power != 0 && effectiveness <= AI_EFFECTIVENESS_1)
        return AI_EFFECTIVENESS_0;
    return effectiveness;
}

// Sum of the two attacking types' effectiveness on the battler: 2x (1 + 1) is neutral.
static u32 GetBattlerTypeMatchup(u8 opposingBattler, u8 battler)
{
    uq4_12_t effectiveness1, effectiveness2;
    u8 atkType1 = gBattleMons[opposingBattler].type1;
    u8 atkType2 = gBattleMons[opposingBattler].type2;

    effectiveness1 = AI_GetTypeEffectiveness(atkType1, battler);
    if (effectiveness1 == 0)
        effectiveness1 = AI_EFFECTIVENESS_1 / 10; // immunity
    if (atkType2 != atkType1)
    {
        effectiveness2 = AI_GetTypeEffectiveness(atkType2, battler);
        if (effectiveness2 == 0)
            effectiveness2 = AI_EFFECTIVENESS_1 / 10;
    }
    else
    {
        effectiveness2 = effectiveness1;
    }
    return effectiveness1 + effectiveness2;
}

static bool32 CanUseSuperEffectiveMoveAgainstOpponent(u8 battler, u8 opposingBattler)
{
    u16 move;
    u8 i;

    if (!IsBattlerAlive(opposingBattler))
        return FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        move = AI_GetMovesArray(battler)[i];
        if (move == MOVE_NONE || AI_DoesChoiceEffectBlockMove(battler, move))
            continue;
        if (AI_DATA->effectiveness[battler][opposingBattler][i] >= AI_EFFECTIVENESS_2)
            return TRUE;
    }
    return FALSE;
}

static bool32 CanUseSuperEffectiveMoveAgainstOpponents(u8 battler)
{
    u8 opposingBattler = BATTLE_OPPOSITE(battler);

    if (CanUseSuperEffectiveMoveAgainstOpponent(battler, opposingBattler))
        return TRUE;
    if (IsDoubleBattle() && CanUseSuperEffectiveMoveAgainstOpponent(battler, BATTLE_PARTNER(opposingBattler)))
        return TRUE;
    return FALSE;
}

// The first mon (not the ace) with a damaging move at least this effective.
static bool32 FindMonWithMoveOfEffectiveness(u8 battler, u8 opposingBattler, uq4_12_t effectiveness)
{
    struct Pokemon *party = AI_GetBattlerParty(battler);
    u16 move;
    u8 monIndex, i;

    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidSwitchin(battler, monIndex) || monIndex == gBattlerPartyIndexes[battler] || IsAceMon(battler, monIndex))
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            move = AI_GetPartyMonMove(battler, monIndex, i);
            if (move != MOVE_NONE && gBattleMoves[move].power != 0
             && AI_GetMoveEffectiveness(move, battler, opposingBattler) >= effectiveness)
                return SetSwitchinAndSwitch(battler, monIndex);
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Should the mon on the field switch out?
// ---------------------------------------------------------------------------

static bool32 CanBattlerWin1v1(u32 hitsToKOAI, u32 hitsToKOPlayer, bool32 isBattlerFirst)
{
    if (hitsToKOAI == 0 && hitsToKOPlayer > 0)   // the player can't hurt the AI
        return TRUE;
    if (hitsToKOPlayer == 0)                     // the AI can't hurt the player
        return FALSE;
    if (isBattlerFirst)
        return hitsToKOAI >= hitsToKOPlayer;
    return hitsToKOAI > hitsToKOPlayer;
}

// Is the matchup bad enough to switch out? The checks run in full every time,
// so the AI's thinking time doesn't give away whether it will switch.
static bool32 ShouldSwitchIfHasBadOdds(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    u16 *playerMoves = AI_GetMovesArray(opposingBattler);
    u16 aiMove, playerMove, bestPlayerPriorityMove = MOVE_NONE, bestPlayerMove = MOVE_NONE, expectedMove;
    bool32 hasStatusMove = FALSE, hasSuperEffectiveMove = FALSE;
    u32 typeMatchup, hitsToKOAI, hitsToKOPlayer;
    u32 minHitsToKOAI = gBattleMons[battler].hp, minHitsToKOAIPriority = gBattleMons[battler].hp;
    bool32 canBattlerWin1v1 = FALSE, isBattlerFirst, isBattlerFirstPriority;
    u8 aiEffect, nonVolatileStatus, i;

    if (!HasSwitchFlag(AI_SWITCH_SMART_SWITCHING) || IsDoubleBattle())
        return FALSE;

    // The most damage the AI's mon can take.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        playerMove = playerMoves[i];
        if (playerMove != MOVE_NONE && !IsBattleMoveStatus(playerMove)
         && gBattleMoves[playerMove].effect != EFFECT_FOCUS_PUNCH && AI_MoveSlotHasPP(opposingBattler, i))
        {
            hitsToKOAI = GetNoOfHitsToKOBattler(opposingBattler, battler, i, AI_DEFENDING, CONSIDER_ENDURE);
            if (hitsToKOAI < minHitsToKOAI && !AI_DoesChoiceEffectBlockMove(opposingBattler, playerMove))
            {
                bestPlayerMove = playerMove;
                minHitsToKOAI = hitsToKOAI;
            }
            if (gBattleMoves[playerMove].priority > 0 && hitsToKOAI < minHitsToKOAIPriority
             && !AI_DoesChoiceEffectBlockMove(opposingBattler, playerMove))
            {
                bestPlayerPriorityMove = playerMove;
                minHitsToKOAIPriority = hitsToKOAI;
            }
        }
    }

    // Who moves first: against the predicted move when the AI predicts moves,
    // else against the player's strongest attack.
    expectedMove = (AI_DATA->knowledgeFlags & AI_PREDICT_MOVE) ? GetIncomingMove(battler, opposingBattler) : bestPlayerMove;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        aiMove = AI_GetMovesArray(battler)[i];
        if (aiMove == MOVE_NONE || !AI_MoveSlotHasPP(battler, i))
            continue;
        aiEffect = gBattleMoves[aiMove].effect;
        nonVolatileStatus = GetMoveNonVolatileStatus(aiMove);

        // A status move worth using even in a bad matchup.
        if (aiEffect == EFFECT_REFLECT || aiEffect == EFFECT_LIGHT_SCREEN || aiEffect == EFFECT_SPIKES
         || aiEffect == EFFECT_LEECH_SEED || IsExplosionMove(aiMove)
         || nonVolatileStatus == MOVE_EFFECT_SLEEP || nonVolatileStatus == MOVE_EFFECT_TOXIC
         || nonVolatileStatus == MOVE_EFFECT_PARALYSIS || nonVolatileStatus == MOVE_EFFECT_BURN
         || aiEffect == EFFECT_YAWN || aiEffect == EFFECT_TRICK || aiEffect == EFFECT_FAKE_OUT)
            hasStatusMove = TRUE;

        if (!IsBattleMoveStatus(aiMove) && !AI_DoesChoiceEffectBlockMove(battler, aiMove))
        {
            if (AI_DATA->effectiveness[battler][opposingBattler][i] >= AI_EFFECTIVENESS_2)
                hasSuperEffectiveMove = TRUE;

            hitsToKOPlayer = GetNoOfHitsToKOBattler(battler, opposingBattler, i, AI_ATTACKING, CONSIDER_ENDURE);
            if (!canBattlerWin1v1)
            {
                isBattlerFirst = AI_IsFaster(battler, opposingBattler, aiMove, expectedMove, CONSIDER_PRIORITY);
                isBattlerFirstPriority = AI_IsFaster(battler, opposingBattler, aiMove, bestPlayerPriorityMove, CONSIDER_PRIORITY);
                canBattlerWin1v1 = CanBattlerWin1v1(minHitsToKOAI, hitsToKOPlayer, isBattlerFirst)
                                && CanBattlerWin1v1(minHitsToKOAIPriority, hitsToKOPlayer, isBattlerFirstPriority);
            }
        }
    }

    typeMatchup = GetBattlerTypeMatchup(opposingBattler, battler);

    // It wins anyway, or there is nothing better to send in.
    if (canBattlerWin1v1 || AI_DATA->mostSuitableMonId[battler] == PARTY_SIZE)
        return FALSE;

    // It gets KO'd in one hit while still healthy.
    if (minHitsToKOAI == 1 && gBattleMons[battler].hp >= gBattleMons[battler].maxHP / 2)
    {
        if (RandomPercentage(100 - SHOULD_SWITCH_HASBADODDS_PERCENTAGE) && !AI_DATA->predictionInProgress)
            return FALSE; // stays in regardless half the time
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }

    // The player has the type advantage and the AI can't hit back hard.
    if (typeMatchup > AI_TYPE_MATCHUP_THRESHOLD && !hasSuperEffectiveMove
     && gBattleMons[battler].hp >= gBattleMons[battler].maxHP / 2)
    {
        if (hasStatusMove)
            return FALSE;
        if (RandomPercentage(100 - SHOULD_SWITCH_HASBADODDS_PERCENTAGE) && !AI_DATA->predictionInProgress)
            return FALSE;
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }
    return FALSE;
}

// Truant loafs every other turn: get out if the target Protects or digs through it.
static bool32 IsTruantMonVulnerable(u8 battlerAi, u8 opposingBattler)
{
    u16 *moves = AI_GetMovesArray(opposingBattler);
    u8 i, effect;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        effect = gBattleMoves[moves[i]].effect;
        if (effect == EFFECT_PROTECT && moves[i] != MOVE_ENDURE)
            return TRUE;
        if (effect == EFFECT_SEMI_INVULNERABLE && AI_IsSlower(battlerAi, opposingBattler, MOVE_NONE, MOVE_NONE, CONSIDER_PRIORITY))
            return TRUE;
    }
    return FALSE;
}

static bool32 ShouldSwitchIfTruant(u8 battler)
{
    if (AI_DATA->abilities[battler] == ABILITY_TRUANT
     && IsTruantMonVulnerable(battler, GetOppositeBattler(battler))
     && gDisableStructs[battler].truantCounter
     && gBattleMons[battler].hp >= gBattleMons[battler].maxHP / 2
     && AI_DATA->mostSuitableMonId[battler] != PARTY_SIZE
     && RandomPercentage(SHOULD_SWITCH_TRUANT_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    return FALSE;
}

// None of its moves can affect the target.
static bool32 ShouldSwitchIfAllMovesBad(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (AI_GetMovesArray(battler)[i] == MOVE_NONE)
            continue;
        if (AI_DATA->effectiveness[battler][opposingBattler][i] > AI_EFFECTIVENESS_0)
            return FALSE;
        if (IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(opposingBattler))
         && AI_DATA->effectiveness[battler][BATTLE_PARTNER(opposingBattler)][i] > AI_EFFECTIVENESS_0)
            return FALSE;
    }

    if (RandomPercentage(SHOULD_SWITCH_ALL_MOVES_BAD_PERCENTAGE))
    {
        if (AI_DATA->mostSuitableMonId[battler] == PARTY_SIZE) // no good switch-in: any mon that can deal damage
            return FindMonWithMoveOfEffectiveness(battler, opposingBattler, AI_EFFECTIVENESS_1);
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }
    return FALSE;
}

static bool32 ShouldSwitchIfWonderGuard(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    u8 i;

    if (IsDoubleBattle() || AI_DATA->abilities[opposingBattler] != ABILITY_WONDER_GUARD)
        return FALSE;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (AI_GetMovesArray(battler)[i] != MOVE_NONE && AI_DATA->effectiveness[battler][opposingBattler][i] >= AI_EFFECTIVENESS_2)
            return FALSE;
    }

    if (RandomPercentage(SHOULD_SWITCH_WONDER_GUARD_PERCENTAGE))
    {
        if (AI_DATA->mostSuitableMonId[battler] == PARTY_SIZE)
            return FindMonWithMoveOfEffectiveness(battler, opposingBattler, AI_EFFECTIVENESS_2);
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }
    return FALSE;
}

// Switch to a mon whose ability absorbs or blocks the move the AI expects:
// the predicted move (level 7+), else the move the target used last.
static bool32 FindMonThatAbsorbsOpponentsMove(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    u16 incomingMove = GetIncomingMove(battler, opposingBattler);
    u8 incomingType = GetBattleMoveType(incomingMove, opposingBattler);
    u8 absorbingAbilities[2];
    u8 numAbsorbingAbilities = 0;
    u8 battlerIn1, battlerIn2, monIndex, monAbility, i;

    if (!HasSwitchFlag(AI_SWITCH_SMART_SWITCHING) || incomingMove == MOVE_NONE)
        return FALSE;
    if (gBattleMoves[incomingMove].effect == EFFECT_HIDDEN_POWER && RandomPercentage(SHOULD_SWITCH_ABSORBS_HIDDEN_POWER_PERCENTAGE))
        return FALSE;
    // The AI's mon changed: the player's last move was aimed at another mon.
    // A predicted move is aimed at this one.
    if (AI_DATA->prevTurnSpecies[battler] != gBattleMons[battler].species && !(AI_DATA->knowledgeFlags & AI_PREDICT_MOVE))
        return FALSE;
    if (CanUseSuperEffectiveMoveAgainstOpponents(battler)
     && (RandomPercentage(STAY_IN_ABSORBING_PERCENTAGE) || AI_DATA->predictionInProgress))
        return FALSE;
    if (AreStatsRaised(battler))
        return FALSE;

    // Don't switch if it can KO.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u16 aiMove = AI_GetMovesArray(battler)[i];

        if (aiMove != MOVE_NONE && !IsBattleMoveStatus(aiMove) && !AI_DoesChoiceEffectBlockMove(battler, aiMove)
         && AI_GetDamage(battler, opposingBattler, i, AI_ATTACKING) > gBattleMons[opposingBattler].hp)
            return FALSE;
    }

    if (incomingType == TYPE_FIRE)
        absorbingAbilities[numAbsorbingAbilities++] = ABILITY_FLASH_FIRE;
    else if (incomingType == TYPE_WATER)
        absorbingAbilities[numAbsorbingAbilities++] = ABILITY_WATER_ABSORB;
    else if (incomingType == TYPE_ELECTRIC && gBattleMoves[incomingMove].power != 0)
        absorbingAbilities[numAbsorbingAbilities++] = ABILITY_VOLT_ABSORB;
    else if (incomingType == TYPE_GROUND && gBattleMoves[incomingMove].power != 0)
        absorbingAbilities[numAbsorbingAbilities++] = ABILITY_LEVITATE;
    if (IsSoundMove(incomingMove))
        absorbingAbilities[numAbsorbingAbilities++] = ABILITY_SOUNDPROOF;
    if (numAbsorbingAbilities == 0)
        return FALSE;

    for (i = 0; i < numAbsorbingAbilities; i++)
    {
        if (AI_DATA->abilities[battler] == absorbingAbilities[i])
            return FALSE; // already absorbs it
    }

    GetActiveBattlerIds(battler, &battlerIn1, &battlerIn2);
    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidSwitchin(battler, monIndex)
         || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2)
         || IsAceMon(battler, monIndex))
            continue;
        monAbility = AI_GetPartyMonAbility(battler, monIndex);
        for (i = 0; i < numAbsorbingAbilities; i++)
        {
            if (absorbingAbilities[i] == monAbility && RandomPercentage(SHOULD_SWITCH_ABSORBS_MOVE_PERCENTAGE))
                return SetSwitchinAndSwitch(battler, monIndex);
        }
    }
    return FALSE;
}

// The target is charging Solar Beam or is in the air / underground: a free turn to switch.
static bool32 ShouldSwitchIfOpponentChargingOrInvulnerable(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);

    if (IsDoubleBattle() || !HasSwitchFlag(AI_SWITCH_SMART_SWITCHING))
        return FALSE;
    if (!((gBattleMons[opposingBattler].status2 & STATUS2_MULTIPLETURNS)
          && IsTwoTurnNotSemiInvulnerableMove(opposingBattler, gLockedMoves[opposingBattler]))
     && !(gStatuses3[opposingBattler] & STATUS3_SEMI_INVULNERABLE))
        return FALSE;

    if (AI_DATA->mostSuitableMonId[battler] != PARTY_SIZE && RandomPercentage(SHOULD_SWITCH_FREE_TURN_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    return FALSE;
}

static bool32 AI_CanSwitchinAbilityTrapOpponent(u8 ability, u8 opposingBattler)
{
    if ((gBattleTypeFlags & BATTLE_TYPE_TRAINER) && CountUsablePartyMons(opposingBattler) == 0)
        return FALSE; // the target can't switch anyway
    if (ability == ABILITY_SHADOW_TAG)
        return TRUE;
    if (ability == ABILITY_ARENA_TRAP && !IS_BATTLER_OF_TYPE(opposingBattler, TYPE_FLYING)
     && AI_DATA->abilities[opposingBattler] != ABILITY_LEVITATE)
        return TRUE;
    if (ability == ABILITY_MAGNET_PULL && IS_BATTLER_OF_TYPE(opposingBattler, TYPE_STEEL))
        return TRUE;
    return FALSE;
}

// Switch to a trapping mon (Shadow Tag, Arena Trap, Magnet Pull) that wins the 1v1.
static bool32 ShouldSwitchIfTrapperInParty(u8 battler)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    u8 monIndex, monAbility;

    if (!HasSwitchFlag(AI_SWITCH_SMART_SWITCHING) || IsBattlerTrapped(battler, opposingBattler))
        return FALSE;

    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (IsAceMon(battler, monIndex) || !IsValidSwitchin(battler, monIndex))
            continue;
        monAbility = AI_GetPartyMonAbility(battler, monIndex);
        if (AI_CanSwitchinAbilityTrapOpponent(monAbility, opposingBattler)
         || (monAbility == ABILITY_TRACE && AI_CanSwitchinAbilityTrapOpponent(AI_DATA->abilities[opposingBattler], opposingBattler)))
        {
            // The best switch-in is a trapper that wins the 1v1.
            if (monIndex == AI_DATA->mostSuitableMonId[battler] && RandomPercentage(SHOULD_SWITCH_TRAPPER_PERCENTAGE))
                return SetSwitchinAndSwitch(battler, PARTY_SIZE);
        }
    }
    return FALSE;
}

static bool32 SwitchChanceWithStats(bool32 hasStatRaised, u32 chanceRaised, u32 chance)
{
    return RandomPercentage(hasStatRaised ? chanceRaised : chance);
}

static bool32 ShouldSwitchIfBadlyStatused(u8 battler)
{
    u8 monAbility = AI_DATA->abilities[battler];
    u8 holdEffect = AI_DATA->holdEffects[battler];
    u8 opposingBattler = GetOppositeBattler(battler);
    bool32 hasStatRaised = AnyUsefulStatIsRaised(battler);
    bool32 switchMon;

    // Perish Song is about to KO it.
    if ((gStatuses3[battler] & STATUS3_PERISH_SONG)
     && gDisableStructs[battler].perishSongTimer == 0
     && monAbility != ABILITY_SOUNDPROOF
     && RandomPercentage(SHOULD_SWITCH_PERISH_SONG_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);

    if (!HasSwitchFlag(AI_SWITCH_SMART_SWITCHING))
        return FALSE;

    // About to fall asleep from Yawn.
    if ((gStatuses3[battler] & STATUS3_YAWN)
     && AI_CanBeSlept(battler, monAbility)
     && gBattleMons[battler].hp > gBattleMons[battler].maxHP / 3
     && RandomPercentage(SHOULD_SWITCH_YAWN_PERCENTAGE))
    {
        switchMon = TRUE;
        if (AI_DATA->mostSuitableMonId[battler] == PARTY_SIZE)
            switchMon = FALSE;
        if (AiExpectsToFaintPlayer(battler)) // KO the target first
            switchMon = FALSE;
        if (monAbility == ABILITY_NATURAL_CURE || monAbility == ABILITY_SHED_SKIN || monAbility == ABILITY_EARLY_BIRD
         || holdEffect == HOLD_EFFECT_CURE_SLP || holdEffect == HOLD_EFFECT_CURE_STATUS
         || HasMoveWithEffect(battler, EFFECT_SLEEP_TALK) || HasMoveWithEffect(battler, EFFECT_SNORE))
            switchMon = FALSE;
        // High evasion may dodge until it wakes up.
        if (gBattleMons[battler].statStages[STAT_EVASION] > DEFAULT_STAT_STAGE + 3
         && AI_DATA->abilities[opposingBattler] != ABILITY_KEEN_EYE
         && !(gBattleMons[battler].status2 & STATUS2_FORESIGHT))
            switchMon = FALSE;
        if (switchMon)
            return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }

    // Damage at the end of every turn.
    if (!AiExpectsToFaintPlayer(battler) && AI_DATA->mostSuitableMonId[battler] != PARTY_SIZE)
    {
        if ((gBattleMons[battler].status1 & STATUS1_TOXIC_COUNTER) >= STATUS1_TOXIC_TURN(2)
         && gBattleMons[battler].hp >= gBattleMons[battler].maxHP / 3
         && SwitchChanceWithStats(hasStatRaised, SHOULD_SWITCH_BADLY_POISONED_STATS_RAISED_PERCENTAGE, SHOULD_SWITCH_BADLY_POISONED_PERCENTAGE))
            return SetSwitchinAndSwitch(battler, PARTY_SIZE);
        if ((gBattleMons[battler].status2 & STATUS2_CURSED)
         && SwitchChanceWithStats(hasStatRaised, SHOULD_SWITCH_CURSED_STATS_RAISED_PERCENTAGE, SHOULD_SWITCH_CURSED_PERCENTAGE))
            return SetSwitchinAndSwitch(battler, PARTY_SIZE);
        if ((gBattleMons[battler].status2 & STATUS2_NIGHTMARE)
         && SwitchChanceWithStats(hasStatRaised, SHOULD_SWITCH_NIGHTMARE_STATS_RAISED_PERCENTAGE, SHOULD_SWITCH_NIGHTMARE_PERCENTAGE))
            return SetSwitchinAndSwitch(battler, PARTY_SIZE);
        if ((gStatuses3[battler] & STATUS3_LEECHSEED)
         && SwitchChanceWithStats(hasStatRaised, SHOULD_SWITCH_SEEDED_STATS_RAISED_PERCENTAGE, SHOULD_SWITCH_SEEDED_PERCENTAGE))
            return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }

    if ((gBattleMons[battler].status2 & STATUS2_INFATUATION)
     && !AiExpectsToFaintPlayer(battler)
     && AI_DATA->mostSuitableMonId[battler] != PARTY_SIZE
     && RandomPercentage(SHOULD_SWITCH_INFATUATION_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);

    return FALSE;
}

// Natural Cure heals status on switching out.
static bool32 ShouldSwitchIfAbilityBenefit(u8 battler)
{
    bool32 hasStatRaised = AnyUsefulStatIsRaised(battler);

    if (AI_DATA->abilities[battler] != ABILITY_NATURAL_CURE || AI_DATA->mostSuitableMonId[battler] == PARTY_SIZE)
        return FALSE;

    if ((gBattleMons[battler].status1 & (STATUS1_SLEEP | STATUS1_FREEZE | STATUS1_TOXIC_POISON))
     && SwitchChanceWithStats(hasStatRaised, SHOULD_SWITCH_NATURAL_CURE_STRONG_STATS_RAISED_PERCENTAGE, SHOULD_SWITCH_NATURAL_CURE_STRONG_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    if ((gBattleMons[battler].status1 & STATUS1_ANY)
     && gBattleMons[battler].hp >= gBattleMons[battler].maxHP / 2
     && SwitchChanceWithStats(hasStatRaised, SHOULD_SWITCH_NATURAL_CURE_WEAK_STATS_RAISED_PERCENTAGE, SHOULD_SWITCH_NATURAL_CURE_WEAK_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    return FALSE;
}

// Basic AI: switch to a mon that resists the move that just hit, and has a
// super effective move.
static bool32 FindMonWithFlagsAndSuperEffective(u8 battler, u16 flags, u32 percentChance)
{
    u16 lastLandedMove = gLastLandedMoves[battler];
    u8 battlerIn1, battlerIn2, attacker, monIndex, i;
    uq4_12_t effectiveness;
    u16 resultFlags, move, species;

    if (HasSwitchFlag(AI_SWITCH_SMART_SWITCHING))
        return FALSE; // ShouldSwitchIfHasBadOdds does this better
    if (lastLandedMove == MOVE_NONE || lastLandedMove == MOVE_UNAVAILABLE || gLastHitBy[battler] == 0xFF)
        return FALSE;
    if (IsBattleMoveStatus(lastLandedMove))
        return FALSE;

    GetActiveBattlerIds(battler, &battlerIn1, &battlerIn2);
    attacker = gLastHitBy[battler];
    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidSwitchin(battler, monIndex)
         || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2)
         || IsAceMon(battler, monIndex))
            continue;

        species = GetMonData(&AI_GetBattlerParty(battler)[monIndex], MON_DATA_SPECIES);
        effectiveness = GetPartyMonTypeEffectiveness(lastLandedMove, gLastHitByType[battler], species, AI_GetPartyMonAbility(battler, monIndex));
        if (effectiveness == AI_EFFECTIVENESS_0)
            resultFlags = MOVE_RESULT_DOESNT_AFFECT_FOE;
        else if (effectiveness < AI_EFFECTIVENESS_1)
            resultFlags = MOVE_RESULT_NOT_VERY_EFFECTIVE;
        else
            resultFlags = 0;

        if (resultFlags & flags)
        {
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                move = AI_GetPartyMonMove(battler, monIndex, i);
                if (move != MOVE_NONE && gBattleMoves[move].power != 0
                 && AI_GetMoveEffectiveness(move, battler, attacker) >= AI_EFFECTIVENESS_2
                 && (RandomPercentage(percentChance) || AI_DATA->predictionInProgress))
                    return SetSwitchinAndSwitch(battler, monIndex);
            }
        }
    }
    return FALSE;
}

static u32 GetSwitchinHazardsDamage(u8 battler)
{
    u8 spikes = gSideTimers[GetBattlerSide(battler)].spikesAmount;
    u32 damage;

    // Gen 3 Spikes: 1/8, 1/6 or 1/4 of max HP to grounded mons.
    if (spikes == 0 || IS_BATTLER_OF_TYPE(battler, TYPE_FLYING) || AI_DATA->abilities[battler] == ABILITY_LEVITATE)
        return 0;
    damage = gBattleMons[battler].maxHP / ((5 - spikes) * 2);
    return damage == 0 ? 1 : damage;
}

// Don't switch a mon out if the next one would faint to Spikes coming back in
// and no one can spin them away.
static bool32 CanMonSurviveHazardSwitchin(u8 battler)
{
    u8 battlerIn1, battlerIn2, monIndex, i;

    if (GetSwitchinHazardsDamage(battler) <= gBattleMons[battler].hp)
        return TRUE;

    GetActiveBattlerIds(battler, &battlerIn1, &battlerIn2);
    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidSwitchin(battler, monIndex)
         || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2)
         || IsAceMon(battler, monIndex))
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (gBattleMoves[AI_GetPartyMonMove(battler, monIndex, i)].effect == EFFECT_RAPID_SPIN)
                return TRUE;
        }
    }
    return FALSE;
}

static bool32 ShouldSwitchIfEncored(u8 battler)
{
    u16 encoredMove = gDisableStructs[battler].encoredMove;
    u8 opposingBattler = GetOppositeBattler(battler);
    u8 moveIndex;

    if (!HasSwitchFlag(AI_SWITCH_SMART_SWITCHING) || encoredMove == MOVE_NONE)
        return FALSE;

    if (IsBattleMoveStatus(encoredMove) && RandomPercentage(SHOULD_SWITCH_ENCORE_STATUS_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    moveIndex = GetMoveIndex(battler, encoredMove);
    if (moveIndex < MAX_MON_MOVES && AI_DATA->effectiveness[battler][opposingBattler][moveIndex] >= AI_EFFECTIVENESS_2)
        return FALSE; // the encored move is good: stay in
    if ((RandomPercentage(SHOULD_SWITCH_ENCORE_DAMAGE_PERCENTAGE) || AI_DATA->predictionInProgress)
     && AI_DATA->mostSuitableMonId[battler] != PARTY_SIZE)
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    return FALSE;
}

// Choice Band locked into a status move or a move the target is immune to.
static bool32 ShouldSwitchIfBadChoiceLock(u8 battler)
{
    u16 choicedMove = gBattleStruct->choicedMove[battler];
    u8 opposingBattler = GetOppositeBattler(battler);
    u8 moveIndex;
    bool32 canAffect;

    if (choicedMove == MOVE_NONE || choicedMove == MOVE_UNAVAILABLE || AI_DATA->holdEffects[battler] != HOLD_EFFECT_CHOICE_BAND)
        return FALSE;
    moveIndex = GetMoveIndex(battler, choicedMove);
    if (moveIndex == MAX_MON_MOVES)
        return FALSE;

    canAffect = AI_DATA->effectiveness[battler][opposingBattler][moveIndex] > AI_EFFECTIVENESS_0;
    if (IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(opposingBattler)))
        canAffect |= AI_DATA->effectiveness[battler][BATTLE_PARTNER(opposingBattler)][moveIndex] > AI_EFFECTIVENESS_0;

    if ((IsBattleMoveStatus(choicedMove) || !canAffect) && RandomPercentage(SHOULD_SWITCH_CHOICE_LOCKED_PERCENTAGE))
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    return FALSE;
}

// Attacks with Attack more than Special Attack. The AI judges a mon it
// doesn't fully know by its species' base stats.
static bool32 IsPhysicalAttacker(u8 battler)
{
    u16 species = gBattleMons[battler].species;

    if (AI_KnowsBattler(battler))
        return gBattleMons[battler].attack > gBattleMons[battler].spAttack;
    return gSpeciesInfo[species].baseAttack > gSpeciesInfo[species].baseSpAttack;
}

// Its attacking stat was lowered so much it is setup fodder.
static bool32 ShouldSwitchIfAttackingStatsLowered(u8 battler)
{
    s8 stage;

    if (!HasSwitchFlag(AI_SWITCH_SMART_SWITCHING))
        return FALSE;

    if (IsPhysicalAttacker(battler))
        stage = gBattleMons[battler].statStages[STAT_ATK];
    else
        stage = gBattleMons[battler].statStages[STAT_SPATK];

    if (stage > DEFAULT_STAT_STAGE - 2)
        return FALSE;
    if (stage == DEFAULT_STAT_STAGE - 2)
    {
        if (AI_DATA->mostSuitableMonId[battler] != PARTY_SIZE
         && (RandomPercentage(SHOULD_SWITCH_ATTACKING_STAT_MINUS_TWO_PERCENTAGE) || AI_DATA->predictionInProgress))
            return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }
    else if (RandomPercentage(SHOULD_SWITCH_ATTACKING_STAT_MINUS_THREE_PLUS_PERCENTAGE))
    {
        return SetSwitchinAndSwitch(battler, PARTY_SIZE);
    }
    return FALSE;
}

// Expansion's ShouldSwitch. The order of the checks matters: some pick a
// specific mon, the rest send in the best switch-in.
static bool32 ShouldSwitch(u8 battler)
{
    u8 battlerIn1, battlerIn2, monIndex;
    s32 availableToSwitch = 0;

    if (!CanBattlerConsiderSwitch(battler))
        return FALSE;

    GetActiveBattlerIds(battler, &battlerIn1, &battlerIn2);
    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidForBattle(&AI_GetBattlerParty(battler)[monIndex]) // the party balls show how many are left
         || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2)
         || IsAceMon(battler, monIndex))
            continue;
        availableToSwitch++;
    }
    if (availableToSwitch == 0)
        return FALSE;

    if (ShouldSwitchIfWonderGuard(battler))
        return TRUE;
    if (HasSwitchFlag(AI_SWITCH_SMART_SWITCHING) && !CanMonSurviveHazardSwitchin(battler))
        return FALSE;
    if (ShouldSwitchIfTrapperInParty(battler))
        return TRUE;
    if (FindMonThatAbsorbsOpponentsMove(battler))
        return TRUE;
    if (ShouldSwitchIfOpponentChargingOrInvulnerable(battler))
        return TRUE;
    if (ShouldSwitchIfTruant(battler))
        return TRUE;
    if (ShouldSwitchIfAllMovesBad(battler))
        return TRUE;
    if (ShouldSwitchIfBadlyStatused(battler))
        return TRUE;
    if (ShouldSwitchIfAbilityBenefit(battler))
        return TRUE;
    if (ShouldSwitchIfHasBadOdds(battler))
        return TRUE;
    if (ShouldSwitchIfEncored(battler))
        return TRUE;
    if (ShouldSwitchIfBadChoiceLock(battler))
        return TRUE;
    if (ShouldSwitchIfAttackingStatsLowered(battler))
        return TRUE;

    // The basic AI also switches to a mon that resists the last hit.
    if (HasSwitchFlag(AI_SWITCH_SMART_SWITCHING))
        return FALSE;
    if (CanUseSuperEffectiveMoveAgainstOpponents(battler) || AreStatsRaised(battler))
        return FALSE;
    if (FindMonWithFlagsAndSuperEffective(battler, MOVE_RESULT_DOESNT_AFFECT_FOE, 50)
     || FindMonWithFlagsAndSuperEffective(battler, MOVE_RESULT_NOT_VERY_EFFECTIVE, 33))
        return TRUE;
    return FALSE;
}

// The move's score against the opposing battler, or in double battles the
// better of its scores against the two foes.
static s32 GetBestScoreAgainstFoes(u8 battler, u8 moveIndex)
{
    u8 opposingBattler = GetOppositeBattler(battler);
    s32 score = AI_DATA->finalScores[opposingBattler][moveIndex];

    if (IsDoubleBattle() && AI_DATA->finalScores[BATTLE_PARTNER(opposingBattler)][moveIndex] > score)
        score = AI_DATA->finalScores[BATTLE_PARTNER(opposingBattler)][moveIndex];
    return score;
}

// After move scoring: switch if every move is bad, stay in to Baton Pass.
static bool32 ShouldSwitchIfAllScoresBad(u8 battler)
{
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (GetBestScoreAgainstFoes(battler, i) > AI_BAD_SCORE_THRESHOLD)
            return FALSE;
    }
    return RandomPercentage(SHOULD_SWITCH_ALL_SCORES_BAD_PERCENTAGE);
}

static bool32 ShouldStayInToUseMove(u8 battler)
{
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMoves[AI_GetMovesArray(battler)[i]].effect == EFFECT_BATON_PASS
         && GetBestScoreAgainstFoes(battler, i) > AI_GOOD_SCORE_THRESHOLD)
            return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Which party mon should come in?
// ---------------------------------------------------------------------------

static void GetIncomingHealInfo(u8 battler, struct IncomingHealInfo *healInfo)
{
    healInfo->healAmount = 0;
    healInfo->wishCounter = gWishFutureKnock.wishCounter[battler];
    healInfo->hasHealing = FALSE;
    if (healInfo->wishCounter > 0) // Gen 3 Wish heals half of the max HP at the end of the turn
    {
        healInfo->hasHealing = TRUE;
        healInfo->healAmount = gBattleMons[battler].maxHP / 2;
    }
}

static s32 GetSwitchinWeatherImpact(u8 battler)
{
    u16 weather = AI_GetWeather();
    u8 ability = AI_DATA->abilities[battler];
    s32 maxHP = gBattleMons[battler].maxHP;
    s32 impact = 0;

    s32 sixteenth = (maxHP / 16 == 0) ? 1 : maxHP / 16;

    if ((weather & B_WEATHER_HAIL) && !IS_BATTLER_OF_TYPE(battler, TYPE_ICE))
        impact = sixteenth;
    else if ((weather & B_WEATHER_SANDSTORM) && ability != ABILITY_SAND_VEIL
          && !IS_BATTLER_OF_TYPE(battler, TYPE_ROCK) && !IS_BATTLER_OF_TYPE(battler, TYPE_GROUND) && !IS_BATTLER_OF_TYPE(battler, TYPE_STEEL))
        impact = sixteenth;
    if ((weather & B_WEATHER_RAIN) && ability == ABILITY_RAIN_DISH)
        impact = -sixteenth;
    return impact;
}

static u32 GetSwitchinRecurringHealing(u8 battler)
{
    u32 heal;

    if (AI_DATA->holdEffects[battler] != HOLD_EFFECT_LEFTOVERS)
        return 0;
    heal = gBattleMons[battler].maxHP / 16;
    return heal == 0 ? 1 : heal;
}

// One turn of status damage (Gen 3: burn and poison 1/8, Toxic n/16).
static u32 GetSwitchinStatusDamage(u8 battler)
{
    u32 status = gBattleMons[battler].status1;
    u32 maxHP = gBattleMons[battler].maxHP;
    u32 damage = 0;

    if (status & (STATUS1_BURN | STATUS1_POISON))
    {
        damage = maxHP / 8;
    }
    else if (status & STATUS1_TOXIC_POISON)
    {
        if ((status & STATUS1_TOXIC_COUNTER) != STATUS1_TOXIC_TURN(15))
            gBattleMons[battler].status1 += STATUS1_TOXIC_TURN(1); // this is the simulated candidate
        damage = (maxHP / 16 == 0 ? 1 : maxHP / 16) * ((gBattleMons[battler].status1 & STATUS1_TOXIC_COUNTER) >> 8);
    }
    if ((status & (STATUS1_BURN | STATUS1_POISON | STATUS1_TOXIC_POISON)) && damage == 0)
        damage = 1;
    return damage;
}

// Hits the switch-in survives, counting Spikes, weather, Leftovers, berries,
// status and Wish.
static u32 GetSwitchinHitsToKO(s32 damageTaken, u8 battler, const struct IncomingHealInfo *healInfo)
{
    u32 hazardDamage = GetSwitchinHazardsDamage(battler);
    s32 weatherImpact = GetSwitchinWeatherImpact(battler);
    u32 recurringHealing = GetSwitchinRecurringHealing(battler);
    u32 statusDamage = GetSwitchinStatusDamage(battler);
    u16 maxHP = gBattleMons[battler].maxHP;
    u8 holdEffect = AI_DATA->holdEffects[battler];
    u8 holdEffectParam = AI_DATA->holdEffectParams[battler];
    u8 weatherDuration = gWishFutureKnock.weatherDuration;
    bool32 usedSingleUseHealingItem = FALSE;
    bool32 applyWishNow = healInfo->hasHealing && healInfo->wishCounter == 1;
    s32 currentHP, singleUseItemHeal;
    u32 hitsToKO = 0;

    if (hazardDamage >= gBattleMons[battler].hp)
        return 1;
    currentHP = gBattleMons[battler].hp - hazardDamage;

    // No damage is being dealt.
    if (damageTaken + statusDamage <= recurringHealing || damageTaken + statusDamage == 0)
        return 0;

    while (currentHP > 0)
    {
        // Weather runs out (permanent weather has a duration of 0 too, but rarely matters here).
        if (weatherImpact != 0 && weatherDuration == 0)
            weatherImpact = 0;

        currentHP -= damageTaken;
        if (currentHP > 0)
            currentHP -= weatherImpact;

        // A berry that heals at half HP.
        if (currentHP > 0 && !usedSingleUseHealingItem)
        {
            singleUseItemHeal = 0;
            switch (holdEffect)
            {
            case HOLD_EFFECT_RESTORE_HP:
                if (currentHP < maxHP / 2)
                    singleUseItemHeal = holdEffectParam;
                break;
            case HOLD_EFFECT_CONFUSE_SPICY:
            case HOLD_EFFECT_CONFUSE_DRY:
            case HOLD_EFFECT_CONFUSE_SWEET:
            case HOLD_EFFECT_CONFUSE_BITTER:
            case HOLD_EFFECT_CONFUSE_SOUR:
                if (currentHP < maxHP / CONFUSE_BERRY_HP_FRACTION && holdEffectParam != 0)
                    singleUseItemHeal = maxHP / holdEffectParam == 0 ? 1 : maxHP / holdEffectParam;
                break;
            }
            if (singleUseItemHeal > 0)
            {
                currentHP += singleUseItemHeal;
                if (currentHP > maxHP)
                    currentHP = maxHP;
                usedSingleUseHealingItem = TRUE;
            }
        }

        if (currentHP > 0)
            currentHP = currentHP + recurringHealing - statusDamage;

        if (applyWishNow && currentHP > 0)
        {
            currentHP += healInfo->healAmount;
            if (currentHP > maxHP)
                currentHP = maxHP;
            applyWishNow = FALSE;
        }

        if (gBattleMons[battler].status1 & STATUS1_TOXIC_POISON)
            statusDamage = GetSwitchinStatusDamage(battler);
        if (weatherDuration != 0)
            weatherDuration--;
        hitsToKO++;
        if (hitsToKO >= 100)
            break;
    }
    return hitsToKO;
}

static s32 GetMaxDamagePlayerCouldDealToSwitchin(u8 battler, u8 opposingBattler, u16 *bestPlayerMove, bool32 priorityOnly)
{
    u16 *playerMoves = AI_GetMovesArray(opposingBattler);
    u16 choicedMove = gBattleStruct->choicedMove[opposingBattler];
    s32 damageTaken, maxDamageTaken = 0;
    u16 playerMove;
    u8 i;

    // A player choiced into a non-priority move can't use priority.
    if (priorityOnly && choicedMove != MOVE_NONE && choicedMove != MOVE_UNAVAILABLE && gBattleMoves[choicedMove].priority < 1)
        return 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        playerMove = playerMoves[i];
        if (playerMove == MOVE_NONE || IsBattleMoveStatus(playerMove)
         || gBattleMoves[playerMove].effect == EFFECT_FOCUS_PUNCH || !AI_MoveSlotHasPP(opposingBattler, i))
            continue;
        if (priorityOnly && gBattleMoves[playerMove].priority <= 0)
            continue;

        damageTaken = AI_GetDamage(opposingBattler, battler, i, AI_DEFENDING);
        if (playerMove == choicedMove && AI_DATA->holdEffects[opposingBattler] == HOLD_EFFECT_CHOICE_BAND)
        {
            *bestPlayerMove = playerMove; // a choiced player can only use this move
            return damageTaken;
        }
        if (damageTaken > maxDamageTaken)
        {
            maxDamageTaken = damageTaken;
            *bestPlayerMove = playerMove;
        }
    }
    return maxDamageTaken;
}

static bool32 CanSwitchinWin1v1(u32 hitsToKOAI, u32 hitsToKOPlayer, bool32 isSwitchinFirst, bool32 isFreeSwitch)
{
    if (hitsToKOAI == 0 && hitsToKOPlayer > 0)
        return TRUE;
    if (hitsToKOPlayer == 0)
        return FALSE;
    // A free switch (after a KO) needs to outspeed or take one extra hit.
    if (isFreeSwitch && (hitsToKOAI > hitsToKOPlayer || (hitsToKOAI == hitsToKOPlayer && isSwitchinFirst)))
        return TRUE;
    // Switching mid-battle, it takes a hit on the way in.
    if (hitsToKOAI > hitsToKOPlayer + 1 || (hitsToKOAI == hitsToKOPlayer + 1 && isSwitchinFirst))
        return TRUE;
    return FALSE;
}

// The last mon in party order among the candidates.
static u8 GetSwitchinCandidate(u32 candidateIds)
{
    s32 monIndex;

    for (monIndex = PARTY_SIZE - 1; monIndex >= 0; monIndex--)
    {
        if (candidateIds & (1u << monIndex))
            return monIndex;
    }
    return PARTY_SIZE;
}

// Expansion's GetBestMonIntegrated: sorts each party mon into categories
// (revenge killer, good type matchup, defensive, ...) and picks the best one.
static u8 GetBestMonIntegrated(u8 battler, u8 opposingBattler, u8 battlerIn1, u8 battlerIn2, u8 switchType)
{
    struct Pokemon *party = AI_GetBattlerParty(battler);
    struct IncomingHealInfo healInfo;
    u32 revengeKillerIds = 0, slowRevengeKillerIds = 0, fastThreatenIds = 0, slowThreatenIds = 0, damageMonIds = 0, generic1v1MonIds = 0;
    u32 batonPassIds = 0, typeMatchupIds = 0, typeMatchupEffectiveIds = 0, defensiveMonIds = 0, trapperIds = 0, healingCandidateIds = 0;
    u8 bestDefensiveMonId = PARTY_SIZE, bestTypeMatchupId = PARTY_SIZE, bestTypeMatchupEffectiveId = PARTY_SIZE, bestDamageMonId = PARTY_SIZE, bestHealGainId = PARTY_SIZE;
    u8 aceMonId = PARTY_SIZE, aceMonCount = 0;
    s32 playerMonHP = gBattleMons[opposingBattler].hp, maxDamageDealt = AI_SWITCHIN_DAMAGE_THRESHOLD, damageDealt, bestHealGain = 0, healGain;
    u16 aiMove, bestPlayerMove, bestPlayerPriorityMove;
    u32 hitsToKOAI, hitsToKOPlayer, hitsToKOAIPriority, maxHitsToKO = AI_DEFENSIVE_KO_THRESHOLD;
    u32 bestResist = AI_TYPE_MATCHUP_THRESHOLD, bestResistEffective = AI_TYPE_MATCHUP_THRESHOLD, typeMatchup;
    bool32 isFreeSwitch = (switchType == SWITCH_AFTER_KO);
    bool32 isSwitchinFirst, isSwitchinFirstPriority, canSwitchinWin1v1, anyMoveCanWin1v1;
    u32 validMonIds = 0, originalHp;
    void *backup;
    u8 monIndex, moveIndex;

    GetIncomingHealInfo(battler, &healInfo);
    backup = AI_BeginSwitchinSimulation(battler);
    if (backup == NULL)
        return PARTY_SIZE;

    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidSwitchin(battler, monIndex) || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2))
            continue;
        if (IsAceMon(battler, monIndex)) // save the ace for last
        {
            aceMonId = monIndex;
            aceMonCount++;
            continue;
        }
        validMonIds |= (1u << monIndex);

        AI_SetSwitchinCandidate(battler, &party[monIndex], monIndex);
        originalHp = gBattleMons[battler].hp;

        // Not wise to send in a Truant mon against Protect or Fly/Dig users.
        if (AI_DATA->abilities[battler] == ABILITY_TRUANT && IsTruantMonVulnerable(battler, opposingBattler))
            continue;

        bestPlayerMove = MOVE_NONE;
        bestPlayerPriorityMove = MOVE_NONE;
        hitsToKOAI = GetSwitchinHitsToKO(GetMaxDamagePlayerCouldDealToSwitchin(battler, opposingBattler, &bestPlayerMove, FALSE), battler, &healInfo);
        hitsToKOAIPriority = GetSwitchinHitsToKO(GetMaxDamagePlayerCouldDealToSwitchin(battler, opposingBattler, &bestPlayerPriorityMove, TRUE), battler, &healInfo);
        typeMatchup = GetBattlerTypeMatchup(opposingBattler, battler);
        anyMoveCanWin1v1 = FALSE;

        for (moveIndex = 0; moveIndex < MAX_MON_MOVES; moveIndex++)
        {
            if (!AI_MoveSlotHasPP(battler, moveIndex))
                continue;
            aiMove = AI_GetMovesArray(battler)[moveIndex];
            damageDealt = AI_GetDamage(battler, opposingBattler, moveIndex, AI_ATTACKING);
            hitsToKOPlayer = GetNoOfHitsToKOBattler(battler, opposingBattler, moveIndex, AI_ATTACKING, CONSIDER_ENDURE);

            // It must win the 1v1 with and without the player's priority moves.
            isSwitchinFirst = AI_IsFaster(battler, opposingBattler, aiMove, bestPlayerMove, CONSIDER_PRIORITY);
            isSwitchinFirstPriority = AI_IsFaster(battler, opposingBattler, aiMove, bestPlayerPriorityMove, CONSIDER_PRIORITY);
            canSwitchinWin1v1 = CanSwitchinWin1v1(hitsToKOAI, hitsToKOPlayer, isSwitchinFirst, isFreeSwitch)
                             && CanSwitchinWin1v1(hitsToKOAIPriority, hitsToKOPlayer, isSwitchinFirstPriority, isFreeSwitch);
            anyMoveCanWin1v1 |= canSwitchinWin1v1;

            // Baton Pass: can set up and pass without fainting.
            if (gBattleMoves[aiMove].effect == EFFECT_BATON_PASS && ((isSwitchinFirst && hitsToKOAI > 1) || hitsToKOAI > 2))
                batonPassIds |= (1u << monIndex);

            if (typeMatchup < AI_TYPE_MATCHUP_THRESHOLD && canSwitchinWin1v1)
            {
                typeMatchupIds |= (1u << monIndex);
                if (typeMatchup < bestResist)
                {
                    bestResist = typeMatchup;
                    bestTypeMatchupId = monIndex;
                }
            }

            if (hitsToKOAI > AI_DEFENSIVE_KO_THRESHOLD && canSwitchinWin1v1)
            {
                defensiveMonIds |= (1u << monIndex);
                if (hitsToKOAI > maxHitsToKO)
                {
                    maxHitsToKO = hitsToKOAI;
                    bestDefensiveMonId = monIndex;
                }
            }

            if (canSwitchinWin1v1)
                generic1v1MonIds |= (1u << monIndex);

            if (aiMove == MOVE_NONE || IsBattleMoveStatus(aiMove))
                continue;

            // Resists and has a super effective move.
            if (typeMatchup < AI_TYPE_MATCHUP_THRESHOLD && canSwitchinWin1v1
             && AI_DATA->effectiveness[battler][opposingBattler][moveIndex] >= AI_EFFECTIVENESS_2)
            {
                typeMatchupEffectiveIds |= (1u << monIndex);
                if (typeMatchup < bestResistEffective)
                {
                    bestResistEffective = typeMatchup;
                    bestTypeMatchupEffectiveId = monIndex;
                }
            }

            // An Explosion that doesn't KO doesn't count for revenge killing.
            if (IsExplosionMove(aiMove) && damageDealt < playerMonHP)
                continue;

            // The fallback: deals damage and isn't KO'd on the way in.
            if (damageDealt > AI_SWITCHIN_DAMAGE_THRESHOLD && ((isFreeSwitch && hitsToKOAI > 1) || hitsToKOAI > 2))
            {
                damageMonIds |= (1u << monIndex);
                if (damageDealt > maxDamageDealt)
                {
                    maxDamageDealt = damageDealt;
                    bestDamageMonId = monIndex;
                }
            }

            if (canSwitchinWin1v1)
            {
                if (damageDealt >= playerMonHP) // OHKO
                {
                    if (isSwitchinFirst)
                        revengeKillerIds |= (1u << monIndex);
                    else
                        slowRevengeKillerIds |= (1u << monIndex);
                }
                if (damageDealt >= (playerMonHP / 2 + playerMonHP % 2)) // 2HKO
                {
                    if (isSwitchinFirst)
                        fastThreatenIds |= (1u << monIndex);
                    else
                        slowThreatenIds |= (1u << monIndex);
                }
                if (AI_CanSwitchinAbilityTrapOpponent(AI_DATA->abilities[battler], opposingBattler)
                 || (AI_DATA->abilities[battler] == ABILITY_TRACE && AI_CanSwitchinAbilityTrapOpponent(AI_DATA->abilities[opposingBattler], opposingBattler)))
                    trapperIds |= (1u << monIndex);
            }
        }

        // Wish is about to heal whoever is on the field.
        if (healInfo.hasHealing)
        {
            healGain = healInfo.healAmount;
            if (healGain > (s32)(gBattleMons[battler].maxHP - originalHp))
                healGain = gBattleMons[battler].maxHP - originalHp;
            if (anyMoveCanWin1v1 && healGain > (s32)(gBattleMons[battler].maxHP / AI_WISH_HEAL_THRESHOLD) && healGain > bestHealGain)
            {
                healingCandidateIds |= (1u << monIndex);
                bestHealGain = healGain;
                bestHealGainId = monIndex;
            }
        }
    }

    AI_EndSwitchinSimulation(battler, backup);

    if (isFreeSwitch)
    {
        // Trapper > revenge killer > type matchup > Wish target > Baton Pass > best damage
        if (trapperIds != 0)               return GetSwitchinCandidate(trapperIds);
        if (revengeKillerIds != 0)         return GetSwitchinCandidate(revengeKillerIds);
        if (slowRevengeKillerIds != 0)     return GetSwitchinCandidate(slowRevengeKillerIds);
        if (fastThreatenIds != 0)          return GetSwitchinCandidate(fastThreatenIds);
        if (slowThreatenIds != 0)          return GetSwitchinCandidate(slowThreatenIds);
        if (typeMatchupEffectiveIds != 0)  return bestTypeMatchupEffectiveId;
        if (typeMatchupIds != 0)           return bestTypeMatchupId;
        if (healingCandidateIds != 0)      return bestHealGainId;
        if (batonPassIds != 0)             return GetSwitchinCandidate(batonPassIds);
        if (generic1v1MonIds != 0)         return GetSwitchinCandidate(generic1v1MonIds);
        if (damageMonIds != 0)             return bestDamageMonId;
    }
    else
    {
        // Trapper > type matchup > defensive > Wish target > Baton Pass
        if (trapperIds != 0)               return GetSwitchinCandidate(trapperIds);
        if (typeMatchupEffectiveIds != 0)  return bestTypeMatchupEffectiveId;
        if (typeMatchupIds != 0)           return bestTypeMatchupId;
        if (defensiveMonIds != 0)          return bestDefensiveMonId;
        if (healingCandidateIds != 0)      return bestHealGainId;
        if (batonPassIds != 0)             return GetSwitchinCandidate(batonPassIds);
        if (generic1v1MonIds != 0)         return GetSwitchinCandidate(generic1v1MonIds);
    }

    // Not required to switch and no good candidate.
    if (switchType == SWITCH_MID_BATTLE_OPTIONAL)
        return PARTY_SIZE;
    if (validMonIds != 0)
        return GetSwitchinCandidate(validMonIds);
    // The ace is the only mon left.
    if (aceMonId != PARTY_SIZE && CountUsablePartyMons(battler) <= aceMonCount)
        return aceMonId;
    return PARTY_SIZE;
}

// Expansion's GetBestMonVanilla, for double battles: Baton Pass, then type
// matchup with a super effective move, then the most damage. Keeps the ace for last.
static u8 GetBestMonVanilla(u8 battler, u8 opposingBattler, u8 battlerIn1, u8 battlerIn2, u8 switchType)
{
    struct Pokemon *party = AI_GetBattlerParty(battler);
    u32 validMonIds = 0, batonPassIds = 0, typeMatchupIds = 0, bestResist = AI_EFFECTIVENESS_2, typeMatchup, bestDamage = 0, aiDmg;
    u8 bestDamageId = PARTY_SIZE, aceMonId = PARTY_SIZE, aceMonCount = 0, monIndex, moveIndex;
    u16 aiMove;
    void *backup;

    backup = AI_BeginSwitchinSimulation(battler);
    if (backup == NULL)
        return PARTY_SIZE;

    for (monIndex = 0; monIndex < PARTY_SIZE; monIndex++)
    {
        if (!IsValidSwitchin(battler, monIndex) || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2))
            continue;
        if (IsAceMon(battler, monIndex))
        {
            aceMonId = monIndex;
            aceMonCount++;
            continue;
        }
        validMonIds |= (1u << monIndex);

        AI_SetSwitchinCandidate(battler, &party[monIndex], monIndex);
        if (AI_DATA->abilities[battler] == ABILITY_TRUANT && IsTruantMonVulnerable(battler, opposingBattler))
            continue;

        typeMatchup = GetBattlerTypeMatchup(opposingBattler, battler);
        for (moveIndex = 0; moveIndex < MAX_MON_MOVES; moveIndex++)
        {
            if (!AI_MoveSlotHasPP(battler, moveIndex))
                continue;
            aiMove = AI_GetMovesArray(battler)[moveIndex];
            if (gBattleMoves[aiMove].effect == EFFECT_BATON_PASS)
                batonPassIds |= (1u << monIndex);
            if (typeMatchup < bestResist && AI_DATA->effectiveness[battler][opposingBattler][moveIndex] >= AI_EFFECTIVENESS_2)
            {
                bestResist = typeMatchup;
                typeMatchupIds |= (1u << monIndex);
            }
            if (aiMove != MOVE_NONE && !IsBattleMoveStatus(aiMove))
            {
                aiDmg = AI_GetDamage(battler, opposingBattler, moveIndex, AI_ATTACKING);
                if (aiDmg > bestDamage)
                {
                    bestDamage = aiDmg;
                    bestDamageId = monIndex;
                }
            }
        }
    }

    AI_EndSwitchinSimulation(battler, backup);

    if (batonPassIds != 0)
        return GetSwitchinCandidate(batonPassIds);
    if (typeMatchupIds != 0)
        return GetSwitchinCandidate(typeMatchupIds);
    if (bestDamageId != PARTY_SIZE)
        return bestDamageId;
    if (switchType == SWITCH_MID_BATTLE_OPTIONAL)
        return PARTY_SIZE;
    if (validMonIds != 0)
        return GetSwitchinCandidate(validMonIds);
    if (aceMonId != PARTY_SIZE && CountUsablePartyMons(battler) <= aceMonCount)
        return aceMonId;
    return PARTY_SIZE;
}

static u8 AI_GetMostSuitableMonToSwitchInto(u8 battler, u8 switchType)
{
    u8 battlerIn1, battlerIn2, opposingBattler;

    // A mon the AI already chose (never the player's choice this turn).
    if (GetBattlerSide(battler) == B_SIDE_OPPONENT && gBattleStruct->monToSwitchIntoId[battler] != PARTY_SIZE)
        return gBattleStruct->monToSwitchIntoId[battler];

    opposingBattler = GetActiveBattlerIds(battler, &battlerIn1, &battlerIn2);
    if (HasSwitchFlag(AI_SWITCH_SMART_MON_CHOICES) && !IsDoubleBattle() && IsBattlerAlive(opposingBattler))
        return GetBestMonIntegrated(battler, opposingBattler, battlerIn1, battlerIn2, switchType);
    return GetBestMonVanilla(battler, opposingBattler, battlerIn1, battlerIn2, switchType);
}

// Any mon that can come in, keeping the ace for last.
static u8 GetFallbackSwitchin(u8 battler)
{
    u8 battlerIn1, battlerIn2, aceMonId = PARTY_SIZE;
    s32 monIndex;

    GetActiveBattlerIds(battler, &battlerIn1, &battlerIn2);
    for (monIndex = PARTY_SIZE - 1; monIndex >= 0; monIndex--)
    {
        if (!IsValidSwitchin(battler, monIndex) || IsPartyMonOnFieldOrChosenToSwitch(monIndex, battlerIn1, battlerIn2))
            continue;
        if (IsAceMon(battler, monIndex))
        {
            aceMonId = monIndex;
            continue;
        }
        return monIndex;
    }
    return aceMonId;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

bool32 AI_TrySwitchWithLogic(u8 battler)
{
    bool32 shouldSwitch, hasMoveScores = FALSE;
    u8 monId;

    if (AI_LOGIC_DATA == NULL)
        return FALSE;

    // Smart switching also looks at how good the mon's moves are, so score them
    // first (against both foes in a double battle).
    if (HasSwitchFlag(AI_SWITCH_SMART_SWITCHING))
    {
        BattleAI_SetupAIData();
        if (AI_THINKING_STRUCT->aiLogicFlags != 0)
        {
            BattleAI_ChooseMoveIndexWithLogic();
            hasMoveScores = TRUE;
        }
    }

    AI_SetLogicDataForTurn(battler);
    AI_SetupPredictions(battler);
    *GetAiMonToSwitchInto(battler) = PARTY_SIZE;
    AI_DATA->mostSuitableMonId[battler] = AI_GetMostSuitableMonToSwitchInto(battler, SWITCH_MID_BATTLE_OPTIONAL);
    shouldSwitch = ShouldSwitch(battler);

    if (hasMoveScores && CanBattlerConsiderSwitch(battler) && GetFallbackSwitchin(battler) != PARTY_SIZE
     && !IsAceMon(battler, GetFallbackSwitchin(battler)))
    {
        if (ShouldSwitchIfAllScoresBad(battler))
            shouldSwitch = TRUE;
        else if (ShouldStayInToUseMove(battler))
            shouldSwitch = FALSE;
    }

    AI_DATA->prevTurnSpecies[battler] = gBattleMons[battler].species;

    if (shouldSwitch)
    {
        monId = *GetAiMonToSwitchInto(battler);
        if (monId == PARTY_SIZE)
            monId = AI_DATA->mostSuitableMonId[battler];
        if (monId == PARTY_SIZE)
            monId = GetFallbackSwitchin(battler);
        if (monId != PARTY_SIZE && !IsAceMon(battler, monId))
        {
#if AI_DEBUG_CHECKS
            DebugPrintfLevel(MGBA_LOG_INFO, "AISWITCH out species=%d hp=%d in=%d species=%d",
                gBattleMons[battler].species, gBattleMons[battler].hp, monId, GetMonData(&gEnemyParty[monId], MON_DATA_SPECIES));
#endif
            *GetAiMonToSwitchInto(battler) = monId;
            gBattleStruct->monToSwitchIntoId[battler] = monId;
            BtlController_EmitTwoReturnValues(1, B_ACTION_SWITCH, 0);
            AI_ClearLogicData();
            return TRUE;
        }
        *GetAiMonToSwitchInto(battler) = PARTY_SIZE;
    }

    AI_ClearLogicData();
    return FALSE;
}

bool32 AI_WouldBattlerSwitch(u8 battler)
{
    AI_DATA->mostSuitableMonId[battler] = AI_GetMostSuitableMonToSwitchInto(battler, SWITCH_MID_BATTLE_OPTIONAL);
    return ShouldSwitch(battler);
}

u8 AI_ChooseMonToSendIn(u8 battler)
{
    u8 switchType, monId;

    if (AI_LOGIC_DATA == NULL)
        return PARTY_SIZE;

    switchType = (gBattleMons[battler].hp == 0) ? SWITCH_AFTER_KO : SWITCH_MID_BATTLE_FORCED;
    AI_SetLogicDataForTurn(battler);
    monId = AI_GetMostSuitableMonToSwitchInto(battler, switchType);
    AI_ClearLogicData();
    if (monId == PARTY_SIZE)
        monId = GetFallbackSwitchin(battler);
#if AI_DEBUG_CHECKS
    DebugPrintfLevel(MGBA_LOG_INFO, "AISENDIN type=%d in=%d species=%d vs=%d",
        switchType, monId, monId < PARTY_SIZE ? GetMonData(&gEnemyParty[monId], MON_DATA_SPECIES) : 0,
        gBattleMons[GetOppositeBattler(battler)].species);
#endif
    return monId;
}
