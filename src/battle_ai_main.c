#include "global.h"
#include "battle.h"
#include "battle_ai_engine.h"
#include "battle_ai_main.h"
#include "battle_ai_switch.h"
#include "battle_ai_util.h"
#include "battle_anim.h"
#include "battle_util.h"
#include "pokemon.h"
#include "random.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/moves.h"
#if AI_DEBUG_CHECKS
#include "gba/isagbprint.h"
#endif

// Move scoring ported from pokefirered-expansion's battle_ai_main.c, trimmed to
// the moves, abilities and items of Gen 1-3 and to Gen 3 mechanics (per-type
// physical/special split included).
//
// Every usable move starts at 100. Each AI_LOGIC_* step adds to or takes from
// the scores, and the highest score wins (ties are broken at random). Moves
// the AI can't use score 0.
//
// From level 7 the AI predicts the player's move and switches by scoring the
// player's side as if it were the AI, using only what it knows about it
// (AI_SetupPredictions). Without a prediction it expects the move the target
// used last, like the expansion AI without its prediction flags.

#define AI_DATA (AI_LOGIC_DATA)

#define ADJUST_SCORE(val)               \
    do                                  \
    {                                   \
        score += (val);                 \
    } while (0)

#define ADJUST_AND_RETURN_SCORE(val)    \
    do                                  \
    {                                   \
        score += (val);                 \
        return score;                   \
    } while (0)

#define RETURN_SCORE_MINUS(val)         \
    do                                  \
    {                                   \
        score -= (val);                 \
        return score;                   \
    } while (0)

#define RETURN_SCORE_PLUS(val)          \
    do                                  \
    {                                   \
        score += (val);                 \
        return score;                   \
    } while (0)

#define AI_SCORE_DEFAULT 100

typedef s32 (*AiScoreFunc)(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);

static s32 AI_CheckBadMove(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_TryToFaint(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_CheckViability(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_ForceSetupFirstTurn(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_Risky(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_TryTo2HKO(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_PreferBatonPass(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_HPAware(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_PredictSwitch(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static s32 AI_DoubleBattle(u8 battlerAtk, u8 battlerDef, u16 move, s32 score);
static void AI_CompareDamagingMoves(u8 battlerAtk, u8 battlerDef);

// Indexed by the bit number of AI_LOGIC_*.
static const AiScoreFunc sBattleAiFuncTable[] =
{
    AI_CheckBadMove,        // AI_LOGIC_CHECK_BAD_MOVE
    AI_TryToFaint,          // AI_LOGIC_TRY_TO_FAINT
    AI_CheckViability,      // AI_LOGIC_CHECK_VIABILITY
    AI_ForceSetupFirstTurn, // AI_LOGIC_FORCE_SETUP_FIRST_TURN
    AI_Risky,               // AI_LOGIC_RISKY
    AI_TryTo2HKO,           // AI_LOGIC_TRY_TO_2HKO
    AI_PreferBatonPass,     // AI_LOGIC_PREFER_BATON_PASS
    AI_DoubleBattle,        // AI_LOGIC_DOUBLE_BATTLE
    AI_HPAware,             // AI_LOGIC_HP_AWARE
    AI_PredictSwitch,       // AI_LOGIC_PREDICT_SWITCH
};

// Moves whose damage the AI can't simulate in advance: Counter and Mirror Coat
// return the damage taken this turn, Bide over two turns, Future Sight hits
// two turns later. Like the expansion AI, don't treat them as doing nothing.
static bool32 IsDamageUnknownEffect(u8 effect)
{
    switch (effect)
    {
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
    case EFFECT_BIDE:
    case EFFECT_FUTURE_SIGHT:
        return TRUE;
    default:
        return FALSE;
    }
}

static bool32 IsHighCritMove(u16 move)
{
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_HIGH_CRITICAL:
    case EFFECT_SKY_ATTACK:
    case EFFECT_BLAZE_KICK:
    case EFFECT_POISON_TAIL:
        return TRUE;
    default:
        return FALSE;
    }
}

static bool32 IsPinchBerryHoldEffect(u8 holdEffect)
{
    switch (holdEffect)
    {
    case HOLD_EFFECT_ATTACK_UP:
    case HOLD_EFFECT_DEFENSE_UP:
    case HOLD_EFFECT_SPEED_UP:
    case HOLD_EFFECT_SP_ATTACK_UP:
    case HOLD_EFFECT_SP_DEFENSE_UP:
    case HOLD_EFFECT_CRITICAL_UP:
    case HOLD_EFFECT_RANDOM_STAT_UP:
        return TRUE;
    default:
        return FALSE;
    }
}

// ---------------------------------------------------------------------------
// Choosing a move
// ---------------------------------------------------------------------------

// In double battles moves are scored against every other battler. Moves that
// only hit the foes aren't scored against the partner.
static bool32 ShouldConsiderMoveForBattler(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (battlerDef == BATTLE_PARTNER(battlerAtk) && (gBattleMoves[move].target & (MOVE_TARGET_BOTH | MOVE_TARGET_OPPONENTS_FIELD)))
        return FALSE;
    return TRUE;
}

// Runs the AI_LOGIC_* steps on the move slots in slotMask.
static void RunScoringSteps(u8 battlerAtk, u8 battlerDef, u8 slotMask)
{
    struct AiLogicData *aiData = AI_DATA;
    u16 flags = AI_THINKING_STRUCT->aiLogicFlags;
    u8 logicId, i;
    u16 move;

    for (logicId = 0; flags != 0 && logicId < ARRAY_COUNT(sBattleAiFuncTable); logicId++, flags >>= 1)
    {
        if (!(flags & 1))
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (!(slotMask & gBitTable[i]))
                continue;
            if (gBattleMons[battlerAtk].pp[i] == 0)
                move = MOVE_NONE;
            else
                move = gBattleMons[battlerAtk].moves[i];

            AI_THINKING_STRUCT->movesetIndex = i;
            AI_THINKING_STRUCT->moveConsidered = move;
            if (move != MOVE_NONE && aiData->moveScores[i] > 0 && ShouldConsiderMoveForBattler(battlerAtk, battlerDef, move))
                aiData->moveScores[i] = sBattleAiFuncTable[logicId](battlerAtk, battlerDef, move, aiData->moveScores[i]);
            else
                aiData->moveScores[i] = 0;
        }
    }
    AI_THINKING_STRUCT->movesetIndex = 0;
}

// Level 10, when it expects the target to switch: scores the moves against
// the mon it expects to come in. Pursuit hits the mon going out, so it is
// scored against that one.
static void RunScoringStepsAgainstSwitchin(u8 battlerAtk, u8 battlerDef)
{
    struct AiLogicData *aiData = AI_DATA;
    s32 scores[MAX_MON_MOVES];
    u8 switchin = aiData->predictedSwitchin[battlerDef];
    u8 pursuitSlots = 0;
    void *backup;
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMoves[gBattleMons[battlerAtk].moves[i]].effect == EFFECT_PURSUIT)
            pursuitSlots |= gBitTable[i];
    }

    backup = AI_BeginSwitchinSimulation(battlerDef);
    if (backup == NULL)
    {
        RunScoringSteps(battlerAtk, battlerDef, 0xF);
        return;
    }
    AI_SetSwitchinCandidate(battlerDef, &AI_GetBattlerParty(battlerDef)[switchin], switchin);
    RunScoringSteps(battlerAtk, battlerDef, 0xF & ~pursuitSlots);
    memcpy(scores, aiData->moveScores, sizeof(scores));
    AI_EndSwitchinSimulation(battlerDef, backup);
    memcpy(aiData->moveScores, scores, sizeof(scores));
    RunScoringSteps(battlerAtk, battlerDef, pursuitSlots);
}

// Scores the attacker's usable moves against one target, starting from
// startScores. Leaves the scores in moveScores and returns the best slot
// (ties at random) and its score.
static u8 ScoreMovesAgainstTarget(u8 battlerAtk, u8 battlerDef, const s8 *startScores, u8 usableMoves, s32 *bestScoreOut)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 consideredMoves[MAX_MON_MOVES];
    u8 numOfBestMoves, i;
    s32 bestScore;

    for (i = 0; i < MAX_MON_MOVES; i++)
        aiData->moveScores[i] = startScores[i];

    if ((aiData->knowledgeFlags & AI_PREDICT_INCOMING_MON) && IsBattlerPredictedToSwitch(battlerDef)
     && aiData->predictedSwitchin[battlerDef] < PARTY_SIZE)
        RunScoringStepsAgainstSwitchin(battlerAtk, battlerDef);
    else
        RunScoringSteps(battlerAtk, battlerDef, 0xF);

    if (AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_CHECK_VIABILITY)
        AI_CompareDamagingMoves(battlerAtk, battlerDef);

    for (i = 0; i < MAX_MON_MOVES; i++)
        aiData->finalScores[battlerDef][i] = (aiData->moveScores[i] < -999) ? -999 : (aiData->moveScores[i] > 999) ? 999 : aiData->moveScores[i];

    numOfBestMoves = 0;
    bestScore = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (!(usableMoves & gBitTable[i]))
            continue;
        if (numOfBestMoves == 0 || bestScore < aiData->moveScores[i])
        {
            numOfBestMoves = 1;
            bestScore = aiData->moveScores[i];
            consideredMoves[0] = i;
        }
        else if (bestScore == aiData->moveScores[i])
        {
            consideredMoves[numOfBestMoves++] = i;
        }
    }
    *bestScoreOut = bestScore;
    return consideredMoves[Random() % numOfBestMoves];
}

// Scores the attacker's moves and returns the slot to use. Moves start at
// AI_THINKING_STRUCT->score (100, or 0 if unusable). In single battles the
// target is battlerDef. In double battles, like the expansion AI, the moves
// are scored against every other battler and the best target wins; the
// partner is only picked for a move scoring at least 100.
static u8 ScoreAndChooseMove(u8 battlerAtk, u8 battlerDef, u8 *targetOut)
{
    struct AiLogicData *aiData = AI_DATA;
    s8 startScores[MAX_MON_MOVES];
    s32 targetScores[MAX_BATTLERS_COUNT][MAX_MON_MOVES];
    s32 bestPerTarget[MAX_BATTLERS_COUNT];
    u8 slotPerTarget[MAX_BATTLERS_COUNT];
    u8 bestTargets[MAX_BATTLERS_COUNT];
    u8 numBestTargets, target, slot, i;
    s32 bestScore;
    u8 usableMoves = 0;

    aiData->shouldConsiderExplosion = RandomPercentage(GetAIExplosionChanceFromHP(aiData->hpPercents[battlerAtk]));
    aiData->partnerMove = GetAllyChosenMove(battlerAtk);
    memset(aiData->finalScores, 0, sizeof(aiData->finalScores));

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        startScores[i] = AI_THINKING_STRUCT->score[i]; // 100, or 0 if the move can't be used
        if (startScores[i] > 0 && gBattleMons[battlerAtk].pp[i] != 0 && gBattleMons[battlerAtk].moves[i] != MOVE_NONE)
            usableMoves |= gBitTable[i];
    }
    if (usableMoves == 0) // never happens: the battle uses Struggle without asking the AI
        usableMoves = 1;

    if (!IsDoubleBattle())
    {
        target = battlerDef;
        slot = ScoreMovesAgainstTarget(battlerAtk, battlerDef, startScores, usableMoves, &bestScore);
    }
    else
    {
        for (target = 0; target < MAX_BATTLERS_COUNT; target++)
        {
            bestPerTarget[target] = -1;
            slotPerTarget[target] = 0;
            if (target == battlerAtk || target >= gBattlersCount || !IsBattlerAlive(target))
                continue;
            slotPerTarget[target] = ScoreMovesAgainstTarget(battlerAtk, target, startScores, usableMoves, &bestPerTarget[target]);
            memcpy(targetScores[target], aiData->moveScores, sizeof(aiData->moveScores));
            // Don't use a move on the partner unless it is good.
            if (target == BATTLE_PARTNER(battlerAtk) && bestPerTarget[target] < AI_SCORE_DEFAULT)
                bestPerTarget[target] = -1;
        }

        numBestTargets = 0;
        bestScore = -1;
        for (target = 0; target < MAX_BATTLERS_COUNT; target++)
        {
            if (target == battlerAtk || target >= gBattlersCount || !IsBattlerAlive(target))
                continue;
            if (numBestTargets == 0 || bestScore < bestPerTarget[target])
            {
                numBestTargets = 1;
                bestScore = bestPerTarget[target];
                bestTargets[0] = target;
            }
            else if (bestScore == bestPerTarget[target])
            {
                bestTargets[numBestTargets++] = target;
            }
        }
        if (numBestTargets == 0) // nobody else is left; the battle is about to end
        {
            target = battlerDef;
            slot = ScoreMovesAgainstTarget(battlerAtk, battlerDef, startScores, usableMoves, &bestScore);
        }
        else
        {
            target = bestTargets[Random() % numBestTargets];
            slot = slotPerTarget[target];
            memcpy(aiData->moveScores, targetScores[target], sizeof(aiData->moveScores));
        }
    }

    // Keep the vanilla score array in step for anything that reads it.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (aiData->moveScores[i] < 0)
            AI_THINKING_STRUCT->score[i] = 0;
        else if (aiData->moveScores[i] > 127)
            AI_THINKING_STRUCT->score[i] = 127;
        else
            AI_THINKING_STRUCT->score[i] = aiData->moveScores[i];
    }

    *targetOut = target;
    return slot;
}

u8 BattleAI_ChooseMoveIndexWithLogic(void)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 battlerAtk = gBattlerAttacker;
    u8 battlerDef = gBattlerTarget;
    u8 i, target;

    AI_SetLogicDataForTurn(battlerAtk);
    AI_SetupPredictions(battlerAtk);
#if AI_DEBUG_CHECKS
    for (i = 0; i < gBattlersCount; i++)
    {
        if (GetBattlerSide(i) == B_SIDE_PLAYER && IsBattlerAlive(i))
            DebugPrintfLevel(MGBA_LOG_INFO, "AIKNOWN turn=%d lvl=%d opp=%d species=%d known=%d,%d,%d,%d real=%d,%d,%d,%d ability=%d/%d item=%d/%d",
                gBattleResults.battleTurnCounter, AI_THINKING_STRUCT->aiLevel, i, gBattleMons[i].species,
                AI_GetMovesArray(i)[0], AI_GetMovesArray(i)[1], AI_GetMovesArray(i)[2], AI_GetMovesArray(i)[3],
                gBattleMons[i].moves[0], gBattleMons[i].moves[1], gBattleMons[i].moves[2], gBattleMons[i].moves[3],
                aiData->abilities[i], gBattleMons[i].ability, aiData->items[i], gBattleMons[i].item);
    }
#endif
    i = ScoreAndChooseMove(battlerAtk, battlerDef, &target);
    gBattlerTarget = target; // the battle controller sends this target with the move
#if AI_DEBUG_CHECKS
    DebugPrintfLevel(MGBA_LOG_INFO, "AISCORE atk=%d def=%d lvl=%d %d:%d %d:%d %d:%d %d:%d pick=%d",
        battlerAtk, target, AI_THINKING_STRUCT->aiLevel,
        gBattleMons[battlerAtk].moves[0], aiData->moveScores[0], gBattleMons[battlerAtk].moves[1], aiData->moveScores[1],
        gBattleMons[battlerAtk].moves[2], aiData->moveScores[2], gBattleMons[battlerAtk].moves[3], aiData->moveScores[3],
        gBattleMons[battlerAtk].moves[i]);
#endif
    AI_ClearLogicData();
    return i;
}

// ---------------------------------------------------------------------------
// Predicting the player (levels 7+)
// ---------------------------------------------------------------------------

// Chance that the AI acts on a predicted switch, as in the expansion AI. The
// AI always works out whether it expects a switch, so its thinking time doesn't
// give the prediction away.
#define PREDICT_SWITCH_CHANCE 50

// The move the AI would use in the player's place, with only the moves, item
// and ability it knows of (AI_BeginPlayAsBattler). MOVE_NONE if it knows of
// no usable move.
static u16 PredictMove(u8 battler, u8 battlerAi)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 usableMoves = 0;
    u8 i, target;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMons[battler].moves[i] != MOVE_NONE && gBattleMons[battler].pp[i] != 0
         && !(aiData->moveLimitations[battler] & gBitTable[i]))
        {
            AI_THINKING_STRUCT->score[i] = 100;
            usableMoves |= gBitTable[i];
        }
        else
        {
            AI_THINKING_STRUCT->score[i] = 0;
        }
    }
    if (usableMoves == 0)
        return MOVE_NONE;
    return gBattleMons[battler].moves[ScoreAndChooseMove(battler, battlerAi, &target)];
}

void AI_SetupPredictions(u8 battlerAi)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 flags = aiData->knowledgeFlags;
    u8 turn = gBattleResults.battleTurnCounter + 1;
    u16 predictedMove[MAX_BATTLERS_COUNT];
    u8 predictedSwitchin[MAX_BATTLERS_COUNT];
    u8 predictedSwitches = 0;
    u8 battler;
    void *backup;

    aiData->predictingMove = FALSE;
    aiData->predictingSwitch = FALSE;
    if (!(flags & (AI_PREDICT_MOVE | AI_PREDICT_SWITCH)) || aiData->predictionInProgress)
        return;

    // Predict once per turn, so every decision of the turn (both AI battlers
    // in a double battle) agrees.
    if (aiData->predictionTurn != turn)
    {
        for (battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
        {
            predictedMove[battler] = MOVE_NONE;
            predictedSwitchin[battler] = PARTY_SIZE;
        }
        for (battler = 0; battler < gBattlersCount; battler++)
        {
            if (GetBattlerSide(battler) != B_SIDE_PLAYER || !IsBattlerAlive(battler))
                continue;
            backup = AI_BeginPlayAsBattler(battler);
            if (backup == NULL)
                continue;
            aiData->predictionInProgress = TRUE;
            if (flags & AI_PREDICT_MOVE)
                predictedMove[battler] = PredictMove(battler, battlerAi);
            if (flags & AI_PREDICT_SWITCH)
            {
                if (AI_WouldBattlerSwitch(battler))
                    predictedSwitches |= gBitTable[battler];
                predictedSwitchin[battler] = aiData->mostSuitableMonId[battler];
            }
            AI_EndPlayAsBattler(backup); // also ends predictionInProgress
        }

        memcpy(aiData->predictedMove, predictedMove, sizeof(predictedMove));
        memcpy(aiData->predictedSwitchin, predictedSwitchin, sizeof(predictedSwitchin));
        aiData->predictedSwitches = predictedSwitches;
        aiData->predictSwitchRoll = RandomPercentage(PREDICT_SWITCH_CHANCE);
        aiData->predictionTurn = turn;
        aiData->predictionBattler = battlerAi;
#if AI_DEBUG_CHECKS
        for (battler = 0; battler < gBattlersCount; battler++)
        {
            if (GetBattlerSide(battler) == B_SIDE_PLAYER && IsBattlerAlive(battler))
                DebugPrintfLevel(MGBA_LOG_INFO, "AIPREDICT turn=%d opp=%d species=%d known=%d,%d,%d,%d move=%d switch=%d in=%d roll=%d",
                    turn - 1, battler, gBattleMons[battler].species,
                    AI_GetMovesArray(battler)[0], AI_GetMovesArray(battler)[1], AI_GetMovesArray(battler)[2], AI_GetMovesArray(battler)[3],
                    predictedMove[battler], (predictedSwitches & gBitTable[battler]) != 0, predictedSwitchin[battler],
                    aiData->predictSwitchRoll);
        }
#endif
    }

    aiData->predictingMove = (flags & AI_PREDICT_MOVE) != 0;
    aiData->predictingSwitch = (flags & AI_PREDICT_SWITCH) && aiData->predictSwitchRoll;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_CHECK_BAD_MOVE: lower the score of moves that fail or do nothing
// ---------------------------------------------------------------------------

static s32 AI_CheckBadMove(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 moveEffect = gBattleMoves[move].effect;
    u8 nonVolatileStatus = GetMoveNonVolatileStatus(move);
    u8 moveTarget = gBattleMoves[move].target;
    u8 moveIndex = AI_THINKING_STRUCT->movesetIndex;
    uq4_12_t effectiveness = aiData->effectiveness[battlerAtk][battlerDef][moveIndex];
    bool32 isBattle1v1 = IsBattle1v1();
    bool32 hasPartner = HasPartner(battlerAtk);
    u16 weather = AI_GetWeather();
    u16 predictedMove = GetIncomingMove(battlerAtk, battlerDef);
    u8 abilityAtk = aiData->abilities[battlerAtk];
    u8 abilityDef = aiData->abilities[battlerDef];
    u8 moveType = GetBattleMoveType(move, battlerAtk);
    u16 *aiMoves = gBattleMons[battlerAtk].moves;
    bool32 sureHit;
    u8 i;

    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    // The target is out of reach (Fly, Dig, Dive) and the AI moves first.
    sureHit = (gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS) && gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk;
    if (!(moveTarget & (MOVE_TARGET_USER | MOVE_TARGET_OPPONENTS_FIELD))
     && moveEffect != EFFECT_SEMI_INVULNERABLE
     && !sureHit
     && !AI_CanHitSemiInvulnerable(move, battlerDef)
     && AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY))
        RETURN_SCORE_MINUS(10);

    // Don't charge up a move when the target can KO the AI first.
    if (CanTargetFaintAi(battlerDef, battlerAtk) && IsTwoTurnNotSemiInvulnerableMove(battlerAtk, move))
        RETURN_SCORE_MINUS(10);

    // Don't set up into an expected Focus Punch.
    if (IsBattleMoveStatus(move)
     && nonVolatileStatus != MOVE_EFFECT_SLEEP
     && gBattleMoves[predictedMove].effect != EFFECT_FOCUS_PUNCH
     && BestDmgMoveHasEffect(battlerDef, battlerAtk, AI_DEFENDING, EFFECT_FOCUS_PUNCH)
     && RandomPercentage(STATUS_MOVE_FOCUS_PUNCH_CHANCE))
        RETURN_SCORE_MINUS(20);

    // A frozen target is thawed by Fire moves: prefer other moves unless super effective.
    if (moveType == TYPE_FIRE && !IsBattleMoveStatus(move) && effectiveness < AI_EFFECTIVENESS_2
     && (gBattleMons[battlerDef].status1 & STATUS1_FREEZE))
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (aiMoves[i] != MOVE_NONE && GetBattleMoveType(aiMoves[i], battlerAtk) != TYPE_FIRE)
            {
                ADJUST_SCORE(-1);
                break;
            }
        }
    }

    if (effectiveness == AI_EFFECTIVENESS_0)
        RETURN_SCORE_MINUS(20);

    if (IsExplosionMove(move) && !aiData->shouldConsiderExplosion)
        ADJUST_SCORE(-5);

    switch (moveEffect)
    {
    case EFFECT_VITAL_THROW: // moves last
        if (AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY) && aiData->hpPercents[battlerAtk] < 40)
            ADJUST_SCORE(-2);
        break;
    // stat raising effects
    case EFFECT_ATTACK_UP:
    case EFFECT_ATTACK_UP_2:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_ATK) || !HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_DEFENSE_UP:
    case EFFECT_DEFENSE_UP_2:
    case EFFECT_DEFENSE_CURL:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_DEF))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPEED_UP:
    case EFFECT_SPEED_UP_2:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPEED))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPECIAL_ATTACK_UP:
    case EFFECT_SPECIAL_ATTACK_UP_2:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPATK) || !HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_SPECIAL))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPECIAL_DEFENSE_UP:
    case EFFECT_SPECIAL_DEFENSE_UP_2:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPDEF))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_ACCURACY_UP:
    case EFFECT_ACCURACY_UP_2:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_ACC))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_EVASION_UP:
    case EFFECT_EVASION_UP_2:
    case EFFECT_MINIMIZE:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_EVASION))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_COSMIC_POWER:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_DEF))
            ADJUST_SCORE(-10);
        else if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPDEF))
            ADJUST_SCORE(-8);
        break;
    case EFFECT_BULK_UP:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_ATK) || !HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL))
            ADJUST_SCORE(-10);
        else if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_DEF))
            ADJUST_SCORE(-8);
        break;
    case EFFECT_CALM_MIND:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPATK))
            ADJUST_SCORE(-10);
        else if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPDEF))
            ADJUST_SCORE(-8);
        break;
    case EFFECT_DRAGON_DANCE:
        if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_ATK) || !HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL))
            ADJUST_SCORE(-10);
        else if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_SPEED))
            ADJUST_SCORE(-8);
        break;
    case EFFECT_CHARGE:
        if (gStatuses3[battlerAtk] & STATUS3_CHARGED_UP)
            ADJUST_SCORE(-20);
        else if (!HasMoveWithType(battlerAtk, TYPE_ELECTRIC))
            ADJUST_SCORE(-10);
        break;
    // stat lowering effects
    case EFFECT_ATTACK_DOWN:
    case EFFECT_ATTACK_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_ATK))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_DEFENSE_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_DEF))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPEED_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_SPEED))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_SPATK))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_SPDEF))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_ACCURACY_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_ACC))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_EVASION_DOWN:
    case EFFECT_EVASION_DOWN_2:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_EVASION))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_TICKLE:
        if (!CanLowerStat(battlerAtk, battlerDef, STAT_ATK))
            ADJUST_SCORE(-10);
        else if (!CanLowerStat(battlerAtk, battlerDef, STAT_DEF))
            ADJUST_SCORE(-8);
        break;
    // other
    case EFFECT_HAZE:
    case EFFECT_PSYCH_UP:
        for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
        {
            if (gBattleMons[battlerAtk].statStages[i] > DEFAULT_STAT_STAGE
             || (hasPartner && gBattleMons[BATTLE_PARTNER(battlerAtk)].statStages[i] > DEFAULT_STAT_STAGE))
                ADJUST_SCORE(-10); // don't reset our boosts
        }
        for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
        {
            if (gBattleMons[battlerDef].statStages[i] < DEFAULT_STAT_STAGE
             || (HasPartner(battlerDef) && gBattleMons[BATTLE_PARTNER(battlerDef)].statStages[i] < DEFAULT_STAT_STAGE))
                ADJUST_SCORE(-10); // don't reset (or copy) the target's drops
        }
        break;
    case EFFECT_PRESENT:
    case EFFECT_FOCUS_PUNCH:
        // Risky to use while the target can hit back.
        if (HasDamagingMove(battlerDef)
         && !((gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE)
           || IsBattlerIncapacitated(battlerDef, abilityDef)
           || (gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION))))
            ADJUST_SCORE(-10);
        if (HasMoveWithEffect(battlerAtk, EFFECT_SUBSTITUTE) && !(gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE))
            ADJUST_SCORE(-10);
        if (HasNonVolatileMoveEffect(battlerAtk, MOVE_EFFECT_SLEEP) && !(gBattleMons[battlerDef].status1 & STATUS1_SLEEP))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
        if (IsBattlerIncapacitated(battlerDef, abilityDef)
         || (gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION)))
            ADJUST_SCORE(-1);
        if ((predictedMove == MOVE_NONE || IsBattleMoveStatus(predictedMove)
          || DoesSubstituteBlockMove(battlerAtk, BATTLE_PARTNER(battlerDef), predictedMove))
         && !(predictedMove == MOVE_NONE && (AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_RISKY))) // risky AI guesses from stats
            ADJUST_SCORE(-10);
        break;
    case EFFECT_ROAR:
        if (CountUsablePartyMons(battlerDef) == 0)
            ADJUST_SCORE(-10);
        else if (abilityDef == ABILITY_SUCTION_CUPS)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_LIGHT_SCREEN:
        if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_LIGHTSCREEN)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_REFLECT:
        if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_REFLECT)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_OHKO:
        if (!ShouldTryOHKO(battlerAtk, battlerDef, abilityAtk, abilityDef, move))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_MIST:
        if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_MIST)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_FOCUS_ENERGY:
        if (gBattleMons[battlerAtk].status2 & STATUS2_FOCUS_ENERGY)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_CONFUSE:
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
    case EFFECT_TEETER_DANCE:
        if (!AI_CanConfuse(battlerAtk, battlerDef, abilityDef, move))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SUBSTITUTE:
        if (gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE)
            ADJUST_SCORE(-8);
        else if (aiData->hpPercents[battlerAtk] <= 25)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_LEECH_SEED:
        if ((gStatuses3[battlerDef] & STATUS3_LEECHSEED) || IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS))
            ADJUST_SCORE(-10);
        else if (abilityDef == ABILITY_LIQUID_OOZE)
            ADJUST_SCORE(-3);
        break;
    case EFFECT_DISABLE:
        if (gDisableStructs[battlerDef].disabledMove == MOVE_NONE)
        {
            if (AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY))
            {
                if (aiData->lastUsedMove[battlerDef] == MOVE_NONE)
                    ADJUST_SCORE(-10); // nothing to disable
            }
            else if (predictedMove == MOVE_NONE)
            {
                ADJUST_SCORE(-10);
            }
        }
        else
        {
            ADJUST_SCORE(-10);
        }
        break;
    case EFFECT_ENCORE:
        if (gDisableStructs[battlerDef].encoredMove == MOVE_NONE)
        {
            if (AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY))
            {
                if (aiData->lastUsedMove[battlerDef] == MOVE_NONE)
                    ADJUST_SCORE(-10); // nothing to encore
            }
            else if (predictedMove == MOVE_NONE)
            {
                ADJUST_SCORE(-10);
            }
        }
        else
        {
            ADJUST_SCORE(-10);
        }
        break;
    case EFFECT_SNORE:
    case EFFECT_SLEEP_TALK:
        if (IsWakeupTurn(battlerAtk) || !AI_IsBattlerAsleep(battlerAtk))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_MEAN_LOOK:
        if (IsBattlerTrapped(battlerAtk, battlerDef))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_NIGHTMARE:
        if (gBattleMons[battlerDef].status2 & STATUS2_NIGHTMARE)
            ADJUST_SCORE(-10);
        else if (!AI_IsBattlerAsleep(battlerDef))
            ADJUST_SCORE(-8);
        break;
    case EFFECT_CURSE:
        if (IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GHOST))
        {
            if (gBattleMons[battlerDef].status2 & STATUS2_CURSED)
                ADJUST_SCORE(-10);
            else if (aiData->hpPercents[battlerAtk] <= 50)
                ADJUST_SCORE(-6);
        }
        else
        {
            if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_ATK) || !HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL))
                ADJUST_SCORE(-10);
            else if (!BattlerStatCanRise(battlerAtk, abilityAtk, STAT_DEF))
                ADJUST_SCORE(-8);
        }
        break;
    case EFFECT_SPIKES:
        if (gSideTimers[GetBattlerSide(battlerDef)].spikesAmount >= 3)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_FORESIGHT:
        if (gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT)
            ADJUST_SCORE(-10);
        else if (gBattleMons[battlerDef].statStages[STAT_EVASION] <= DEFAULT_STAT_STAGE - 2
              || !IS_BATTLER_OF_TYPE(battlerDef, TYPE_GHOST))
            ADJUST_SCORE(-9);
        break;
    case EFFECT_PERISH_SONG:
        if (!isBattle1v1)
        {
            if (CountUsablePartyMons(battlerAtk) == 0
             && abilityAtk != ABILITY_SOUNDPROOF
             && CountUsablePartyMons(battlerDef) >= 1
             && (!IsBattlerAlive(BATTLE_PARTNER(battlerAtk)) || aiData->abilities[BATTLE_PARTNER(battlerAtk)] != ABILITY_SOUNDPROOF))
            {
                ADJUST_SCORE(-10); // don't wipe our own team if we'll lose
            }
            else if ((!IsBattlerAlive(LEFT_FOE(battlerAtk)) || aiData->abilities[LEFT_FOE(battlerAtk)] == ABILITY_SOUNDPROOF
                   || (gStatuses3[LEFT_FOE(battlerAtk)] & STATUS3_PERISH_SONG))
                  && (!IsBattlerAlive(RIGHT_FOE(battlerAtk)) || aiData->abilities[RIGHT_FOE(battlerAtk)] == ABILITY_SOUNDPROOF
                   || (gStatuses3[RIGHT_FOE(battlerAtk)] & STATUS3_PERISH_SONG)))
            {
                ADJUST_SCORE(-10); // both foes are already counting down
            }
        }
        else
        {
            if (CountUsablePartyMons(battlerAtk) == 0 && abilityAtk != ABILITY_SOUNDPROOF && CountUsablePartyMons(battlerDef) >= 1)
                ADJUST_SCORE(-10);
            if ((gStatuses3[battlerDef] & STATUS3_PERISH_SONG) || abilityDef == ABILITY_SOUNDPROOF)
                ADJUST_SCORE(-10);
        }
        break;
    case EFFECT_RAIN_DANCE:
        if (weather & B_WEATHER_RAIN)
            ADJUST_SCORE(-8);
        break;
    case EFFECT_SUNNY_DAY:
        if (weather & B_WEATHER_SUN)
            ADJUST_SCORE(-8);
        break;
    case EFFECT_SANDSTORM:
        if (weather & B_WEATHER_SANDSTORM)
            ADJUST_SCORE(-8);
        break;
    case EFFECT_HAIL:
        if (weather & B_WEATHER_HAIL)
            ADJUST_SCORE(-8);
        break;
    case EFFECT_ATTRACT:
        if (!AI_CanBeInfatuated(battlerAtk, battlerDef, abilityDef))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SAFEGUARD:
        if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_SAFEGUARD)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_BATON_PASS:
        if (CountUsablePartyMons(battlerAtk) == 0)
            ADJUST_SCORE(-10);
        else if ((gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE)
              || (gStatuses3[battlerAtk] & STATUS3_ROOTED)
              || AnyStatIsRaised(battlerAtk))
            break;
        else
            ADJUST_SCORE(-6);
        break;
    case EFFECT_BELLY_DRUM:
        if (aiData->hpPercents[battlerAtk] <= 60)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_FUTURE_SIGHT:
        if (gWishFutureKnock.futureSightCounter[LEFT_FOE(battlerAtk)] > 0
         || gWishFutureKnock.futureSightCounter[RIGHT_FOE(battlerAtk)] > 0)
            ADJUST_SCORE(-12);
        else
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_TELEPORT: // fails in trainer battles
        ADJUST_SCORE(-10);
        break;
    case EFFECT_FAKE_OUT:
        if (!gDisableStructs[battlerAtk].isFirstTurn)
            ADJUST_SCORE(-10);
        if (HasChoiceEffect(battlerAtk))
            ADJUST_SCORE(-5);
        break;
    case EFFECT_STOCKPILE:
        if (gDisableStructs[battlerAtk].stockpileCounter >= 3)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SWALLOW:
        if (gDisableStructs[battlerAtk].stockpileCounter == 0)
        {
            ADJUST_SCORE(-10);
        }
        else
        {
            if (AI_BattlerAtMaxHp(battlerAtk))
                ADJUST_SCORE(-10);
            else if (aiData->hpPercents[battlerAtk] >= 80)
                ADJUST_SCORE(-5); // only if nothing better
        }
        break;
    case EFFECT_TORMENT:
        if (gBattleMons[battlerDef].status2 & STATUS2_TORMENT)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_MEMENTO:
        if (CountUsablePartyMons(battlerAtk) == 0)
            ADJUST_SCORE(-10);
        else if (gBattleMons[battlerDef].statStages[STAT_ATK] == MIN_STAT_STAGE && gBattleMons[battlerDef].statStages[STAT_SPATK] == MIN_STAT_STAGE)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_FOLLOW_ME:
    case EFFECT_HELPING_HAND:
        if (!hasPartner
         || (aiData->partnerMove != MOVE_NONE && IsBattleMoveStatus(aiData->partnerMove)))
            ADJUST_SCORE(-20);
        break;
    case EFFECT_TRICK:
        if ((gBattleMons[battlerAtk].item == ITEM_NONE && aiData->items[battlerDef] == ITEM_NONE)
         || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
            ADJUST_SCORE(-10);
        // fallthrough
    case EFFECT_KNOCK_OFF:
        if (abilityDef == ABILITY_STICKY_HOLD)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_INGRAIN:
        if (gStatuses3[battlerAtk] & STATUS3_ROOTED)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_RECYCLE:
        if (gBattleStruct->usedHeldItems[battlerAtk] == ITEM_NONE || gBattleMons[battlerAtk].item != ITEM_NONE)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_IMPRISON:
        if (gStatuses3[battlerAtk] & STATUS3_IMPRISONED_OTHERS)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_REFRESH:
        if (!(gBattleMons[battlerAtk].status1 & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_PARALYSIS))
         || !ShouldCureStatus(battlerAtk, battlerAtk))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_MUD_SPORT:
        for (i = 0; i < gBattlersCount; i++)
        {
            if (gStatuses3[i] & STATUS3_MUDSPORT)
            {
                ADJUST_SCORE(-10);
                break;
            }
        }
        break;
    case EFFECT_WATER_SPORT:
        for (i = 0; i < gBattlersCount; i++)
        {
            if (gStatuses3[i] & STATUS3_WATERSPORT)
            {
                ADJUST_SCORE(-10);
                break;
            }
        }
        break;
    case EFFECT_MIRROR_MOVE:
        if (predictedMove != MOVE_NONE
         && gBattleMoves[predictedMove].effect != EFFECT_MIRROR_MOVE
         && (gBattleMoves[predictedMove].flags & FLAG_MIRROR_MOVE_AFFECTED))
            return AI_CheckBadMove(battlerAtk, battlerDef, predictedMove, score);
        ADJUST_SCORE(-10);
        break;
    case EFFECT_BIDE:
        if (!HasDamagingMove(battlerDef)
         || aiData->hpPercents[battlerAtk] < 30 // close to fainting
         || (gBattleMons[battlerDef].status1 & (STATUS1_SLEEP | STATUS1_FREEZE))) // nothing to take back
            ADJUST_SCORE(-10);
        break;
    case EFFECT_CONVERSION:
        if (IS_BATTLER_OF_TYPE(battlerAtk, gBattleMoves[aiMoves[0]].type))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_REST:
        if (!AI_CanBeSlept(battlerAtk, abilityAtk))
            ADJUST_SCORE(-10);
        // fallthrough
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
        if (AI_BattlerAtMaxHp(battlerAtk))
            ADJUST_SCORE(-10);
        else if (aiData->hpPercents[battlerAtk] >= 90)
            ADJUST_SCORE(-9); // healing is pointless, but better than nothing
        break;
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
        if (AI_BattlerAtMaxHp(battlerAtk))
            ADJUST_SCORE(-10);
        else if (aiData->hpPercents[battlerAtk] >= 90)
            ADJUST_SCORE(-9);
        else if (weather & (B_WEATHER_RAIN | B_WEATHER_SANDSTORM | B_WEATHER_HAIL))
            ADJUST_SCORE(-3);
        break;
    case EFFECT_RECOIL_IF_MISS:
        if (aiData->moveAccuracy[battlerAtk][battlerDef][moveIndex] < 75
         && !(AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_RISKY))
            ADJUST_SCORE(-6);
        break;
    case EFFECT_TRANSFORM:
        if ((gBattleMons[battlerAtk].status2 & STATUS2_TRANSFORMED)
         || (gBattleMons[battlerDef].status2 & (STATUS2_TRANSFORMED | STATUS2_SUBSTITUTE)))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SPITE:
    case EFFECT_MIMIC:
        if (AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY))
        {
            if (aiData->lastUsedMove[battlerDef] == MOVE_NONE)
                ADJUST_SCORE(-10);
        }
        else if (predictedMove == MOVE_NONE)
        {
            ADJUST_SCORE(-10);
        }
        break;
    case EFFECT_LOCK_ON:
        if (sureHit)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_SKETCH:
        if (aiData->lastUsedMove[battlerDef] == MOVE_NONE)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_DESTINY_BOND:
        if (gBattleMons[battlerAtk].status2 & STATUS2_DESTINY_BOND)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_HEAL_BELL:
        if (!AnyPartyMemberStatused(battlerAtk, move == MOVE_HEAL_BELL))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_ENDURE:
        if (gBattleMons[battlerAtk].hp == 1 || GetBattlerSecondaryDamage(battlerAtk)) // would faint at the end of the turn anyway
            ADJUST_SCORE(-10);
        break;
    case EFFECT_PROTECT:
        if (IsBattlerIncapacitated(battlerDef, abilityDef))
        {
            ADJUST_SCORE(-10);
            break;
        }
        if (GetBattlerSecondaryDamage(battlerAtk) >= gBattleMons[battlerAtk].hp)
        {
            ADJUST_SCORE(-10); // would faint after protecting
        }
        else if (gDisableStructs[battlerAtk].protectUses == 1 && Random() % 100 < 50)
        {
            if (isBattle1v1)
                ADJUST_SCORE(-6);
            else
                ADJUST_SCORE(-10); // don't double protect in doubles
        }
        else if (gDisableStructs[battlerAtk].protectUses >= 2)
        {
            ADJUST_SCORE(-10);
        }
        break;
    case EFFECT_SEMI_INVULNERABLE:
        if (predictedMove != MOVE_NONE
         && AI_IsSlower(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY)
         && gBattleMoves[predictedMove].effect == EFFECT_SEMI_INVULNERABLE)
            ADJUST_SCORE(-10); // the target will just dodge too
        if (BattlerWillFaintFromWeather(battlerAtk, abilityAtk) && (move == MOVE_FLY || move == MOVE_BOUNCE))
            ADJUST_SCORE(-10); // would faint in the air
        break;
    case EFFECT_TAUNT:
        if (gDisableStructs[battlerDef].tauntTimer > 0)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_WISH:
        if (gWishFutureKnock.wishCounter[battlerAtk] > 0)
            ADJUST_SCORE(-10);
        break;
    case EFFECT_ASSIST:
        if (CountUsablePartyMons(battlerAtk) == 0)
            ADJUST_SCORE(-10); // no teammates to call moves from
        break;
    case EFFECT_MAGIC_COAT:
        if (!HasMoveWithFlag(battlerDef, FLAG_MAGIC_COAT_AFFECTED))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_YAWN:
        if (gStatuses3[battlerDef] & STATUS3_YAWN)
            ADJUST_SCORE(-10);
        else if (!AI_CanPutToSleep(battlerAtk, battlerDef, abilityDef, move))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_ROLE_PLAY:
        if (abilityDef == ABILITY_NONE || abilityDef == abilityAtk || abilityDef == ABILITY_WONDER_GUARD)
            ADJUST_AND_RETURN_SCORE(NO_DAMAGE_OR_FAILS);
        break;
    case EFFECT_SKILL_SWAP:
        if (abilityAtk == ABILITY_WONDER_GUARD || abilityDef == ABILITY_WONDER_GUARD || abilityAtk == abilityDef)
            ADJUST_AND_RETURN_SCORE(NO_DAMAGE_OR_FAILS);
        break;
    case EFFECT_SNATCH:
        if (!HasMoveWithFlag(battlerDef, FLAG_SNATCH_AFFECTED))
            ADJUST_SCORE(-10);
        break;
    case EFFECT_FLAIL:
        if (AI_IsSlower(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY) || aiData->hpPercents[battlerAtk] > 50)
            ADJUST_SCORE(-4);
        break;
    default:
        break;
    }

    // Status conditions
    switch (nonVolatileStatus)
    {
    case MOVE_EFFECT_POISON:
    case MOVE_EFFECT_TOXIC:
        if (!AI_CanPoison(battlerAtk, battlerDef, abilityDef, move))
            ADJUST_SCORE(-10);
        if (!ShouldPoison(battlerAtk, battlerDef))
            ADJUST_SCORE(-5);
        break;
    case MOVE_EFFECT_SLEEP:
        if (!AI_CanPutToSleep(battlerAtk, battlerDef, abilityDef, move))
            ADJUST_SCORE(-10);
        break;
    case MOVE_EFFECT_PARALYSIS:
        if (!AI_CanParalyze(battlerAtk, battlerDef, abilityDef, move))
            ADJUST_SCORE(-10);
        if (!ShouldParalyze(battlerAtk, battlerDef, abilityDef))
            ADJUST_SCORE(-5);
        break;
    case MOVE_EFFECT_BURN:
        if (!AI_CanBurn(battlerAtk, battlerDef, abilityDef, move))
            ADJUST_SCORE(-10);
        if (!ShouldBurn(battlerAtk, battlerDef, abilityDef))
            ADJUST_SCORE(-5);
        break;
    }

    // Choice Band locks the user into the move.
    if (HasChoiceEffect(battlerAtk))
    {
        // Don't lock into a move on the user (Swords Dance), with a few exceptions.
        if ((moveTarget & MOVE_TARGET_USER) && moveEffect != EFFECT_DESTINY_BOND && moveEffect != EFFECT_WISH)
            ADJUST_SCORE(-30);
        // Don't lock into a status move when the AI can't switch out of it.
        else if (IsBattleMoveStatus(move) && (CountUsablePartyMons(battlerAtk) < 1 || IsBattlerTrapped(battlerDef, battlerAtk)))
            ADJUST_SCORE(-30);
    }

    if (score < 0)
        score = 0;
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_TRY_TO_FAINT: go for KOs
// ---------------------------------------------------------------------------

static s32 AI_TryToFaint(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    u8 movesetIndex = AI_THINKING_STRUCT->movesetIndex;
    bool32 aiIsFaster = AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY);

    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;
    if (IsBattleMoveStatus(move))
        return score;

    if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, movesetIndex, AI_ATTACKING)
     && (!IsSelfSacrificeEffect(move) || ShouldConsiderSelfSacrificeDamageEffect(battlerAtk, battlerDef, move, aiIsFaster)))
    {
        if (aiIsFaster)
            ADJUST_SCORE(FAST_KILL);
        else
            ADJUST_SCORE(SLOW_KILL);
    }
    else if (CanTargetFaintAi(battlerDef, battlerAtk)
          && AI_WhoStrikesFirst(battlerAtk, battlerDef, MOVE_NONE, MOVE_NONE, DONT_CONSIDER_PRIORITY) != AI_IS_FASTER
          && gBattleMoves[move].priority > 0)
    {
        // About to be KO'd: get a priority hit in. Doesn't outscore a fast KO
        // (which also gets the best damage move bonus). Too easy to abuse in doubles.
        if (RandomPercentage(PRIORITIZE_LAST_CHANCE_CHANCE) && !IsDoubleBattle())
            ADJUST_SCORE(SLOW_KILL + 2);
        else
            ADJUST_SCORE(LAST_CHANCE);
    }
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_CHECK_VIABILITY: how good each move's effect is right now
// ---------------------------------------------------------------------------

static s32 AI_CalcMoveEffectScore(u8 battlerAtk, u8 battlerDef, u16 move)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 moveEffect = gBattleMoves[move].effect;
    u8 movesetIndex = AI_THINKING_STRUCT->movesetIndex;
    u16 predictedMove = GetIncomingMove(battlerAtk, battlerDef);
    u8 predictedMoveSlot = GetMoveIndex(battlerDef, predictedMove);
    bool32 isBattle1v1 = IsBattle1v1();
    bool32 hasTwoOpponents = HasTwoOpponents(battlerAtk);
    bool32 hasPartner = HasPartner(battlerAtk);
    bool32 moveTargetsBothOpponents = hasTwoOpponents && (gBattleMoves[move].target & (MOVE_TARGET_BOTH | MOVE_TARGET_FOES_AND_ALLY));
    u8 abilityAtk = aiData->abilities[battlerAtk];
    u8 abilityDef = aiData->abilities[battlerDef];
    s32 score = 0;
    u32 healPercent;

    // Guaranteed flinch (Fake Out).
    if (IsFlinchGuaranteed(battlerAtk, battlerDef, move))
        ADJUST_SCORE(BEST_EFFECT);

    switch (GetMoveNonVolatileStatus(move))
    {
    case MOVE_EFFECT_POISON:
    case MOVE_EFFECT_TOXIC:
        IncreasePoisonScore(battlerAtk, battlerDef, move, &score);
        break;
    case MOVE_EFFECT_SLEEP:
        IncreaseSleepScore(battlerAtk, battlerDef, move, &score);
        break;
    case MOVE_EFFECT_PARALYSIS:
        IncreaseParalyzeScore(battlerAtk, battlerDef, move, &score);
        break;
    case MOVE_EFFECT_BURN:
        IncreaseBurnScore(battlerAtk, battlerDef, move, &score);
        break;
    }

    switch (moveEffect)
    {
    case EFFECT_YAWN:
        IncreaseSleepScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_ABSORB:
    case EFFECT_DREAM_EATER:
        if (ShouldAbsorb(battlerAtk, battlerDef, move))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_ATTACK_UP:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK));
        break;
    case EFFECT_ATTACK_UP_2:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK_2));
        break;
    case EFFECT_DEFENSE_UP:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_DEF));
        break;
    case EFFECT_DEFENSE_UP_2:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_DEF_2));
        break;
    case EFFECT_SPEED_UP:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPEED));
        break;
    case EFFECT_SPEED_UP_2:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPEED_2));
        break;
    case EFFECT_SPECIAL_ATTACK_UP:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPATK));
        break;
    case EFFECT_SPECIAL_ATTACK_UP_2:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPATK_2));
        break;
    case EFFECT_SPECIAL_DEFENSE_UP:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPDEF));
        break;
    case EFFECT_SPECIAL_DEFENSE_UP_2:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPDEF_2));
        break;
    case EFFECT_ACCURACY_UP:
    case EFFECT_ACCURACY_UP_2:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ACC));
        break;
    case EFFECT_EVASION_UP:
    case EFFECT_EVASION_UP_2:
    case EFFECT_MINIMIZE:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_EVASION));
        break;
    case EFFECT_ATTACK_DOWN:
    case EFFECT_ATTACK_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_ATK));
        break;
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_DEFENSE_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_DEF));
        break;
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPEED_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_SPEED));
        break;
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_SPATK));
        break;
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_SPDEF));
        break;
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_ACCURACY_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_ACC));
        break;
    case EFFECT_EVASION_DOWN:
    case EFFECT_EVASION_DOWN_2:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_EVASION));
        break;
    case EFFECT_BIDE:
        if (aiData->hpPercents[battlerAtk] < 90)
            ADJUST_SCORE(-2);
        break;
    case EFFECT_HAZE:
        if (hasPartner && AnyStatIsRaised(BATTLE_PARTNER(battlerAtk)))
            break;
        score += AI_TryToClearStats(battlerAtk, battlerDef, moveTargetsBothOpponents);
        break;
    case EFFECT_ROAR:
        if (abilityDef == ABILITY_SOUNDPROOF || abilityDef == ABILITY_SUCTION_CUPS)
            break;
        score += AI_TryToClearStats(battlerAtk, battlerDef, moveTargetsBothOpponents);
        break;
    case EFFECT_CONVERSION:
        if (!IS_BATTLER_OF_TYPE(battlerAtk, gBattleMoves[gBattleMons[battlerAtk].moves[0]].type))
            ADJUST_SCORE(WEAK_EFFECT);
        break;
    case EFFECT_SWALLOW:
        switch (gDisableStructs[battlerAtk].stockpileCounter)
        {
        case 1:
            healPercent = 25;
            break;
        case 2:
            healPercent = 50;
            break;
        case 3:
            healPercent = 100;
            break;
        default:
            healPercent = 0;
            break;
        }
        if (healPercent != 0 && ShouldRecover(battlerAtk, battlerDef, move, healPercent))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
        if (ShouldRecover(battlerAtk, battlerDef, move, 50))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_LIGHT_SCREEN:
    case EFFECT_REFLECT:
        if (ShouldSetScreen(battlerAtk, battlerDef, moveEffect))
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_REST:
        if (AI_CanBeSlept(battlerAtk, abilityAtk) && ShouldRecover(battlerAtk, battlerDef, move, 100))
        {
            if (aiData->holdEffects[battlerAtk] == HOLD_EFFECT_CURE_SLP
             || aiData->holdEffects[battlerAtk] == HOLD_EFFECT_CURE_STATUS
             || HasUsableWhileAsleepMove(battlerAtk)
             || abilityAtk == ABILITY_SHED_SKIN
             || abilityAtk == ABILITY_EARLY_BIRD)
                ADJUST_SCORE(GOOD_EFFECT);
        }
        break;
    case EFFECT_OHKO:
        if ((gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS) && gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk)
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_MEAN_LOOK:
        if (ShouldTrap(battlerAtk, battlerDef, move))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_FOCUS_ENERGY:
        if (aiData->holdEffects[battlerAtk] == HOLD_EFFECT_SCOPE_LENS
         || IsHighCritMove(gBattleMons[battlerAtk].moves[0]) || IsHighCritMove(gBattleMons[battlerAtk].moves[1])
         || IsHighCritMove(gBattleMons[battlerAtk].moves[2]) || IsHighCritMove(gBattleMons[battlerAtk].moves[3]))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_CONFUSE:
    case EFFECT_TEETER_DANCE:
        IncreaseConfusionScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_SUBSTITUTE:
        ADJUST_SCORE(IncreaseSubstituteMoveScore(battlerAtk, battlerDef, move));
        break;
    case EFFECT_LEECH_SEED:
        if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS)
         || (gStatuses3[battlerDef] & STATUS3_LEECHSEED)
         || HasMoveWithEffect(battlerDef, EFFECT_RAPID_SPIN)
         || abilityDef == ABILITY_LIQUID_OOZE)
            break;
        ADJUST_SCORE(GOOD_EFFECT);
        if (!HasDamagingMove(battlerDef) || IsBattlerTrapped(battlerAtk, battlerDef))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_DISABLE:
        if (gDisableStructs[battlerDef].disabledMove == MOVE_NONE
         && aiData->lastUsedMove[battlerDef] != MOVE_NONE
         && AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY)
         && CanTargetMoveFaintAi(aiData->lastUsedMove[battlerDef], battlerDef, battlerAtk, 1))
            ADJUST_SCORE(GOOD_EFFECT); // disable the move that can KO the AI
        break;
    case EFFECT_ENCORE:
        // Lock the target into the status move it just used.
        if (gDisableStructs[battlerDef].encoredMove == MOVE_NONE
         && aiData->lastUsedMove[battlerDef] != MOVE_NONE
         && IsBattleMoveStatus(aiData->lastUsedMove[battlerDef]))
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_SLEEP_TALK:
    case EFFECT_SNORE:
        if (!IsWakeupTurn(battlerAtk) && AI_IsBattlerAsleep(battlerAtk))
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_LOCK_ON:
        if (HasMoveWithEffect(battlerAtk, EFFECT_OHKO))
            ADJUST_SCORE(GOOD_EFFECT);
        else if (HasMoveWithLowAccuracy(battlerAtk, battlerDef, 85, TRUE))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_DESTINY_BOND:
        if (AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY) && CanTargetFaintAi(battlerDef, battlerAtk))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_WISH:
    case EFFECT_HEAL_BELL:
        if (ShouldUseWishAromatherapy(battlerAtk, battlerDef, move))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_CURSE:
        if (IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GHOST))
        {
            if (IsBattlerTrapped(battlerAtk, battlerDef))
                ADJUST_SCORE(GOOD_EFFECT);
            else
                ADJUST_SCORE(WEAK_EFFECT);
        }
        else
        {
            ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK));
            ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_DEF));
        }
        break;
    case EFFECT_PROTECT:
        ADJUST_SCORE(ProtectChecks(battlerAtk, battlerDef, move, predictedMove));
        break;
    case EFFECT_ENDURE:
        if (CanTargetFaintAi(battlerDef, battlerAtk))
        {
            if (gBattleMons[battlerAtk].hp > gBattleMons[battlerAtk].maxHP / 4 // the pinch berry hasn't activated yet
             && IsPinchBerryHoldEffect(aiData->holdEffects[battlerAtk]))
                ADJUST_SCORE(GOOD_EFFECT);
            else if (gBattleMons[battlerAtk].hp > 1 // only for Flail/Reversal/Endeavor when not already at 1 HP
                  && (HasMoveWithEffect(battlerAtk, EFFECT_FLAIL) || HasMoveWithEffect(battlerAtk, EFFECT_ENDEAVOR)))
                ADJUST_SCORE(GOOD_EFFECT);
        }
        break;
    case EFFECT_SPIKES:
        if (AI_ShouldSetUpHazards(battlerAtk, battlerDef, move))
        {
            if (gDisableStructs[battlerAtk].isFirstTurn)
                ADJUST_SCORE(BEST_EFFECT);
            else
                ADJUST_SCORE(DECENT_EFFECT);
        }
        break;
    case EFFECT_FORESIGHT:
        if (gBattleMons[battlerDef].statStages[STAT_EVASION] > DEFAULT_STAT_STAGE
         || (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GHOST)
          && (HasMoveWithType(battlerAtk, TYPE_NORMAL) || HasMoveWithType(battlerAtk, TYPE_FIGHTING))))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_PERISH_SONG:
        if (IsBattlerTrapped(battlerAtk, battlerDef))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_RAIN_DANCE:
    case EFFECT_SUNNY_DAY:
    case EFFECT_SANDSTORM:
    case EFFECT_HAIL:
        ADJUST_SCORE(CalcWeatherScore(battlerAtk, battlerDef, move));
        break;
    case EFFECT_BELLY_DRUM:
        if (HasHPForDamagingSetup(battlerAtk, battlerDef, 50))
            ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK_MAX));
        break;
    case EFFECT_PSYCH_UP:
        score += AI_ShouldCopyStatChanges(battlerAtk, battlerDef);
        break;
    case EFFECT_SEMI_INVULNERABLE:
        if (predictedMove != MOVE_NONE && isBattle1v1)
        {
            u8 predictedEffect = gBattleMoves[predictedMove].effect;

            if (AI_IsFaster(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY)
             && (IsExplosionMove(predictedMove) || predictedEffect == EFFECT_PROTECT))
                ADJUST_SCORE(GOOD_EFFECT);
            else if (predictedEffect == EFFECT_SEMI_INVULNERABLE && !(gStatuses3[battlerDef] & STATUS3_SEMI_INVULNERABLE))
                ADJUST_SCORE(GOOD_EFFECT);
        }
        break;
    case EFFECT_DEFENSE_CURL:
        if (HasMoveWithEffect(battlerAtk, EFFECT_ROLLOUT) && !(gBattleMons[battlerAtk].status2 & STATUS2_DEFENSE_CURL))
            ADJUST_SCORE(DECENT_EFFECT);
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_DEF));
        break;
    case EFFECT_FAKE_OUT:
        if (gDisableStructs[battlerAtk].isFirstTurn && IsBestDmgMove(battlerAtk, battlerDef, AI_ATTACKING, move))
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_STOCKPILE: // Gen 3 Stockpile raises no stats
        if (HasMoveWithEffect(battlerAtk, EFFECT_SWALLOW) || HasMoveWithEffect(battlerAtk, EFFECT_SPIT_UP))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
        if (HasMoveWithEffect(battlerAtk, EFFECT_PSYCH_UP) || (hasPartner && HasMoveWithEffect(BATTLE_PARTNER(battlerAtk), EFFECT_PSYCH_UP)))
            ADJUST_SCORE(DECENT_EFFECT);
        IncreaseConfusionScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_ATTRACT:
        if (isBattle1v1
         && AI_IsSlower(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY)
         && BattlerWillFaintFromSecondaryDamage(battlerDef, abilityDef))
            break; // the target faints before it matters
        if ((gBattleMons[battlerDef].status1 & STATUS1_ANY)
         || (gBattleMons[battlerDef].status2 & STATUS2_CONFUSION)
         || IsBattlerTrapped(battlerAtk, battlerDef))
            ADJUST_SCORE(GOOD_EFFECT);
        else
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_SAFEGUARD:
        ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_FOLLOW_ME:
        if (hasPartner && !IsBattlerIncapacitated(battlerDef, abilityDef))
        {
            u16 predictedMoveOnPartner = aiData->lastUsedMove[BATTLE_PARTNER(battlerAtk)];

            if (predictedMoveOnPartner != MOVE_NONE && !IsBattleMoveStatus(predictedMoveOnPartner))
                ADJUST_SCORE(GOOD_EFFECT);
        }
        break;
    case EFFECT_CHARGE:
        if (HasDamagingMoveOfType(battlerAtk, TYPE_ELECTRIC))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_TAUNT:
        if (predictedMove != MOVE_NONE && IsBattleMoveStatus(predictedMove))
            ADJUST_SCORE(GOOD_EFFECT);
        else if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_STATUS))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_TRICK:
        if (aiData->holdEffects[battlerAtk] == HOLD_EFFECT_CHOICE_BAND)
        {
            if (!HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL))
                ADJUST_SCORE(DECENT_EFFECT); // the target can't use it
        }
        else if (aiData->items[battlerAtk] == ITEM_NONE && aiData->items[battlerDef] != ITEM_NONE
              && aiData->holdEffects[battlerDef] != HOLD_EFFECT_CHOICE_BAND)
        {
            ADJUST_SCORE(WEAK_EFFECT); // hold effects are generally good to have
        }
        break;
    case EFFECT_INGRAIN:
        ADJUST_SCORE(WEAK_EFFECT);
        break;
    case EFFECT_MAGIC_COAT:
        if (predictedMove != MOVE_NONE && (gBattleMoves[predictedMove].flags & FLAG_MAGIC_COAT_AFFECTED))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_RECYCLE:
        if (gBattleStruct->usedHeldItems[battlerAtk] != ITEM_NONE)
            ADJUST_SCORE(WEAK_EFFECT);
        break;
    case EFFECT_IMPRISON:
        if (predictedMove != MOVE_NONE && HasMove(battlerAtk, predictedMove))
            ADJUST_SCORE(DECENT_EFFECT);
        else if (!gDisableStructs[battlerAtk].isFirstTurn)
            ADJUST_SCORE(WEAK_EFFECT);
        break;
    case EFFECT_REFRESH:
        if ((gBattleMons[battlerAtk].status1 & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_PARALYSIS))
         && ShouldCureStatus(battlerAtk, battlerAtk))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_SNATCH:
        if (predictedMove != MOVE_NONE && (gBattleMoves[predictedMove].flags & FLAG_SNATCH_AFFECTED))
            ADJUST_SCORE(GOOD_EFFECT); // steal the move
        break;
    case EFFECT_MUD_SPORT:
        if (!HasMoveWithType(battlerAtk, TYPE_ELECTRIC) && HasMoveWithType(battlerDef, TYPE_ELECTRIC))
            ADJUST_SCORE(WEAK_EFFECT);
        break;
    case EFFECT_WATER_SPORT:
        if (!HasMoveWithType(battlerAtk, TYPE_FIRE) && HasMoveWithType(battlerDef, TYPE_FIRE))
            ADJUST_SCORE(WEAK_EFFECT);
        break;
    case EFFECT_TICKLE:
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_ATK));
        ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_DEF));
        break;
    case EFFECT_COSMIC_POWER:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_DEF));
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPDEF));
        break;
    case EFFECT_BULK_UP:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK));
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_DEF));
        break;
    case EFFECT_CALM_MIND:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPATK));
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPDEF));
        break;
    case EFFECT_DRAGON_DANCE:
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_SPEED));
        ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK));
        break;
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
        // Reflect the damage of the move the AI expects, if it survives it.
        if (!IsBattlerIncapacitated(battlerDef, abilityDef)
         && predictedMove != MOVE_NONE
         && predictedMoveSlot < MAX_MON_MOVES
         && GetNoOfHitsToKOBattler(battlerDef, battlerAtk, predictedMoveSlot, AI_DEFENDING, CONSIDER_ENDURE) >= 2
         && !IsBattleMoveStatus(predictedMove)
         && (moveEffect == EFFECT_COUNTER) == IsBattleMovePhysical(predictedMove, battlerDef))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_ENDEAVOR:
        if (AI_IsSlower(battlerAtk, battlerDef, move, GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY) && !CanTargetFaintAi(battlerDef, battlerAtk))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_RAPID_SPIN:
        if ((gSideTimers[GetBattlerSide(battlerAtk)].spikesAmount != 0 && CountUsablePartyMons(battlerAtk) != 0)
         || (gStatuses3[battlerAtk] & STATUS3_LEECHSEED)
         || (gBattleMons[battlerAtk].status2 & STATUS2_WRAPPED))
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_KNOCK_OFF:
        if (aiData->items[battlerDef] != ITEM_NONE && abilityDef != ABILITY_STICKY_HOLD)
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_BRICK_BREAK:
        if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_REFLECT)
            ADJUST_SCORE(DECENT_EFFECT);
        if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_LIGHTSCREEN)
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    default:
        break;
    }
    return score;
}

// Secondary effects that surely happen (Icy Wind's speed drop, Wrap, ...).
static s32 AI_CalcAdditionalEffectScore(u8 battlerAtk, u8 battlerDef, u16 move)
{
    struct AiLogicData *aiData = AI_DATA;
    struct AiSecondaryEffect secondary = GetMoveSecondaryEffect(move);
    s32 score = 0;
    u32 scoreIncrease;
    u8 stat;

    if (!MoveEffectIsGuaranteed(battlerAtk, move, &secondary))
        return score;

    if (secondary.self)
    {
        if (secondary.moveEffect >= MOVE_EFFECT_ATK_PLUS_1 && secondary.moveEffect <= MOVE_EFFECT_SP_DEF_PLUS_1)
            ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ATK + secondary.moveEffect - MOVE_EFFECT_ATK_PLUS_1));
        else if (secondary.moveEffect == MOVE_EFFECT_ACC_PLUS_1)
            ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_ACC));
        else if (secondary.moveEffect == MOVE_EFFECT_EVS_PLUS_1)
            ADJUST_SCORE(IncreaseStatUpScore(battlerAtk, battlerDef, STAT_CHANGE_EVASION));
        return score;
    }

    if (aiData->abilities[battlerDef] == ABILITY_SHIELD_DUST)
        return score;

    switch (secondary.moveEffect)
    {
    case MOVE_EFFECT_FLINCH:
        if (ShouldTryToFlinch(battlerAtk, battlerDef, aiData->abilities[battlerAtk], aiData->abilities[battlerDef], move))
            score += 2;
        break;
    case MOVE_EFFECT_SPD_MINUS_1:
        if (CanLowerStat(battlerAtk, battlerDef, STAT_SPEED))
            ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, STAT_SPEED));
        break;
    case MOVE_EFFECT_ACC_MINUS_1:
        if (CanLowerStat(battlerAtk, battlerDef, STAT_ACC))
        {
            scoreIncrease = IncreaseStatDownScore(battlerAtk, battlerDef, STAT_ACC);
            if (scoreIncrease == WEAK_EFFECT)
                scoreIncrease = DECENT_EFFECT;
            ADJUST_SCORE(scoreIncrease);
        }
        break;
    case MOVE_EFFECT_ATK_MINUS_1:
    case MOVE_EFFECT_DEF_MINUS_1:
    case MOVE_EFFECT_SP_ATK_MINUS_1:
    case MOVE_EFFECT_SP_DEF_MINUS_1:
    case MOVE_EFFECT_EVS_MINUS_1:
        stat = STAT_ATK + secondary.moveEffect - MOVE_EFFECT_ATK_MINUS_1;
        if (CanLowerStat(battlerAtk, battlerDef, stat))
            ADJUST_SCORE(IncreaseStatDownScore(battlerAtk, battlerDef, stat));
        break;
    case MOVE_EFFECT_POISON:
        IncreasePoisonScore(battlerAtk, battlerDef, move, &score);
        break;
    case MOVE_EFFECT_WRAP:
        if (!HasMoveWithEffect(battlerDef, EFFECT_RAPID_SPIN) && ShouldTrap(battlerAtk, battlerDef, move))
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case MOVE_EFFECT_REMOVE_PARALYSIS: // SmellingSalt
        if (gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS)
        {
            if (ShouldCureStatus(battlerAtk, battlerDef))
                ADJUST_SCORE(DECENT_EFFECT);
            else
                ADJUST_SCORE(BAD_EFFECT);
        }
        break;
    }
    return score;
}

static s32 AI_CheckViability(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    if (gBattleMoves[move].power != 0)
    {
        if (GetNoOfHitsToKOBattler(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, AI_ATTACKING, CONSIDER_ENDURE) == 0
         && !IsDamageUnknownEffect(gBattleMoves[move].effect))
            ADJUST_AND_RETURN_SCORE(NO_DAMAGE_OR_FAILS); // no point checking it further
        if ((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_RISKY) && IsBestDmgMove(battlerAtk, battlerDef, AI_ATTACKING, move))
            ADJUST_SCORE(BEST_DAMAGE_MOVE);
    }

    ADJUST_SCORE(AI_CalcMoveEffectScore(battlerAtk, battlerDef, move));
    ADJUST_SCORE(AI_CalcAdditionalEffectScore(battlerAtk, battlerDef, move));
    return score;
}

// For moves that KO in the same number of hits, the tiebreakers below decide
// which one is best, the last one counting most.
enum
{
    PRIORITY_EFFECT,
    PRIORITY_ACCURACY,
    PRIORITY_GUARANTEE,
    PRIORITY_AVOID_SELF_SACRIFICE,
    PRIORITY_SPEED,
    PRIORITY_NOT_CHARGING,
};

static u32 MovesPow(u32 exponent)
{
    u32 result = 1;

    while (exponent-- != 0)
        result *= MAX_MON_MOVES;
    return result;
}

static enum MoveComparisonResult CompareMoveAccuracies(u8 battlerAtk, u8 battlerDef, u8 moveSlot1, u8 moveSlot2)
{
    u32 acc1 = AI_DATA->moveAccuracy[battlerAtk][battlerDef][moveSlot1];
    u32 acc2 = AI_DATA->moveAccuracy[battlerAtk][battlerDef][moveSlot2];

    if (acc1 > acc2)
        return MOVE_WON_COMPARISON;
    if (acc2 > acc1)
        return MOVE_LOST_COMPARISON;
    return MOVE_NEUTRAL_COMPARISON;
}

static enum MoveComparisonResult CompareMoveSpeeds(bool32 move1Faster, bool32 move2Faster)
{
    if (move1Faster && !move2Faster)
        return MOVE_WON_COMPARISON;
    if (move2Faster && !move1Faster)
        return MOVE_LOST_COMPARISON;
    return MOVE_NEUTRAL_COMPARISON;
}

// A KO even with the lowest damage roll.
static enum MoveComparisonResult CompareGuaranteeFaintTarget(u8 battlerAtk, u8 battlerDef, u8 moveSlot1, u8 moveSlot2)
{
    bool32 guarantee1, guarantee2;

    if (!(AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_TRY_TO_FAINT))
        return MOVE_NEUTRAL_COMPARISON;

    guarantee1 = gBattleMons[battlerDef].hp <= AI_DATA->simulatedDmg[battlerAtk][battlerDef][moveSlot1].minimum;
    guarantee2 = gBattleMons[battlerDef].hp <= AI_DATA->simulatedDmg[battlerAtk][battlerDef][moveSlot2].minimum;
    if (guarantee1 && !guarantee2)
        return MOVE_WON_COMPARISON;
    if (guarantee2 && !guarantee1)
        return MOVE_LOST_COMPARISON;
    return MOVE_NEUTRAL_COMPARISON;
}

static enum MoveComparisonResult CompareMoveSelfSacrifice(u16 move1, u16 move2)
{
    bool32 selfSacrifice1 = IsSelfSacrificeEffect(move1);
    bool32 selfSacrifice2 = IsSelfSacrificeEffect(move2);

    if (selfSacrifice1 && !selfSacrifice2)
        return MOVE_LOST_COMPARISON;
    if (selfSacrifice2 && !selfSacrifice1)
        return MOVE_WON_COMPARISON;
    return MOVE_NEUTRAL_COMPARISON;
}

static enum MoveComparisonResult CompareMoveTwoTurnEffect(u8 battlerAtk, u16 move1, u16 move2)
{
    bool32 twoTurn1 = IsTwoTurnNotSemiInvulnerableMove(battlerAtk, move1);
    bool32 twoTurn2 = IsTwoTurnNotSemiInvulnerableMove(battlerAtk, move2);

    if (twoTurn1 && !twoTurn2)
        return MOVE_LOST_COMPARISON;
    if (twoTurn2 && !twoTurn1)
        return MOVE_WON_COMPARISON;
    return MOVE_NEUTRAL_COMPARISON;
}

static void AddComparisonPoints(enum MoveComparisonResult result, u32 *tempMoveScores, u8 currId, u8 compareId, u32 priority)
{
    if (result == MOVE_WON_COMPARISON)
        tempMoveScores[currId] += MovesPow(priority);
    else if (result == MOVE_LOST_COMPARISON)
        tempMoveScores[compareId] += MovesPow(priority);
}

static bool32 ShouldCompareMove(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u16 move)
{
    if (IsTargetingPartner(battlerAtk, battlerDef))
        return FALSE;
    if (gBattleMoves[move].power == 0)
        return FALSE;
    if (GetNoOfHitsToKOBattler(battlerAtk, battlerDef, moveIndex, AI_ATTACKING, DONT_CONSIDER_ENDURE) == 0)
        return FALSE;
    if ((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_RISKY) && IsBestDmgMove(battlerAtk, battlerDef, AI_ATTACKING, move))
        return FALSE; // already got its bonus in AI_CheckViability
    return TRUE;
}

// Gives BEST_DAMAGE_MOVE to the damaging move that KOs in the fewest hits,
// breaking ties by: not charging, using priority to KO first, avoiding
// self-sacrifice, a guaranteed KO, accuracy, then side effects.
static void AI_CompareDamagingMoves(u8 battlerAtk, u8 battlerDef)
{
    u16 *moves = gBattleMons[battlerAtk].moves;
    u32 tempMoveScores[MAX_MON_MOVES];
    u32 moveComparisonScores[MAX_MON_MOVES];
    u32 bestScore = AI_SCORE_DEFAULT;
    bool32 multipleBestMoves = FALSE;
    s32 noOfHits[MAX_MON_MOVES];
    s32 leastHits = 1000;
    bool32 moveIsFaster[MAX_MON_MOVES];
    u8 currId, compareId;

    for (currId = 0; currId < MAX_MON_MOVES; currId++)
    {
        moveComparisonScores[currId] = 0;
        if (moves[currId] == MOVE_NONE || !ShouldCompareMove(battlerAtk, battlerDef, currId, moves[currId]))
            continue;

        for (compareId = 0; compareId < MAX_MON_MOVES; compareId++)
        {
            if (moves[compareId] != MOVE_NONE && gBattleMoves[moves[compareId]].power != 0)
            {
                noOfHits[compareId] = GetNoOfHitsToKOBattler(battlerAtk, battlerDef, compareId, AI_ATTACKING, CONSIDER_ENDURE);
                moveIsFaster[compareId] = AI_IsFaster(battlerAtk, battlerDef, moves[compareId], GetIncomingMoveSpeedCheck(battlerAtk, battlerDef), CONSIDER_PRIORITY);
                tempMoveScores[compareId] = AI_SCORE_DEFAULT;
                if (noOfHits[compareId] < leastHits && noOfHits[compareId] != 0)
                    leastHits = noOfHits[compareId];

                // Decided against a self-sacrificing move this turn.
                if (IsSelfSacrificeEffect(moves[compareId])
                 && !ShouldConsiderSelfSacrificeDamageEffect(battlerAtk, battlerDef, moves[compareId], moveIsFaster[compareId]))
                {
                    noOfHits[compareId] = gBattleMons[battlerDef].maxHP;
                    tempMoveScores[compareId] = 0;
                }
            }
            else
            {
                noOfHits[compareId] = -1;
                moveIsFaster[compareId] = FALSE;
                tempMoveScores[compareId] = 0;
            }
        }

        if (leastHits == noOfHits[currId])
        {
            for (compareId = 0; compareId < MAX_MON_MOVES; compareId++)
            {
                if (compareId == currId || noOfHits[currId] != noOfHits[compareId])
                    continue;

                multipleBestMoves = TRUE;
                if (noOfHits[currId] == 1) // both KO
                {
                    AddComparisonPoints(CompareMoveSpeeds(moveIsFaster[currId], moveIsFaster[compareId]),
                                        tempMoveScores, currId, compareId, PRIORITY_SPEED);
                    AddComparisonPoints(CompareGuaranteeFaintTarget(battlerAtk, battlerDef, currId, compareId),
                                        tempMoveScores, currId, compareId, PRIORITY_GUARANTEE);
                }
                AddComparisonPoints(CompareMoveTwoTurnEffect(battlerAtk, moves[currId], moves[compareId]),
                                    tempMoveScores, currId, compareId, PRIORITY_NOT_CHARGING);
                AddComparisonPoints(CompareMoveAccuracies(battlerAtk, battlerDef, currId, compareId),
                                    tempMoveScores, currId, compareId, PRIORITY_ACCURACY);
                AddComparisonPoints(CompareMoveSelfSacrifice(moves[currId], moves[compareId]),
                                    tempMoveScores, currId, compareId, PRIORITY_AVOID_SELF_SACRIFICE);
                AddComparisonPoints(CompareMoveEffects(moves[currId], moves[compareId], battlerAtk, battlerDef, noOfHits[currId]),
                                    tempMoveScores, currId, compareId, PRIORITY_EFFECT);
            }
            // The only move with the fewest hits to KO.
            if (!multipleBestMoves)
                moveComparisonScores[currId] = 0xFFFFFFFF;
            else
                moveComparisonScores[currId] = tempMoveScores[currId];
        }
    }

    for (currId = 0; currId < MAX_MON_MOVES; currId++)
    {
        if (moveComparisonScores[currId] > bestScore)
            bestScore = moveComparisonScores[currId];
    }
    for (currId = 0; currId < MAX_MON_MOVES; currId++)
    {
        if (moveComparisonScores[currId] == bestScore)
            AI_DATA->moveScores[currId] += BEST_DAMAGE_MOVE;
    }
}

// ---------------------------------------------------------------------------
// AI_LOGIC_FORCE_SETUP_FIRST_TURN: prefer setup moves on the first turn
// ---------------------------------------------------------------------------

static s32 AI_ForceSetupFirstTurn(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    if (IsTargetingPartner(battlerAtk, battlerDef) || gBattleResults.battleTurnCounter != 0)
        return score;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_ATTACK_UP:
    case EFFECT_DEFENSE_UP:
    case EFFECT_SPEED_UP:
    case EFFECT_SPECIAL_ATTACK_UP:
    case EFFECT_SPECIAL_DEFENSE_UP:
    case EFFECT_ACCURACY_UP:
    case EFFECT_EVASION_UP:
    case EFFECT_ATTACK_DOWN:
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_EVASION_DOWN:
    case EFFECT_CONVERSION:
    case EFFECT_LIGHT_SCREEN:
    case EFFECT_FOCUS_ENERGY:
    case EFFECT_CONFUSE:
    case EFFECT_ATTACK_UP_2:
    case EFFECT_DEFENSE_UP_2:
    case EFFECT_SPEED_UP_2:
    case EFFECT_SPECIAL_ATTACK_UP_2:
    case EFFECT_SPECIAL_DEFENSE_UP_2:
    case EFFECT_ACCURACY_UP_2:
    case EFFECT_EVASION_UP_2:
    case EFFECT_ATTACK_DOWN_2:
    case EFFECT_DEFENSE_DOWN_2:
    case EFFECT_SPEED_DOWN_2:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
    case EFFECT_ACCURACY_DOWN_2:
    case EFFECT_EVASION_DOWN_2:
    case EFFECT_REFLECT:
    case EFFECT_SLEEP:
    case EFFECT_POISON:
    case EFFECT_TOXIC:
    case EFFECT_PARALYZE:
    case EFFECT_WILL_O_WISP:
    case EFFECT_SUBSTITUTE:
    case EFFECT_LEECH_SEED:
    case EFFECT_MINIMIZE:
    case EFFECT_CURSE:
    case EFFECT_SWAGGER:
    case EFFECT_CAMOUFLAGE:
    case EFFECT_YAWN:
    case EFFECT_DEFENSE_CURL:
    case EFFECT_TORMENT:
    case EFFECT_FLATTER:
    case EFFECT_INGRAIN:
    case EFFECT_IMPRISON:
    case EFFECT_TICKLE:
    case EFFECT_COSMIC_POWER:
    case EFFECT_BULK_UP:
    case EFFECT_CALM_MIND:
    case EFFECT_DRAGON_DANCE:
    case EFFECT_RAIN_DANCE:
    case EFFECT_SUNNY_DAY:
    case EFFECT_SANDSTORM:
    case EFFECT_HAIL:
        ADJUST_SCORE(DECENT_EFFECT);
        break;
    default:
        break;
    }
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_RISKY: bonus for risky moves and high critical-hit moves
// ---------------------------------------------------------------------------

static s32 AI_Risky(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    const struct SpeciesInfo *atkInfo = &gSpeciesInfo[gBattleMons[battlerAtk].species];
    const struct SpeciesInfo *defInfo = &gSpeciesInfo[gBattleMons[battlerDef].species];

    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    if (IsHighCritMove(move))
        ADJUST_SCORE(DECENT_EFFECT);

    if (IsExplosionMove(move))
        RETURN_SCORE_PLUS(STRONG_RISKY_EFFECT);

    switch (gBattleMoves[move].effect)
    {
    // +3
    case EFFECT_COUNTER: // the target looks like a physical attacker
        if (defInfo->baseAttack >= defInfo->baseSpAttack + 10)
            ADJUST_SCORE(STRONG_RISKY_EFFECT);
        break;
    case EFFECT_MIRROR_COAT: // the target looks like a special attacker
        if (defInfo->baseSpAttack >= defInfo->baseAttack + 10)
            ADJUST_SCORE(STRONG_RISKY_EFFECT);
        break;
    // +2
    case EFFECT_MEMENTO:
        if (AI_DATA->hpPercents[battlerAtk] < 50 && (Random() % 256) < 128)
            ADJUST_SCORE(AVERAGE_RISKY_EFFECT);
        break;
    case EFFECT_REVENGE:
        if (defInfo->baseSpeed >= atkInfo->baseSpeed + 10)
            ADJUST_SCORE(AVERAGE_RISKY_EFFECT);
        break;
    case EFFECT_BELLY_DRUM:
        if (AI_DATA->hpPercents[battlerAtk] >= 90)
            ADJUST_SCORE(AVERAGE_RISKY_EFFECT);
        break;
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
    case EFFECT_ATTRACT:
    case EFFECT_OHKO:
        ADJUST_SCORE(AVERAGE_RISKY_EFFECT);
        break;
    case EFFECT_ALL_STATS_UP_HIT:
        if (Random() & 1)
            ADJUST_SCORE(AVERAGE_RISKY_EFFECT);
        break;
    default:
        break;
    }
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_TRY_TO_2HKO: bonus for moves that KO in one or two hits
// ---------------------------------------------------------------------------

static s32 AI_TryTo2HKO(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    u32 hitsToKO;

    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    hitsToKO = GetNoOfHitsToKOBattler(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, AI_ATTACKING, CONSIDER_ENDURE);
    if (hitsToKO == 1)
        ADJUST_SCORE(BEST_EFFECT);
    else if (hitsToKO == 2)
        ADJUST_SCORE(DECENT_EFFECT);
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_PREFER_BATON_PASS: raise stats, then pass them on
// ---------------------------------------------------------------------------

static s32 AI_PreferBatonPass(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    u8 effect = gBattleMoves[move].effect;

    if (IsTargetingPartner(battlerAtk, battlerDef)
     || CountUsablePartyMons(battlerAtk) == 0
     || !IsBattleMoveStatus(move)
     || !HasMoveWithEffect(battlerAtk, EFFECT_BATON_PASS)
     || IsBattlerTrapped(battlerDef, battlerAtk))
        return score;

    if (IsStatRaisingEffect(effect))
    {
        if (gBattleResults.battleTurnCounter == 0)
            ADJUST_SCORE(GOOD_EFFECT);
        else if (AI_DATA->hpPercents[battlerAtk] < 60)
            ADJUST_SCORE(-10);
        else
            ADJUST_SCORE(WEAK_EFFECT);
    }

    switch (effect)
    {
    case EFFECT_INGRAIN:
        if (!(gStatuses3[battlerAtk] & STATUS3_ROOTED))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_PROTECT:
        if (gBattleMoves[AI_DATA->lastUsedMove[battlerAtk]].effect == EFFECT_PROTECT)
            ADJUST_SCORE(-2);
        else
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_BATON_PASS:
        if (gStatuses3[battlerAtk] & STATUS3_ROOTED)
            ADJUST_SCORE(DECENT_EFFECT);
        if (gStatuses3[battlerAtk] & STATUS3_LEECHSEED)
            ADJUST_SCORE(-3);
        ADJUST_SCORE((s32)CountPositiveStatStages(battlerAtk) - (s32)CountNegativeStatStages(battlerAtk));
        break;
    default:
        break;
    }
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_HP_AWARE: pick effects that fit its own and the target's HP
// ---------------------------------------------------------------------------

static s32 AI_HPAware(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 effect = gBattleMoves[move].effect;
    u8 moveType = GetBattleMoveType(move, battlerAtk);
    u8 partner = BATTLE_PARTNER(battlerAtk);

    if (IsTargetingPartner(battlerAtk, battlerDef))
    {
        // Feed the partner's absorbing ability.
        if ((moveType == TYPE_ELECTRIC && aiData->abilities[partner] == ABILITY_VOLT_ABSORB)
         || (moveType == TYPE_WATER && aiData->abilities[partner] == ABILITY_WATER_ABSORB))
        {
            if (CanTargetFaintAi(LEFT_FOE(battlerAtk), partner) || CanTargetFaintAi(RIGHT_FOE(battlerAtk), partner))
                ADJUST_SCORE(-1);
            if (aiData->hpPercents[battlerDef] <= 50)
                ADJUST_SCORE(WEAK_EFFECT);
        }
    }
    else
    {
        // The AI's own HP
        if (IsExplosionMove(move) && aiData->hpPercents[battlerAtk] > 70)
        {
            ADJUST_SCORE(-2);
        }
        else if (aiData->hpPercents[battlerAtk] > 70)
        {
            switch (effect)
            {
            case EFFECT_RESTORE_HP:
            case EFFECT_REST:
            case EFFECT_DESTINY_BOND:
            case EFFECT_FLAIL:
            case EFFECT_ENDURE:
            case EFFECT_MORNING_SUN:
            case EFFECT_SYNTHESIS:
            case EFFECT_MOONLIGHT:
            case EFFECT_SOFTBOILED:
            case EFFECT_MEMENTO:
            case EFFECT_GRUDGE:
                ADJUST_SCORE(-2);
                break;
            default:
                break;
            }
        }
        else if (aiData->hpPercents[battlerAtk] > 30)
        {
            if (IsStatRaisingEffect(effect) || IsStatLoweringEffect(effect) || IsExplosionMove(move))
                ADJUST_SCORE(-2);
            switch (effect)
            {
            case EFFECT_BIDE:
            case EFFECT_CONVERSION:
            case EFFECT_LIGHT_SCREEN:
            case EFFECT_REFLECT:
            case EFFECT_MIST:
            case EFFECT_FOCUS_ENERGY:
            case EFFECT_CONVERSION_2:
            case EFFECT_SAFEGUARD:
            case EFFECT_BELLY_DRUM:
                ADJUST_SCORE(-2);
                break;
            default:
                break;
            }
        }
        else
        {
            if (IsStatRaisingEffect(effect) || IsStatLoweringEffect(effect))
                ADJUST_SCORE(-2);
            switch (effect)
            {
            case EFFECT_BIDE:
            case EFFECT_CONVERSION:
            case EFFECT_REFLECT:
            case EFFECT_LIGHT_SCREEN:
            case EFFECT_MIST:
            case EFFECT_FOCUS_ENERGY:
            case EFFECT_CONVERSION_2:
            case EFFECT_LOCK_ON:
            case EFFECT_SAFEGUARD:
            case EFFECT_BELLY_DRUM:
            case EFFECT_PSYCH_UP:
            case EFFECT_COUNTER:
            case EFFECT_MIRROR_COAT:
            case EFFECT_TICKLE:
            case EFFECT_RAIN_DANCE:
            case EFFECT_SUNNY_DAY:
            case EFFECT_SANDSTORM:
            case EFFECT_HAIL:
                ADJUST_SCORE(-2);
                break;
            default:
                break;
            }
        }
    }

    // The target's HP
    if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, AI_ATTACKING))
    {
        ADJUST_SCORE(DECENT_EFFECT);
    }
    else if (aiData->hpPercents[battlerDef] > 70)
    {
        // nothing yet
    }
    else if (aiData->hpPercents[battlerDef] > 30)
    {
        switch (effect)
        {
        case EFFECT_ATTACK_UP:
        case EFFECT_DEFENSE_UP:
        case EFFECT_SPEED_UP:
        case EFFECT_SPECIAL_ATTACK_UP:
        case EFFECT_SPECIAL_DEFENSE_UP:
        case EFFECT_ACCURACY_UP:
        case EFFECT_EVASION_UP:
        case EFFECT_ATTACK_DOWN:
        case EFFECT_DEFENSE_DOWN:
        case EFFECT_SPEED_DOWN:
        case EFFECT_SPECIAL_ATTACK_DOWN:
        case EFFECT_SPECIAL_DEFENSE_DOWN:
        case EFFECT_ACCURACY_DOWN:
        case EFFECT_EVASION_DOWN:
        case EFFECT_MIST:
        case EFFECT_FOCUS_ENERGY:
        case EFFECT_ATTACK_UP_2:
        case EFFECT_DEFENSE_UP_2:
        case EFFECT_SPEED_UP_2:
        case EFFECT_SPECIAL_ATTACK_UP_2:
        case EFFECT_SPECIAL_DEFENSE_UP_2:
        case EFFECT_ACCURACY_UP_2:
        case EFFECT_EVASION_UP_2:
        case EFFECT_ATTACK_DOWN_2:
        case EFFECT_DEFENSE_DOWN_2:
        case EFFECT_SPEED_DOWN_2:
        case EFFECT_SPECIAL_ATTACK_DOWN_2:
        case EFFECT_SPECIAL_DEFENSE_DOWN_2:
        case EFFECT_ACCURACY_DOWN_2:
        case EFFECT_EVASION_DOWN_2:
        case EFFECT_PAIN_SPLIT:
        case EFFECT_PERISH_SONG:
        case EFFECT_SAFEGUARD:
        case EFFECT_TICKLE:
        case EFFECT_COSMIC_POWER:
        case EFFECT_BULK_UP:
        case EFFECT_CALM_MIND:
        case EFFECT_DRAGON_DANCE:
        case EFFECT_POISON:
            ADJUST_SCORE(-2);
            break;
        default:
            break;
        }
    }
    else
    {
        // Don't use status moves on a target with little HP left.
        if (IsBattleMoveStatus(move))
            ADJUST_SCORE(-2);
    }
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_PREDICT_SWITCH: when the AI expects the target to switch out,
// prefer moves that make use of the free turn and avoid moves that would be
// wasted on the mon coming in
// ---------------------------------------------------------------------------

static s32 AI_PredictSwitch(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    struct AiLogicData *aiData = AI_DATA;
    s32 unmodifiedScore = score;
    u8 moveIndex = AI_THINKING_STRUCT->movesetIndex;
    uq4_12_t effectiveness = aiData->effectiveness[battlerAtk][battlerDef][moveIndex];
    bool32 opposingHazards = (gSideStatuses[GET_BATTLER_SIDE(battlerDef)] & SIDE_STATUS_SPIKES) != 0;
    bool32 aiHazards = (gSideStatuses[GET_BATTLER_SIDE(battlerAtk)] & SIDE_STATUS_SPIKES) != 0;
    u32 hitsToKO;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_PURSUIT:
        hitsToKO = GetNoOfHitsToKOBattler(battlerAtk, battlerDef, moveIndex, AI_ATTACKING, CONSIDER_ENDURE);
        if (hitsToKO == 2)
            ADJUST_SCORE(GOOD_EFFECT);
        else if (hitsToKO == 1)
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_FOCUS_PUNCH:
        ADJUST_SCORE(DECENT_EFFECT);
        if (AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_CHECK_BAD_MOVE)
        {
            if (aiData->abilities[battlerDef] == ABILITY_WONDER_GUARD && effectiveness < AI_EFFECTIVENESS_2)
                ADJUST_SCORE(10);
            if (HasDamagingMove(battlerDef)
             && !((gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE)
              || IsBattlerIncapacitated(battlerDef, aiData->abilities[battlerDef])
              || (gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION))))
                ADJUST_SCORE(10);
        }
        break;
    case EFFECT_LIGHT_SCREEN:
    case EFFECT_REFLECT:
    case EFFECT_SPIKES:
        ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_FUTURE_SIGHT:
    case EFFECT_RAIN_DANCE:
    case EFFECT_SUNNY_DAY:
    case EFFECT_SANDSTORM:
    case EFFECT_HAIL:
        ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_ROAR:
        if (opposingHazards)
            ADJUST_SCORE(GOOD_EFFECT);
        break;
    case EFFECT_RAPID_SPIN:
        if (aiHazards)
            ADJUST_SCORE(BEST_EFFECT);
        break;
    case EFFECT_WISH:
    case EFFECT_HEAL_BELL:
        if (ShouldUseWishAromatherapy(battlerAtk, battlerDef, move))
            ADJUST_SCORE(DECENT_EFFECT);
        break;
    case EFFECT_RESTORE_HP:
        if (aiData->hpPercents[battlerAtk] < 60)
            ADJUST_SCORE(GOOD_EFFECT);
        break;

    // Fail or are wasted if the target switches.
    case EFFECT_PROTECT:
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
    case EFFECT_ENCORE:
    case EFFECT_FOLLOW_ME:
    case EFFECT_DISABLE:
    case EFFECT_ENDURE:
    case EFFECT_HAZE:
    case EFFECT_MAGIC_COAT:
    case EFFECT_SNATCH:
        ADJUST_SCORE(WORST_EFFECT);
        break;

    // Get the AI stuck facing the mon coming in.
    case EFFECT_IMPRISON:
    case EFFECT_TAUNT:
    case EFFECT_INGRAIN:
    case EFFECT_MEAN_LOOK:
    case EFFECT_TRAP:
        ADJUST_SCORE(AWFUL_EFFECT);
        break;
    default:
        break;
    }

    // Like the expansion AI, always do the work above, so the AI's thinking
    // time doesn't show whether it expects a switch.
    if (!IsBattlerPredictedToSwitch(battlerDef))
        return unmodifiedScore;
    return score;
}

// ---------------------------------------------------------------------------
// AI_LOGIC_DOUBLE_BATTLE: work with the partner in double battles. The Gen 3
// part of the expansion AI's AI_DoubleBattle.
// ---------------------------------------------------------------------------

// Hits the partner may take from friendly fire, as in the expansion AI.
#define FRIENDLY_FIRE_RISKY_THRESHOLD  2
#define FRIENDLY_FIRE_NORMAL_THRESHOLD 3

// Does hitting the partner with this ability's type help it? Volt Absorb and
// Water Absorb heal it (only an HP aware AI counts on that), Flash Fire powers
// up its Fire moves.
static bool32 ShouldTriggerPartnerAbility(u8 partner, u8 ability)
{
    switch (ability)
    {
    case ABILITY_VOLT_ABSORB:
    case ABILITY_WATER_ABSORB:
        return (AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_HP_AWARE) != 0;
    case ABILITY_FLASH_FIRE:
        return HasMoveWithType(partner, TYPE_FIRE) && !(gBattleResources->flags->flags[partner] & RESOURCE_FLAG_FLASH_FIRE);
    default:
        return FALSE;
    }
}

static s32 AI_DoubleBattle(u8 battlerAtk, u8 battlerDef, u16 move, s32 score)
{
    struct AiLogicData *aiData = AI_DATA;
    u8 effect = gBattleMoves[move].effect;
    u8 moveTarget = gBattleMoves[move].target;
    u8 moveType = GetBattleMoveType(move, battlerAtk);
    u8 moveIndex = AI_THINKING_STRUCT->movesetIndex;
    u8 partner = BATTLE_PARTNER(battlerAtk);
    u8 partnerAbility = aiData->abilities[partner];
    u8 partnerHoldEffect = aiData->holdEffects[partner];
    u8 partnerEffect = gBattleMoves[aiData->partnerMove].effect;
    u16 predictedMove = GetIncomingMove(battlerAtk, battlerDef);
    bool32 hasPartner = HasPartner(battlerAtk);
    bool32 hasTwoOpponents = HasTwoOpponents(battlerAtk);
    bool32 partnerProtecting = hasPartner && aiData->partnerMove != MOVE_NONE && partnerEffect == EFFECT_PROTECT;
    u32 friendlyFireThreshold = (AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_RISKY) ? FRIENDLY_FIRE_RISKY_THRESHOLD : FRIENDLY_FIRE_NORMAL_THRESHOLD;
    u32 hitsToKOPartner = hasPartner ? GetNoOfHitsToKOBattler(battlerAtk, partner, moveIndex, AI_ATTACKING, CONSIDER_ENDURE) : 0;
    bool32 wouldPartnerFaint = hasPartner && CanIndexMoveFaintTarget(battlerAtk, partner, moveIndex, AI_ATTACKING) && !partnerProtecting;
    bool32 isFriendlyFireOK = !wouldPartnerFaint && (hitsToKOPartner == 0 || hitsToKOPartner > friendlyFireThreshold);
    bool32 isMoveAffectedByPartnerAbility;
    u32 ownHits1, ownHits2, partnerHits1, partnerHits2;

    // What the partner does this turn.
    if (aiData->partnerMove != MOVE_NONE && hasPartner)
    {
        // Weather, screens, Haze and so on: once is enough.
        if (AreMovesEquivalent(battlerAtk, battlerDef, partner, move, aiData->partnerMove))
            ADJUST_SCORE(-10);

        switch (partnerEffect)
        {
        case EFFECT_HELPING_HAND:
            if (IsBattleMoveStatus(move))
                ADJUST_SCORE(-7);
            break;
        case EFFECT_PERISH_SONG:
            // Trap the target so Perish Song can't be switched out of.
            if (!(gBattleMons[battlerDef].status2 & (STATUS2_ESCAPE_PREVENTION | STATUS2_WRAPPED))
             && (IsTrappingMove(move) || gBattleMoves[predictedMove].effect == EFFECT_INGRAIN))
                ADJUST_SCORE(WEAK_EFFECT);
            break;
        default:
            break;
        }
    }

    // Our move compared with the partner's situation.
    switch (effect)
    {
    case EFFECT_HELPING_HAND:
        if (!hasPartner || !HasDamagingMove(partner)
         || (aiData->partnerMove != MOVE_NONE && IsBattleMoveStatus(aiData->partnerMove)))
        {
            ADJUST_SCORE(WORST_EFFECT);
        }
        else
        {
            ownHits1 = GetBestNoOfHitsToKO(battlerAtk, LEFT_FOE(battlerAtk), AI_ATTACKING);
            partnerHits1 = GetBestNoOfHitsToKO(partner, LEFT_FOE(battlerAtk), AI_ATTACKING);
            ownHits2 = GetBestNoOfHitsToKO(battlerAtk, RIGHT_FOE(battlerAtk), AI_ATTACKING);
            partnerHits2 = GetBestNoOfHitsToKO(partner, RIGHT_FOE(battlerAtk), AI_ATTACKING);

            if (hasTwoOpponents)
            {
                // It may faint before doing anything else.
                if (CanTargetFaintAi(LEFT_FOE(battlerAtk), battlerAtk) && CanTargetFaintAi(RIGHT_FOE(battlerAtk), battlerAtk)
                 && AI_IsSlower(battlerAtk, LEFT_FOE(battlerAtk), move, predictedMove, DONT_CONSIDER_PRIORITY)
                 && AI_IsSlower(battlerAtk, RIGHT_FOE(battlerAtk), move, predictedMove, DONT_CONSIDER_PRIORITY))
                    ADJUST_SCORE(GOOD_EFFECT);
                // The partner hits much harder.
                if (ownHits1 > partnerHits1 && partnerHits1 > 1 && ownHits2 > partnerHits2 && partnerHits2 > 1)
                    ADJUST_SCORE(GOOD_EFFECT);
            }
            else if (IsBattlerAlive(LEFT_FOE(battlerAtk)))
            {
                if (CanTargetFaintAi(LEFT_FOE(battlerAtk), battlerAtk)
                 && AI_IsSlower(battlerAtk, LEFT_FOE(battlerAtk), move, predictedMove, DONT_CONSIDER_PRIORITY))
                    ADJUST_SCORE(GOOD_EFFECT);
                if (ownHits1 > partnerHits1 && partnerHits1 > 1)
                    ADJUST_SCORE(GOOD_EFFECT);
            }
            else if (IsBattlerAlive(RIGHT_FOE(battlerAtk)))
            {
                if (CanTargetFaintAi(RIGHT_FOE(battlerAtk), battlerAtk)
                 && AI_IsSlower(battlerAtk, RIGHT_FOE(battlerAtk), move, predictedMove, DONT_CONSIDER_PRIORITY))
                    ADJUST_SCORE(GOOD_EFFECT);
                if (ownHits2 > partnerHits2 && partnerHits2 > 1)
                    ADJUST_SCORE(GOOD_EFFECT);
            }
        }
        break;
    case EFFECT_PERISH_SONG:
        if (aiData->partnerMove != MOVE_NONE && HasTrappingMoveEffect(partner))
            ADJUST_SCORE(WEAK_EFFECT);
        break;
    default:
        break;
    }

    // Moves that hit both foes and the partner (Earthquake, Magnitude,
    // Explosion, Selfdestruct): don't knock out the partner for no reason.
    if ((moveTarget & MOVE_TARGET_FOES_AND_ALLY) && wouldPartnerFaint)
    {
        bool32 faintsFoe1 = CanIndexMoveFaintTarget(battlerAtk, BATTLE_OPPOSITE(battlerAtk), moveIndex, AI_ATTACKING);
        bool32 faintsFoe2 = CanIndexMoveFaintTarget(battlerAtk, BATTLE_OPPOSITE(partner), moveIndex, AI_ATTACKING);

        if (faintsFoe1 && faintsFoe2)
        {
            // Knocking out both foes is a good reason.
            if (!IsExplosionMove(move) || (AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_RISKY))
                RETURN_SCORE_PLUS(10);
        }
        else if (CanTargetFaintAi(BATTLE_OPPOSITE(battlerAtk), partner) && CanTargetFaintAi(BATTLE_OPPOSITE(partner), partner))
        {
            // The partner is going down anyway: take a foe with it.
            if (faintsFoe1 || faintsFoe2)
                ADJUST_SCORE(WEAK_EFFECT);
        }
        else if (faintsFoe1 || faintsFoe2)
        {
            ADJUST_SCORE(AWFUL_EFFECT); // a trade the player can easily exploit
        }
        else
        {
            RETURN_SCORE_MINUS(10);
        }
    }

    // Moves aimed at the partner.
    if (IsTargetingPartner(battlerAtk, battlerDef))
    {
        isMoveAffectedByPartnerAbility = TRUE;

        if (wouldPartnerFaint)
            RETURN_SCORE_MINUS(30);

        if (!partnerProtecting && !(moveTarget & MOVE_TARGET_BOTH))
        {
            switch (partnerAbility)
            {
            case ABILITY_LIGHTNING_ROD:
                // In Gen 3 Lightning Rod only draws the move in; the partner still takes it.
                if (moveType == TYPE_ELECTRIC)
                    RETURN_SCORE_MINUS(10);
                isMoveAffectedByPartnerAbility = FALSE;
                break;
            case ABILITY_VOLT_ABSORB:
            case ABILITY_WATER_ABSORB:
                if (moveType == (partnerAbility == ABILITY_VOLT_ABSORB ? TYPE_ELECTRIC : TYPE_WATER))
                {
                    if (moveTarget & MOVE_TARGET_FOES_AND_ALLY)
                        ADJUST_SCORE(DECENT_EFFECT);
                    else if (ShouldTriggerPartnerAbility(partner, partnerAbility))
                        RETURN_SCORE_PLUS(WEAK_EFFECT);
                    else
                        RETURN_SCORE_MINUS(10);
                }
                else
                {
                    isMoveAffectedByPartnerAbility = FALSE;
                }
                break;
            case ABILITY_LEVITATE:
                if (moveType == TYPE_GROUND)
                {
                    if (moveTarget & MOVE_TARGET_FOES_AND_ALLY)
                        ADJUST_SCORE(DECENT_EFFECT);
                }
                else
                {
                    isMoveAffectedByPartnerAbility = FALSE;
                }
                break;
            case ABILITY_FLASH_FIRE:
                if (moveType == TYPE_FIRE)
                {
                    if (moveTarget & MOVE_TARGET_FOES_AND_ALLY)
                        ADJUST_SCORE(DECENT_EFFECT);
                    if (ShouldTriggerPartnerAbility(partner, partnerAbility))
                        RETURN_SCORE_PLUS(WEAK_EFFECT);
                }
                else
                {
                    isMoveAffectedByPartnerAbility = FALSE;
                }
                break;
            default:
                break;
            }
        }

        // Moves that help the partner.
        if (!partnerProtecting)
        {
            switch (effect)
            {
            case EFFECT_SWAGGER:
                if (gBattleMons[partner].statStages[STAT_ATK] < MAX_STAT_STAGE
                 && HasMoveWithCategory(partner, DAMAGE_CATEGORY_PHYSICAL)
                 && (!AI_CanConfuse(battlerAtk, partner, partnerAbility, move)
                  || partnerHoldEffect == HOLD_EFFECT_CURE_CONFUSION
                  || partnerHoldEffect == HOLD_EFFECT_CURE_STATUS))
                    RETURN_SCORE_PLUS(WEAK_EFFECT);
                break;
            case EFFECT_FLATTER:
                if (gBattleMons[partner].statStages[STAT_SPATK] < MAX_STAT_STAGE
                 && HasMoveWithCategory(partner, DAMAGE_CATEGORY_SPECIAL)
                 && (!AI_CanConfuse(battlerAtk, partner, partnerAbility, move)
                  || partnerHoldEffect == HOLD_EFFECT_CURE_CONFUSION
                  || partnerHoldEffect == HOLD_EFFECT_CURE_STATUS))
                    RETURN_SCORE_PLUS(WEAK_EFFECT);
                break;
            default:
                break;
            }
        }

        if (!isMoveAffectedByPartnerAbility || score <= AI_SCORE_DEFAULT)
            RETURN_SCORE_MINUS(10);
    }

    return score;
}
